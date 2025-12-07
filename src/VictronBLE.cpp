#include "VictronBLE.h"
#include <cctype>
#include <aes/esp_aes.h>

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
            device.errorMessage = "Decryption failed. Please verify the encryption key is correct.";
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

String VictronBLE::normalizeAddress(const String& address) {
    // Normalize MAC address for consistent comparison
    // Removes colons and converts to lowercase
    // Example: "E5:78:04:B9:4D:55" -> "e57804b94d55"
    String normalized = "";
    normalized.reserve(12);  // Pre-allocate for MAC address without colons (12 hex chars)
    for (size_t i = 0; i < address.length(); i++) {
        char c = address[i];
        if (c != ':') {
            normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return normalized;
}

void VictronBLE::setEncryptionKey(const String& address, const String& key) {
    // Note: Encryption keys are stored using normalized addresses (no colons, lowercase)
    // while devices in the devices map use BLE format addresses (with colons).
    // This allows users to enter addresses in any format when configuring encryption keys.
    String normalizedAddr = normalizeAddress(address);
    encryptionKeys[normalizedAddr] = key;
    Serial.printf("Set encryption key for device %s (normalized: %s)\n", address.c_str(), normalizedAddr.c_str());
}

String VictronBLE::getEncryptionKey(const String& address) {
    // Normalize the BLE address before looking up the encryption key
    // This ensures the lookup works regardless of the address format used
    String normalizedAddr = normalizeAddress(address);
    if (encryptionKeys.find(normalizedAddr) != encryptionKeys.end()) {
        return encryptionKeys[normalizedAddr];
    }
    return "";
}

void VictronBLE::clearEncryptionKeys() {
    encryptionKeys.clear();
}

bool VictronBLE::decryptData(const uint8_t* encryptedData, size_t length, uint8_t* decryptedData, const String& key) {
    // Victron uses AES-128-CTR encryption for BLE data
    // Packet structure:
    // [0-1]: Manufacturer ID (0x02E1)
    // [2]: Record Type
    // [3-4]: Model ID
    // [5]: Data Counter LSB (nonce)
    // [6]: Data Counter MSB (nonce)
    // [7]: Encryption Key Match byte
    // [8+]: Encrypted payload
    
    if (key.isEmpty() || length < 8) {
        Serial.println("ERROR: Invalid encryption key or data length");
        return false;
    }
    
    // Validate key format (should be 32 hex characters = 16 bytes)
    if (key.length() != 32) {
        Serial.printf("ERROR: Encryption key must be 32 hex characters, got %d\n", key.length());
        return false;
    }
    
    // Convert hex string key to 16-byte array with validation
    // Using direct character access for efficiency (no String allocations)
    uint8_t keyBytes[16];
    for (int i = 0; i < 16; i++) {
        char highNibble = key.charAt(i * 2);
        char lowNibble = key.charAt(i * 2 + 1);
        
        // Validate and convert high nibble
        uint8_t highVal;
        if (highNibble >= '0' && highNibble <= '9') {
            highVal = highNibble - '0';
        } else if (highNibble >= 'a' && highNibble <= 'f') {
            highVal = 10 + (highNibble - 'a');
        } else if (highNibble >= 'A' && highNibble <= 'F') {
            highVal = 10 + (highNibble - 'A');
        } else {
            Serial.printf("ERROR: Invalid hex character '%c' in encryption key at position %d\n", highNibble, i * 2);
            return false;
        }
        
        // Validate and convert low nibble
        uint8_t lowVal;
        if (lowNibble >= '0' && lowNibble <= '9') {
            lowVal = lowNibble - '0';
        } else if (lowNibble >= 'a' && lowNibble <= 'f') {
            lowVal = 10 + (lowNibble - 'a');
        } else if (lowNibble >= 'A' && lowNibble <= 'F') {
            lowVal = 10 + (lowNibble - 'A');
        } else {
            Serial.printf("ERROR: Invalid hex character '%c' in encryption key at position %d\n", lowNibble, i * 2 + 1);
            return false;
        }
        
        // Combine nibbles into byte
        keyBytes[i] = (highVal << 4) | lowVal;
    }
    
    // Verify the encryption key match byte (byte 7 should match keyBytes[0])
    // This is a critical validation - if it doesn't match, the key is almost certainly wrong
    if (encryptedData[7] != keyBytes[0]) {
        Serial.println("==========================================");
        Serial.printf("ERROR: Encryption key match byte mismatch!\n");
        Serial.printf("  Expected: 0x%02X (first byte of your key)\n", keyBytes[0]);
        Serial.printf("  Got:      0x%02X (byte 7 of BLE packet)\n", encryptedData[7]);
        Serial.println("  This indicates an INCORRECT encryption key.");
        Serial.println("  Please verify the key from VictronConnect app.");
        Serial.println("==========================================");
        return false;
    }
    
    // Extract nonce/counter from bytes 5 and 6
    uint8_t dataCounterLSB = encryptedData[5];
    uint8_t dataCounterMSB = encryptedData[6];
    
    // Copy header (first 8 bytes) as-is - they are not encrypted
    memcpy(decryptedData, encryptedData, 8);
    
    // Decrypt the payload starting from byte 8
    size_t encryptedPayloadLength = length - 8;
    if (encryptedPayloadLength == 0) {
        Serial.println("WARNING: No encrypted payload to decrypt");
        return true; // No payload to decrypt, but not an error
    }
    
    // Initialize AES context
    esp_aes_context aesCtx;
    esp_aes_init(&aesCtx);
    
    // Set encryption key (128 bits)
    int status = esp_aes_setkey(&aesCtx, keyBytes, 128);
    if (status != 0) {
        Serial.printf("ERROR: Failed to set AES key (status %d)\n", status);
        esp_aes_free(&aesCtx);
        return false;
    }
    
    // Prepare nonce/counter for AES-CTR mode
    // The counter is initialized with data counter bytes and remaining bytes set to zero
    uint8_t nonceCounter[16];
    memset(nonceCounter, 0, sizeof(nonceCounter));
    nonceCounter[0] = dataCounterLSB;
    nonceCounter[1] = dataCounterMSB;
    
    uint8_t streamBlock[16];
    memset(streamBlock, 0, sizeof(streamBlock));
    size_t ncOffset = 0;
    
    // Decrypt using AES-128-CTR
    status = esp_aes_crypt_ctr(&aesCtx, 
                               encryptedPayloadLength, 
                               &ncOffset, 
                               nonceCounter, 
                               streamBlock, 
                               &encryptedData[8],        // source: encrypted payload
                               &decryptedData[8]);       // destination: decrypted payload
    
    esp_aes_free(&aesCtx);
    
    if (status != 0) {
        Serial.printf("ERROR: AES-CTR decryption failed (status %d)\n", status);
        return false;
    }
    
    Serial.printf("Successfully decrypted %d bytes of data\n", encryptedPayloadLength);
    return true;
}
