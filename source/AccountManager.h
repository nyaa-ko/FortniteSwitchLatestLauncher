#pragma once
#include <filesystem>
#include <fstream>
#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

class AccountManager {
private:
    const std::string authFilePath = "sdmc:/switch/FortLatestLauncher/auth.json";
    json accountsArray = json::array();
    int activeAccountIndex = 0;

public:
    AccountManager() {
        if (fs::exists(authFilePath)) {
            std::ifstream file(authFilePath);
            try {
                file >> accountsArray;
                if (accountsArray.is_object()) {
                    json singleAcc = accountsArray;
                    accountsArray = json::array();
                    accountsArray.push_back(singleAcc);
                    Save();
                }
            } catch (...) {}
        }
    }

    void Save() {
        std::ofstream file(authFilePath);
        file << accountsArray.dump(4);
    }

    void AddAccount(json dauthData) {
        for (auto& account : accountsArray) {
            if (account["accountId"] == dauthData["accountId"]) {
                account = dauthData; 
                Save();
                return;
            }
        }
        accountsArray.push_back(dauthData);
        Save();
    }

    json GetAccounts() { return accountsArray; }
    
    void SetActive(int index) { 
        if (index >= 0 && index < accountsArray.size()) {
            activeAccountIndex = index; 
        }
    }
    
    json GetActiveAccount() { 
        if (accountsArray.empty()) return json();
        return accountsArray[activeAccountIndex]; 
    }
};