#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "base.h"
#include "hid_usage.h"

// Minimal HID Consumer Control report parser.
//
// The CH559 sends each interface's HID report descriptor in a MSG_HID_INFO
// frame and raw input reports in MSG_DEVICE_POLL frames. This parser uses the
// descriptor to map report bits/array entries to Consumer-page usages, then
// diffs consecutive reports into KeyEvents (so volume/media keys get proper
// press and release edges).
//
// It intentionally supports the common variable-bitmap and array report forms
// used by keyboards. Unsupported/unknown reports are ignored rather than
// producing bogus key events. Pure C++ so it is unit-testable on the native host.
namespace hid_consumer {

struct Field {
    uint8_t  reportId     = 0;
    uint32_t bitOffset    = 0;
    uint8_t  reportSize   = 0;
    uint16_t reportCount  = 0;
    bool     variable     = false;
    bool     hasRange     = false;
    uint16_t usageMin     = 0;
    uint16_t usageMax     = 0;
    uint32_t logicalMin   = 0;
    std::vector<uint16_t> usages;  // explicit variable usages, in bit order
};

class ReportParser {
public:
    void reset() {
        fields_.clear();
        hasReportIds_ = false;
        prev_.clear();
    }

    // Parses an Input-item subset of a HID report descriptor. Only Consumer-page
    // fields are retained. Returns true if at least one consumer field was found.
    bool setDescriptor(const uint8_t* data, size_t length) {
        reset();
        if (!data || length == 0) return false;

        uint16_t usagePage = 0;
        uint8_t reportId = 0;
        uint8_t reportSize = 0;
        uint16_t reportCount = 0;
        uint32_t logicalMin = 0;

        bool hasUsageMin = false, hasUsageMax = false;
        uint16_t usageMin = 0, usageMax = 0;
        std::vector<uint16_t> usages;

        std::vector<ReportOffset> offsets;

        size_t i = 0;
        while (i < length) {
            uint8_t prefix = data[i++];
            if (prefix == 0xFE) {  // long item: [prefix][size][tag][data...]
                if (i + 1 >= length) break;
                size_t size = data[i++];
                i++;  // long-item tag
                if (i + size > length) break;
                i += size;
                continue;
            }

            size_t sizeCode = prefix & 0x03;
            size_t size = (sizeCode == 3) ? 4 : sizeCode;
            if (i + size > length) break;
            uint8_t tag = static_cast<uint8_t>(prefix >> 4);
            uint8_t type = static_cast<uint8_t>((prefix >> 2) & 0x03);

            uint32_t value = 0;
            for (size_t b = 0; b < size; b++) {
                value |= static_cast<uint32_t>(data[i + b]) << (8 * b);
            }
            i += size;

            if (type == 0) {  // Main item
                if (tag == 0x8) {  // Input
                    uint32_t& offset = offsetFor(offsets, reportId);
                    bool constant = (value & 0x01) != 0;
                    bool variable = (value & 0x02) != 0;

                    if (!constant && usagePage == hid_usage::PAGE_CONSUMER &&
                        reportSize > 0 && reportCount > 0) {
                        Field f;
                        f.reportId = reportId;
                        f.bitOffset = offset;
                        f.reportSize = reportSize;
                        f.reportCount = reportCount;
                        f.variable = variable;
                        f.hasRange = hasUsageMin;
                        f.usageMin = usageMin;
                        f.usageMax = hasUsageMax ? usageMax : usageMin;
                        f.logicalMin = logicalMin;
                        f.usages = usages;
                        if (f.usages.empty() && !f.hasRange) {
                            // No local usage information: cannot decode it.
                        } else {
                            if (reportId != 0) hasReportIds_ = true;
                            fields_.push_back(std::move(f));
                        }
                    }

                    offset += static_cast<uint32_t>(reportSize) * reportCount;
                }
                // Locals apply only to the next main item.
                usages.clear();
                hasUsageMin = false;
                hasUsageMax = false;
                usageMin = usageMax = 0;
            } else if (type == 1) {  // Global item
                switch (tag) {
                    case 0x0: usagePage = static_cast<uint16_t>(value); break;
                    case 0x1: logicalMin = value; break;
                    case 0x7: reportSize = static_cast<uint8_t>(value); break;
                    case 0x8:
                        reportId = static_cast<uint8_t>(value);
                        if (reportId != 0) hasReportIds_ = true;
                        break;
                    case 0x9: reportCount = static_cast<uint16_t>(value); break;
                    default: break;
                }
            } else if (type == 2) {  // Local item
                switch (tag) {
                    case 0x0: usages.push_back(static_cast<uint16_t>(value)); break;
                    case 0x1:
                        usageMin = static_cast<uint16_t>(value);
                        hasUsageMin = true;
                        break;
                    case 0x2:
                        usageMax = static_cast<uint16_t>(value);
                        hasUsageMax = true;
                        break;
                    default: break;
                }
            }
        }

        return !fields_.empty();
    }

