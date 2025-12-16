#ifndef ECOWORTHY_BMS_H
#define ECOWORTHY_BMS_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <functional>

// Eco Worthy BMS Service and Characteristic UUIDs
#define ECOWORTHY_SERVICE_UUID "0000fff0-0000-1000-8000-00805f9b34fb"
#define ECOWORTHY_RX_UUID "0000fff1-0000-1000-8000-00805f9b34fb"

// Eco Worthy BMS data structure
struct EcoWorthyBMSData {
    String name;
    String address;
    int rssi;
    
    // Battery measurements
    float voltage;              // V
    float current;              // A (positive = charging, negative = discharging)
    float power;                // W (calculated)
    float batteryLevel;         // % (0-100)
    float batteryHealth;        // % (0-100)
    float designCapacity;       // Ah
    
    // Cell information
    int cellCount;
    float cellVoltages[16];     // Individual cell voltages
    
    // Temperature information
    int tempSensorCount;
    float temperatures[4];      // Temperature readings
    
    // Status
    uint16_t problemCode;       // Error/problem code
    unsigned long lastUpdate;
    bool dataValid;
    bool connected;
    
    EcoWorthyBMSData() : 
        rssi(0),
        voltage(0), 
        current(0), 
        power(0),
        batteryLevel(0),
        batteryHealth(100),
        designCapacity(0),
        cellCount(0),
        tempSensorCount(0),
        problemCode(0),
        lastUpdate(0), 
        dataValid(false),
        connected(false) {
        memset(cellVoltages, 0, sizeof(cellVoltages));
        memset(temperatures, 0, sizeof(temperatures));
    }
};

class EcoWorthyBMS {
private:
    NimBLEClient* pClient;
    NimBLERemoteService* pService;
    NimBLERemoteCharacteristic* pRxCharacteristic;
    
    EcoWorthyBMSData currentData;
    bool isConnected;
    bool isScanning;
    
    uint8_t dataBuffer[256];
    size_t dataBufferLen;
    bool dataReceived;
    
    // MAC address of the device in bytes (for protocol)
    uint8_t macBytes[6];
    
    // Protocol constants
    static const uint8_t HEAD_A1 = 0xA1;
    static const uint8_t HEAD_A2 = 0xA2;
    
    // Data storage for multi-packet messages
    uint8_t dataA1[256];
    uint8_t dataA2[256];
    size_t dataA1Len;
    size_t dataA2Len;
    bool hasDataA1;
    bool hasDataA2;
    
    // Helper functions
    static uint16_t calculateModbusCRC(const uint8_t* data, size_t length);
    bool parseResponse(const uint8_t* data, size_t length);
    bool parseDataA1(const uint8_t* data, size_t length);
    bool parseDataA2(const uint8_t* data, size_t length);
    void extractCellVoltages(const uint8_t* data, size_t length);
    void extractTemperatures(const uint8_t* data, size_t length);
    

    
public:
    EcoWorthyBMS();
    ~EcoWorthyBMS();
    
    bool begin();
    bool connectToDevice(NimBLEAdvertisedDevice* device);
    bool connectToAddress(const String& address);
    bool disconnect();
    bool isDeviceConnected() const;
    
    bool updateData();
    EcoWorthyBMSData* getData();
    
    // Static helpers for device identification
    static bool isEcoWorthyDevice(const String& name);
    static bool isEcoWorthyDevice(NimBLEAdvertisedDevice* device);
};

#endif // ECOWORTHY_BMS_H
