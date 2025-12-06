/*
 * ESP32-Victron BLE Monitor for M5StickC PLUS
 * 
 * This sketch monitors Victron Energy devices (Smart Shunt, Smart Solar, Blue Smart Charger)
 * via Bluetooth Low Energy and displays the data on the M5StickC PLUS screen.
 * 
 * Hardware Required:
 * - M5StickC PLUS
 * - Victron Energy device with BLE (in Instant Readout mode)
 * 
 * Libraries Required:
 * - M5StickCPlus (https://github.com/m5stack/M5StickC-Plus)
 * - NimBLE-Arduino (https://github.com/h2zero/NimBLE-Arduino)
 * 
 * Author: ESP32-Victron Project
 * License: MIT
 */

#include <M5StickCPlus.h>
#include <NimBLEDevice.h>
#include <map>
#include <vector>

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
    BATTERY_SOC = 0x06,
    BATTERY_TEMPERATURE = 0x07,
    EXTERNAL_TEMPERATURE = 0x08,
    CHARGER_VOLTAGE = 0x09,
    CHARGER_CURRENT = 0x0A,
    DEVICE_STATE = 0x0B,
    CHARGER_ERROR = 0x0C,
    CONSUMED_AH = 0x0D,
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
    float voltage;
    float current;
    float power;
    float batterySOC;
    float temperature;
    float consumedAh;
    int timeToGo;
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
        type(DEVICE_UNKNOWN), rssi(0), voltage(0), current(0), power(0), 
        batterySOC(-1), temperature(-273.15), consumedAh(0), timeToGo(0),
        deviceState(0), alarmState(0), lastUpdate(0), dataValid(false),
        hasVoltage(false), hasCurrent(false), hasPower(false), hasSOC(false), hasTemperature(false) {}
};

// Global variables
std::map<String, VictronDeviceData> devices;
std::vector<String> deviceAddresses;
NimBLEScan* pBLEScan;
int currentDeviceIndex = 0;
unsigned long lastScanTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long SCAN_INTERVAL = 30000;
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;
bool scanning = false;

// Function declarations
VictronDeviceType identifyDeviceType(const String& name);
bool parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device);
float decodeValue(const uint8_t* data, int len, float scale);
void updateDeviceList();
void drawDisplay();
void scanForDevices(int duration);

// BLE Scan Callback
class VictronAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->haveManufacturerData()) {
            std::string mfgData = advertisedDevice->getManufacturerData();
            if (mfgData.length() >= 2) {
                uint16_t mfgId = (uint8_t)mfgData[1] << 8 | (uint8_t)mfgData[0];
                if (mfgId == VICTRON_MANUFACTURER_ID) {
                    Serial.println("Found Victron device: " + String(advertisedDevice->getName().c_str()));
                }
            }
        }
    }
};

void setup() {
    // Initialize M5StickC PLUS
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("M5StickC PLUS - Victron BLE Monitor");
    
    // Display splash screen
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Victron BLE");
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Initializing...");
    
    // Initialize BLE
    Serial.println("Initializing Victron BLE...");
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    delay(1000);
    
    // Initial scan
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.println("Scanning for");
    M5.Lcd.setCursor(10, 35);
    M5.Lcd.println("Victron devices...");
    
    scanForDevices(5);
    updateDeviceList();
    lastScanTime = millis();
    
    if (deviceAddresses.empty()) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(10, 20);
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("No devices found!");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(10, 40);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Please ensure:");
        M5.Lcd.setCursor(10, 55);
        M5.Lcd.println("- Device is on");
        M5.Lcd.setCursor(10, 68);
        M5.Lcd.println("- BLE is enabled");
        M5.Lcd.setCursor(10, 81);
        M5.Lcd.println("- In range");
    } else {
        Serial.printf("Found %d device(s)\n", deviceAddresses.size());
    }
}

