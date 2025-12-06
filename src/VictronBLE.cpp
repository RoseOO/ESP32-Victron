#include "VictronBLE.h"

// BLE Scan Callback - used for real-time device discovery logging
// Note: Actual device processing happens in the scan() method
class VictronAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
private:
    VictronBLE* victronBLE;
    
public:
    VictronAdvertisedDeviceCallbacks(VictronBLE* vble) : victronBLE(vble) {}
    
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        // Real-time logging of discovered Victron devices during scan
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

VictronBLE::VictronBLE() {
    pBLEScan = nullptr;
}

void VictronBLE::begin() {
    Serial.println("Initializing Victron BLE...");
    NimBLEDevice::init("");
    
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisedDeviceCallbacks(this), false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

void VictronBLE::scan(int duration) {
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
                    
                    // Parse manufacturer data
                    if (mfgData.length() > 2) {
                        parseVictronAdvertisement((const uint8_t*)mfgData.data(), mfgData.length(), devData);
                    }
                    
                    devices[devData.address] = devData;
                    
                    Serial.printf("Device: %s (%s) RSSI: %d\n", 
                        devData.name.c_str(), 
                        devData.address.c_str(), 
                        devData.rssi);
                }
            }
        }
    }
    
    pBLEScan->clearResults();
    Serial.printf("Found %d Victron device(s)\n", devices.size());
}

VictronDeviceType VictronBLE::identifyDeviceType(const String& name) {
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

bool VictronBLE::parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device) {
    // Victron BLE advertisement format:
    // [0-1]: Manufacturer ID (0x02E1)
    // [2]: Record Type
    // [3-4]: Model ID
    // [5]: Encryption key (0x00 for instant readout)
    // [6+]: Data records
    
    if (length < 6) return false;
    
    device.dataValid = true;
    
    // Start parsing from byte 6 (after header)
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
                device.voltage = decodeValue(recordData, recordLen, 0.01); // 10mV resolution
                device.hasVoltage = true;
                break;
                
            case BATTERY_CURRENT:
            case SOLAR_CHARGER_CURRENT:
            case CHARGER_CURRENT:
                device.current = decodeValue(recordData, recordLen, 0.001); // 1mA resolution
                device.hasCurrent = true;
                break;
                
            case BATTERY_POWER:
                device.power = decodeValue(recordData, recordLen, 1.0);
                device.hasPower = true;
                break;
                
            case BATTERY_SOC:
                device.batterySOC = decodeValue(recordData, recordLen, 0.01); // 0.01% resolution
                device.hasSOC = true;
                break;
                
            case BATTERY_TEMPERATURE:
            case EXTERNAL_TEMPERATURE:
                device.temperature = decodeValue(recordData, recordLen, 0.01) - 273.15; // Kelvin to Celsius
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

float VictronBLE::decodeValue(const uint8_t* data, int len, float scale) {
    if (len <= 0 || len > 4) return 0.0;
    
    int32_t value = 0;
    
    // Little-endian signed integer
    for (int i = 0; i < len; i++) {
        value |= ((int32_t)data[i]) << (8 * i);
    }
    
    // Sign extend if necessary
    if (len < 4 && (data[len - 1] & 0x80)) {
        for (int i = len; i < 4; i++) {
            value |= 0xFF << (8 * i);
        }
    }
    
    return value * scale;
}

std::map<String, VictronDeviceData>& VictronBLE::getDevices() {
    return devices;
}

VictronDeviceData* VictronBLE::getDevice(const String& address) {
    if (devices.find(address) != devices.end()) {
        return &devices[address];
    }
    return nullptr;
}

bool VictronBLE::hasDevices() {
    return !devices.empty();
}

int VictronBLE::getDeviceCount() {
    return devices.size();
}
