#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdbool.h>
#include <sys/stat.h>
#include <switch.h>
#include <dirent.h>
#include <algorithm>
#include <unistd.h>
#include <curl/curl.h>

#include "EpicGamesDAuthManager.h"
#include "UE4CommandLineManager.h"
#include "AccountManager.h"

#define VERSION "2.0.0"
#define GAME_TITLE_ID 0x010025400AECE000ULL
#define COMMAND_LINE_PATH "sdmc:/atmosphere/contents/010025400AECE000/romfs/UECommandLine.txt"
#define OLD_COMMAND_LINE_PATH "sdmc:/switch/FortLatestLauncher/OldCommandLine.txt"

using namespace std;
using json = nlohmann::json;

bool HasConnection()
{
    NifmInternetConnectionStatus status;
    nifmGetInternetConnectionStatus(nullptr, nullptr, &status);
    return status == NifmInternetConnectionStatus_Connected;
}

string ReplaceAll(string str, const string &from, const string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

void EnsureCommandLine()
{
    FILE *file = fopen(COMMAND_LINE_PATH, "r");
    if (file) {
        fclose(file);
        return;
    }

    file = fopen(COMMAND_LINE_PATH, "w");
    if (!file) return;

    fprintf(file, "%s", "../../../FortniteGame/FortniteGame.uproject -skippatchcheck");
    fclose(file);
}

void PrintHeader(const string &title)
{
    consoleClear();
    printf("\n");
    printf("  ==================================================\n");
    printf("                 MIDNIGHT LAUNCHER\n");
    printf("  ==================================================\n");
    printf("  %s\n\n", title.c_str());
}

void PrintAccount(const json &account, bool selected, bool active)
{
    string label = account.value("label", "Unnamed Account");
    string color = account.value("color", "#7C5CFF");
    string marker = selected ? ">" : " ";
    string activeMarker = active ? " [ACTIVE]" : "";

    // Keep the hex color visible even on consoles that do not implement ANSI colors.
    printf("  %s %-24s %s%s\n", marker.c_str(), label.c_str(), color.c_str(), activeMarker.c_str());
}

bool Confirm(const string &message, PadState *pad)
{
    printf("\n  %s\n", message.c_str());
    printf("  A - Confirm     B - Cancel\n");
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);
        if (kDown & HidNpadButton_A) return true;
        if (kDown & HidNpadButton_B) return false;
        if (kDown & HidNpadButton_Plus) return false;
        consoleUpdate(NULL);
    }
    return false;
}

void DrawMain(const json &db, const string &message = "")
{
    PrintHeader("HOME");

    json active = AccountManager::GetActive(db);
    if (active.empty()) {
        printf("  ACCOUNT\n");
        printf("  No account selected.\n\n");
    } else {
        printf("  ACCOUNT\n");
        printf("  %s\n", active.value("label", "Unnamed Account").c_str());
        printf("  Accent: %s\n\n", active.value("color", "#7C5CFF").c_str());
    }

    if (!message.empty()) printf("  %s\n\n", message.c_str());

    printf("  A  Accounts\n");
    printf("  B  Launch Fortnite\n");
    printf("  X  Restore CommandLine\n");
    printf("  +  Exit\n\n");
    printf("  Midnight Launcher %s\n", VERSION);
    consoleUpdate(NULL);
}

