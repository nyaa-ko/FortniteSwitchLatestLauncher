#pragma once
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

class PatchSwitcher {
public:
    static bool IsEnabled(const std::string& path) {
        return fs::exists(path);
    }

    static void Toggle(const std::string& path) {
        std::string disabledPath = path + ".disabled";
        std::error_code ec;

        if (fs::exists(path)) {
            fs::rename(path, disabledPath, ec);
        } else if (fs::exists(disabledPath)) {
            fs::rename(disabledPath, path, ec);
        }
    }
};