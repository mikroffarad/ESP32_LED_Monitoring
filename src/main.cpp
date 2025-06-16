#include "config.h"
#include "effects.h"
#include "webserver.h"
#include "system_manager.h"
#include <Preferences.h>
#include <WiFi.h>

LEDEffects ledEffects;
Preferences preferences;
WebServerManager webServer(ledEffects, preferences);
SystemManager systemManager(preferences);
bool internetStatus = false;
String deviceMode = "factory";

void setup() {
    Serial.begin(115200);
    delay(1000); // Даємо час на ініціалізацію
    Serial.println("ESP32 WiFi Monitor Starting...");

    // Ініціалізуємо WiFi перед усім іншим
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    
    ledEffects.init();
    preferences.begin("wifi-monitor", false);
    pinMode(RESET_PIN, INPUT_PULLUP);

    // Перевірка збережених WiFi налаштувань
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    
    if (savedSSID.length() > 0) {
        Serial.println("Trying to connect to saved network: " + savedSSID);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        // Чекаємо на підключення
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to WiFi");
            deviceMode = "monitoring";
        } else {
            Serial.println("\nFailed to connect. Starting in factory mode.");
            deviceMode = "factory";
        }
    }

    if (deviceMode == "factory") {
        ledEffects.setEffect("waiting");
        webServer.startFactoryMode();
    } else {
        ledEffects.setEffect("static");
        webServer.startMonitoringMode();
    }
    
    Serial.println("Setup completed");
}

void loop() {
    webServer.handleClient();
    systemManager.checkFactoryReset();
    ledEffects.update();
    
    if (deviceMode == "monitoring") {
        static unsigned long lastInternetCheck = 0;
        if (millis() - lastInternetCheck >= INTERNET_CHECK_INTERVAL) {
            internetStatus = systemManager.checkInternetConnection();
            webServer.setInternetStatus(internetStatus);
            lastInternetCheck = millis();
            
            // Додаємо діагностичну інформацію
            Serial.printf("Internet check: %s\n", internetStatus ? "OK" : "Failed");
        }
    }
    
    delay(10); // Зменшуємо затримку для кращої відгучності
}