void AccountMenu(PadState *pad)
{
    json db = AccountManager::Load();
    int selected = 0;

    while (appletMainLoop()) {
        db = AccountManager::Load();
        int count = db["accounts"].size();
        if (count == 0) selected = 0;
        else if (selected >= count) selected = count - 1;

        PrintHeader("ACCOUNTS");

        if (count == 0) {
            printf("  No accounts saved.\n\n");
            printf("  X  Add Account\n");
            printf("  B  Back\n");
        } else {
            for (int i = 0; i < count; ++i) {
                const json &account = db["accounts"][i];
                bool active = account.value("id", "") == db.value("activeAccount", "");
                PrintAccount(account, i == selected, active);
            }

            printf("\n  A  Select account\n");
            printf("  X  Add account\n");
            printf("  Y  Delete account\n");
            printf("  R  Change color\n");
            printf("  B  Back\n");
        }

        consoleUpdate(NULL);
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);

        if (kDown & HidNpadButton_Up) {
            if (count > 0) selected = (selected + count - 1) % count;
        }
        else if (kDown & HidNpadButton_Down) {
            if (count > 0) selected = (selected + 1) % count;
        }
        else if (kDown & HidNpadButton_B) {
            return;
        }
        else if (kDown & HidNpadButton_X) {
            PrintHeader("ADD ACCOUNT");
            printf("  Starting Epic Games device authentication...\n\n");
            printf("  Follow the verification URL shown below.\n");
            consoleUpdate(NULL);

            json auth = InitializeAuthProcess();
            if (auth.empty()) {
                PrintHeader("ACCOUNTS");
                printf("  Account authentication failed or was cancelled.\n");
                printf("  Press B to return.\n");
                consoleUpdate(NULL);
                while (appletMainLoop()) {
                    padUpdate(pad);
                    u64 b = padGetButtonsDown(pad);
                    if (b & HidNpadButton_B) break;
                    if (b & HidNpadButton_Plus) return;
                }
                continue;
            }

            string label = auth.value("displayName", "Epic Account");
            string id;
            if (AccountManager::Add(auth, label, AccountManager::DefaultColor(count), id)) {
                db = AccountManager::Load();
                selected = AccountManager::FindIndex(db, id);
                PrintHeader("ACCOUNT ADDED");
                printf("  %s\n", label.c_str());
                printf("  Color: %s\n\n", db["accounts"][selected].value("color", "#7C5CFF").c_str());
                printf("  This account is now active.\n");
                printf("  Press B to continue.\n");
                consoleUpdate(NULL);
                while (appletMainLoop()) {
                    padUpdate(pad);
                    u64 b = padGetButtonsDown(pad);
                    if (b & HidNpadButton_B) break;
                    if (b & HidNpadButton_Plus) return;
                }
            }
        }
        else if (kDown & HidNpadButton_A) {
            if (count > 0) {
                string id = db["accounts"][selected].value("id", "");
                AccountManager::SetActive(id);
            }
        }
        else if (kDown & HidNpadButton_R) {
            if (count > 0) {
                string id = db["accounts"][selected].value("id", "");
                AccountManager::CycleColor(id);
            }
        }
        else if (kDown & HidNpadButton_Y) {
            if (count > 0) {
                string id = db["accounts"][selected].value("id", "");
                string label = db["accounts"][selected].value("label", "this account");
                if (Confirm("Delete " + label + "?", pad)) {
                    AccountManager::Remove(id);
                    db = AccountManager::Load();
                    if (selected >= static_cast<int>(db["accounts"].size()) && !db["accounts"].empty())
                        selected = db["accounts"].size() - 1;
                }
            }
        }
    }
}

void RestoreCommandLine()
{
    auto arguments = ParseUE4CommandLine(OLD_COMMAND_LINE_PATH);
    if (arguments["failedtoopen"].empty()) {
        SaveUE4CommandLine(RebuildUE4CommandLine(arguments));
        remove(OLD_COMMAND_LINE_PATH);
    }
}

bool LaunchActiveAccount(PadState *pad)
{
    json db = AccountManager::Load();
    json account = AccountManager::GetActive(db);

    if (account.empty() || !account.contains("auth")) {
        DrawMain(db, "Add an account before launching.");
        return false;
    }

    json auth = account["auth"];
    auto arguments = ParseUE4CommandLine(COMMAND_LINE_PATH);

    if (arguments["AUTH_TYPE"] != "exchangecode")
        storeOldUE4CommandLine(arguments);

    PrintHeader("LAUNCHING");
    printf("  Account: %s\n", account.value("label", "Unnamed Account").c_str());
    printf("  Authenticating with Epic Games services...\n\n");
    consoleUpdate(NULL);

    string exchangeCode = getExchangeCode(auth);
    if (exchangeCode == "INVALID_DEVICE_AUTH") {
        DrawMain(db, "Saved credentials are invalid. Re-authenticate this account.");
        return false;
    }

    if (exchangeCode.empty()) {
        DrawMain(db, "Authentication failed.");
        return false;
    }

    arguments["AUTH_PASSWORD"] = exchangeCode;
    arguments["AUTH_LOGIN"] = "unused";
    arguments["AUTH_TYPE"] = "exchangecode";
    arguments["AuthClient"] = "3f69e56c7649492c8cc29f1af08a8a12";
    arguments["AuthSecret"] = "b51ee9cb12234f50a69efa67ef53812e";

    SaveUE4CommandLine(RebuildUE4CommandLine(arguments));

    PrintHeader("LAUNCHING");
    printf("  Account: %s\n\n", account.value("label", "Unnamed Account").c_str());
    printf("  Launching Fortnite...\n");
    printf("  Press B to return if the application does not open.\n");
    consoleUpdate(NULL);
    sleep(2);

    appletRequestLaunchApplication(GAME_TITLE_ID, NULL);
    return true;
}

int main(int argc, char* argv[])
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);

    AccountManager::EnsureDirectory();
    EnsureCommandLine();

    if (!HasConnection()) {
        PrintHeader("OFFLINE");
        printf("  An internet connection is required.\n");
        printf("  Press + to exit.\n");
        consoleUpdate(NULL);

        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            consoleUpdate(NULL);
        }

        socketExit();
        nifmExit();
        consoleExit(NULL);
        return 0;
    }

    while (appletMainLoop()) {
        json db = AccountManager::Load();
        DrawMain(db);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break;

        if (kDown & HidNpadButton_A) {
            AccountMenu(&pad);
        }
        else if (kDown & HidNpadButton_B) {
            LaunchActiveAccount(&pad);
        }
        else if (kDown & HidNpadButton_X) {
            RestoreCommandLine();
            DrawMain(AccountManager::Load(), "CommandLine arguments restored.");
            sleep(1);
        }
    }

    socketExit();
    nifmExit();
    consoleExit(NULL);
    return 0;
}
