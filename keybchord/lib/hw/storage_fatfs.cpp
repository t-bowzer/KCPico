#include "storage_fatfs.h"

#ifndef KEYBCHORD_NATIVE

#include <FatFS.h>
#include <ff.h>
#include <cstring>

#include "debug_log.h"
#include "defaults.h"


bool StorageFatFs::begin() {
    if (!FatFS.begin()) {
        logError("FatFS: mount failed");
        return false;
    }

    // First boot: the filesystem is empty/unformatted. Self-provision the
    // default config, presets, and rhythms (NFR-9) so they are editable via the
    // USB Mass Storage drive.
    if (!FatFS.exists("/config.json")) {
        logInfo("FatFS: empty filesystem, provisioning defaults");
        provisionDefaults(*this);
    }

    // Friendly volume label ("KeybChord" in the host OS). Read-then-set so it
    // self-heals on existing devices without an extra write on every boot.
    fatfs::TCHAR label[12] = {};
    fatfs::DWORD vsn = 0;
    if (fatfs::f_getlabel("", label, &vsn) == fatfs::FR_OK &&
        std::strcmp(label, "KeybChord") != 0) {
        fatfs::f_setlabel("KeybChord");
    }

    return true;
}

bool StorageFatFs::exists(const std::string& path) {
    return FatFS.exists(path.c_str());
}

std::string StorageFatFs::readFile(const std::string& path) {
    File f = FatFS.open(path.c_str(), "r");
    if (!f) return "";
    std::string result = f.readString().c_str();
    f.close();
    return result;
}

bool StorageFatFs::writeFile(const std::string& path, const std::string& data) {
    File f = FatFS.open(path.c_str(), "w");
    if (!f) return false;
    f.print(data.c_str());
    f.close();
    return true;
}

bool StorageFatFs::mkdir(const std::string& path) {
    if (!FatFS.exists(path.c_str())) {
        FatFS.mkdir(path.c_str());
    }
    return true;
}


#endif // !KEYBCHORD_NATIVE
