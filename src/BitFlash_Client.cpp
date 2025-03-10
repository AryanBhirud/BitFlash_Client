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
std::unique_ptr<Client> BitFlash_Client::createClient(const String& url) {
    if (url.startsWith("https://")) {
        auto secureClient = std::make_unique<WiFiClientSecure>();
        
        // SSL verification settings
        if (_config.verifySSL) {
            // Add root CA certificate if verification is needed
            // secureClient->setCACert(rootCACertificate);
        } else {
            secureClient->setInsecure();
        }
        
        return secureClient;
    } else if (url.startsWith("http://")) {
        return std::make_unique<WiFiClient>();
    } else {
        notifyCallback("Invalid URL protocol");
        return nullptr;
    }
}

HTTPClient* BitFlash_Client::createHTTPClient(Client* client, const String& url) {
    if (!client) return nullptr;
    
    HTTPClient* https = new HTTPClient();
    
    if (url.startsWith("https://")) {
        WiFiClientSecure* secureClient = static_cast<WiFiClientSecure*>(client);
        if (secureClient) {
            https->begin(*secureClient, url);
        } else {
            notifyCallback("Failed to create secure client");
            delete https;
            return nullptr;
        }
    } else if (url.startsWith("http://")) {
        WiFiClient* regularClient = static_cast<WiFiClient*>(client);
        if (regularClient) {
            https->begin(*regularClient, url);
        } else {
            notifyCallback("Failed to create client");
            delete https;
            return nullptr;
        }
    } else {
        notifyCallback("Invalid URL protocol");
        delete https;
        return nullptr;
    }
    
    return https;
}

bool BitFlash_Client::checkVersion() {
    _updateInProgress = true;
    
    // Create appropriate client
    auto client = createClient(_config.jsonEndpoint);
    if (!client) {
        _updateInProgress = false;
        return false;
    }
    
    // Create HTTPClient
    HTTPClient* https = createHTTPClient(client.get(), _config.jsonEndpoint);
    if (!https) {
        _updateInProgress = false;
        return false;
    }
    
    int httpCode = https->GET();
    if (httpCode != HTTP_CODE_OK) {
        notifyCallback("Failed to fetch version info");
        https->end();
        delete https;
        _updateInProgress = false;
        return false;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, https->getString());
    
    https->end();
    delete https;
    
    if (error) {
        notifyCallback("Failed to parse version info");
        _updateInProgress = false;
        return false;
    }
    
    const char* latestVersion = doc["version"];
    const char* firmwareUrl = doc["firmware_url"];
    
    if (!latestVersion || !firmwareUrl) {
        notifyCallback("Invalid version info format");
        _updateInProgress = false;
        return false;
    }
    
    if (compareVersions(_config.currentVersion, latestVersion) < 0) {
        return performUpdate(firmwareUrl);
    }
    
    _updateInProgress = false;
    return false;
}

bool BitFlash_Client::performUpdate(const char* firmwareUrl) {
    // Create appropriate client for firmware download
    auto client = createClient(firmwareUrl);
    if (!client) {
        _updateInProgress = false;
        return false;
    }
    
    // Create HTTPClient
    HTTPClient* https = createHTTPClient(client.get(), firmwareUrl);
    if (!https) {
        _updateInProgress = false;
        return false;
    }
    
    int httpCode = https->GET();
    if (httpCode != HTTP_CODE_OK) {
        notifyCallback("Failed to download firmware");
        https->end();
        delete https;
        _updateInProgress = false;
        return false;
    }
    
    int contentLength = https->getSize();
    if (contentLength <= 0) {
        notifyCallback("Invalid firmware size");
        https->end();
        delete https;
        _updateInProgress = false;
        return false;
    }
    
    if (!Update.begin(contentLength)) {
        notifyCallback("Not enough space for update");
        https->end();
        delete https;
        _updateInProgress = false;
        return false;
    }
    
    WiFiClient* stream = https->getStreamPtr();
    size_t written = 0;
    uint8_t buff[1024] = { 0 };
    
    while (https->connected() && (written < contentLength)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            written += Update.write(buff, c);
            int progress = (written * 100) / contentLength;
            notifyCallback("Downloading update", progress);
        }
        yield();
    }
    
    https->end();
    delete https;
    
    if (written != contentLength) {
        notifyCallback("Download incomplete");
        Update.abort();
        _updateInProgress = false;
        return false;
    }
    
    if (!Update.end()) {
        notifyCallback("Update failed");
        _updateInProgress = false;
        return false;
    }
    
    notifyCallback("Update complete, restarting...");
    delay(1000);
    ESP.restart();
    return true;
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

int BitFlash_Client::compareVersions(const char* v1, const char* v2) {
    // Parse versions into components
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    // Compare major version
    if (major1 != major2) {
        return major1 - major2;
    }
    
    // Compare minor version
    if (minor1 != minor2) {
        return minor1 - minor2;
    }
    
    // Compare patch version
    return patch1 - patch2;
}