    // True when `report` looks like a Consumer report described by this
    // descriptor. Used to disentangle Consumer reports (report ID + length)
    // from keyboard boot reports on QMK-style shared endpoints, where a boot
    // keyboard report's modifier byte can otherwise look like a report ID.
    bool matchesReport(const uint8_t* report, uint16_t length) const {
        if (!report || fields_.empty()) return false;

        uint8_t reportId = 0;
        if (hasReportIds_) {
            if (length == 0) return false;
            reportId = report[0];
        }

        uint32_t bits = 0;
        bool found = false;
        for (const Field& f : fields_) {
            if (f.reportId != reportId) continue;
            found = true;
            uint32_t end = f.bitOffset +
                           static_cast<uint32_t>(f.reportSize) * f.reportCount;
            if (end > bits) bits = end;
        }
        if (!found) return false;

        size_t expected = static_cast<size_t>((bits + 7) / 8);
        if (hasReportIds_) expected++;
        return length == expected;
    }

    // Fallback for QMK's standard Consumer report when the report descriptor
    // was not available/matched: one report-ID byte followed by one 16-bit
    // usage. Accept any report ID because QMK report IDs vary by build; normal
    // boot keyboard reports are 8 bytes, so a 3-byte report here is a Consumer
    // (or System Control) usage. Only Consumer-page-range usages are emitted.
    bool applyQmkConsumer(const uint8_t* report, uint16_t length,
                          uint64_t received_us, std::vector<KeyEvent>& out) {
        if (!report || length != 3) return false;

        uint16_t usage = static_cast<uint16_t>(report[1]) |
                         (static_cast<uint16_t>(report[2]) << 8);
        if (usage > 0x2A0) return false;
        std::vector<uint16_t> current;
        addUnique(current, usage);

        std::vector<uint16_t>& prev = prevFor(prev_, report[0]);
        for (uint16_t u : current) {
            if (!contains(prev, u)) {
                out.push_back({u, true, 0, received_us,
                               hid_usage::PAGE_CONSUMER});
            }
        }
        for (uint16_t u : prev) {
            if (!contains(current, u)) {
                out.push_back({u, false, 0, received_us,
                               hid_usage::PAGE_CONSUMER});
            }
        }
        prev = current;
        return true;
    }

