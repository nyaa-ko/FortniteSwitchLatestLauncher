// ---------------------------------------------------------------------------
// Midnight Launcher
// A dedicated, single-application launcher for Nintendo Switch CFW.
//
// Based on the original "Fortnite Latest Launcher" homebrew:
//   * kept: Atmosphere contents layout, UECommandLine.txt parse/rebuild,
//           appletRequestLaunchApplication() launch path, controller loop
//   * added: multi-profile system, transactional backup/restore, recovery,
//           structured error codes, logging, settings
//   * removed: on-device storage of account credentials
//
// The launcher stores NO passwords, tokens, exchange codes or device auth.
// Profile data is display information typed by the user.
// ---------------------------------------------------------------------------

#include "Common.h"
#include "Environment.h"
#include "Logger.h"
#include "Profiles.h"
#include "Settings.h"
#include "UI.h"

enum class Screen {
    Dashboard,
    ProfileSelect,
    ProfileDetails,
    ProfileEditor,
    ColorPicker,
    AvatarPicker,
    LaunchConfirm,
    Recovery,
    Settings,
    SettingsSection,
    Logs,
    Error,
    ConfigError,
};

struct App {
    Screen screen = Screen::Dashboard;
    Screen previous = Screen::Dashboard;
    bool dirty = true;
    bool running = true;

    int cursor = 0;
    int dashCursor = 0;
    int editCursor = 0;
    int settingsSection = 0;
    int settingsCursor = 0;
    int logPage = 0;
    int logFilter = 0; // 0 all, 1 info, 2 warning, 3 error, 4 debug

    std::string currentProfileId;
    std::string editProfileId;
    std::string toast;

    EnvStatus env;
    LaunchError error;

    Profile *current() { return ProfileStore::get().byId(currentProfileId); }

    std::string accent() {
        Profile *p = current();
        return p ? p->color : std::string(kAccentColors[0].hex);
    }

    void go(Screen s) {
        previous = screen;
        screen = s;
        cursor = 0;
        dirty = true;
        ui::transition();
    }

    void back(Screen s) {
        screen = s;
        dirty = true;
    }

    void refreshEnv() { env = inspectEnvironment(); }
};

static App app;

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

static const char *kDashItems[] = {"LAUNCH", "SELECT PROFILE", "SETTINGS", "LOGS", "EXIT"};
static const int kDashCount = 5;

static void drawDashboard() {
    std::string a = app.accent();
    ui::header(a, "");

    ui::section(a, "TARGET APPLICATION");
    std::printf("  %s%s[ # ]  %s%s\n", fg(a).c_str(), C_BOLD, TARGET_TITLE_NAME, C_RESET);
    ui::status(app.env.color(), app.env.label(), app.env.detail);
    ui::blank();

    ui::section(a, "CURRENT PROFILE");
    Profile *p = app.current();
    if (p) {
        std::printf("  %s%s %s%s\n", fg(p->color).c_str(), p->avatar.c_str(),
                    p->displayName.c_str(), C_RESET);
        if (!p->email.empty())
            std::printf("  %s%s%s\n", C_MUTED, p->email.c_str(), C_RESET);
        std::printf("  %sLaunches: %ld   Last used: %s%s\n", C_DIM, p->launchCount,
                    p->lastUsed.empty() ? "never" : p->lastUsed.c_str(), C_RESET);
    } else {
        std::printf("  %sNo profile selected - press Y to add or choose one%s\n", C_WARN, C_RESET);
    }
    ui::blank();

    for (int i = 0; i < kDashCount; i++)
        ui::item(a, app.dashCursor == i, kDashItems[i]);

    if (!app.toast.empty())
        ui::toast(a, app.toast);

    ui::footer(a, " A Select   B Back   X Settings   Y Profile   ZL/ZR Switch Profile   + Exit");
}

static void drawProfileSelect() {
    std::string a = app.accent();
    ui::header(a, "SELECT PROFILE");

    auto &list = allProfiles();
    if (list.empty())
        std::printf("  %sNo profiles yet.%s\n\n", C_MUTED, C_RESET);

    for (size_t i = 0; i < list.size(); i++) {
        const Profile &p = list[i];
        std::string mark = (p.id == app.currentProfileId) ? "  [active]" : "";
        std::printf("%s", fg(p.color).c_str());
        ui::item(p.color, app.cursor == (int)i, ui::profileLine(p) + mark,
                 p.email.empty() ? p.memo : p.email);
    }
    ui::item(a, app.cursor == (int)list.size(), "+ Add Profile");

    ui::footer(a, " A Select   B Back   X Details   Y Edit");
}

