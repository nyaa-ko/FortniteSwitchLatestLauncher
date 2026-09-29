#pragma once

// ---------------------------------------------------------------------------
// Environment validation + transactional launch.
//
//   Validate -> Backup -> Prepare -> Launch -> Verify -> Restore
//
// The original file is NEVER deleted. It is copied to a timestamped backup
// first, a temporary environment is written on top, and the original content
// is restored afterwards (or on the next start, if the console jumped straight
// into the game / the launcher crashed).
//
// launcher_state.json records every step so an interrupted operation can be
// detected and recovered.
// ---------------------------------------------------------------------------

#include "Common.h"
#include "Logger.h"
#include "Profiles.h"
#include "Settings.h"
#include "UE4CommandLineManager.h"
#include <json.hpp>

using json = nlohmann::json;

enum class EnvLevel { Ready, Warning, RecoveryRequired };

struct EnvStatus {
    EnvLevel level = EnvLevel::Ready;
    bool atmosphereDetected = false;
    bool targetContentsPresent = false;
    bool commandLinePresent = false;
    bool backupReady = false;
    std::string previousOperation = "None";
    std::string detail;

    const char *label() const {
        switch (level) {
        case EnvLevel::Ready:
            return "READY";
        case EnvLevel::Warning:
            return "WARNING";
        default:
            return "RECOVERY REQUIRED";
        }
    }

    const char *color() const {
        switch (level) {
        case EnvLevel::Ready:
            return C_OK;
        case EnvLevel::Warning:
            return C_WARN;
        default:
            return C_ERR;
        }
    }
};

// --- Persistent operation state -------------------------------------------
struct LauncherState {
    std::string operation; // "launch" | ""
    std::string status;    // idle | validated | backup_created | prepared | launched | restored
    std::string profileId;
    std::string timestamp;
    std::string backupPath;
    std::string targetPath;

    bool pending() const {
        return !status.empty() && status != "idle" && status != "restored";
    }
};

class StateStore {
  public:
    static StateStore &get() {
        static StateStore s;
        return s;
    }

    LauncherState state;
    bool corrupted = false;

    void load() {
        corrupted = false;
        state = LauncherState();
        if (!fileExists(CFG_STATE))
            return;
        bool ok = false;
        std::string raw = readWholeFile(CFG_STATE, &ok);
        if (!ok)
            return;
        try {
            json j = json::parse(raw);
            state.operation = j.value("operation", std::string());
            state.status = j.value("status", std::string("idle"));
            state.profileId = j.value("profileId", std::string());
            state.timestamp = j.value("timestamp", std::string());
            state.backupPath = j.value("backupPath", std::string());
            state.targetPath = j.value("targetPath", std::string());
        } catch (const std::exception &e) {
            LOGE(std::string(Err::CONFIG_001.id) + " launcher_state.json parse error");
            corrupted = true;
        }
    }

    void write(const std::string &status) {
        state.status = status;
        state.timestamp = nowIso();
        ensureDir(CFG_ROOT);
        json j{{"operation", state.operation},   {"status", state.status},
               {"profileId", state.profileId},   {"timestamp", state.timestamp},
               {"backupPath", state.backupPath}, {"targetPath", state.targetPath}};
        writeWholeFile(CFG_STATE, j.dump(2));
        LOGD("state -> " + status);
    }

    void clear() {
        state = LauncherState();
        state.status = "idle";
        write("idle");
    }
};

inline LauncherState &launcherState() { return StateStore::get().state; }

