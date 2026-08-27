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

    std::vector<std::string> listFiles(const std::string& dir) override {
        std::vector<std::string> out;
        std::string prefix = dir;
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        for (const auto& kv : store_) {
            if (kv.first.rfind(prefix, 0) == 0) {
                std::string name = kv.first.substr(prefix.size());
                // Skip nested paths (no further '/'), returning bare basenames.
                if (name.find('/') == std::string::npos) out.push_back(name);
            }
        }
        return out;
    }

    void clear() { store_.clear(); }

private:
    std::unordered_map<std::string, std::string> store_;
};