static void drawProfileDetails() {
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p) {
        app.back(Screen::ProfileSelect);
        return;
    }
    std::string a = p->color;
    ui::header(a, "PROFILE");

    std::printf("  %s%s%s  %s%s%s\n\n", fg(a).c_str(), p->avatar.c_str(), C_RESET, C_BOLD,
                p->displayName.c_str(), C_RESET);
    ui::kv("Account Label", p->accountLabel.empty() ? "-" : p->accountLabel);
    ui::kv("Email", p->email.empty() ? "-" : p->email);
    ui::kv("Memo", p->memo.empty() ? "-" : p->memo);
    ui::kv("Color", p->colorName() + " " + p->color);
    ui::kv("Created", p->created.empty() ? "-" : p->created);
    ui::kv("Last Used", p->lastUsed.empty() ? "never" : p->lastUsed);
    ui::kv("Launch Count", std::to_string(p->launchCount));
    ui::blank();

    const char *items[] = {"Launch", "Edit", "Delete", "Set as current"};
    for (int i = 0; i < 4; i++)
        ui::item(a, app.cursor == i, items[i]);

    ui::footer(a, " A Select   B Back");
}

static const char *kEditFields[] = {"Display Name", "Account Label", "Email Information",
                                    "Memo",         "Color",         "Avatar",
                                    "Save & Back"};
static const int kEditFieldCount = 7;

static void drawProfileEditor() {
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p) {
        app.back(Screen::ProfileSelect);
        return;
    }
    std::string a = p->color;
    ui::header(a, "PROFILE EDITOR");

    std::string values[] = {p->displayName,
                            p->accountLabel.empty() ? "-" : p->accountLabel,
                            p->email.empty() ? "-" : p->email,
                            p->memo.empty() ? "-" : p->memo,
                            p->colorName() + " " + p->color,
                            p->avatar,
                            ""};

    for (int i = 0; i < kEditFieldCount; i++)
        ui::item(a, app.editCursor == i,
                 std::string(kEditFields[i]) + (values[i].empty() ? "" : ":  " + values[i]));

    std::printf("\n  %sEmail is a display label only. No password or token is ever stored.%s\n",
                C_DIM, C_RESET);
    ui::footer(a, " A Edit   B Back");
}

static void drawColorPicker() {
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p) {
        app.back(Screen::ProfileEditor);
        return;
    }
    ui::header(p->color, "ACCENT COLOR");
    for (int i = 0; i < kAccentColorCount; i++) {
        std::printf("%s", fg(kAccentColors[i].hex).c_str());
        ui::item(kAccentColors[i].hex, app.cursor == i,
                 std::string(kAccentColors[i].name) + "  " + kAccentColors[i].hex);
    }
    ui::item(p->color, app.cursor == kAccentColorCount, "Custom Color (#RRGGBB)");
    ui::footer(p->color, " A Select   B Back");
}

static void drawAvatarPicker() {
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p) {
        app.back(Screen::ProfileEditor);
        return;
    }
    ui::header(p->color, "AVATAR");
    for (int i = 0; i < kAvatarCount; i++)
        ui::item(p->color, app.cursor == i, kAvatars[i]);
    ui::footer(p->color, " A Select   B Back");
}

static void drawLaunchConfirm() {
    std::string a = app.accent();
    Profile *p = app.current();
    ui::header(a, "LAUNCH APPLICATION");

    ui::kv("Application", TARGET_TITLE_NAME);
    ui::kv("Profile", p ? p->displayName : "None");
    ui::kv("Environment", app.env.label(), app.env.color());
    ui::kv("Backup", settings().createBackup ? "Will be created" : "Disabled");
    ui::blank();
    std::printf("  %sValidate -> Backup -> Prepare -> Launch -> Verify -> Restore%s\n\n", C_DIM,
                C_RESET);

    const char *items[] = {"Cancel", "Continue"};
    for (int i = 0; i < 2; i++)
        ui::item(a, app.cursor == i, items[i]);

    ui::footer(a, " A Select   B Cancel");
}

