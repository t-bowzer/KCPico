#include "msc_fatfs.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include "arduino/msc/Adafruit_USBD_MSC.h"
#include <FatFS.h>
#include <ff.h>
#include <diskio.h>

#include "debug_log.h"


MscFatFs* MscFatFs::instance_ = nullptr;

namespace {

Adafruit_USBD_MSC* g_msc = nullptr;

} // namespace


bool MscFatFs::begin() {
    // Query the FatFS disk geometry (sector size + count) from the low-level
    // disk layer shared with FatFS.
    uint32_t sectorCount = 0;
    uint16_t sectorSize  = 512;
    fatfs::disk_ioctl(0, GET_SECTOR_COUNT, &sectorCount);
    fatfs::disk_ioctl(0, GET_SECTOR_SIZE, &sectorSize);
    if (sectorCount == 0 || sectorSize == 0) {
        logError("MSC: FatFS disk geometry unavailable");
        return false;
    }

    g_msc = new Adafruit_USBD_MSC();
    g_msc->setID("KeybChord", "KeybChord", "1.0");
    g_msc->setCapacity(0, sectorCount, sectorSize);
    g_msc->setReadWriteCallback(0, &MscFatFs::readCb, &MscFatFs::writeCb,
                                &MscFatFs::flushCb);
    g_msc->setStartStopCallback(0, &MscFatFs::startStopCb);
    g_msc->setUnitReady(0, false);   // drive hidden until the boot-key enables it

    if (!g_msc->begin()) {
        logError("MSC: failed to register interface");
        return false;
    }

    instance_ = this;
    logInfo("MSC: mass storage ready (disabled)");
    return true;
}

void MscFatFs::setDriveEnabled(bool on) {
    enabled_ = on;
    if (g_msc) g_msc->setUnitReady(0, on);
    if (on) {
        logInfo("MSC: drive presented");
    } else {
        logInfo("MSC: drive withdrawn");
    }
}

int32_t MscFatFs::readCb(uint32_t lba, void* buffer, uint32_t bufsize) {
    uint16_t sectorSize = 512;
    fatfs::disk_ioctl(0, GET_SECTOR_SIZE, &sectorSize);
    unsigned int count = bufsize / sectorSize;
    if (count == 0) return 0;
    return (fatfs::disk_read(0, static_cast<fatfs::BYTE*>(buffer), lba, count)
                == fatfs::RES_OK)
               ? static_cast<int32_t>(bufsize)
               : -1;
}

int32_t MscFatFs::writeCb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
    uint16_t sectorSize = 512;
    fatfs::disk_ioctl(0, GET_SECTOR_SIZE, &sectorSize);
    unsigned int count = bufsize / sectorSize;
    if (count == 0) return 0;
    return (fatfs::disk_write(0, buffer, lba, count) == fatfs::RES_OK)
               ? static_cast<int32_t>(bufsize)
               : -1;
}

void MscFatFs::flushCb() {
    fatfs::disk_ioctl(0, CTRL_SYNC, nullptr);
}

bool MscFatFs::startStopCb(uint8_t power_condition, bool start, bool load_eject) {
    (void)power_condition;
    // load_eject && !start == the host ejected the drive. Remount FatFS so the
    // firmware's cached FAT reflects any files the host wrote, and withdraw the
    // drive (unit not-ready) so the host does not immediately re-mount it.
    if (load_eject && !start) {
        FatFS.end();
        FatFS.begin();
        if (instance_) instance_->setDriveEnabled(false);
        logInfo("MSC: drive ejected, FatFS remounted");
    }
    return true;
}

#endif // !KEYBCHORD_NATIVE
