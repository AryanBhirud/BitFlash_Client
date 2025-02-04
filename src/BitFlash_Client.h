#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
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
    void performUpdate(const char* firmwareUrl);
    void notifyCallback(const char* status, int progress = -1);
};