static void drawRecovery() {
    std::string a = app.accent();
    ui::header(a, "RECOVERY REQUIRED");
    std::printf("  %sThe previous launch operation did not finish correctly.%s\n\n", C_WARN,
                C_RESET);
    ui::kv("Operation", launcherState().operation.empty() ? "-" : launcherState().operation);
    ui::kv("Status", launcherState().status);
    ui::kv("Timestamp", launcherState().timestamp);
    ui::kv("Backup", launcherState().backupPath.empty() ? "-" : launcherState().backupPath);
    ui::blank();

    const char *items[] = {"Restore Previous State", "Verify Environment", "Keep Current State",
                           "View Logs"};
    for (int i = 0; i < 4; i++)
        ui::item(a, app.cursor == i, items[i]);

    ui::footer(a, " A Select   B Back");
}

static const char *kSettingsSections[] = {"General",  "Appearance", "Environment",
                                          "Recovery", "Logs",       "About"};
static const int kSettingsSectionCount = 6;

static void drawSettings() {
    std::string a = app.accent();
    ui::header(a, "SETTINGS");
    for (int i = 0; i < kSettingsSectionCount; i++)
        ui::item(a, app.cursor == i, kSettingsSections[i]);
    ui::footer(a, " A Open   B Back");
}

static std::string onOff(bool v) { return v ? "On" : "Off"; }

static void drawSettingsSection() {
    std::string a = app.accent();
    Settings &s = settings();
    ui::header(a, std::string("SETTINGS / ") + kSettingsSections[app.settingsSection]);

    switch (app.settingsSection) {
    case 0:
        ui::item(a, app.settingsCursor == 0, "Confirm Before Launch:  " + onOff(s.confirmBeforeLaunch));
        ui::item(a, app.settingsCursor == 1, "Auto Recovery:  " + onOff(s.autoRecovery));
        ui::item(a, app.settingsCursor == 2, "Remember Last Profile:  " + onOff(s.rememberLastProfile));
        break;
    case 1:
        ui::item(a, app.settingsCursor == 0, "Theme:  " + s.theme);
        ui::item(a, app.settingsCursor == 1, "Accent Color:  follows current profile");
        ui::item(a, app.settingsCursor == 2, "Animation:  " + onOff(s.animations));
        ui::item(a, app.settingsCursor == 3, "UI Scale:  " + std::string(s.uiScale == 1 ? "Normal" : "Compact"));
        ui::item(a, app.settingsCursor == 4, "Transparency:  " + onOff(s.transparency));
        break;
    case 2:
        ui::item(a, app.settingsCursor == 0, "Target Application:  " TARGET_TITLE_NAME " (" TARGET_TITLE_ID_STR ")");
        ui::item(a, app.settingsCursor == 1, "Atmosphere Path:  " + s.atmospherePath);
        ui::item(a, app.settingsCursor == 2, "Contents Path:  " + s.contentsPath);
        ui::item(a, app.settingsCursor == 3, "exefs_patches Path:  " + s.exefsPatchesPath);
        ui::item(a, app.settingsCursor == 4, "Command Line File:  " + s.targetCommandLine);
        ui::item(a, app.settingsCursor == 5, "Backup Path:  " + s.backupPath);
        break;
    case 3:
        ui::item(a, app.settingsCursor == 0, "Automatic Recovery:  " + onOff(s.automaticRecovery));
        ui::item(a, app.settingsCursor == 1, "Verify Files:  " + onOff(s.verifyFiles));
        ui::item(a, app.settingsCursor == 2, "Create Backup:  " + onOff(s.createBackup));
        break;
    case 4:
        ui::item(a, app.settingsCursor == 0, "Debug Logging:  " + onOff(s.debugLogging));
        ui::item(a, app.settingsCursor == 1, "Open Logs");
        ui::item(a, app.settingsCursor == 2, "Clear Logs");
        ui::item(a, app.settingsCursor == 3, "Export Logs");
        break;
    default:
        ui::kv("Version", MIDNIGHT_VERSION);
        ui::kv("Target", TARGET_TITLE_NAME);
        ui::kv("Config", CFG_ROOT);
        ui::blank();
        std::printf("  %sBased on the original Fortnite Latest Launcher homebrew.%s\n", C_DIM,
                    C_RESET);
        std::printf("  %sNo credentials are stored by this launcher.%s\n", C_DIM, C_RESET);
        break;
    }
    ui::footer(a, " A Change   B Back");
}

