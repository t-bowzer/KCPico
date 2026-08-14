#pragma once

#include "base.h"


class StorageLittleFs : public StorageAdapter {
public:
    bool begin() override;
    bool exists(const std::string& path) override;
    std::string readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::string& data) override;
    bool mkdir(const std::string& path) override;
};