// --- Environment inspection -----------------------------------------------
inline EnvStatus inspectEnvironment() {
    EnvStatus s;
    const Settings &cfg = settings();

    s.atmosphereDetected = dirExists(cfg.atmospherePath);
    s.targetContentsPresent = dirExists(cfg.contentsPath + "/" + TARGET_TITLE_ID_STR);
    s.commandLinePresent = fileExists(cfg.targetCommandLine);
    ensureDir(CFG_ROOT);
    ensureDir(cfg.backupPath);
    s.backupReady = dirExists(cfg.backupPath);

    const LauncherState &st = launcherState();
    s.previousOperation = st.status.empty() ? "None" : st.status;

    if (st.pending()) {
        s.level = EnvLevel::RecoveryRequired;
        s.detail = "Previous operation did not finish (" + st.status + ")";
    } else if (!s.atmosphereDetected) {
        s.level = EnvLevel::Warning;
        s.detail = std::string(Err::CFW_001.id) + " Atmosphere not detected";
    } else if (!s.targetContentsPresent) {
        s.level = EnvLevel::Warning;
        s.detail = std::string(Err::CFW_001.id) + " Target contents folder missing";
    } else if (!s.backupReady) {
        s.level = EnvLevel::Warning;
        s.detail = std::string(Err::FS_003.id) + " Backup folder unavailable";
    } else {
        s.level = EnvLevel::Ready;
        s.detail = "Environment ready";
    }
    return s;
}

// --- Error payload used by the error screen --------------------------------
struct LaunchError {
    bool failed = false;
    Err::Code code{"", ""};
    std::string details;
    std::vector<std::string> causes;
};

// --- Transactional launch --------------------------------------------------
class LaunchTransaction {
  public:
    // Step 1: Validate.
    static bool validate(LaunchError &err) {
        const Settings &cfg = settings();
        EnvStatus st = inspectEnvironment();

        if (!st.atmosphereDetected) {
            err = {true,
                   Err::CFW_001,
                   "Atmosphere folder not found at " + cfg.atmospherePath,
                   {"CFW is not installed", "SD card path is different",
                    "Console booted into stock firmware"}};
            return false;
        }
        if (!st.targetContentsPresent) {
            err = {true,
                   Err::CFW_001,
                   "Missing " + cfg.contentsPath + "/" + TARGET_TITLE_ID_STR,
                   {"Target application layer is not installed",
                    "Contents path is misconfigured"}};
            return false;
        }
        StateStore::get().state.operation = "launch";
        StateStore::get().state.targetPath = cfg.targetCommandLine;
        StateStore::get().write("validated");
        LOGI("Environment validated");
        return true;
    }

    // Step 2: Backup (never delete the original).
    static bool backup(LaunchError &err) {
        const Settings &cfg = settings();
        if (!cfg.createBackup) {
            LOGW("Backup disabled in settings - continuing without backup");
            StateStore::get().write("backup_skipped");
            return true;
        }

        ensureDir(cfg.backupPath);

        std::string original;
        if (fileExists(cfg.targetCommandLine)) {
            bool ok = false;
            original = readWholeFile(cfg.targetCommandLine, &ok);
            if (!ok) {
                err = {true,
                       Err::FS_002,
                       "Could not read " + cfg.targetCommandLine,
                       {"File is locked", "SD card error"}};
                return false;
            }
        } else {
            // Keep the original behaviour: create a default command line.
            original = DEFAULT_CMDLINE_BODY;
            if (!writeWholeFile(cfg.targetCommandLine, original)) {
                err = {true,
                       Err::FS_001,
                       "UECommandLine.txt is missing and could not be created",
                       {"romfs folder does not exist", "SD card is write protected"}};
                return false;
            }
            LOGW("UECommandLine.txt was missing - default created");
        }

        std::string path = cfg.backupPath + "/UECommandLine." + stamp() + ".bak";
        if (!writeWholeFile(path, original)) {
            err = {true, Err::FS_003, "Backup write failed: " + path, {"SD card full", "Bad path"}};
            return false;
        }
        writeWholeFile(cfg.backupPath + "/UECommandLine.latest.bak", original);

        StateStore::get().state.backupPath = path;
        StateStore::get().write("backup_created");
        LOGI("Backup created: " + path);
        pruneBackups();
        return true;
    }