static void drawLogs() {
    std::string a = app.accent();
    static const char *filters[] = {"ALL", "INFO", "WARNING", "ERROR", "DEBUG"};
    ui::header(a, std::string("LOGS  [") + filters[app.logFilter] + "]");

    std::vector<const LogEntry *> shown;
    for (const auto &e : Logger::get().entries()) {
        if (app.logFilter == 0 || (int)e.level == app.logFilter - 1)
            shown.push_back(&e);
    }

    const int perPage = 26;
    int pages = (int)((shown.size() + perPage - 1) / perPage);
    if (pages == 0)
        pages = 1;
    if (app.logPage >= pages)
        app.logPage = pages - 1;

    int start = app.logPage * perPage;
    for (int i = start; i < (int)shown.size() && i < start + perPage; i++) {
        const LogEntry *e = shown[i];
        std::printf("%s[%s]%s %s%s%s\n", Logger::levelColor(e->level), Logger::levelName(e->level),
                    C_RESET, C_TEXT, ui::truncate(e->message, 62).c_str(), C_RESET);
    }
    std::printf("\n  %sPage %d/%d%s\n", C_DIM, app.logPage + 1, pages, C_RESET);
    ui::footer(a, " L/R Page   X Filter   Y Clear   A Export   B Back");
}

static void drawError() {
    std::string a = app.accent();
    ui::header(a, "LAUNCH FAILED");
    ui::kv("Code", app.error.code.id, C_ERR);
    ui::kv("Message", app.error.code.message);
    ui::blank();
    std::printf("  %sDetails:%s\n  %s%s%s\n\n", C_MUTED, C_RESET, C_TEXT,
                app.error.details.c_str(), C_RESET);
    std::printf("  %sPossible causes:%s\n", C_MUTED, C_RESET);
    for (const auto &c : app.error.causes)
        std::printf("   %s- %s%s\n", C_DIM, c.c_str(), C_RESET);
    ui::blank();

    const char *items[] = {"Retry", "Recovery", "Logs", "Close"};
    for (int i = 0; i < 4; i++)
        ui::item(a, app.cursor == i, items[i]);
    ui::footer(a, " A Select   B Close");
}

static void drawConfigError() {
    ui::header(kAccentColors[6].hex, "CONFIGURATION ERROR");
    std::printf("  %sThe configuration file appears to be corrupted.%s\n\n", C_ERR, C_RESET);
    ui::kv("Code", Err::CONFIG_001.id, C_ERR);
    ui::blank();
    const char *items[] = {"Restore Backup", "Reset Configuration", "Cancel"};
    for (int i = 0; i < 3; i++)
        ui::item(kAccentColors[6].hex, app.cursor == i, items[i]);
    ui::footer(kAccentColors[6].hex, " A Select");
}

static void draw() {
    ui::clear();
    switch (app.screen) {
    case Screen::Dashboard:
        drawDashboard();
        break;
    case Screen::ProfileSelect:
        drawProfileSelect();
        break;
    case Screen::ProfileDetails:
        drawProfileDetails();
        break;
    case Screen::ProfileEditor:
        drawProfileEditor();
        break;
    case Screen::ColorPicker:
        drawColorPicker();
        break;
    case Screen::AvatarPicker:
        drawAvatarPicker();
        break;
    case Screen::LaunchConfirm:
        drawLaunchConfirm();
        break;
    case Screen::Recovery:
        drawRecovery();
        break;
    case Screen::Settings:
        drawSettings();
        break;
    case Screen::SettingsSection:
        drawSettingsSection();
        break;
    case Screen::Logs:
        drawLogs();
        break;
    case Screen::Error:
        drawError();
        break;
    case Screen::ConfigError:
        drawConfigError();
        break;
    }
    ui::flush();
}

