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
        NimBLEAdvertisedDevice device = foundDevices.getDevice(i);
        
        if (device.haveManufacturerData()) {
            std::string mfgData = device.getManufacturerData();
            
            if (mfgData.length() >= 2) {
                uint16_t mfgId = (uint8_t)mfgData[1] << 8 | (uint8_t)mfgData[0];
                
                if (mfgId == VICTRON_MANUFACTURER_ID) {
                    VictronDeviceData devData;
                    devData.name = device.getName().c_str();
                    devData.address = device.getAddress().toString().c_str();
                    devData.rssi = device.getRSSI();
                    devData.type = identifyDeviceType(devData.name);
                    devData.lastUpdate = millis();
                    
                    // Store raw manufacturer data for debug purposes
                    devData.manufacturerId = mfgId;
                    devData.rawDataLength = mfgData.length() > sizeof(devData.rawManufacturerData) 
                                           ? sizeof(devData.rawManufacturerData) 
                                           : mfgData.length();
                    memcpy(devData.rawManufacturerData, mfgData.data(), devData.rawDataLength);
                    
                    // Extract model ID if available (bytes 3-4, little-endian)
                    if (mfgData.length() >= 5) {
                        devData.modelId = (uint8_t)mfgData[3] | ((uint8_t)mfgData[4] << 8);
                    }
                    
                    // Check if encrypted
                    if (mfgData.length() >= 6) {
                        devData.encrypted = (mfgData[5] != 0x00);
                    }
                    
                    // Parse manufacturer data with encryption key if available
                    if (mfgData.length() > 2) {
                        String encKey = getEncryptionKey(devData.address);
                        parseVictronAdvertisement((const uint8_t*)mfgData.data(), mfgData.length(), devData, encKey);
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
    } else if (lowerName.indexOf("inverter") >= 0 || lowerName.indexOf("phoenix") >= 0 || 
               lowerName.indexOf("multiplus") >= 0 || lowerName.indexOf("quattro") >= 0) {
        return DEVICE_INVERTER;
    } else if (lowerName.indexOf("orion") >= 0 || lowerName.indexOf("dc-dc") >= 0 || 
               lowerName.indexOf("dcdc") >= 0 || lowerName.indexOf("converter") >= 0) {
        return DEVICE_DCDC_CONVERTER;
    }
    
    return DEVICE_UNKNOWN;
}

bool VictronBLE::parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device, const String& encryptionKey) {
    // Victron BLE advertisement format:
    // [0-1]: Manufacturer ID (0x02E1)
    // [2]: Record Type
    // [3-4]: Model ID
    // [5]: Encryption key (0x00 for instant readout)
    // [6+]: Data records
    
    if (length < 6) return false;
    
    // Clear previous parsed records
    device.parsedRecords.clear();
    
    // Check if data is encrypted (byte 5 != 0x00)
    bool isEncrypted = (data[5] != 0x00);
    
    const uint8_t* dataToProcess = data;
    uint8_t* decryptedBuffer = nullptr;
    
    if (isEncrypted) {
        if (encryptionKey.isEmpty()) {
            device.errorMessage = "Device is encrypted. Add encryption key in web configuration, or enable 'Instant Readout' in VictronConnect app.";
            Serial.printf("Device %s is encrypted but no key provided\n", device.address.c_str());
            return false;
        }
        
        // Allocate buffer for decrypted data
        decryptedBuffer = new uint8_t[length];
        if (!decryptData(data, length, decryptedBuffer, encryptionKey)) {
            device.errorMessage = "Decryption failed. Encryption is not fully implemented. Please enable 'Instant Readout' mode.";
            delete[] decryptedBuffer;
            Serial.printf("Failed to decrypt data for %s\n", device.address.c_str());
            return false;
        }
        
        dataToProcess = decryptedBuffer;
        Serial.printf("Successfully decrypted data for %s\n", device.address.c_str());
    }
    
    device.dataValid = true;
    device.errorMessage = "";  // Clear any previous error message
    
    // Start parsing from byte 6 (after header)
    size_t pos = 6;
    
    while (pos < length) {
        if (pos + 1 >= length) break;
        
        uint8_t recordType = dataToProcess[pos];
        uint8_t recordLen = dataToProcess[pos + 1];
        
        if (pos + 2 + recordLen > length) break;
        
        const uint8_t* recordData = &dataToProcess[pos + 2];
        
        // Store parsed record for debug purposes
        VictronRecord rec;
        rec.type = recordType;
        rec.length = recordLen;
        if (recordLen <= sizeof(rec.data)) {
            memcpy(rec.data, recordData, recordLen);
            device.parsedRecords.push_back(rec);
        } else {
            // Log when records are too large to store
            Serial.printf("Warning: Record type 0x%02X length %d exceeds buffer size, skipping\n", 
                         recordType, recordLen);
        }
        
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
                
            // Inverter specific records
            case AC_OUT_VOLTAGE:
                device.acOutVoltage = decodeValue(recordData, recordLen, 0.01); // 10mV resolution
                device.hasAcOut = true;
                break;
                
            case AC_OUT_CURRENT:
                device.acOutCurrent = decodeValue(recordData, recordLen, 0.1); // 100mA resolution
                device.hasAcOut = true;
                break;
                
            case AC_OUT_POWER:
                device.acOutPower = decodeValue(recordData, recordLen, 1.0); // 1W resolution
                device.hasAcOut = true;
                break;
                
            // DC-DC Converter specific records
            case INPUT_VOLTAGE:
                device.inputVoltage = decodeValue(recordData, recordLen, 0.01); // 10mV resolution
                device.hasInputVoltage = true;
                break;
                
            case OUTPUT_VOLTAGE:
                device.outputVoltage = decodeValue(recordData, recordLen, 0.01); // 10mV resolution
                device.hasOutputVoltage = true;
                break;
                
            case OFF_REASON:
                device.offReason = (int)decodeValue(recordData, recordLen, 1.0);
                break;
        }
        
        pos += 2 + recordLen;
    }
    
    if (decryptedBuffer) {
        delete[] decryptedBuffer;
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

void VictronBLE::setEncryptionKey(const String& address, const String& key) {
    encryptionKeys[address] = key;
    Serial.printf("Set encryption key for device %s\n", address.c_str());
}

String VictronBLE::getEncryptionKey(const String& address) {
    if (encryptionKeys.find(address) != encryptionKeys.end()) {
        return encryptionKeys[address];
    }
    return "";
}

void VictronBLE::clearEncryptionKeys() {
    encryptionKeys.clear();
}

bool VictronBLE::decryptData(const uint8_t* encryptedData, size_t length, uint8_t* decryptedData, const String& key) {
    // NOTE: Victron uses AES-128-CTR encryption for BLE data
    // This is a placeholder implementation that copies the data as-is
    // A full implementation would require:
    // 1. Convert hex key string to 16-byte array
    // 2. Extract IV/nonce from the encrypted payload
    // 3. Use mbedtls or similar library for AES-128-CTR decryption
    //
    // Return value:
    // - true: Decryption successful, decryptedData contains valid data
    // - false: Decryption failed, decryptedData is in undefined state
    
    if (key.isEmpty() || length < 6) {
        return false;
    }
    
    // TODO: Implement proper AES-128-CTR decryption
    // For now, this is a placeholder that indicates encryption support is present
    // but not fully implemented. Users with encrypted devices will need instant readout mode.
    
    Serial.println("WARNING: Encryption decryption not fully implemented yet.");
    Serial.println("Please enable 'Instant Readout' mode in VictronConnect app.");
    
    // Copy header (first 6 bytes) as-is
    memcpy(decryptedData, encryptedData, 6);
    
    // For encrypted data, we would decrypt the payload here
    // Currently returning false to indicate decryption failed
    // decryptedData buffer is left in partially initialized state (header only)
    return false;
}
