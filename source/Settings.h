#pragma once

// settings.json - general / appearance / environment / recovery options.
// Corrupted files are never silently overwritten: a .bak copy is kept and the
// UI offers "Restore Backup" / "Reset Configuration".

#include "Common.h"
#include "Logger.h"
#include <json.hpp>

using json = nlohmann::json;

struct Settings {
    // General
    bool confirmBeforeLaunch = true;
    bool autoRecovery = true;
    bool rememberLastProfile = true;
    std::string lastProfileId;

    // Appearance
    std::string theme = "midnight";
    bool animations = true;
    int uiScale = 1; // 1 = normal, 2 = compact list density
    bool transparency = false;

    // Environment
    std::string atmospherePath = DEFAULT_ATMOSPHERE_PATH;
    std::string contentsPath = DEFAULT_CONTENTS_PATH;
    std::string exefsPatchesPath = DEFAULT_EXEFS_PATCHES_PATH;
    std::string targetCommandLine = DEFAULT_TARGET_CMDLINE;
    std::string backupPath = CFG_BACKUPDIR;

    // Recovery
    bool automaticRecovery = true;
    bool verifyFiles = true;
    bool createBackup = true;

    // Logs
    bool debugLogging = false;

    json toJson() const {
        return json{
            {"general",
             {{"confirmBeforeLaunch", confirmBeforeLaunch},
              {"autoRecovery", autoRecovery},
              {"rememberLastProfile", rememberLastProfile},
              {"lastProfileId", lastProfileId}}},
            {"appearance",
             {{"theme", theme},
              {"animations", animations},
              {"uiScale", uiScale},
              {"transparency", transparency}}},
            {"environment",
             {{"atmospherePath", atmospherePath},
              {"contentsPath", contentsPath},
              {"exefsPatchesPath", exefsPatchesPath},
              {"targetCommandLine", targetCommandLine},
              {"backupPath", backupPath}}},
            {"recovery",
             {{"automaticRecovery", automaticRecovery},
              {"verifyFiles", verifyFiles},
              {"createBackup", createBackup}}},
            {"logs", {{"debug", debugLogging}}}};
    }

    void fromJson(const json &j) {
        auto g = j.value("general", json::object());
        confirmBeforeLaunch = g.value("confirmBeforeLaunch", true);
        autoRecovery = g.value("autoRecovery", true);
        rememberLastProfile = g.value("rememberLastProfile", true);
        lastProfileId = g.value("lastProfileId", std::string());

        auto a = j.value("appearance", json::object());
        theme = a.value("theme", std::string("midnight"));
        animations = a.value("animations", true);
        uiScale = a.value("uiScale", 1);
        transparency = a.value("transparency", false);

        auto e = j.value("environment", json::object());
        atmospherePath = e.value("atmospherePath", std::string(DEFAULT_ATMOSPHERE_PATH));
        contentsPath = e.value("contentsPath", std::string(DEFAULT_CONTENTS_PATH));
        exefsPatchesPath = e.value("exefsPatchesPath", std::string(DEFAULT_EXEFS_PATCHES_PATH));
        targetCommandLine = e.value("targetCommandLine", std::string(DEFAULT_TARGET_CMDLINE));
        backupPath = e.value("backupPath", std::string(CFG_BACKUPDIR));

        auto r = j.value("recovery", json::object());
        automaticRecovery = r.value("automaticRecovery", true);
        verifyFiles = r.value("verifyFiles", true);
        createBackup = r.value("createBackup", true);

        debugLogging = j.value("logs", json::object()).value("debug", false);
    }
};

class SettingsStore {
  public:
    static SettingsStore &get() {
        static SettingsStore s;
        return s;
    }

    Settings data;
    bool corrupted = false;

    bool load() {
        corrupted = false;
        if (!fileExists(CFG_SETTINGS)) {
            save();
            return true;
        }
        bool ok = false;
        std::string raw = readWholeFile(CFG_SETTINGS, &ok);
        if (!ok) {
            LOGE(std::string(Err::FS_002.id) + " settings.json could not be read");
            corrupted = true;
            return false;
        }
        try {
            data.fromJson(json::parse(raw));
        } catch (const std::exception &e) {
            LOGE(std::string(Err::CONFIG_001.id) + " settings.json parse error: " + e.what());
            corrupted = true;
            return false;
        }
        Logger::get().setDebug(data.debugLogging);
        return true;
    }

    bool save() {
        ensureDir(CFG_ROOT);
        if (fileExists(CFG_SETTINGS))
            copyToBackup();
        bool ok = writeWholeFile(CFG_SETTINGS, data.toJson().dump(2));
        if (!ok)
            LOGE(std::string(Err::FS_002.id) + " failed to write settings.json");
        return ok;
    }

    bool restoreBackup() {
        bool ok = false;
        std::string raw = readWholeFile(std::string(CFG_SETTINGS) + ".bak", &ok);
        if (!ok)
            return false;
        try {
            Settings s;
            s.fromJson(json::parse(raw));
            data = s;
        } catch (...) {
            return false;
        }
        corrupted = false;
        return save();
    }

    void reset() {
        data = Settings();
        corrupted = false;
        save();
        LOGW("Settings reset to defaults");
    }

  private:
    void copyToBackup() {
        bool ok = false;
        std::string cur = readWholeFile(CFG_SETTINGS, &ok);
        if (ok && !cur.empty())
            writeWholeFile(std::string(CFG_SETTINGS) + ".bak", cur);
    }
};

inline Settings &settings() { return SettingsStore::get().data; }