// ---------------------------------------------------------------------------
// Launch sequence (transactional)
// ---------------------------------------------------------------------------
static void runLaunch() {
    Profile *p = app.current();
    if (!p) {
        app.toast = "Select a profile first.";
        app.back(Screen::Dashboard);
        return;
    }

    ui::clear();
    ui::header(p->color, "LAUNCHING");
    std::printf("  %sPreparing environment...%s\n", C_MUTED, C_RESET);
    ui::flush();

    LaunchError err;
    LOGI("Launch requested with profile: " + p->displayName);

    if (!LaunchTransaction::validate(err)) {
        app.error = err;
        app.go(Screen::Error);
        return;
    }
    if (!LaunchTransaction::backup(err)) {
        app.error = err;
        app.go(Screen::Error);
        return;
    }
    if (!LaunchTransaction::prepare(*p, err)) {
        LaunchTransaction::restore();
        app.error = err;
        app.go(Screen::Error);
        return;
    }
    if (!LaunchTransaction::verify()) {
        LaunchTransaction::restore();
        app.error = {true,
                     Err::FS_001,
                     "Verification of the prepared command line failed",
                     {"Required file is missing", "SD card write error"}};
        app.go(Screen::Error);
        return;
    }

    ProfileStore::get().markLaunched(p->id);
    settings().lastProfileId = p->id;
    SettingsStore::get().save();

    if (!LaunchTransaction::launch(err)) {
        app.error = err;
        app.go(Screen::Error);
        return;
    }

    // The system takes over from here. The original environment is restored on
    // the next start of the launcher (state = "launched").
    std::printf("  %sStarting %s...%s\n", C_OK, TARGET_TITLE_NAME, C_RESET);
    ui::flush();
    svcSleepThread(1500000000ULL);
    app.running = false;
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------
static void moveCursor(int &cursor, int count, u64 kDown) {
    if (count <= 0)
        return;
    if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
        cursor = (cursor + 1) % count;
        app.dirty = true;
    }
    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
        cursor = (cursor - 1 + count) % count;
        app.dirty = true;
    }
}

static void cycleProfile(int dir) {
    auto &list = allProfiles();
    if (list.empty())
        return;
    int idx = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id == app.currentProfileId)
            idx = (int)i;
    idx = (idx + dir + (int)list.size()) % (int)list.size();
    app.currentProfileId = list[idx].id;
    settings().lastProfileId = app.currentProfileId;
    SettingsStore::get().save();
    LOGI("Profile selected: " + list[idx].displayName);
    app.toast = "Profile: " + list[idx].displayName;
    app.dirty = true;
}

static void editField(Profile &p, int field) {
    std::string out;
    switch (field) {
    case 0:
        if (ui::keyboard("Display Name", p.displayName, out, 32) && !out.empty())
            p.displayName = out;
        break;
    case 1:
        if (ui::keyboard("Account Label", p.accountLabel, out, 32))
            p.accountLabel = out;
        break;
    case 2:
        if (ui::keyboard("Email information (display only)", p.email, out, 64))
            p.email = out;
        break;
    case 3:
        if (ui::keyboard("Memo", p.memo, out, 96))
            p.memo = out;
        break;
    default:
        break;
    }
    ProfileStore::get().save();
    app.dirty = true;
}

static void handleDashboard(u64 kDown) {
    moveCursor(app.dashCursor, kDashCount, kDown);

    if (kDown & HidNpadButton_A) {
        app.toast.clear();
        switch (app.dashCursor) {
        case 0:
            app.refreshEnv();
            if (app.env.level == EnvLevel::RecoveryRequired) {
                app.go(Screen::Recovery);
            } else if (settings().confirmBeforeLaunch) {
                app.go(Screen::LaunchConfirm);
            } else {
                runLaunch();
            }
            break;
        case 1:
            app.go(Screen::ProfileSelect);
            break;
        case 2:
            app.go(Screen::Settings);
            break;
        case 3:
            app.go(Screen::Logs);
            break;
        default:
            app.running = false;
            break;
        }
    }
    if (kDown & HidNpadButton_X)
        app.go(Screen::Settings);
    if (kDown & HidNpadButton_Y)
        app.go(Screen::ProfileSelect);
    if (kDown & HidNpadButton_ZL)
        cycleProfile(-1);
    if (kDown & HidNpadButton_ZR)
        cycleProfile(1);
}