    bool apply(const uint8_t* report, uint16_t length, uint64_t received_us,
               std::vector<KeyEvent>& out) {
        if (!report || length == 0 || fields_.empty()) return false;

        size_t dataOffset = 0;
        uint8_t reportId = 0;
        if (hasReportIds_) {
            reportId = report[0];
            dataOffset = 1;
        }
        if (dataOffset >= length) return false;

        std::vector<uint16_t> current;
        for (const Field& f : fields_) {
            if (f.reportId != reportId) continue;

            if (f.variable) {
                for (uint16_t n = 0; n < f.reportCount; n++) {
                    uint32_t bit = f.bitOffset +
                                   static_cast<uint32_t>(n) * f.reportSize;
                    uint32_t v = readBits(report, length, dataOffset, bit,
                                          f.reportSize);
                    if (v == 0) continue;

                    uint16_t usage = 0;
                    if (n < f.usages.size()) {
                        usage = f.usages[n];
                    } else if (f.hasRange) {
                        usage = static_cast<uint16_t>(f.usageMin + n);
                    }
                    addUnique(current, usage);
                }
            } else if (f.reportSize <= 16) {
                for (uint16_t n = 0; n < f.reportCount; n++) {
                    uint32_t bit = f.bitOffset +
                                   static_cast<uint32_t>(n) * f.reportSize;
                    uint32_t v = readBits(report, length, dataOffset, bit,
                                          f.reportSize);

                    uint16_t usage = 0;
                    if (v >= f.logicalMin) {
                        uint32_t index = v - f.logicalMin;
                        if (!f.usages.empty()) {
                            // Array value indexes the local usage list. The
                            // index is relative to the logical minimum.
                            if (index < f.usages.size()) {
                                usage = f.usages[index];
                            }
                        } else if (f.hasRange) {
                            usage = static_cast<uint16_t>(f.usageMin + index);
                            if (usage > f.usageMax) usage = 0;
                        }
                    }
                    addUnique(current, usage);
                }
            }
        }

        std::vector<uint16_t>& prev = prevFor(prev_, reportId);
        for (uint16_t usage : current) {
            if (!contains(prev, usage)) {
                out.push_back({usage, true, 0, received_us,
                               hid_usage::PAGE_CONSUMER});
            }
        }
        for (uint16_t usage : prev) {
            if (!contains(current, usage)) {
                out.push_back({usage, false, 0, received_us,
                               hid_usage::PAGE_CONSUMER});
            }
        }
        prev = current;
        return true;
    }

private:
    struct ReportOffset {
        uint8_t id = 0;
        uint32_t bits = 0;
    };
    struct PrevReport {
        uint8_t id = 0;
        std::vector<uint16_t> usages;
    };

    std::vector<Field> fields_;
    bool hasReportIds_ = false;
    std::vector<PrevReport> prev_;

    static uint32_t& offsetFor(std::vector<ReportOffset>& offsets, uint8_t id) {
        for (auto& o : offsets) {
            if (o.id == id) return o.bits;
        }
        offsets.push_back({id, 0});
        return offsets.back().bits;
    }

    static std::vector<uint16_t>& prevFor(std::vector<PrevReport>& prevs,
                                          uint8_t id) {
        for (auto& p : prevs) {
            if (p.id == id) return p.usages;
        }
        prevs.push_back({id, {}});
        return prevs.back().usages;
    }

    static bool contains(const std::vector<uint16_t>& v, uint16_t value) {
        for (uint16_t x : v) {
            if (x == value) return true;
        }
        return false;
    }

    static void addUnique(std::vector<uint16_t>& v, uint16_t value) {
        if (value == 0) return;
        if (!contains(v, value)) v.push_back(value);
    }

    static uint32_t readBits(const uint8_t* data, size_t length, size_t offset,
                             uint32_t bit, uint8_t size) {
        if (size == 0 || size > 32) return 0;
        if (static_cast<uint64_t>(offset) * 8 + bit + size >
            static_cast<uint64_t>(length) * 8) {
            return 0;
        }
        uint32_t out = 0;
        for (uint8_t i = 0; i < size; i++) {
            uint64_t abs = static_cast<uint64_t>(offset) * 8 + bit + i;
            size_t byte = static_cast<size_t>(abs / 8);
            uint8_t b = static_cast<uint8_t>((data[byte] >> (abs % 8)) & 0x01);
            out |= static_cast<uint32_t>(b) << i;
        }
        return out;
    }
};

}  // namespace hid_consumer
