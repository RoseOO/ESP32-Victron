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

// Voltage sanity check thresholds (in volts)
// Packets with voltage readings outside this range will be discarded
// as they indicate clearly incorrect data
#define MAX_VALID_VOLTAGE 30.0f
#define MIN_VALID_VOLTAGE -30.0f

// Temperature sanity check threshold (in Celsius)
// Packets with temperature readings exceeding this value will be discarded
// as they indicate clearly incorrect data
#define MAX_VALID_TEMPERATURE 50.0f

// Device Types
enum VictronDeviceType {
    DEVICE_UNKNOWN = 0,
    DEVICE_SMART_SHUNT = 1,
    DEVICE_SMART_SOLAR = 2,
    DEVICE_BLUE_SMART_CHARGER = 3,
    DEVICE_INVERTER = 4,
    DEVICE_DCDC_CONVERTER = 5,
    DEVICE_SMART_LITHIUM = 6,       // SmartLithium Battery
    DEVICE_INVERTER_RS = 7,          // Inverter RS
    DEVICE_AC_CHARGER = 8,           // AC Charger (Blue Smart IP65/IP22)
    DEVICE_SMART_BATTERY_PROTECT = 9, // Smart Battery Protect
    DEVICE_LYNX_SMART_BMS = 10,      // Lynx Smart BMS
    DEVICE_MULTI_RS = 11,            // Multi RS
    DEVICE_VE_BUS = 12,              // VE.Bus (MultiPlus, Quattro)
    DEVICE_DC_ENERGY_METER = 13,     // DC Energy Meter
    DEVICE_ORION_XS = 14,            // Orion XS DC-DC Charger
    DEVICE_SMART_BATTERY_SENSE = 15  // Smart Battery Sense
};

// Victron Product IDs (Model IDs) - comprehensive list from reference implementation
// These can be used for more precise device identification
enum VictronProductID {
    // BMV Series
    BMV_700 = 0x0203,
    BMV_702 = 0x0204,
    BMV_700H = 0x0205,
    BMV_712_SMART = 0xA381,
    BMV_710H_SMART = 0xA382,
    BMV_712_SMART_REV2 = 0xA383,
    
    // SmartShunt Series
    SMARTSHUNT_500A_50MV = 0xA389,
    SMARTSHUNT_1000A_50MV = 0xA38A,
    SMARTSHUNT_2000A_50MV = 0xA38B,
    
    // SmartSolar MPPT Series (selected common models)
    SMARTSOLAR_MPPT_75_10 = 0xA054,
    SMARTSOLAR_MPPT_75_15 = 0xA053,
    SMARTSOLAR_MPPT_100_15 = 0xA055,
    SMARTSOLAR_MPPT_100_20 = 0xA05F,
    SMARTSOLAR_MPPT_100_30 = 0xA056,
    SMARTSOLAR_MPPT_100_50 = 0xA057,
    SMARTSOLAR_MPPT_150_35 = 0xA058,
    SMARTSOLAR_MPPT_150_45 = 0xA061,
    SMARTSOLAR_MPPT_150_60 = 0xA062,
    SMARTSOLAR_MPPT_150_70 = 0xA063,
    SMARTSOLAR_MPPT_150_85 = 0xA05A,
    SMARTSOLAR_MPPT_150_100 = 0xA059,
    SMARTSOLAR_MPPT_250_60 = 0xA05D,
    SMARTSOLAR_MPPT_250_70 = 0xA05B,
    SMARTSOLAR_MPPT_250_85 = 0xA05C,
    SMARTSOLAR_MPPT_250_100 = 0xA050,
    
    // Phoenix Inverter Series (selected models)
    PHOENIX_INVERTER_12V_250VA = 0xA231,
    PHOENIX_INVERTER_24V_250VA = 0xA232,
    PHOENIX_INVERTER_48V_250VA = 0xA234,
    PHOENIX_INVERTER_12V_500VA = 0xA251,
    PHOENIX_INVERTER_24V_500VA = 0xA252,
    PHOENIX_INVERTER_48V_500VA = 0xA254,
    PHOENIX_INVERTER_12V_800VA = 0xA261,
    PHOENIX_INVERTER_24V_800VA = 0xA262,
    PHOENIX_INVERTER_48V_800VA = 0xA264,
    PHOENIX_INVERTER_12V_1200VA = 0xA271,
    PHOENIX_INVERTER_24V_1200VA = 0xA272,
    PHOENIX_INVERTER_48V_1200VA = 0xA274,
    