static void handleProfileSelect(u64 kDown) {
    auto &list = allProfiles();
    moveCursor(app.cursor, (int)list.size() + 1, kDown);

    if (kDown & HidNpadButton_A) {
        if (app.cursor == (int)list.size()) {
            Profile &p = ProfileStore::get().create();
            app.editProfileId = p.id;
            app.editCursor = 0;
            app.go(Screen::ProfileEditor);
        } else {
            app.currentProfileId = list[app.cursor].id;
            settings().lastProfileId = app.currentProfileId;
            SettingsStore::get().save();
            LOGI("Profile selected: " + list[app.cursor].displayName);
            app.toast = "Profile: " + list[app.cursor].displayName;
            app.back(Screen::Dashboard);
        }
    }
    if ((kDown & HidNpadButton_X) && app.cursor < (int)list.size()) {
        app.editProfileId = list[app.cursor].id;
        app.go(Screen::ProfileDetails);
    }
    if ((kDown & HidNpadButton_Y) && app.cursor < (int)list.size()) {
        app.editProfileId = list[app.cursor].id;
        app.editCursor = 0;
        app.go(Screen::ProfileEditor);
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Dashboard);
}

static void handleProfileDetails(u64 kDown) {
    moveCursor(app.cursor, 4, kDown);
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p)
        return;

    if (kDown & HidNpadButton_A) {
        switch (app.cursor) {
        case 0:
            app.currentProfileId = p->id;
            app.refreshEnv();
            app.go(settings().confirmBeforeLaunch ? Screen::LaunchConfirm : Screen::Dashboard);
            if (!settings().confirmBeforeLaunch)
                runLaunch();
            break;
        case 1:
            app.editCursor = 0;
            app.go(Screen::ProfileEditor);
            break;
        case 2: {
            std::string id = p->id;
            ProfileStore::get().remove(id);
            if (app.currentProfileId == id)
                app.currentProfileId = allProfiles().empty() ? "" : allProfiles()[0].id;
            app.go(Screen::ProfileSelect);
            break;
        }
        default:
            app.currentProfileId = p->id;
            settings().lastProfileId = p->id;
            SettingsStore::get().save();
            app.toast = "Profile: " + p->displayName;
            app.go(Screen::Dashboard);
            break;
        }
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::ProfileSelect);
}

static void handleProfileEditor(u64 kDown) {
    moveCursor(app.editCursor, kEditFieldCount, kDown);
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p)
        return;

    if (kDown & HidNpadButton_A) {
        if (app.editCursor <= 3) {
            editField(*p, app.editCursor);
        } else if (app.editCursor == 4) {
            app.go(Screen::ColorPicker);
        } else if (app.editCursor == 5) {
            app.go(Screen::AvatarPicker);
        } else {
            ProfileStore::get().save();
            app.toast = "Profile saved.";
            app.go(Screen::ProfileSelect);
        }
    }
    if (kDown & HidNpadButton_B) {
        ProfileStore::get().save();
        app.back(Screen::ProfileSelect);
    }
}

static void handleColorPicker(u64 kDown) {
    moveCursor(app.cursor, kAccentColorCount + 1, kDown);
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p)
        return;

    if (kDown & HidNpadButton_A) {
        if (app.cursor < kAccentColorCount) {
            p->color = kAccentColors[app.cursor].hex;
        } else {
            std::string out;
            if (ui::keyboard("Custom color #RRGGBB", p->color, out, 7) && out.size() == 7 &&
                out[0] == '#')
                p->color = out;
        }
        ProfileStore::get().save();
        app.go(Screen::ProfileEditor);
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::ProfileEditor);
}

static void handleAvatarPicker(u64 kDown) {
    moveCursor(app.cursor, kAvatarCount, kDown);
    Profile *p = ProfileStore::get().byId(app.editProfileId);
    if (!p)
        return;
    if (kDown & HidNpadButton_A) {
        p->avatar = kAvatars[app.cursor];
        ProfileStore::get().save();
        app.go(Screen::ProfileEditor);
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::ProfileEditor);
}

static void handleLaunchConfirm(u64 kDown) {
    moveCursor(app.cursor, 2, kDown);
    if (kDown & HidNpadButton_A) {
        if (app.cursor == 1)
            runLaunch();
        else
            app.back(Screen::Dashboard);
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Dashboard);
}