void loop() {
    M5.update();
    
    // Button A: Switch to next device
    if (M5.BtnA.wasPressed()) {
        if (!deviceAddresses.empty()) {
            currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
            drawDisplay();
        }
    }
    
    // Periodic BLE scan
    unsigned long currentTime = millis();
    if (currentTime - lastScanTime > SCAN_INTERVAL && !scanning) {
        scanning = true;
        Serial.println("Periodic scan...");
        scanForDevices(2);
        updateDeviceList();
        lastScanTime = currentTime;
        scanning = false;
        
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
    }
    
    // Update display periodically
    if (currentTime - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
        lastDisplayUpdate = currentTime;
    }
    
    delay(10);
}

void scanForDevices(int duration) {
    Serial.println("Scanning for Victron devices...");
    NimBLEScanResults foundDevices = pBLEScan->start(duration, false);
    
    for (int i = 0; i < foundDevices.getCount(); i++) {
        NimBLEAdvertisedDevice* device = foundDevices.getDevice(i);
        
        if (device->haveManufacturerData()) {
            std::string mfgData = device->getManufacturerData();
            
            if (mfgData.length() >= 2) {
                uint16_t mfgId = (uint8_t)mfgData[1] << 8 | (uint8_t)mfgData[0];
                
                if (mfgId == VICTRON_MANUFACTURER_ID) {
                    VictronDeviceData devData;
                    devData.name = device->getName().c_str();
                    devData.address = device->getAddress().toString().c_str();
                    devData.rssi = device->getRSSI();
                    devData.type = identifyDeviceType(devData.name);
                    devData.lastUpdate = millis();
                    
                    if (mfgData.length() > 2) {
                        parseVictronAdvertisement((const uint8_t*)mfgData.data(), mfgData.length(), devData);
                    }
                    
                    devices[devData.address] = devData;
                    Serial.printf("Device: %s (%s) RSSI: %d\n", 
                        devData.name.c_str(), devData.address.c_str(), devData.rssi);
                }
            }
        }
    }
    
    pBLEScan->clearResults();
    Serial.printf("Found %d Victron device(s)\n", devices.size());
}

VictronDeviceType identifyDeviceType(const String& name) {
    String lowerName = name;
    lowerName.toLowerCase();
    
    if (lowerName.indexOf("shunt") >= 0 || lowerName.indexOf("smartshunt") >= 0) {
        return DEVICE_SMART_SHUNT;
    } else if (lowerName.indexOf("solar") >= 0 || lowerName.indexOf("mppt") >= 0) {
        return DEVICE_SMART_SOLAR;
    } else if (lowerName.indexOf("charger") >= 0 || lowerName.indexOf("blue") >= 0) {
        return DEVICE_BLUE_SMART_CHARGER;
    }
    
    return DEVICE_UNKNOWN;
}

bool parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device) {
    if (length < 6) return false;
    
    device.dataValid = true;
    size_t pos = 6;
    
    while (pos < length) {
        if (pos + 1 >= length) break;
        
        uint8_t recordType = data[pos];
        uint8_t recordLen = data[pos + 1];
        
        if (pos + 2 + recordLen > length) break;
        
        const uint8_t* recordData = &data[pos + 2];
        
        switch (recordType) {
            case BATTERY_VOLTAGE:
            case SOLAR_CHARGER_VOLTAGE:
            case CHARGER_VOLTAGE:
                device.voltage = decodeValue(recordData, recordLen, 0.01);
                device.hasVoltage = true;
                break;
            case BATTERY_CURRENT:
            case SOLAR_CHARGER_CURRENT:
            case CHARGER_CURRENT:
                device.current = decodeValue(recordData, recordLen, 0.001);
                device.hasCurrent = true;
                break;
            case BATTERY_POWER:
                device.power = decodeValue(recordData, recordLen, 1.0);
                device.hasPower = true;
                break;
            case BATTERY_SOC:
                device.batterySOC = decodeValue(recordData, recordLen, 0.01);
                device.hasSOC = true;
                break;
            case BATTERY_TEMPERATURE:
            case EXTERNAL_TEMPERATURE:
                device.temperature = decodeValue(recordData, recordLen, 0.01) - 273.15;
                device.hasTemperature = true;
                break;
            case CONSUMED_AH:
                device.consumedAh = decodeValue(recordData, recordLen, 0.1);
                break;
            case TIME_TO_GO:
                device.timeToGo = (int)decodeValue(recordData, recordLen, 1.0);
                break;
            case DEVICE_STATE:
                device.deviceState = (int)decodeValue(recordData, recordLen, 1.0);
                break;
            case ALARM:
                device.alarmState = (int)decodeValue(recordData, recordLen, 1.0);
                break;
        }
        
        pos += 2 + recordLen;
    }
    
    return true;
}

