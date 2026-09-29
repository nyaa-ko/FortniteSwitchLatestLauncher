#pragma once

#include <switch.h>
#include <json.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

using json = nlohmann::json;

namespace AccountManager {

static const char* ROOT = "sdmc:/switch/FortLatestLauncher";
static const char* FILE_PATH = "sdmc:/switch/FortLatestLauncher/accounts.json";

inline void EnsureDirectory() {
    DIR* dir = opendir(ROOT);
    if (!dir) mkdir(ROOT, 0777);
    else closedir(dir);
}

inline json DefaultDatabase() {
    return json{
        {"activeAccount", ""},
        {"accounts", json::array()}
    };
}

inline json Load() {
    EnsureDirectory();
    FILE* file = fopen(FILE_PATH, "r");
    if (!file) return DefaultDatabase();

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        fclose(file);
        return DefaultDatabase();
    }

    std::string contents(static_cast<size_t>(size), '\0');
    size_t read = fread(&contents[0], 1, static_cast<size_t>(size), file);
    fclose(file);
    contents.resize(read);

    try {
        json db = json::parse(contents);
        if (!db.is_object()) return DefaultDatabase();
        if (!db.contains("activeAccount")) db["activeAccount"] = "";
        if (!db.contains("accounts") || !db["accounts"].is_array()) db["accounts"] = json::array();
        return db;
    } catch (...) {
        return DefaultDatabase();
    }
}

inline bool Save(const json& db) {
    EnsureDirectory();
    FILE* file = fopen(FILE_PATH, "w");
    if (!file) return false;

    std::string text = db.dump(4);
    size_t written = fwrite(text.data(), 1, text.size(), file);
    fclose(file);
    return written == text.size();
}

inline int FindIndex(const json& db, const std::string& id) {
    if (!db.contains("accounts") || !db["accounts"].is_array()) return -1;
    for (size_t i = 0; i < db["accounts"].size(); ++i) {
        if (db["accounts"][i].value("id", "") == id) return static_cast<int>(i);
    }
    return -1;
}

inline json GetActive(const json& db) {
    std::string id = db.value("activeAccount", "");
    int index = FindIndex(db, id);
    if (index < 0) return json();
    return db["accounts"][index];
}

inline std::string NextId(const json& db) {
    int n = 0;
    while (true) {
        std::string id = "account_" + std::to_string(n++);
        if (FindIndex(db, id) < 0) return id;
    }
}

inline std::string DefaultColor(size_t index) {
    static const char* colors[] = {"red", "green", "yellow"};
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

inline bool Add(const json& auth, const std::string& label, const std::string& color, std::string& createdId) {
    json db = Load();
    createdId = NextId(db);

    json account;
    account["id"] = createdId;
    account["auth"] = auth;
    account["label"] = label.empty() ? "New Account" : label;
    account["color"] = color.empty() ? DefaultColor(db["accounts"].size()) : color;

    db["accounts"].push_back(account);
    db["activeAccount"] = createdId;
    return Save(db);
}

inline bool Remove(const std::string& id) {
    json db = Load();
    int index = FindIndex(db, id);
    if (index < 0) return false;

    db["accounts"].erase(db["accounts"].begin() + index);
    if (db["activeAccount"] == id) {
        if (db["accounts"].empty()) db["activeAccount"] = "";
        else db["activeAccount"] = db["accounts"][0].value("id", "");
    }
    return Save(db);
}

inline bool SetActive(const std::string& id) {
    json db = Load();
    if (FindIndex(db, id) < 0) return false;
    db["activeAccount"] = id;
    return Save(db);
}

inline bool UpdateLabel(const std::string& id, const std::string& label) {
    json db = Load();
    int index = FindIndex(db, id);
    if (index < 0 || label.empty()) return false;
    db["accounts"][index]["label"] = label;
    return Save(db);
}

inline bool UpdateColor(const std::string& id, const std::string& color) {
    json db = Load();
    int index = FindIndex(db, id);
    if (index < 0) return false;
    db["accounts"][index]["color"] = color;
    return Save(db);
}

inline bool CycleColor(const std::string& id) {
    static const char* colors[] = {"red", "green", "yellow"};

    json db = Load();
    int index = FindIndex(db, id);
    if (index < 0) return false;

    std::string current = db["accounts"][index].value("color", "red");
    size_t next = 0;
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        if (current == colors[i]) {
            next = (i + 1) % (sizeof(colors) / sizeof(colors[0]));
            break;
        }
    }
    db["accounts"][index]["color"] = colors[next];
    return Save(db);
}

} // namespace AccountManager
