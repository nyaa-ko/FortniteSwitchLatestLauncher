#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdbool.h>
#include <sys/stat.h>
#include <switch.h>
#include <dirent.h>
#include <algorithm>

#include "EpicGamesDAuthManager.h"
#include "UE4CommandLineManager.h"

#include <unistd.h>
#include <curl/curl.h>

#define VERSION "1.0.2" 

#define TRACE(fmt, ...)                                          \
    printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__); \
    consoleUpdate(NULL);

// カラーコードの定義
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"

bool HasConnection()
{
    NifmInternetConnectionStatus status;
    nifmGetInternetConnectionStatus(nullptr, nullptr, &status);
    return status == NifmInternetConnectionStatus_Connected;
}

std::string ReplaceAll(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
    return str;
}

// 簡単なメッセージを表示してAかBで閉じる関数
void ShowMessage(PadState* pad, const std::string& msg) {
    while (appletMainLoop()) {
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);
        
        consoleClear();
        printf("%s\n\nPress [A] or [B] to return.\n", msg.c_str());
        consoleUpdate(NULL);
        
        if (kDown & (HidNpadButton_A | HidNpadButton_B)) {
            break;
        }
    }
}

// ソフトウェアキーボードを用いた文字列入力ヘルパー
std::string GetKeyboardInput(const char* guideText, const char* initialText = "") {
    SwkbdConfig swkbd;
    char out_string[256] = {0};
    Result rc = swkbdCreate(&swkbd, 0);
    if (R_SUCCEEDED(rc)) {
        swkbdConfigMakePresetDefault(&swkbd);
        swkbdConfigSetGuideText(&swkbd, guideText);
        if (initialText[0] != '\0') {
            swkbdConfigSetInitialText(&swkbd, initialText);
        }
        rc = swkbdShow(&swkbd, out_string, sizeof(out_string));
        swkbdClose(&swkbd);
        if (R_SUCCEEDED(rc)) {
            return std::string(out_string);
        }
    }
    return "";
}

// 色を選択するメニュー
std::string ChooseColorMenu(PadState* pad) {
    const char* colors[] = {COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_RESET};
    const char* colorNames[] = {"Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "Default"};
    int selected = 0;
    int max_colors = 7;
    
    while (appletMainLoop()) {
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);
        
        consoleClear();
        printf("=== Select Label Color ===\n\n");
        for (int i = 0; i < max_colors; ++i) {
            if (i == selected) {
                printf("> %s%s%s <\n", colors[i], colorNames[i], COLOR_RESET);
            } else {
                printf("  %s%s%s  \n", colors[i], colorNames[i], COLOR_RESET);
            }
        }
        printf("\nD-Pad Up/Down: Select | A: Confirm | B: Back\n");
        consoleUpdate(NULL);
        
        if (kDown & HidNpadButton_Up) { if (selected > 0) selected--; }
        if (kDown & HidNpadButton_Down) { if (selected < max_colors - 1) selected++; }
        if (kDown & HidNpadButton_A) { return std::string(colors[selected]); }
        if (kDown & HidNpadButton_B) { return "CANCEL"; }
    }
    return "CANCEL";
}

// アカウント選択メニュー
int SelectAccountMenu(PadState* pad, const char* title) {
    json accounts = GetAccounts();
    if (accounts.empty()) return -1;

    int selectedIndex = 0;
    while (appletMainLoop()) {
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);

        consoleClear();
        printf("=== %s ===\n\n", title);
        
        for (size_t i = 0; i < accounts.size(); ++i) {
            std::string label = accounts[i].value("label", "Unknown");
            std::string color = accounts[i].value("color", COLOR_RESET);
            std::string dispName = accounts[i]["auth"].value("displayName", "");
            
            if (i == selectedIndex) {
                printf("> %s%s%s (%s) <\n", color.c_str(), label.c_str(), COLOR_RESET, dispName.c_str());
            } else {
                printf("  %s%s%s (%s)  \n", color.c_str(), label.c_str(), COLOR_RESET, dispName.c_str());
            }
        }
        
        printf("\nD-Pad Up/Down: Select | A: Confirm | B: Back\n");
        consoleUpdate(NULL);

        if (kDown & HidNpadButton_Up) { if (selectedIndex > 0) selectedIndex--; }
        if (kDown & HidNpadButton_Down) { if (selectedIndex < accounts.size() - 1) selectedIndex++; }
        if (kDown & HidNpadButton_A) { return selectedIndex; }
        if (kDown & HidNpadButton_B) { return -1; }
    }
    return -1;
}

