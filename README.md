# BitFlash_Client
An SDK for enabling simultaneous firmware deployment to multiple IoT devices at once.

## Features
- Background firmware updates
- Automatic WiFi connection management
- Version checking against remote server
- Progress tracking and status callbacks
- Configurable update check intervals

## Installation
1. Download the ZIP file of this repository
2. In Arduino IDE: Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded ZIP file

## Dependencies
- ArduinoJson (>= 6.21.3)
- ESP32 board support

## Usage
```cpp
#include <BitFlash_Client.h>

BitFlash_Client::Config config = {
    .ssid = "your_wifi_ssid",
    .password = "your_wifi_password",
    .currentVersion = "1.0.0",
    .jsonEndpoint = "https://your-server.com/firmware/version.json",
    .checkInterval = 5000, // Check every 5 seconds
    .autoConnect = true
};

BitFlash_Client updater(config);

void setup() {
    Serial.begin(115200);
    updater.begin();
}

void loop() {
    updater.handle();
    // Your code here
}
```