#pragma once

// ---------------------------------------------------------------------------
// Midnight Launcher - shared definitions
// Dedicated single-application launcher for Nintendo Switch CFW (Atmosphere).
//
// Derived from the original "Fortnite Latest Launcher" homebrew sources.
// Existing behaviour that is kept: UECommandLine.txt parsing/rebuilding,
// Atmosphere contents layout, appletRequestLaunchApplication() based launch.
//
// SECURITY POLICY (enforced across the whole project):
//   * The launcher never stores passwords, refresh tokens, access tokens,
//     exchange codes, device-auth secrets or private keys in its own files.
//   * profiles.json contains display information only (entered by the user).
//   * No official/permitted authentication mechanism is modified or bypassed.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include <switch.h>

#define MIDNIGHT_VERSION "2.0.0"

// --- Target application (single target by design) --------------------------
#define TARGET_TITLE_NAME "Fortnite"
#define TARGET_TITLE_ID 0x010025400AECE000ULL
#define TARGET_TITLE_ID_STR "010025400AECE000"

// --- Paths (kept configurable through settings.json) -----------------------
#define CFG_ROOT "sdmc:/config/midnight-launcher"
#define CFG_PROFILES CFG_ROOT "/profiles.json"
#define CFG_SETTINGS CFG_ROOT "/settings.json"
#define CFG_STATE CFG_ROOT "/launcher_state.json"
#define CFG_LOGDIR CFG_ROOT "/logs"
#define CFG_LOGFILE CFG_LOGDIR "/launcher.log"
#define CFG_BACKUPDIR CFG_ROOT "/backup"

#define DEFAULT_ATMOSPHERE_PATH "sdmc:/atmosphere"
#define DEFAULT_CONTENTS_PATH "sdmc:/atmosphere/contents"
#define DEFAULT_EXEFS_PATCHES_PATH "sdmc:/atmosphere/exefs_patches"
#define DEFAULT_TARGET_CMDLINE \
    "sdmc:/atmosphere/contents/" TARGET_TITLE_ID_STR "/romfs/UECommandLine.txt"
#define DEFAULT_CMDLINE_BODY "../../../FortniteGame/FortniteGame.uproject -skippatchcheck"

// Legacy location used by the original launcher. Read-only compatibility.
#define LEGACY_DIR "sdmc:/switch/FortLatestLauncher"
#define LEGACY_OLD_CMDLINE LEGACY_DIR "/OldCommandLine.txt"

// --- Error codes -----------------------------------------------------------
namespace Err {
struct Code {
    const char *id;
    const char *message;
};

static const Code FS_001{"E-FS-001", "File not found"};
static const Code FS_002{"E-FS-002", "File access failed"};
static const Code FS_003{"E-FS-003", "Backup failed"};
static const Code FS_004{"E-FS-004", "Restore failed"};
static const Code CFW_001{"E-CFW-001", "Environment unavailable"};
static const Code CFW_002{"E-CFW-002", "Recovery required"};
static const Code LAUNCH_001{"E-LAUNCH-001", "Application launch failed"};
static const Code CONFIG_001{"E-CONFIG-001", "Configuration corrupted"};
} // namespace Err

// --- ANSI helpers (libnx console supports SGR incl. truecolor) -------------
#define C_RESET "\x1b[0m"
#define C_DIM "\x1b[38;2;120;124;140m"
#define C_TEXT "\x1b[38;2;226;228;236m"
#define C_MUTED "\x1b[38;2;150;154;170m"
#define C_WARN "\x1b[38;2;240;186;80m"
#define C_ERR "\x1b[38;2;240;96;104m"
#define C_OK "\x1b[38;2;104;214;150m"
#define C_BOLD "\x1b[1m"

// Convert "#RRGGBB" to a truecolor foreground escape sequence.
inline std::string fg(const std::string &hex) {
    unsigned r = 124, g = 92, b = 255;
    if (hex.size() >= 7 && hex[0] == '#')
        std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\x1b[38;2;%u;%u;%um", r, g, b);
    return std::string(buf);
}

inline bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

inline bool dirExists(const std::string &path) {
    DIR *d = opendir(path.c_str());
    if (!d)
        return false;
    closedir(d);
    return true;
}

inline void ensureDir(const std::string &path) {
    if (!dirExists(path))
        mkdir(path.c_str(), 0777);
}

inline std::string nowIso() {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
    return std::string(buf);
}

inline std::string readWholeFile(const std::string &path, bool *ok = nullptr) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (ok)
            *ok = false;
        return "";
    }
    std::string out;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    if (ok)
        *ok = true;
    return out;
}

inline bool writeWholeFile(const std::string &path, const std::string &data) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    size_t written = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return written == data.size();
}