int main(int argc, char* argv[])
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);

    if (!HasConnection())
    {
        printf("Internet connection is required.\nPress + to exit.\n");
        while (appletMainLoop())
        {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            consoleUpdate(NULL);
        }
        socketExit();
        nifmExit();
        consoleExit(NULL);
        return 0;
    }
    
    DIR *dir = opendir("sdmc:/switch/FortLatestLauncher");
    if(!dir) { mkdir("sdmc:/switch/FortLatestLauncher", 0777); }
    closedir(dir);

    FILE *file = fopen("sdmc:/atmosphere/contents/010025400AECE000/romfs/UECommandLine.txt", "r");
    if (!file) {
        fclose(file);
        file = fopen("sdmc:/atmosphere/contents/010025400AECE000/romfs/UECommandLine.txt", "w");
        fprintf(file, "../../../FortniteGame/FortniteGame.uproject -skippatchcheck");
    }
    fclose(file);

    const char* mainOptions[] = {
        "Launch Fortnite",
        "Manage Accounts (Add/Delete)",
        "Restore CommandLine Arguments",
        "Exit"
    };
    int mainSelected = 0;
    int maxMainOptions = 4;

    while (appletMainLoop())
    {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        // メインメニュー描画
        consoleClear();
        printf("Version %s\n\n", VERSION);
        printf("-----Main Menu-----\n\n");
        
        for (int i = 0; i < maxMainOptions; ++i) {
            if (i == mainSelected) {
                printf("> %s <\n", mainOptions[i]);
            } else {
                printf("  %s  \n", mainOptions[i]);
            }
        }
        
        printf("\nD-Pad Up/Down: Select | A: Confirm\n");
        consoleUpdate(NULL);

        // メインメニュー操作
        if (kDown & HidNpadButton_Up) { if (mainSelected > 0) mainSelected--; }
        if (kDown & HidNpadButton_Down) { if (mainSelected < maxMainOptions - 1) mainSelected++; }
        
        if (kDown & HidNpadButton_A) {
            // 1. 起動
            if (mainSelected == 0) {
                json accounts = GetAccounts();
                if (accounts.empty()) {
                    ShowMessage(&pad, "No registered accounts found.\nPlease add an account from 'Manage Accounts'.");
                    continue;
                }

                int accIndex = 0;
                if (accounts.size() > 1) {
                    accIndex = SelectAccountMenu(&pad, "Select Account to Launch");
                    if (accIndex == -1) continue; 
                }

                json targetAuth = accounts[accIndex]["auth"];
                std::unordered_map<string, string> arguments = ParseUE4CommandLine("sdmc:/atmosphere/contents/010025400AECE000/romfs/UECommandLine.txt");

                if (arguments["AUTH_TYPE"] != "exchangecode") 
                    storeOldUE4CommandLine(arguments);

                consoleClear();
                printf("Authenticating with Epic Games services...\n\n");
                consoleUpdate(NULL);
                
                std::string exchangeCode = getExchangeCode(targetAuth);
                if (exchangeCode == "INVALID_DEVICE_AUTH") {
                    ShowMessage(&pad, "Invalid credentials.\nPlease delete and re-add this account.");
                    continue;
                }

                arguments["AUTH_PASSWORD"] = exchangeCode;
                arguments["AUTH_LOGIN"] = "unused";
                arguments["AUTH_TYPE"] = "exchangecode";
                arguments["AuthClient"] = "98f7e42c2e3a4f86a74eb43fbb41ed39";
                arguments["AuthSecret"] = "0a2449a2-001a-451e-afec-3e812901c4d7";

                std::string commandLine = RebuildUE4CommandLine(arguments);
                SaveUE4CommandLine(commandLine);

                printf("Launching Fortnite...\n\n");
                consoleUpdate(NULL);
                sleep(2);

                appletRequestLaunchApplication(0x010025400AECE000, NULL);
            }
            // 2. アカウント管理
            else if (mainSelected == 1) {
                const char* accOptions[] = {"Add New Account", "Delete Account"};
                int accSelected = 0;
                
                while (appletMainLoop()) {
                    padUpdate(&pad);
                    u64 subDown = padGetButtonsDown(&pad);

                    consoleClear();
                    printf("=== Account Management ===\n\n");
                    for (int i = 0; i < 2; ++i) {
                        if (i == accSelected) printf("> %s <\n", accOptions[i]);
                        else printf("  %s  \n", accOptions[i]);
                    }
                    printf("\nD-Pad Up/Down: Select | A: Confirm | B: Back\n");
                    consoleUpdate(NULL);

                    if (subDown & HidNpadButton_Up) { if (accSelected > 0) accSelected--; }
                    if (subDown & HidNpadButton_Down) { if (accSelected < 1) accSelected++; }
                    if (subDown & HidNpadButton_B) { break; } 

                    if (subDown & HidNpadButton_A) {
                        // 新規追加
                        if (accSelected == 0) {
                            consoleClear();
                            json newDauth = InitializeAuthProcess();
                            
                            if (!newDauth.empty()) {
                                std::string defaultLabel = newDauth["displayName"].get<std::string>();
                                // キーボード入力ダイアログはOS標準のものなので日本語入力・表示が可能です
                                std::string label = GetKeyboardInput("Enter Account Label", defaultLabel.c_str());
                                if (label.empty()) label = defaultLabel;

                                std::string color = ChooseColorMenu(&pad);
                                if (color == "CANCEL") {
                                    ShowMessage(&pad, "Account creation canceled.");
                                    continue; 
                                }

                                json newAccount;
                                newAccount["label"] = label;
                                newAccount["color"] = color;
                                newAccount["auth"] = newDauth;

                                json accounts = GetAccounts();
                                accounts.push_back(newAccount);
                                SaveAccounts(accounts);
                                
                                ShowMessage(&pad, "Account added successfully!");
                            } else {
                                ShowMessage(&pad, "Authentication failed or was canceled.");
                            }
                        }
                        // 削除
                        else if (accSelected == 1) {
                            int idx = SelectAccountMenu(&pad, "Select Account to Delete");
                            if (idx != -1) {
                                json accounts = GetAccounts();
                                accounts.erase(accounts.begin() + idx);
                                SaveAccounts(accounts);
                                ShowMessage(&pad, "Account deleted successfully.");
                            }
                        }
                    }
                }
            }
            // 3. コマンドライン復元
            else if (mainSelected == 2) {
                std::unordered_map<string, string> arguments = ParseUE4CommandLine("sdmc:/switch/FortLatestLauncher/OldCommandLine.txt");
                if (arguments["failedtoopen"].empty()) {
                    SaveUE4CommandLine(RebuildUE4CommandLine(arguments));
                    remove("sdmc:/switch/FortLatestLauncher/OldCommandLine.txt");
                    ShowMessage(&pad, "CommandLine arguments restored successfully.");
                } else {
                    ShowMessage(&pad, "No old CommandLine data found to restore.");
                }
            }
            // 4. 終了
            else if (mainSelected == 3) {
                break;
            }
        }
    }

    socketExit();
    nifmExit();
    consoleExit(NULL);
    return 0;
}