static void handleRecovery(u64 kDown) {
    moveCursor(app.cursor, 4, kDown);
    if (kDown & HidNpadButton_A) {
        switch (app.cursor) {
        case 0:
            if (LaunchTransaction::restore()) {
                StateStore::get().clear();
                app.toast = "Previous state restored.";
            } else {
                app.toast = std::string(Err::FS_004.id) + " restore failed - see logs";
            }
            app.refreshEnv();
            app.go(Screen::Dashboard);
            break;
        case 1:
            app.refreshEnv();
            app.toast = std::string("Environment: ") + app.env.label();
            LOGI("Manual verification: " + app.env.detail);
            app.dirty = true;
            break;
        case 2:
            StateStore::get().clear();
            app.refreshEnv();
            app.toast = "Current state kept.";
            app.go(Screen::Dashboard);
            break;
        default:
            app.go(Screen::Logs);
            break;
        }
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Dashboard);
}

static void handleSettings(u64 kDown) {
    moveCursor(app.cursor, kSettingsSectionCount, kDown);
    if (kDown & HidNpadButton_A) {
        app.settingsSection = app.cursor;
        app.settingsCursor = 0;
        app.go(Screen::SettingsSection);
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Dashboard);
}

static int settingsItemCount() {
    switch (app.settingsSection) {
    case 0:
        return 3;
    case 1:
        return 5;
    case 2:
        return 6;
    case 3:
        return 3;
    case 4:
        return 4;
    default:
        return 0;
    }
}

static void editPath(std::string &target, const char *guide) {
    std::string out;
    if (ui::keyboard(guide, target, out, 160) && !out.empty())
        target = out;
    SettingsStore::get().save();
    app.dirty = true;
}

static void handleSettingsSection(u64 kDown) {
    moveCursor(app.settingsCursor, settingsItemCount(), kDown);
    Settings &s = settings();

    if (kDown & HidNpadButton_A) {
        switch (app.settingsSection) {
        case 0:
            if (app.settingsCursor == 0)
                s.confirmBeforeLaunch = !s.confirmBeforeLaunch;
            else if (app.settingsCursor == 1)
                s.autoRecovery = !s.autoRecovery;
            else
                s.rememberLastProfile = !s.rememberLastProfile;
            break;
        case 1:
            if (app.settingsCursor == 0)
                s.theme = (s.theme == "midnight") ? "black" : "midnight";
            else if (app.settingsCursor == 2)
                s.animations = !s.animations;
            else if (app.settingsCursor == 3)
                s.uiScale = (s.uiScale == 1) ? 2 : 1;
            else if (app.settingsCursor == 4)
                s.transparency = !s.transparency;
            break;
        case 2:
            if (app.settingsCursor == 1)
                editPath(s.atmospherePath, "Atmosphere path");
            else if (app.settingsCursor == 2)
                editPath(s.contentsPath, "Contents path");
            else if (app.settingsCursor == 3)
                editPath(s.exefsPatchesPath, "exefs_patches path");
            else if (app.settingsCursor == 4)
                editPath(s.targetCommandLine, "UECommandLine.txt path");
            else if (app.settingsCursor == 5)
                editPath(s.backupPath, "Backup path");
            break;
        case 3:
            if (app.settingsCursor == 0)
                s.automaticRecovery = !s.automaticRecovery;
            else if (app.settingsCursor == 1)
                s.verifyFiles = !s.verifyFiles;
            else
                s.createBackup = !s.createBackup;
            break;
        case 4:
            if (app.settingsCursor == 0) {
                s.debugLogging = !s.debugLogging;
                Logger::get().setDebug(s.debugLogging);
            } else if (app.settingsCursor == 1) {
                app.go(Screen::Logs);
            } else if (app.settingsCursor == 2) {
                Logger::get().clear();
            } else {
                std::string path = std::string(CFG_LOGDIR) + "/export_" + nowIso() + ".log";
                Logger::get().exportTo(path);
                app.toast = "Exported to logs folder.";
            }
            break;
        default:
            break;
        }
        SettingsStore::get().save();
        app.refreshEnv();
        app.dirty = true;
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Settings);
}

static void handleLogs(u64 kDown) {
    if (kDown & HidNpadButton_R) {
        app.logPage++;
        app.dirty = true;
    }
    if (kDown & HidNpadButton_L) {
        if (app.logPage > 0)
            app.logPage--;
        app.dirty = true;
    }
    if (kDown & HidNpadButton_X) {
        app.logFilter = (app.logFilter + 1) % 5;
        app.logPage = 0;
        app.dirty = true;
    }
    if (kDown & HidNpadButton_Y) {
        Logger::get().clear();
        app.dirty = true;
    }
    if (kDown & HidNpadButton_A) {
        std::string path = std::string(CFG_LOGDIR) + "/export.log";
        Logger::get().exportTo(path);
        app.toast = "Logs exported.";
        app.dirty = true;
    }
    if (kDown & HidNpadButton_B)
        app.back(Screen::Dashboard);
}