    // Smart BuckBoost / Orion
    SMART_BUCKBOOST_12V_12V_50A = 0xA3F0,
    
    // Phoenix Smart IP43 Chargers
    PHOENIX_SMART_IP43_CHARGER_12_50 = 0xA340,
    PHOENIX_SMART_IP43_CHARGER_24_50 = 0xA342,
    PHOENIX_SMART_IP43_CHARGER_12_30 = 0xA344,
    PHOENIX_SMART_IP43_CHARGER_24_16 = 0xA346
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

// Device State Enumeration (for MPPT, Inverter, Charger)
// Source: VE.Direct Protocol documentation
enum VictronDeviceState {
    STATE_OFF = 0x00,                    // Off / Not charging
    STATE_LOW_POWER = 0x01,              // Low power
    STATE_FAULT = 0x02,                  // Fault
    STATE_BULK = 0x03,                   // Bulk charging
    STATE_ABSORPTION = 0x04,             // Absorption
    STATE_FLOAT = 0x05,                  // Float
    STATE_STORAGE = 0x06,                // Storage
    STATE_EQUALIZE_MANUAL = 0x07,        // Equalize (manual)
    STATE_PASSTHRU = 0x08,               // Pass Through
    STATE_INVERTING = 0x09,              // Inverting
    STATE_ASSISTING = 0x0A,              // Assisting
    STATE_POWER_SUPPLY = 0x0B,           // Power supply
    STATE_SUSTAIN = 0xF4,                // Sustain
    STATE_STARTING_UP = 0xF5,            // Starting-up / Wake-up
    STATE_REPEATED_ABSORPTION = 0xF6,    // Repeated absorption
    STATE_AUTO_EQUALIZE = 0xF7,          // Auto equalize / Recondition
    STATE_BATTERY_SAFE = 0xF8,           // BatterySafe
    STATE_LOAD_DETECT = 0xF9,            // Load detect
    STATE_BLOCKED = 0xFA,                // Blocked
    STATE_TEST = 0xFB,                   // Test
    STATE_EXTERNAL_CONTROL = 0xFC,       // External Control
    STATE_NOT_AVAILABLE = 0xFF           // Not available
};

// Charger Error Codes
// Source: VE.Direct Protocol and Victron MPPT error codes documentation
enum VictronChargerError {
    ERR_NO_ERROR = 0,
    ERR_TEMPERATURE_BATTERY_HIGH = 1,
    ERR_VOLTAGE_HIGH = 2,
    ERR_REMOTE_TEMPERATURE_A = 3,
    ERR_REMOTE_BATTERY_A = 6,
    ERR_HIGH_RIPPLE = 11,
    ERR_TEMPERATURE_BATTERY_LOW = 14,
    ERR_TEMPERATURE_CHARGER = 17,
    ERR_OVER_CURRENT = 18,
    ERR_POLARITY = 19,
    ERR_BULK_TIME = 20,
    ERR_CURRENT_SENSOR = 21,
    ERR_INTERNAL_TEMPERATURE = 22,
    ERR_FAN = 24,
    ERR_OVERHEATED = 26,
    ERR_SHORT_CIRCUIT = 27,
    ERR_CONVERTER_ISSUE = 28,
    ERR_OVER_CHARGE = 29,
    ERR_INPUT_VOLTAGE = 33,
    ERR_INPUT_CURRENT = 34,
    ERR_INPUT_POWER = 35,
    ERR_INPUT_SHUTDOWN_VOLTAGE = 38,
    ERR_INVERTER_SHUTDOWN = 41,
    ERR_INVERTER_OVERLOAD = 50,
    ERR_INVERTER_TEMPERATURE = 51,
    ERR_BMS = 67,
    ERR_NETWORK = 68,
    ERR_CPU_TEMPERATURE = 114,
    ERR_CALIBRATION_LOST = 116,
    ERR_FIRMWARE = 117,
    ERR_SETTINGS = 119,
    ERR_NOT_AVAILABLE = 0xFF
};

// Alarm Reason Flags (for BMV, Inverter)
enum VictronAlarmReason {
    ALARM_NONE = 0x00,
    ALARM_LOW_VOLTAGE = 0x01,
    ALARM_HIGH_VOLTAGE = 0x02,
    ALARM_LOW_SOC = 0x04,
    ALARM_LOW_STARTER_VOLTAGE = 0x08,
    ALARM_HIGH_STARTER_VOLTAGE = 0x10,
    ALARM_LOW_TEMPERATURE = 0x20,
    ALARM_HIGH_TEMPERATURE = 0x40,
    ALARM_MID_VOLTAGE = 0x80,
    ALARM_OVERLOAD = 0x100,
    ALARM_DC_RIPPLE = 0x200,
    ALARM_LOW_V_AC_OUT = 0x400,
    ALARM_HIGH_V_AC_OUT = 0x800,
    ALARM_SHORT_CIRCUIT = 0x1000,
    ALARM_BMS_LOCKOUT = 0x2000
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
    
