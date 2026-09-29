#include <switch.h>
#include <stdio.h>
#include <unistd.h>
#include "AccountManager.h"
#include "PatchSwitcher.h"
#include "EpicGamesDAuthManager.h"

AccountManager accManager;
const std::string fnTitleIdPath = "sdmc:/atmosphere/contents/0100704000B3A000";

void DrawMenu(int cursor, int menuState) {
    consoleClear();
    printf("\x1b[36m=== Fortnite CUI Manager ===\x1b[0m\n\n");

    if (menuState == 0) { 
        printf("%c 1. Launch Game (Active Account: %s)\n", cursor == 0 ? '>' : ' ', 
            accManager.GetActiveAccount().value("displayName", "None").c_str());
        printf("%c 2. Add New Account\n", cursor == 1 ? '>' : ' ');
        printf("%c 3. Switch Account\n", cursor == 2 ? '>' : ' ');
        
        bool patchEnabled = PatchSwitcher::IsEnabled(fnTitleIdPath);
        printf("%c 4. Toggle FN Patch [%s]\n", cursor == 3 ? '>' : ' ', patchEnabled ? "ON" : "OFF");
        printf("%c 5. Exit\n", cursor == 4 ? '>' : ' ');
    } 
    else if (menuState == 1) { 
        printf("Select an account:\n\n");
        json accounts = accManager.GetAccounts();
        for (size_t i = 0; i < accounts.size(); i++) {
            printf("%c %s\n", cursor == i ? '>' : ' ', accounts[i].value("displayName", "Unknown").c_str());
        }
        printf("\nPress B to return.");
    }
    consoleUpdate(NULL);
}

void InitializeAuthProcessCUI(AccountManager& am) {
    consoleClear();
    printf("\nInitializing auth process...\n");
    consoleUpdate(NULL);

    std::string clientToken = getClientCredentials();
    json dauthInit = startDauthProcess(clientToken);
    
    if (dauthInit.empty()) {
        printf("\nFailed to initialize DAuth. Returning in 3 seconds.\n");
        consoleUpdate(NULL);
        sleep(3);
        return;
    }

    printf("\nGo to %s to authenticate.\n", dauthInit["verification_uri_complete"].get<std::string>().c_str());
    printf("Press + to cancel.\n");
    consoleUpdate(NULL);

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) break;

        json dauthPoll = pollDauth(dauthInit["device_code"].get<std::string>());
        if (!dauthPoll.empty()) {
            json dauth = getDeviceAuth(dauthPoll);
            if (!dauth.empty()) {
                dauth["displayName"] = dauthPoll["displayName"];
                am.AddAccount(dauth); 
                printf("\nSaved auth info for %s!\n", dauth["displayName"].get<std::string>().c_str());
                consoleUpdate(NULL);
                sleep(3);
                break;
            }
        }
        sleep(2); 
    }
}

int main(int argc, char **argv) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    int cursor = 0;
    int menuState = 0; 
    int maxCursor = 4;

    DrawMenu(cursor, menuState);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus) break;

        if (kDown & HidNpadButton_Up) {
            cursor = (cursor - 1 < 0) ? maxCursor : cursor - 1;
            DrawMenu(cursor, menuState);
        }
        if (kDown & HidNpadButton_Down) {
            cursor = (cursor + 1 > maxCursor) ? 0 : cursor + 1;
            DrawMenu(cursor, menuState);
        }

        if (kDown & HidNpadButton_A) {
            if (menuState == 0) {
                if (cursor == 0) {
                    // Placeholder for game launch execution
                } else if (cursor == 1) {
                    InitializeAuthProcessCUI(accManager);
                    DrawMenu(cursor, menuState);
                } else if (cursor == 2) {
                    menuState = 1;
                    cursor = 0;
                    maxCursor = accManager.GetAccounts().empty() ? 0 : accManager.GetAccounts().size() - 1;
                    DrawMenu(cursor, menuState);
                } else if (cursor == 3) {
                    PatchSwitcher::Toggle(fnTitleIdPath);
                    DrawMenu(cursor, menuState);
                } else if (cursor == 4) {
                    break;
                }
            } else if (menuState == 1) {
                if (!accManager.GetAccounts().empty()) {
                    accManager.SetActive(cursor);
                }
                menuState = 0;
                cursor = 0;
                maxCursor = 4;
                DrawMenu(cursor, menuState);
            }
        }

        if ((kDown & HidNpadButton_B) && menuState == 1) {
            menuState = 0;
            cursor = 0;
            maxCursor = 4;
            DrawMenu(cursor, menuState);
        }
    }

    consoleExit(NULL);
    return 0;
}
