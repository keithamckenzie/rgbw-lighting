#ifdef ESP32

#include "wifi_manager.h"

WiFiManager::WiFiManager() : _server(80), _apMode(false) {}

bool WiFiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(100);
    }
    _apMode = false;
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startAP(const char* apName, const char* apPassword) {
    WiFi.mode(WIFI_AP);
    if (apPassword) {
        WiFi.softAP(apName, apPassword);
    } else {
        WiFi.softAP(apName);
    }
    _apMode = true;
}

void WiFiManager::startServer(uint16_t port) {
    _server = WebServer(port);
    _server.begin();
}

void WiFiManager::handleClient() {
    _server.handleClient();
}

bool WiFiManager::isConnected() const {
    if (_apMode) return true;
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIP() const {
    if (_apMode) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

#endif
