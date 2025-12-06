#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <map>

// Victron BLE Service UUID
#define VICTRON_MANUFACTURER_ID 0x02E1

// Device Types
enum VictronDeviceType {
    DEVICE_UNKNOWN = 0,
    DEVICE_SMART_SHUNT = 1,
    DEVICE_SMART_SOLAR = 2,
    DEVICE_BLUE_SMART_CHARGER = 3
};

// Record Types from Victron BLE advertising
enum VictronRecordType {
    SOLAR_CHARGER_VOLTAGE = 0x01,
    SOLAR_CHARGER_CURRENT = 0x02,
    BATTERY_VOLTAGE = 0x03,
    BATTERY_CURRENT = 0x04,
    BATTERY_POWER = 0x05,
    BATTERY_SOC = 0x06,         // State of Charge
    BATTERY_TEMPERATURE = 0x07,
    EXTERNAL_TEMPERATURE = 0x08,
    CHARGER_VOLTAGE = 0x09,
    CHARGER_CURRENT = 0x0A,
    DEVICE_STATE = 0x0B,
    CHARGER_ERROR = 0x0C,
    CONSUMED_AH = 0x0D,         // Amp hours consumed
    TIME_TO_GO = 0x0E,
    ALARM = 0x0F,
    RELAY_STATE = 0x10
};

// Victron Device Data Structure
struct VictronDeviceData {
    String name;
    String address;
    VictronDeviceType type;
    int rssi;
    
    // Common measurements
    float voltage;              // V
    float current;              // A
    float power;                // W
    float batterySOC;           // %
    float temperature;          // °C
    float consumedAh;           // Ah
    int timeToGo;               // minutes
    
    // States
    int deviceState;
    int alarmState;
    
    unsigned long lastUpdate;
    bool dataValid;
    
    // Field availability flags
    bool hasVoltage;
    bool hasCurrent;
    bool hasPower;
    bool hasSOC;
    bool hasTemperature;
    
    VictronDeviceData() : 
        type(DEVICE_UNKNOWN), 
        rssi(0),
        voltage(0), 
        current(0), 
        power(0), 
        batterySOC(-1),  // -1 indicates unavailable
        temperature(-273.15),  // Below absolute zero = unavailable
        consumedAh(0),
        timeToGo(0),
        deviceState(0),
        alarmState(0),
        lastUpdate(0), 
        dataValid(false),
        hasVoltage(false),
        hasCurrent(false),
        hasPower(false),
        hasSOC(false),
        hasTemperature(false) {}
};

class VictronBLE {
private:
    std::map<String, VictronDeviceData> devices;
    std::map<String, String> encryptionKeys;  // MAC address -> encryption key
    NimBLEScan* pBLEScan;
    
    VictronDeviceType identifyDeviceType(const String& name);
    bool parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device, const String& encryptionKey);
    bool decryptData(const uint8_t* encryptedData, size_t length, uint8_t* decryptedData, const String& key);
    float decodeValue(const uint8_t* data, int len, float scale);
    
public:
    VictronBLE();
    void begin();
    void scan(int duration = 5);
    void setEncryptionKey(const String& address, const String& key);
    String getEncryptionKey(const String& address);
    void clearEncryptionKeys();
    std::map<String, VictronDeviceData>& getDevices();
    VictronDeviceData* getDevice(const String& address);
    bool hasDevices();
    int getDeviceCount();
};

#endif // VICTRON_BLE_H
