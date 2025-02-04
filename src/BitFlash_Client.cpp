#include "BitFlash_Client.h"

BitFlash_Client::BitFlash_Client(const Config& config) 
    : _config(config), _lastCheck(0), _updateInProgress(false) {
}

void BitFlash_Client::begin() {
    if (_config.autoConnect) {
        connectWiFi();
    }
}

void BitFlash_Client::handle() {
    if (!_updateInProgress && millis() - _lastCheck >= _config.checkInterval) {
        checkForUpdate();
        _lastCheck = millis();
    }
}

void BitFlash_Client::checkForUpdate() {
    if (!isWiFiConnected() && !connectWiFi()) {
        notifyCallback("WiFi connection failed");
        return;
    }

    if (checkVersion()) {
        notifyCallback("Update available");
    }
}

bool BitFlash_Client::checkVersion() {
    WiFiClientSecure client;
    client.setInsecure();
    
    HTTPClient https;
    https.begin(client, _config.jsonEndpoint);
    
    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
        notifyCallback("Failed to fetch version info");
        https.end();
        return false;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, https.getString());
    https.end();
    
    if (error) {
        notifyCallback("Failed to parse version info");
        return false;
    }
    
    const char* latestVersion = doc["version"];
    const char* firmwareUrl = doc["firmware_url"];
    
    if (strcmp(latestVersion, _config.currentVersion) > 0) {
        _updateInProgress = true;
        performUpdate(firmwareUrl);
        return true;
    }
    
    return false;
}

void BitFlash_Client::performUpdate(const char* firmwareUrl) {
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    
    HTTPClient https;
    https.begin(*client, firmwareUrl);
    
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = https.getSize();
        if (contentLength > 0 && Update.begin(contentLength)) {
            WiFiClient * stream = https.getStreamPtr();
            size_t written = 0;
            uint8_t buff[1024] = { 0 };
            
            while (https.connected() && (written < contentLength)) {
                size_t size = stream->available();
                if (size) {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    written += Update.write(buff, c);
                    int progress = (written * 100) / contentLength;
                    notifyCallback("Downloading update", progress);
                }
                yield();
            }
            
            if (written == contentLength && Update.end()) {
                notifyCallback("Update complete, restarting...");
                ESP.restart();
            } else {
                notifyCallback("Update failed");
            }
        }
    }
    
    https.end();
    delete client;
    _updateInProgress = false;
}

bool BitFlash_Client::connectWiFi() {
    if (isWiFiConnected()) return true;
    
    WiFi.begin(_config.ssid, _config.password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (isWiFiConnected()) {
        setClock();
        return true;
    }
    
    return false;
}

void BitFlash_Client::disconnectWiFi() {
    WiFi.disconnect();
}

bool BitFlash_Client::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void BitFlash_Client::setClock() {
    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        now = time(nullptr);
    }
}

void BitFlash_Client::setCheckInterval(uint32_t interval) {
    _config.checkInterval = interval;
}

void BitFlash_Client::setCallback(std::function<void(const char* status, int progress)> callback) {
    _callback = callback;
}

void BitFlash_Client::notifyCallback(const char* status, int progress) {
    if (_callback) {
        _callback(status, progress);
    }
}