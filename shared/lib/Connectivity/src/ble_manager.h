#pragma once

#ifdef ESP32

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "rgbw.h"

// Callback for when a new RGBW color is received over BLE
typedef void (*BLEColorCallback)(const RGBW& color);
typedef void (*BLEBrightnessCallback)(uint8_t brightness);

class BLEManager {
public:
    BLEManager(const char* deviceName);

    void begin();
    bool isConnected() const;

    void onColorReceived(BLEColorCallback callback);
    void onBrightnessReceived(BLEBrightnessCallback callback);

private:
    const char* _deviceName;
    BLEServer* _server;
    bool _connected;
    BLEColorCallback _colorCallback;
    BLEBrightnessCallback _brightnessCallback;

    void _setupService();

    friend class BLEManagerServerCallbacks;
    friend class BLEColorCharCallbacks;
    friend class BLEBrightnessCharCallbacks;
};

#endif
