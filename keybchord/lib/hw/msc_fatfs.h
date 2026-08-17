#pragma once

#include <cstdint>

// Bridges the onboard FatFS (FAT filesystem over the RP2040 flash, via the
// arduino-pico FatFS library) to the native USB port as a Mass Storage Class
// drive (Adafruit TinyUSB MSC). Lets a PC drag-drop edit config.json, presets,
// and rhythms. The keyboard host remains on the separate PIO-USB port.
//
// Concurrency: while the host has the drive mounted the firmware must not write
// to FatFS; on eject (Start-Stop load_eject) the FatFS cache is remounted so the
// firmware sees the host's edits.
class MscFatFs {
public:
    // Registers the MSC interface and the read/write callbacks. Call after the
    // FatFS filesystem is mounted (StorageFatFs::begin()).
    bool begin();

    // Present / withdraw the drive (unit ready). Default is not-ready (the
    // drive is only exposed when the boot-key requests it).
    void setDriveEnabled(bool on);
    bool driveEnabled() const { return enabled_; }

private:
    static MscFatFs* instance_;

    bool enabled_ = false;

    static int32_t readCb(uint32_t lba, void* buffer, uint32_t bufsize);
    static int32_t writeCb(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
    static void flushCb();
    static bool startStopCb(uint8_t power_condition, bool start, bool load_eject);
};
