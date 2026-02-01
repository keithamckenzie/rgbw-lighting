#ifdef ESP32

#include "ble_manager.h"

#define SERVICE_UUID        "0000ff00-0000-1000-8000-00805f9b34fb"
#define COLOR_CHAR_UUID     "0000ff01-0000-1000-8000-00805f9b34fb"
#define BRIGHTNESS_CHAR_UUID "0000ff02-0000-1000-8000-00805f9b34fb"

static BLEManager* _instance = nullptr;

class BLEManagerServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        if (_instance) _instance->_connected = true;
    }
    void onDisconnect(BLEServer* server) override {
        if (_instance) _instance->_connected = false;
        server->startAdvertising();
    }
};

class BLEColorCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        if (!_instance || !_instance->_colorCallback) return;
        String value = characteristic->getValue();
        if (value.length() >= 4) {
            RGBW color(value[0], value[1], value[2], value[3]);
            _instance->_colorCallback(color);
        }
    }
};

class BLEBrightnessCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        if (!_instance || !_instance->_brightnessCallback) return;
        String value = characteristic->getValue();
        if (value.length() >= 1) {
            _instance->_brightnessCallback(value[0]);
        }
    }
};

BLEManager::BLEManager(const char* deviceName)
    : _deviceName(deviceName), _server(nullptr), _connected(false),
      _colorCallback(nullptr), _brightnessCallback(nullptr) {
    _instance = this;
}

void BLEManager::begin() {
    BLEDevice::init(_deviceName);
    _server = BLEDevice::createServer();
    _server->setCallbacks(new BLEManagerServerCallbacks());
    _setupService();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

bool BLEManager::isConnected() const {
    return _connected;
}

void BLEManager::onColorReceived(BLEColorCallback callback) {
    _colorCallback = callback;
}

void BLEManager::onBrightnessReceived(BLEBrightnessCallback callback) {
    _brightnessCallback = callback;
}

void BLEManager::_setupService() {
    BLEService* service = _server->createService(SERVICE_UUID);

    BLECharacteristic* colorChar = service->createCharacteristic(
        COLOR_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    colorChar->setCallbacks(new BLEColorCharCallbacks());

    BLECharacteristic* brightnessChar = service->createCharacteristic(
        BRIGHTNESS_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
    );
    brightnessChar->setCallbacks(new BLEBrightnessCharCallbacks());

    service->start();
}

#endif
