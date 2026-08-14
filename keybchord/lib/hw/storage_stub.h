#pragma once

#include <string>
#include <unordered_map>
#include "base.h"


class StorageStub : public StorageAdapter {
public:
    bool begin() override { return true; }

    bool exists(const std::string& path) override {
        return store_.find(path) != store_.end();
    }

    std::string readFile(const std::string& path) override {
        auto it = store_.find(path);
        if (it != store_.end()) return it->second;
        return "";
    }

    bool writeFile(const std::string& path, const std::string& data) override {
        store_[path] = data;
        return true;
    }

    bool mkdir(const std::string&) override { return true; }

    void clear() { store_.clear(); }

private:
    std::unordered_map<std::string, std::string> store_;
};