float decodeValue(const uint8_t* data, int len, float scale) {
    if (len <= 0 || len > 4) return 0.0;
    
    int32_t value = 0;
    for (int i = 0; i < len; i++) {
        value |= ((int32_t)data[i]) << (8 * i);
    }
    
    if (len < 4 && (data[len - 1] & 0x80)) {
        for (int i = len; i < 4; i++) {
            value |= 0xFF << (8 * i);
        }
    }
    
    return value * scale;
}

void updateDeviceList() {
    deviceAddresses.clear();
    for (auto& pair : devices) {
        deviceAddresses.push_back(pair.first);
    }
    
    if (currentDeviceIndex >= deviceAddresses.size()) {
        currentDeviceIndex = 0;
    }
}

void drawDisplay() {
    if (deviceAddresses.empty()) return;
    
    auto it = devices.find(deviceAddresses[currentDeviceIndex]);
    if (it == devices.end()) return;
    
    VictronDeviceData& device = it->second;
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setCursor(0, 0);
    
    String deviceName = device.name;
    if (deviceName.length() > 26) {
        deviceName = deviceName.substring(0, 23) + "...";
    }
    M5.Lcd.println(deviceName);
    
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(0, 12);
    switch (device.type) {
        case DEVICE_SMART_SHUNT:
            M5.Lcd.print("Smart Shunt");
            break;
        case DEVICE_SMART_SOLAR:
            M5.Lcd.print("Smart Solar");
            break;
        case DEVICE_BLUE_SMART_CHARGER:
            M5.Lcd.print("Blue Smart Charger");
            break;
        default:
            M5.Lcd.print("Victron Device");
            break;
    }
    
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(220, 12);
    M5.Lcd.printf("%d/%d", currentDeviceIndex + 1, deviceAddresses.size());
    M5.Lcd.drawLine(0, 24, 240, 24, DARKGREY);
    
    int y = 30;
    
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Voltage:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80, y);
    if (device.dataValid) {
        M5.Lcd.printf("%.2f V", device.voltage);
    } else {
        M5.Lcd.print("-- V");
    }
    y += 15;
    
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Current:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80, y);
    if (device.dataValid) {
        M5.Lcd.printf("%.2f A", device.current);
    } else {
        M5.Lcd.print("-- A");
    }
    y += 15;
    
    if (device.hasPower) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Power:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f W", device.power);
        y += 15;
    }
    
    if (device.hasSOC) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Battery:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        uint16_t color = WHITE;
        if (device.batterySOC <= 20) {
            color = RED;
        } else if (device.batterySOC <= 50) {
            color = YELLOW;
        } else {
            color = GREEN;
        }
        M5.Lcd.setTextColor(color, BLACK);
        M5.Lcd.printf("%.1f %%", device.batterySOC);
        M5.Lcd.setTextColor(WHITE, BLACK);
        y += 15;
    }
    
    if (device.hasTemperature) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Temp:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f C", device.temperature);
        y += 15;
    }
    
    M5.Lcd.drawLine(0, 110, 240, 110, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(DARKGREY, BLACK);
    M5.Lcd.setCursor(5, 118);
    M5.Lcd.print("M5: Next Device");
    
    M5.Lcd.setCursor(180, 118);
    if (device.rssi > -60) {
        M5.Lcd.setTextColor(GREEN, BLACK);
    } else if (device.rssi > -80) {
        M5.Lcd.setTextColor(YELLOW, BLACK);
    } else {
        M5.Lcd.setTextColor(RED, BLACK);
    }
    M5.Lcd.printf("RSSI:%d", device.rssi);
}