    // SmartLithium specific
    float cellVoltage[8];       // V - cell voltages (cells 1-8)
    int balancerStatus;         // 0=unknown, 1=balanced, 2=balancing, 3=imbalanced
    uint32_t bmsFlags;          // BMS status flags
    
    // Multi RS / VE.Bus specific
    float acInPower;            // W - AC input power
    int activeAcIn;             // 0=AC in 1, 1=AC in 2, 2=not connected
    
    // AC Charger specific (multi-output chargers)
    float batteryVoltage2;      // V - second battery output
    float batteryCurrent2;      // A - second battery output
    float batteryVoltage3;      // V - third battery output
    float batteryCurrent3;      // A - third battery output
    
    // States
    int deviceState;
    int alarmState;
    uint32_t offReason;         // 32-bit off reason code for DC-DC converters
    
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
        balancerStatus(0),
        bmsFlags(0),
        acInPower(0),
        activeAcIn(2),  // 2 = not connected
        batteryVoltage2(0),
        batteryCurrent2(0),
        batteryVoltage3(0),
        batteryCurrent3(0),
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
        memset(cellVoltage, 0, sizeof(cellVoltage));
    }
};

class VictronBLE {
private:
    std::map<String, VictronDeviceData> devices;
    std::map<String, String> encryptionKeys;  // MAC address -> encryption key
    NimBLEScan* pBLEScan;
    bool retainLastData;  // Flag to retain last good data when parsing fails
    
    VictronDeviceType identifyDeviceType(const String& name);
    bool parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device, const String& encryptionKey);
    bool decryptData(const uint8_t* encryptedData, size_t length, uint8_t* decryptedData, const String& key);
    float decodeValue(const uint8_t* data, int len, float scale);
    
    // Device-specific parsing functions for fixed structures
    // SmartShunt: 15 bytes, Solar Controller & Blue Smart Charger: 16 bytes, DC-DC Converter: 16 bytes
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
    
    // Helper function to validate voltage readings
    // Returns true if voltage is valid, false otherwise
    // If invalid, sets device.dataValid to false and logs an error
    bool validateVoltage(float voltage, const char* source, VictronDeviceData& device);
    
    // Helper function to validate temperature readings
    // Returns true if temperature is valid, false otherwise
    // If invalid, sets device.dataValid to false and logs an error
    bool validateTemperature(float temperature, const char* source, VictronDeviceData& device);
    
    // Helper function to normalize MAC addresses for comparison
    // Removes colons and converts to lowercase
    String normalizeAddress(const String& address);
    
    // Helper function to merge new device data with existing data
    void mergeDeviceData(const VictronDeviceData& newData, VictronDeviceData& existingData);
    
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
    void setRetainLastData(bool retain);
    bool getRetainLastData() const;
    
    // Helper functions to convert codes to human-readable strings
    static String deviceStateToString(int state);
    static String chargerErrorToString(int error);
    static String offReasonToString(uint32_t offReason);
    static String alarmReasonToString(uint16_t alarm);
};

#endif // VICTRON_BLE_H
