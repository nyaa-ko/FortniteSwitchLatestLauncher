#pragma once

// ---------------------------------------------------------------------------
// UE4 command line manager.
// Original implementation by the Fortnite Latest Launcher authors; refactored
// minimally for Midnight Launcher:
//   * include guard added
//   * hard-coded write paths replaced by explicit parameters
//   * fprintf format-string bug fixed (data was used as a format string)
//   * no "using namespace std" leakage
// Behaviour of parsing / rebuilding is intentionally unchanged.
// ---------------------------------------------------------------------------

#include "Common.h"
#include <cstring>
#include <regex>
#include <unordered_map>

using UE4Args = std::unordered_map<std::string, std::string>;

inline std::string RebuildUE4CommandLine(const UE4Args &arguments,
                                         const std::string &uproject =
                                             "../../../FortniteGame/FortniteGame.uproject ") {
    std::string commandLine = uproject;
    for (const auto &argument : arguments) {
        if (argument.first == "failedtoopen")
            continue;
        commandLine += (!argument.second.empty()
                            ? ("-" + argument.first + "=" + argument.second + " ")
                            : ("-" + argument.first + " "));
    }
    return commandLine;
}

inline UE4Args ParseUE4CommandLine(const std::string &filePath) {
    static const std::regex argsRegex("-([A-Za-z_]+)(=(.*))?");
    UE4Args arguments;

    FILE *file = std::fopen(filePath.c_str(), "r");
    if (!file) {
        arguments["failedtoopen"] = "true";
        return arguments;
    }

    char line[1024];
    while (std::fgets(line, sizeof(line), file)) {
        char *token = std::strtok(line, " \t\n");
        while (token) {
            std::string tokenString(token);
            std::smatch match;
            if (std::regex_match(tokenString, match, argsRegex))
                arguments[match[1]] = match[3];
            token = std::strtok(nullptr, " \t\n");
        }
    }

    std::fclose(file);
    return arguments;
}

// Write a command line to an explicit path (no hidden global path any more).
inline bool SaveUE4CommandLine(const std::string &path, const std::string &commandLine) {
    return writeWholeFile(path, commandLine);
}
