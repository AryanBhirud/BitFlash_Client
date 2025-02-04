#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <Update.h>
#include <time.h>
#include <ArduinoJson.h>
#include <functional>

class BitFlash_Client {
public:
    struct Config {
        const char* ssid;
        const char* password;
        const char* currentVersion;
        const char* jsonEndpoint;
        uint32_t checkInterval;  // In milliseconds
        bool autoConnect;        // Whether to auto-connect to WiFi
        bool verifySSL = false; // Whether to verify SSL certificates
    };

    BitFlash_Client(const Config& config);
    
    void begin();
    void handle();
    void checkForUpdate();
    void setCheckInterval(uint32_t interval);
    void setCallback(std::function<void(const char* status, int progress)> callback);
    bool connectWiFi();
    void disconnectWiFi();
    bool isWiFiConnected();

private:
    Config _config;
    unsigned long _lastCheck;
    std::function<void(const char* status, int progress)> _callback;
    bool _updateInProgress;
    
    void setClock();
    bool checkVersion();
    bool performUpdate(const char* firmwareUrl);
    void notifyCallback(const char* status, int progress = -1);
    int compareVersions(const char* v1, const char* v2);
    
    // Helper method to create appropriate client based on URL
    std::unique_ptr<Client> createClient(const String& url);
    
    // Helper method to get the appropriate HTTPClient method
    HTTPClient* createHTTPClient(Client* client, const String& url);
};