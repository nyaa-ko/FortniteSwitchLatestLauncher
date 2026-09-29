#pragma once

// ---------------------------------------------------------------------------
// Console UI primitives for the Midnight theme.
// The libnx text console is used (no heavy renderer), so "animation" means
// short, cheap transitions only. Drawing happens on demand - never every frame.
// ---------------------------------------------------------------------------

#include "Common.h"
#include "Profiles.h"
#include "Settings.h"

namespace ui {

static const int kCols = 80;

inline void clear() { consoleClear(); }
inline void flush() { consoleUpdate(NULL); }

inline void line(const std::string &color, char c = '-') {
    std::string s(kCols - 2, c);
    std::printf("%s%s%s\n", color.c_str(), s.c_str(), C_RESET);
}

inline void blank() { std::printf("\n"); }

inline std::string pad(const std::string &s, size_t w) {
    if (s.size() >= w)
        return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}

inline std::string truncate(const std::string &s, size_t w) {
    if (s.size() <= w)
        return s;
    return s.substr(0, w > 3 ? w - 3 : w) + "...";
}

// Top bar: MIDNIGHT LAUNCHER + screen title, tinted with the active accent.
inline void header(const std::string &accent, const std::string &title) {
    std::string a = fg(accent);
    std::printf("%s%s  MIDNIGHT LAUNCHER%s  %sv%s%s\n", a.c_str(), C_BOLD, C_RESET, C_DIM,
                MIDNIGHT_VERSION, C_RESET);
    std::printf("%s%s%s\n", C_TEXT, title.c_str(), C_RESET);
    line(a, '=');
    blank();
}

// Section label.
inline void section(const std::string &accent, const std::string &label) {
    std::printf("%s%s%s\n", fg(accent).c_str(), label.c_str(), C_RESET);
}

inline void kv(const std::string &key, const std::string &value, const char *vcolor = C_TEXT) {
    std::printf("  %s%s%s %s%s%s\n", C_MUTED, pad(key + ":", 18).c_str(), C_RESET, vcolor,
                value.c_str(), C_RESET);
}

// One selectable row. Focus is always unmistakable.
inline void item(const std::string &accent, bool focused, const std::string &text,
                 const std::string &sub = "") {
    std::string a = fg(accent);
    if (focused)
        std::printf("%s%s > %s%s\n", a.c_str(), C_BOLD, truncate(text, kCols - 6).c_str(), C_RESET);
    else
        std::printf("%s   %s%s\n", C_TEXT, truncate(text, kCols - 6).c_str(), C_RESET);
    if (!sub.empty())
        std::printf("%s     %s%s\n", C_DIM, truncate(sub, kCols - 8).c_str(), C_RESET);
}

// Status pill: READY / WARNING / RECOVERY REQUIRED
inline void status(const char *color, const char *label, const std::string &detail) {
    std::printf("  %s* %s%s  %s%s%s\n", color, label, C_RESET, C_DIM, detail.c_str(), C_RESET);
}

inline void footer(const std::string &accent, const std::string &hints) {
    blank();
    line(fg(accent), '-');
    std::printf("%s%s%s\n", C_MUTED, hints.c_str(), C_RESET);
}

inline void toast(const std::string &accent, const std::string &msg) {
    std::printf("\n%s  %s%s\n", fg(accent).c_str(), msg.c_str(), C_RESET);
}

inline void errorText(const std::string &msg) {
    std::printf("%s  %s%s\n", C_ERR, msg.c_str(), C_RESET);
}

// Cheap "transition": a couple of frames only, and only if enabled.
inline void transition() {
    if (!settings().animations)
        return;
    for (int i = 0; i < 2; i++) {
        flush();
        svcSleepThread(16000000ULL); // ~16 ms
    }
}

// Software keyboard input (real Switch text entry).
inline bool keyboard(const std::string &guide, const std::string &initial, std::string &out,
                     size_t maxLen = 96) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, guide.c_str());
    swkbdConfigSetInitialText(&kbd, initial.c_str());
    swkbdConfigSetStringLenMax(&kbd, (u32)maxLen);
    char buf[256] = {0};
    Result rc = swkbdShow(&kbd, buf, sizeof(buf));
    swkbdClose(&kbd);
    if (R_FAILED(rc))
        return false;
    out = std::string(buf);
    return true;
}

inline std::string profileLine(const Profile &p) {
    std::string label = p.displayName;
    if (!p.accountLabel.empty())
        label += "  (" + p.accountLabel + ")";
    return p.avatar + " " + label;
}

} // namespace ui
