#pragma once

#include <cstdint>

// Minimal integer statistics for on-device latency/jitter measurement
// (NFR-1 / NFR-2). Pure logic, no hardware includes, unit-testable on the
// native host. Samples are non-negative microsecond values; the host feed it
// either a measured latency or an absolute timing deviation.
class PerfStats {
public:
    void reset() {
        count_ = 0;
        minUs_ = UINT32_MAX;
        maxUs_ = 0;
        sumUs_ = 0;
    }

    void record(uint32_t sampleUs) {
        if (sampleUs < minUs_) minUs_ = sampleUs;
        if (sampleUs > maxUs_) maxUs_ = sampleUs;
        sumUs_ += sampleUs;
        count_++;
    }

    uint32_t count() const { return count_; }
    uint32_t minUs()  const { return count_ == 0 ? 0 : minUs_; }
    uint32_t maxUs()  const { return count_ == 0 ? 0 : maxUs_; }
    uint32_t avgUs()  const { return count_ == 0 ? 0 : static_cast<uint32_t>(sumUs_ / count_); }

private:
    uint32_t count_ = 0;
    uint32_t minUs_ = UINT32_MAX;
    uint32_t maxUs_ = 0;
    uint64_t sumUs_ = 0;
};
