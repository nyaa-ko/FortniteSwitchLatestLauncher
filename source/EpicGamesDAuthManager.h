#pragma once
#include <json.hpp>
#include <string>
#include <curl/curl.h>

using json = nlohmann::json;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response) {
    size_t totalSize = size * nmemb;
    response->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

std::string getClientCredentials() {
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, "https://account-public-service-prod.ol.epicgames.com/account/api/oauth/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Authorization: Basic OThmN2U0MmMyZTNhNGY4NmE3NGViNDNmYmI0MWVkMzk6MGEyNDQ5YTItMDAxYS00NTFlLWFmZWMtM2U4MTI5MDFjNGQ3");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    
    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && responseCode == 200) {
        try {
            json j = json::parse(body);
            if (j.contains("access_token")) return j["access_token"].get<std::string>();
        } catch (...) {}
    }
    return "";
}

json startDauthProcess(std::string accessToken) {
    CURL *curl = curl_easy_init();
    if (!curl) return json();

    std::string body;
    struct curl_slist *headers = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, "https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/deviceAuthorization");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");

    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    
    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && responseCode == 200) {
        try { return json::parse(body); } catch (...) {}
    }
    return json();
}

json pollDauth(std::string deviceCode) {
    CURL *curl = curl_easy_init();
    if (!curl) return json();

    std::string body;
    struct curl_slist *headers = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, "https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    std::string postData = "grant_type=device_code&device_code=" + deviceCode;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Authorization: Basic OThmN2U0MmMyZTNhNGY4NmE3NGViNDNmYmI0MWVkMzk6MGEyNDQ5YTItMDAxYS00NTFlLWFmZWMtM2U4MTI5MDFjNGQ3");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    
    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && responseCode == 200) {
        try { return json::parse(body); } catch (...) {}
    }
    return json();
}

std::string getAndroidAccessToken(std::string currentAccessToken) {
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    std::string body;
    struct curl_slist *headers = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, "https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/exchange");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    headers = curl_slist_append(headers, ("Authorization: Bearer " + currentAccessToken).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    curl_easy_perform(curl);
    json j;
    try { j = json::parse(body); } catch (...) { curl_slist_free_all(headers); curl_easy_cleanup(curl); return ""; }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!j.contains("code")) return "";
    std::string exchangeCode = j["code"].get<std::string>();

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    std::string postBody = "grant_type=exchange_code&exchange_code=" + exchangeCode;
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody.c_str());

    body.clear();
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    struct curl_slist *tokenHeaders = NULL;
    tokenHeaders = curl_slist_append(tokenHeaders, "Authorization: Basic M2Y2OWU1NmM3NjQ5NDkyYzhjYzI5ZjFhZjA4YThhMTI6YjUxZWU5Y2IxMjIzNGY1MGE2OWVmYTY3ZWY1MzgxMmU");
    tokenHeaders = curl_slist_append(tokenHeaders, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, tokenHeaders);

    curl_easy_perform(curl);
    curl_slist_free_all(tokenHeaders);
    curl_easy_cleanup(curl);

    try {
        json jToken = json::parse(body);
        if (jToken.contains("access_token")) return jToken["access_token"].get<std::string>();
    } catch (...) {}
    
    return "";
}

json getDeviceAuth(json DAuthResponse) {
    CURL *curl = curl_easy_init();
    if (!curl) return json();

    std::string androidAccessToken = getAndroidAccessToken(DAuthResponse["access_token"].get<std::string>());
    if (androidAccessToken.empty()) {
        curl_easy_cleanup(curl);
        return json();
    }

    std::string body;
    struct curl_slist *headers = NULL;
    std::string accountId = DAuthResponse["account_id"];
    std::string url = "https://account-public-service-prod03.ol.epicgames.com/account/api/public/account/" + accountId + "/deviceAuth";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    headers = curl_slist_append(headers, ("Authorization: Bearer " + androidAccessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    
    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && responseCode == 200) {
        try { return json::parse(body); } catch (...) {}
    }
    return json();
}