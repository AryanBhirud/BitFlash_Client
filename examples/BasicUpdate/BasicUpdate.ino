#include <BitFlash_Client.h>

void updateCallback(const char* status, int progress) {
    Serial.printf("Update Status: %s", status);
    if (progress >= 0) {
        Serial.printf(" (%d%%)", progress);
    }
    Serial.println();
}

BitFlash_Client::Config config = {
    .ssid = "gheepodidosa",
    .password = "hehehehe",
    .currentVersion = "1.0.0",
    .jsonEndpoint = "http://192.168.63.87:3030/",
    .id = "001",  // Unique identifier for your device
    .checkInterval = 1000, // Check every 5 seconds
    .autoConnect = true
};

BitFlash_Client updater(config);

void setup() {
    Serial.begin(115200);
    Serial.println("BitFlash Client Example");
    
    updater.setCallback(updateCallback);
    updater.begin();
}

void loop() {
    updater.handle();
    
    // Your application code here
    delay(1000);
}