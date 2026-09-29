#pragma once

// profiles.json - DISPLAY INFORMATION ONLY.
// No password, OAuth token, exchange code, device auth or private key is ever
// written here. "email" is a free-text label typed by the user.

#include "Common.h"
#include "Logger.h"
#include <json.hpp>

using json = nlohmann::json;

struct AccentColor {
    const char *name;
    const char *hex;
};

static const AccentColor kAccentColors[] = {
    {"Purple", "#7C5CFF"}, {"Blue", "#4C8DFF"},   {"Cyan", "#3FD8D8"},
    {"Green", "#54D68A"},  {"Yellow", "#F2C94C"}, {"Orange", "#F2954C"},
    {"Red", "#F0606C"},    {"Pink", "#F06CB8"},
};
static const int kAccentColorCount = 8;

static const char *kAvatars[] = {"[*]", "[#]", "[@]", "[+]", "[o]", "[x]", "[^]", "[~]"};
static const int kAvatarCount = 8;

struct Profile {
    std::string id;
    std::string displayName = "New Profile";
    std::string accountLabel;
    std::string email;
    std::string avatar = kAvatars[0];
    std::string color = kAccentColors[0].hex;
    std::string memo;
    std::string created;
    std::string lastUsed;
    long launchCount = 0;

    json toJson() const {
        return json{{"id", id},
                    {"displayName", displayName},
                    {"accountLabel", accountLabel},
                    {"email", email},
                    {"avatar", avatar},
                    {"color", color},
                    {"memo", memo},
                    {"created", created},
                    {"lastUsed", lastUsed},
                    {"launchCount", launchCount}};
    }

    static Profile fromJson(const json &j) {
        Profile p;
        p.id = j.value("id", std::string());
        p.displayName = j.value("displayName", std::string("Profile"));
        p.accountLabel = j.value("accountLabel", std::string());
        p.email = j.value("email", std::string());
        p.avatar = j.value("avatar", std::string(kAvatars[0]));
        p.color = j.value("color", std::string(kAccentColors[0].hex));
        p.memo = j.value("memo", std::string());
        p.created = j.value("created", std::string());
        p.lastUsed = j.value("lastUsed", std::string());
        p.launchCount = j.value("launchCount", 0L);
        return p;
    }

    std::string colorName() const {
        for (int i = 0; i < kAccentColorCount; i++)
            if (color == kAccentColors[i].hex)
                return kAccentColors[i].name;
        return "Custom";
    }
};

class ProfileStore {
  public:
    static ProfileStore &get() {
        static ProfileStore s;
        return s;
    }

    std::vector<Profile> profiles;
    bool corrupted = false;

    bool load() {
        corrupted = false;
        profiles.clear();
        if (!fileExists(CFG_PROFILES)) {
            save();
            return true;
        }
        bool ok = false;
        std::string raw = readWholeFile(CFG_PROFILES, &ok);
        if (!ok) {
            LOGE(std::string(Err::FS_002.id) + " profiles.json could not be read");
            corrupted = true;
            return false;
        }
        try {
            json j = json::parse(raw);
            for (const auto &item : j.value("profiles", json::array()))
                profiles.push_back(Profile::fromJson(item));
        } catch (const std::exception &e) {
            LOGE(std::string(Err::CONFIG_001.id) + " profiles.json parse error: " + e.what());
            corrupted = true;
            return false;
        }
        LOGI("Loaded " + std::to_string(profiles.size()) + " profile(s)");
        return true;
    }

    bool save() {
        ensureDir(CFG_ROOT);
        if (fileExists(CFG_PROFILES)) {
            bool ok = false;
            std::string cur = readWholeFile(CFG_PROFILES, &ok);
            if (ok && !cur.empty())
                writeWholeFile(std::string(CFG_PROFILES) + ".bak", cur);
        }
        json arr = json::array();
        for (const auto &p : profiles)
            arr.push_back(p.toJson());
        bool ok = writeWholeFile(CFG_PROFILES, json{{"profiles", arr}}.dump(2));
        if (!ok)
            LOGE(std::string(Err::FS_002.id) + " failed to write profiles.json");
        return ok;
    }

    bool restoreBackup() {
        bool ok = false;
        std::string raw = readWholeFile(std::string(CFG_PROFILES) + ".bak", &ok);
        if (!ok)
            return false;
        try {
            json j = json::parse(raw);
            profiles.clear();
            for (const auto &item : j.value("profiles", json::array()))
                profiles.push_back(Profile::fromJson(item));
        } catch (...) {
            return false;
        }
        corrupted = false;
        return save();
    }

    void reset() {
        profiles.clear();
        corrupted = false;
        save();
        LOGW("Profiles reset");
    }

    Profile *byId(const std::string &id) {
        for (auto &p : profiles)
            if (p.id == id)
                return &p;
        return nullptr;
    }

    Profile &create() {
        Profile p;
        p.id = nextId();
        p.created = nowIso();
        profiles.push_back(p);
        save();
        LOGI("Profile created: " + p.id);
        return profiles.back();
    }

    void remove(const std::string &id) {
        for (size_t i = 0; i < profiles.size(); i++) {
            if (profiles[i].id == id) {
                profiles.erase(profiles.begin() + i);
                save();
                LOGW("Profile deleted: " + id);
                return;
            }
        }
    }

    void markLaunched(const std::string &id) {
        if (Profile *p = byId(id)) {
            p->lastUsed = nowIso();
            p->launchCount++;
            save();
        }
    }

  private:
    std::string nextId() {
        int n = 1;
        char buf[24];
        while (true) {
            std::snprintf(buf, sizeof(buf), "profile_%03d", n);
            if (!byId(buf))
                return std::string(buf);
            n++;
        }
    }
};

inline std::vector<Profile> &allProfiles() { return ProfileStore::get().profiles; }
