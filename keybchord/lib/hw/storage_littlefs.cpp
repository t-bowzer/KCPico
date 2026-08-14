#include "storage_littlefs.h"

#ifndef KEYBCHORD_NATIVE

#include <LittleFS.h>


bool StorageLittleFs::begin() {
    if (!LittleFS.begin()) {
        LittleFS.format();
        if (!LittleFS.begin()) {
            return false;
        }
    }
    return true;
}

bool StorageLittleFs::exists(const std::string& path) {
    return LittleFS.exists(path.c_str());
}

std::string StorageLittleFs::readFile(const std::string& path) {
    File f = LittleFS.open(path.c_str(), "r");
    if (!f) return "";
    std::string result = f.readString().c_str();
    f.close();
    return result;
}

bool StorageLittleFs::writeFile(const std::string& path, const std::string& data) {
    File f = LittleFS.open(path.c_str(), "w");
    if (!f) return false;
    f.print(data.c_str());
    f.close();
    return true;
}

bool StorageLittleFs::mkdir(const std::string& path) {
    if (!LittleFS.exists(path.c_str())) {
        LittleFS.mkdir(path.c_str());
    }
    return true;
}



#endif // !KEYBCHORD_NATIVE
