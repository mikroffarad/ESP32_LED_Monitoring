#include "config.h"
#include "effects.h"
#include "webserver.h"
#include "system_manager.h"
#include <Preferences.h>

LEDEffects ledEffects;
WebServerManager webServer(ledEffects);
Preferences preferences;
SystemManager systemManager(preferences);
bool internetStatus = false;
String deviceMode = "factory";

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 WiFi Monitor Starting...");

    ledEffects.init();
    preferences.begin("wifi-monitor", false);
    pinMode(RESET_PIN, INPUT_PULLUP);

    // Перевірка збережених WiFi налаштувань
    String savedSSID = preferences.getString("ssid", "");
    if (savedSSID.length() > 0) {
        // Спроба підключення до збереженої мережі
        deviceMode = "monitoring";
    }

    if (deviceMode == "factory") {
        webServer.startFactoryMode();
    } else {
        webServer.startMonitoringMode();
    }
}

void loop() {
    webServer.handleClient();
    systemManager.checkFactoryReset();
    ledEffects.update();
    
    if (deviceMode == "monitoring") {
        static unsigned long lastInternetCheck = 0;
        if (millis() - lastInternetCheck >= INTERNET_CHECK_INTERVAL) {
            internetStatus = systemManager.checkInternetConnection();
            lastInternetCheck = millis();
        }
    }
    
    delay(10);
}