static void handleError(u64 kDown) {
    moveCursor(app.cursor, 4, kDown);
    if (kDown & HidNpadButton_A) {
        switch (app.cursor) {
        case 0:
            runLaunch();
            break;
        case 1:
            app.go(Screen::Recovery);
            break;
        case 2:
            app.go(Screen::Logs);
            break;
        default:
            app.refreshEnv();
            app.go(Screen::Dashboard);
            break;
        }
    }
    if (kDown & HidNpadButton_B)
        app.go(Screen::Dashboard);
}

static void handleConfigError(u64 kDown) {
    moveCursor(app.cursor, 3, kDown);
    if (kDown & HidNpadButton_A) {
        if (app.cursor == 0) {
            bool ok = SettingsStore::get().restoreBackup() | ProfileStore::get().restoreBackup();
            app.toast = ok ? "Configuration restored." : "No usable backup found.";
        } else if (app.cursor == 1) {
            SettingsStore::get().reset();
            ProfileStore::get().reset();
            app.toast = "Configuration reset.";
        }
        app.refreshEnv();
        app.go(Screen::Dashboard);
    }
}

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------
static void startup() {
    ensureDir("sdmc:/config");
    ensureDir(CFG_ROOT);
    ensureDir(CFG_LOGDIR);
    ensureDir(CFG_BACKUPDIR);

    Logger::get().init();
    LOGI("Launcher started (v" MIDNIGHT_VERSION ")");

    bool settingsOk = SettingsStore::get().load();
    bool profilesOk = ProfileStore::get().load();
    StateStore::get().load();

    if (!settingsOk || !profilesOk || StateStore::get().corrupted) {
        app.screen = Screen::ConfigError;
        return;
    }

    if (settings().rememberLastProfile && !settings().lastProfileId.empty())
        app.currentProfileId = settings().lastProfileId;
    if (!ProfileStore::get().byId(app.currentProfileId) && !allProfiles().empty())
        app.currentProfileId = allProfiles()[0].id;

    // Interrupted / finished launch from a previous session.
    LauncherState &st = launcherState();
    if (st.pending()) {
        if (st.status == "launched" && settings().autoRecovery) {
            LOGI("Previous session launched the application - restoring environment");
            LaunchTransaction::restore();
            StateStore::get().clear();
        } else {
            LOGW(std::string(Err::CFW_002.id) + " previous operation incomplete: " + st.status);
            app.screen = Screen::Recovery;
        }
    }

    app.refreshEnv();
    LOGI(std::string("Environment: ") + app.env.label());
}

int main(int argc, char *argv[]) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    startup();

    while (appletMainLoop() && app.running) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break;

        if (kDown) {
            switch (app.screen) {
            case Screen::Dashboard:
                handleDashboard(kDown);
                break;
            case Screen::ProfileSelect:
                handleProfileSelect(kDown);
                break;
            case Screen::ProfileDetails:
                handleProfileDetails(kDown);
                break;
            case Screen::ProfileEditor:
                handleProfileEditor(kDown);
                break;
            case Screen::ColorPicker:
                handleColorPicker(kDown);
                break;
            case Screen::AvatarPicker:
                handleAvatarPicker(kDown);
                break;
            case Screen::LaunchConfirm:
                handleLaunchConfirm(kDown);
                break;
            case Screen::Recovery:
                handleRecovery(kDown);
                break;
            case Screen::Settings:
                handleSettings(kDown);
                break;
            case Screen::SettingsSection:
                handleSettingsSection(kDown);
                break;
            case Screen::Logs:
                handleLogs(kDown);
                break;
            case Screen::Error:
                handleError(kDown);
                break;
            case Screen::ConfigError:
                handleConfigError(kDown);
                break;
            }
        }

        // Redraw only when something actually changed (Switch friendly).
        if (app.dirty) {
            draw();
            app.dirty = false;
        } else {
            ui::flush();
        }
    }

    LOGI("Launcher exited");
    consoleExit(NULL);
    return 0;
}
