#pragma once

#ifdef ESP32

#include <WiFi.h>
#include <WebServer.h>

class WiFiManager {
public:
    WiFiManager();

    // Connect to a WiFi network
    bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 10000);

    // Start an access point for configuration
    void startAP(const char* apName, const char* apPassword = nullptr);

    // Start the built-in web server on the given port
    WebServer& getServer() { return _server; }
    void startServer(uint16_t port = 80);
    void handleClient();

    bool isConnected() const;
    String getIP() const;

private:
    WebServer _server;
    bool _apMode;
};

#endif
