#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <map>
#include <vector>

// Victron BLE Service UUID
#define VICTRON_MANUFACTURER_ID 0x02E1

// Fixed payload sizes for different device types
#define SMART_SHUNT_PAYLOAD_SIZE 15
#define SOLAR_CONTROLLER_PAYLOAD_SIZE 16
#define DCDC_CONVERTER_PAYLOAD_SIZE 16

// Device Types
enum VictronDeviceType {
    DEVICE_UNKNOWN = 0,
    DEVICE_SMART_SHUNT = 1,
    DEVICE_SMART_SOLAR = 2,
    DEVICE_BLUE_SMART_CHARGER = 3,
    DEVICE_INVERTER = 4,
    DEVICE_DCDC_CONVERTER = 5
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
    RELAY_STATE = 0x10,
    // Inverter specific records
    AC_OUT_VOLTAGE = 0x11,      // AC output voltage
    AC_OUT_CURRENT = 0x12,      // AC output current
    AC_OUT_POWER = 0x13,        // AC output power
    // DC-DC Converter specific records
    INPUT_VOLTAGE = 0x14,       // DC-DC input voltage
    OUTPUT_VOLTAGE = 0x15,      // DC-DC output voltage
    OFF_REASON = 0x16           // Device off reason
};

// Structure for parsed records
struct VictronRecord {
    uint8_t type;
    uint8_t length;
    uint8_t data[32];  // Maximum record data size
    
    VictronRecord() : type(0), length(0) {
        memset(data, 0, sizeof(data));
    }
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
    
    // SmartShunt specific  
    float auxVoltage;           // V - auxiliary input
    float midVoltage;           // V - midpoint voltage
    int auxMode;                // 0=aux voltage, 1=midpoint, 2=temperature, 3=none
    
    // Solar Controller specific
    float yieldToday;           // kWh - today's yield
    float pvPower;              // W - PV panel power
    float loadCurrent;          // A - load current
    int chargerError;           // charger error code
    
    // Inverter specific
    float acOutVoltage;         // V
    float acOutCurrent;         // A
    float acOutPower;           // W
    
    // DC-DC Converter specific
    float inputVoltage;         // V
    float outputVoltage;        // V
    
    // States
    int deviceState;
    int alarmState;
    int offReason;
    
    unsigned long lastUpdate;
    bool dataValid;
    
    // Field availability flags
    bool hasVoltage;
    bool hasCurrent;
    bool hasPower;
    bool hasSOC;
    bool hasTemperature;
    bool hasAcOut;
    bool hasInputVoltage;
    bool hasOutputVoltage;
    
    // Raw debug data (BLE manufacturer data is typically 20-30 bytes for Victron devices)
    uint8_t rawManufacturerData[64];  // Sufficient for typical BLE advertisement payloads
    size_t rawDataLength;
    uint16_t manufacturerId;
    uint16_t modelId;
    bool encrypted;
    String errorMessage;  // Error message when parsing fails
    std::vector<VictronRecord> parsedRecords;
    
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
        auxVoltage(0),
        midVoltage(0),
        auxMode(3),  // 3 = none
        yieldToday(0),
        pvPower(0),
        loadCurrent(0),
        chargerError(0),
        acOutVoltage(0),
        acOutCurrent(0),
        acOutPower(0),
        inputVoltage(0),
        outputVoltage(0),
        deviceState(0),
        alarmState(0),
        offReason(0),
        lastUpdate(0), 
        dataValid(false),
        hasVoltage(false),
        hasCurrent(false),
        hasPower(false),
        hasSOC(false),
        hasTemperature(false),
        hasAcOut(false),
        hasInputVoltage(false),
        hasOutputVoltage(false),
        rawDataLength(0),
        manufacturerId(0),
        modelId(0),
        encrypted(false) {
        memset(rawManufacturerData, 0, sizeof(rawManufacturerData));
    }
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
    
    // Device-specific parsing functions for fixed structures
    // SmartShunt: 15 bytes, Solar Controller: 16 bytes, DC-DC Converter: 16 bytes
    void parseSmartShuntData(const uint8_t* output, size_t length, VictronDeviceData& device);
    void parseSolarControllerData(const uint8_t* output, size_t length, VictronDeviceData& device);
    void parseDCDCConverterData(const uint8_t* output, size_t length, VictronDeviceData& device);
    void parseTLVRecords(const uint8_t* data, size_t length, size_t startPos, VictronDeviceData& device);
    
    // Helper functions for multi-byte value extraction
    int16_t extractSigned16(const uint8_t* data, int byteIndex);
    uint16_t extractUnsigned16(const uint8_t* data, int byteIndex);
    int32_t extractSigned22(const uint8_t* data, int startByte);
    uint32_t extractUnsigned20(const uint8_t* data, int startByte);
    uint16_t extractUnsigned10(const uint8_t* data, int startByte);
    
    // Helper function to normalize MAC addresses for comparison
    // Removes colons and converts to lowercase
    String normalizeAddress(const String& address);
    
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