    // Step 3: Prepare the temporary environment for this launch.
    // Only non-secret, environment level arguments are written here.
    static bool prepare(const Profile &p, LaunchError &err) {
        const Settings &cfg = settings();
        UE4Args args = ParseUE4CommandLine(cfg.targetCommandLine);
        args.erase("failedtoopen");

        // Keep the original launcher behaviour: make sure the patch check is
        // skipped for the offline/CFW environment.
        args["skippatchcheck"] = "";

        std::string commandLine = RebuildUE4CommandLine(args);
        if (!SaveUE4CommandLine(cfg.targetCommandLine, commandLine)) {
            err = {true,
                   Err::FS_002,
                   "Could not write " + cfg.targetCommandLine,
                   {"SD card is write protected", "romfs folder missing"}};
            return false;
        }

        StateStore::get().state.profileId = p.id;
        StateStore::get().write("prepared");
        LOGI("Temporary environment prepared for profile " + p.displayName);
        return true;
    }

    // Step 4: Launch through the system's own application launch service.
    static bool launch(LaunchError &err) {
        StateStore::get().write("launched");
        LOGI("Starting target application (" TARGET_TITLE_NAME ")");

        Result rc = appletRequestLaunchApplication(TARGET_TITLE_ID, NULL);
        if (R_FAILED(rc)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "appletRequestLaunchApplication rc=0x%08X",
                          (unsigned)rc);
            err = {true,
                   Err::LAUNCH_001,
                   buf,
                   {"Required file is missing", "Environment is unavailable",
                    "Application failed to start"}};
            LOGE(std::string(Err::LAUNCH_001.id) + " " + buf);
            restore(); // Step 6 on failure: put the original back immediately.
            return false;
        }
        return true;
    }

    // Step 6: Restore the original command line from the newest backup.
    static bool restore() {
        const Settings &cfg = settings();
        std::string src = StateStore::get().state.backupPath;
        if (src.empty() || !fileExists(src))
            src = cfg.backupPath + "/UECommandLine.latest.bak";

        if (!fileExists(src)) {
            // Legacy compatibility with the original launcher's backup file.
            if (fileExists(LEGACY_OLD_CMDLINE)) {
                UE4Args old = ParseUE4CommandLine(LEGACY_OLD_CMDLINE);
                if (old.find("failedtoopen") == old.end()) {
                    SaveUE4CommandLine(cfg.targetCommandLine, RebuildUE4CommandLine(old));
                    StateStore::get().write("restored");
                    LOGI("Restored legacy command line backup");
                    return true;
                }
            }
            LOGW(std::string(Err::FS_004.id) + " no backup available to restore");
            StateStore::get().write("restored");
            return false;
        }

        bool ok = false;
        std::string data = readWholeFile(src, &ok);
        if (!ok || !writeWholeFile(cfg.targetCommandLine, data)) {
            LOGE(std::string(Err::FS_004.id) + " restore failed from " + src);
            return false;
        }
        StateStore::get().write("restored");
        LOGI("Original environment restored from " + src);
        return true;
    }

    // Step 5: Verify that the file on disk is consistent.
    static bool verify() {
        const Settings &cfg = settings();
        if (!settings().verifyFiles)
            return true;
        bool ok = false;
        std::string data = readWholeFile(cfg.targetCommandLine, &ok);
        if (!ok || data.empty()) {
            LOGE(std::string(Err::FS_001.id) + " verification failed for " + cfg.targetCommandLine);
            return false;
        }
        LOGD("Verification passed (" + std::to_string(data.size()) + " bytes)");
        return true;
    }

  private:
    static std::string stamp() {
        std::string s = nowIso();
        for (auto &c : s)
            if (c == ':' || c == '-')
                c = '_';
        return s;
    }

    // Keep the backup folder small (performance / SD card friendliness).
    static void pruneBackups() {
        const Settings &cfg = settings();
        DIR *d = opendir(cfg.backupPath.c_str());
        if (!d)
            return;
        std::vector<std::string> files;
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            std::string n(ent->d_name);
            if (n.size() > 4 && n.substr(n.size() - 4) == ".bak" && n.find("latest") == std::string::npos)
                files.push_back(n);
        }
        closedir(d);
        if (files.size() <= 10)
            return;
        std::sort(files.begin(), files.end());
        for (size_t i = 0; i + 10 < files.size(); i++)
            std::remove((cfg.backupPath + "/" + files[i]).c_str());
    }
};
