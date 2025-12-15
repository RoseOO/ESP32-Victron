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

VictronBLE::VictronBLE() : retainLastData(true) {
    pBLEScan = nullptr;
}

void VictronBLE::begin() {
    Serial.println("Initializing Victron BLE...");
    NimBLEDevice::init("");
    
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisedDeviceCallbacks(this), false);
    // Use active scanning for faster device discovery
    // Note: Victron devices broadcast BLE advertisements at their own rate (typically 1-2 seconds)
    // We cannot "request" faster updates as these are advertisement packets, not connection-based
    pBLEScan->setActiveScan(true);
    // Scan interval: 100ms (how often to switch channels)
    // Scan window: 99ms (how long to listen on each channel)
    // This means we're scanning almost continuously for maximum responsiveness
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
                    
                    // Extract model ID if available (bytes 2-3, little-endian)
                    if (mfgData.length() >= 4) {
                        devData.modelId = (uint8_t)mfgData[2] | ((uint8_t)mfgData[3] << 8);
                    }
                    
                    // Check if encrypted (byte 4 indicates readout type/encryption)
                    if (mfgData.length() >= 5) {
                        devData.encrypted = (mfgData[4] != 0x00);
                    }
                    
                    // Parse manufacturer data with encryption key if available
                    if (mfgData.length() > 2) {
                        String encKey = getEncryptionKey(devData.address);
                        parseVictronAdvertisement((const uint8_t*)mfgData.data(), mfgData.length(), devData, encKey);
                    }
                    
                    // Check if device already exists
                    auto it = devices.find(devData.address);
                    if (it != devices.end() && retainLastData) {
                        // Device exists and retain mode is enabled - merge data
                        mergeDeviceData(devData, it->second);
                    } else {
                        // New device or retain mode disabled - replace completely
                        devices[devData.address] = devData;
                    }
                    
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
    
    // Priority order matters - check most specific patterns first
    if (lowerName.indexOf("smartlithium") >= 0 || lowerName.indexOf("smart lithium") >= 0) {
        return DEVICE_SMART_LITHIUM;
    } else if (lowerName.indexOf("lynx") >= 0 && lowerName.indexOf("bms") >= 0) {
        return DEVICE_LYNX_SMART_BMS;
    } else if (lowerName.indexOf("battery protect") >= 0 || lowerName.indexOf("batteryprotect") >= 0) {
        return DEVICE_SMART_BATTERY_PROTECT;
    } else if (lowerName.indexOf("battery sense") >= 0 || lowerName.indexOf("batterysense") >= 0) {
        return DEVICE_SMART_BATTERY_SENSE;
    } else if (lowerName.indexOf("shunt") >= 0 || lowerName.indexOf("smartshunt") >= 0 || 
               lowerName.indexOf("bmv") >= 0) {
        return DEVICE_SMART_SHUNT;
    } else if (lowerName.indexOf("multi rs") >= 0 || lowerName.indexOf("multirs") >= 0) {
        return DEVICE_MULTI_RS;
    } else if (lowerName.indexOf("inverter rs") >= 0 || lowerName.indexOf("inverterrs") >= 0) {
        return DEVICE_INVERTER_RS;
    } else if (lowerName.indexOf("ve.bus") >= 0 || lowerName.indexOf("vebus") >= 0 ||
               lowerName.indexOf("multiplus") >= 0 || lowerName.indexOf("quattro") >= 0) {
        return DEVICE_VE_BUS;
    } else if (lowerName.indexOf("solar") >= 0 || lowerName.indexOf("mppt") >= 0) {
        return DEVICE_SMART_SOLAR;
    } else if (lowerName.indexOf("orion xs") >= 0) {
        return DEVICE_ORION_XS;
    } else if (lowerName.indexOf("orion") >= 0 || lowerName.indexOf("dc-dc") >= 0 || 
               lowerName.indexOf("dcdc") >= 0 || lowerName.indexOf("buckboost") >= 0) {
        return DEVICE_DCDC_CONVERTER;
    } else if ((lowerName.indexOf("blue") >= 0 && lowerName.indexOf("charger") >= 0) || 
               lowerName.indexOf("smartcharger") >= 0 || lowerName.indexOf("smart charger") >= 0 ||
               lowerName.indexOf("ip65") >= 0 || lowerName.indexOf("ip22") >= 0 || 
               lowerName.indexOf("ip43") >= 0) {
        return DEVICE_BLUE_SMART_CHARGER;
    } else if (lowerName.indexOf("ac charger") >= 0 || lowerName.indexOf("accharger") >= 0) {
        return DEVICE_AC_CHARGER;
    } else if (lowerName.indexOf("energy meter") >= 0 || lowerName.indexOf("energymeter") >= 0) {
        return DEVICE_DC_ENERGY_METER;
    } else if (lowerName.indexOf("inverter") >= 0 || lowerName.indexOf("phoenix") >= 0) {
        return DEVICE_INVERTER;
    }
    
    return DEVICE_UNKNOWN;
}

bool VictronBLE::parseVictronAdvertisement(const uint8_t* data, size_t length, VictronDeviceData& device, const String& encryptionKey) {
    // Victron BLE advertisement format (based on reference implementation):
    // [0-1]: Manufacturer ID (0x02E1) - little-endian
    // [2-3]: Model ID - little-endian
    // [4]: Readout type / Encryption indicator (0x00 for instant readout, non-zero for encrypted)
    // For encrypted packets:
    //   [5-6]: Additional flags/padding
    //   [7-8]: IV/Counter (nonce) - little-endian (LSB, MSB)
    //   [9]: Encryption key match byte (should match first byte of key)
    //   [10+]: Encrypted data records
    // For unencrypted packets:
    //   [5+]: Data records (unencrypted)
    
    if (length < 5) return false;
    
    // Clear previous parsed records
    device.parsedRecords.clear();
    
    // Check if data is encrypted (byte 4 != 0x00)
    bool isEncrypted = (data[4] != 0x00);
    
    // For encrypted data, we need at least 10 bytes (manufacturer ID, model ID, readout type, 
    // flags/padding, IV, and key check byte)
    if (isEncrypted && length < 10) {
        Serial.printf("ERROR: Encrypted packet too short: %d bytes (need at least 10)\n", length);
        return false;
    }
    
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
    
    // Determine where the payload data starts
    // For encrypted data: payload starts at byte 10 (after header + IV + key check byte)
    // For unencrypted data: payload starts at byte 5 (after manufacturer ID, model ID, and readout type)
    size_t payloadStart = isEncrypted ? 10 : 5;
    
    // The decrypted/unencrypted payload contains a fixed structure
    // The structure varies by device type (SmartShunt vs Solar Controller vs DC-DC, etc.)
    // According to the reference implementation, we need to parse this as a fixed structure,
    // NOT as TLV records
    // Note: SmartShunt uses 15 bytes, while Solar Controller and DC-DC use 16 bytes
    
    // Determine expected payload size based on device type for informational purposes
    size_t expectedPayloadBytes;
    if (device.type == DEVICE_SMART_SHUNT) {
        expectedPayloadBytes = SMART_SHUNT_PAYLOAD_SIZE;
    } else if (device.type == DEVICE_SMART_SOLAR || device.type == DEVICE_BLUE_SMART_CHARGER) {
        // Both Solar Controllers and Blue Smart Chargers use 16-byte payloads
        expectedPayloadBytes = SOLAR_CONTROLLER_PAYLOAD_SIZE;
    } else if (device.type == DEVICE_DCDC_CONVERTER) {
        expectedPayloadBytes = DCDC_CONVERTER_PAYLOAD_SIZE;
    } else {
        // Unknown device types will use TLV parsing, no expected size
        expectedPayloadBytes = 0;
    }
    
    // Log a warning if we have less data than expected, but continue parsing
    // We'll parse whatever fields are available based on the actual data length
    if (expectedPayloadBytes > 0 && length < payloadStart + expectedPayloadBytes) {
        Serial.printf("WARNING: Partial data received (%d bytes, expected %d) - parsing available fields\n", 
                     length, payloadStart + expectedPayloadBytes);
    }
    
    // Point to the fixed structure payload (size varies by device type)
    const uint8_t* output = &dataToProcess[payloadStart];
    size_t outputLen = length - payloadStart;
    
    // Parse based on device type
    if (device.type == DEVICE_SMART_SHUNT) {
        parseSmartShuntData(output, outputLen, device);
    } else if (device.type == DEVICE_SMART_SOLAR || device.type == DEVICE_BLUE_SMART_CHARGER) {
        // Both Solar Controllers and Blue Smart Chargers use the same 16-byte payload format
        parseSolarControllerData(output, outputLen, device);
    } else if (device.type == DEVICE_DCDC_CONVERTER) {
        parseDCDCConverterData(output, outputLen, device);
    } else {
        // For unknown device types, try to parse as TLV records
        // This provides backwards compatibility for devices we haven't specifically implemented
        parseTLVRecords(dataToProcess, length, payloadStart, device);
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

// Helper function to convert a hex character to its numeric value
// Returns -1 for invalid characters
static int hexCharToValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    } else if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

bool VictronBLE::decryptData(const uint8_t* encryptedData, size_t length, uint8_t* decryptedData, const String& key) {
    // Victron uses AES-128-CTR encryption for BLE data
    // Packet structure (based on reference implementation):
    // [0-1]: Manufacturer ID (0x02E1) - little-endian
    // [2-3]: Model ID - little-endian
    // [4]: Readout type / Encryption indicator
    // [5-6]: Additional flags/padding
    // [7-8]: IV/Counter (nonce) - little-endian (LSB, MSB)
    // [9]: Encryption Key Match byte (should match first byte of key)
    // [10+]: Encrypted payload
    
    if (key.isEmpty() || length < 10) {
        Serial.printf("ERROR: Invalid encryption key or data length (minimum 10 bytes required, got %d)\n", length);
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
        int highVal = hexCharToValue(highNibble);
        if (highVal < 0) {
            Serial.printf("ERROR: Invalid hex character '%c' in encryption key at position %d\n", highNibble, i * 2);
            return false;
        }
        
        // Validate and convert low nibble
        int lowVal = hexCharToValue(lowNibble);
        if (lowVal < 0) {
            Serial.printf("ERROR: Invalid hex character '%c' in encryption key at position %d\n", lowNibble, i * 2 + 1);
            return false;
        }
        
        // Combine nibbles into byte
        keyBytes[i] = (highVal << 4) | lowVal;
    }
    
    // Verify the encryption key match byte (byte 9 should match keyBytes[0])
    // This is a validation check - if it doesn't match, the key might be wrong
    // However, we'll proceed with decryption anyway as requested, since sometimes
    // the validation can reject valid keys (e.g., when data format varies)
    if (encryptedData[9] != keyBytes[0]) {
        Serial.println("==========================================");
        Serial.printf("WARNING: Encryption key match byte mismatch!\n");
        Serial.printf("  Expected: 0x%02X (first byte of your key)\n", keyBytes[0]);
        Serial.printf("  Got:      0x%02X (byte 9 of BLE packet)\n", encryptedData[9]);
        Serial.println("  This may indicate an incorrect encryption key.");
        Serial.println("  Attempting decryption anyway...");
        Serial.println("==========================================");
        // Continue with decryption instead of returning false
    }
    
    // Extract nonce/counter from bytes 7 and 8
    uint8_t dataCounterLSB = encryptedData[7];
    uint8_t dataCounterMSB = encryptedData[8];
    
    // Copy header (first 10 bytes) as-is - they are not encrypted
    memcpy(decryptedData, encryptedData, 10);
    
    // Decrypt the payload starting from byte 10
    size_t encryptedPayloadLength = length - 10;
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
    // The counter is initialized with data counter bytes (from BLE packet) and remaining bytes set to zero
    // Format: [dataCounterLSB, dataCounterMSB, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    // This follows the Victron BLE AES-CTR specification
    uint8_t nonceCounter[16];
    memset(nonceCounter, 0, sizeof(nonceCounter));  // Initialize all 16 bytes to zero
    nonceCounter[0] = dataCounterLSB;
    nonceCounter[1] = dataCounterMSB;
    // Bytes 2-15 remain zero as per AES-CTR spec
    
    uint8_t streamBlock[16];
    memset(streamBlock, 0, sizeof(streamBlock));
    size_t ncOffset = 0;
    
    // Decrypt using AES-128-CTR
    status = esp_aes_crypt_ctr(&aesCtx, 
                               encryptedPayloadLength, 
                               &ncOffset, 
                               nonceCounter, 
                               streamBlock, 
                               &encryptedData[10],        // source: encrypted payload
                               &decryptedData[10]);       // destination: decrypted payload
    
    esp_aes_free(&aesCtx);
    
    if (status != 0) {
        Serial.printf("ERROR: AES-CTR decryption failed (status %d)\n", status);
        return false;
    }
    
    Serial.printf("Successfully decrypted %d bytes of data\n", encryptedPayloadLength);
    return true;
}

// Helper functions for multi-byte value extraction

// Extract signed 16-bit value (little-endian with sign bit)
int16_t VictronBLE::extractSigned16(const uint8_t* data, int byteIndex) {
    bool neg = (data[byteIndex + 1] & 0x80) >> 7;  // extract sign bit from MSB
    int16_t value = ((data[byteIndex + 1] & 0x7F) << 8) | data[byteIndex];  // exclude sign bit
    if (neg) value = value - 32768;  // 2's complement = val - 2^(b-1), b=16
    return value;
}

// Extract unsigned 16-bit value (little-endian)
uint16_t VictronBLE::extractUnsigned16(const uint8_t* data, int byteIndex) {
    return (data[byteIndex + 1] << 8) | data[byteIndex];
}

// Extract signed 22-bit value (spread across 3 bytes, little-endian)
// Battery current for SmartShunt: bytes 8-10
int32_t VictronBLE::extractSigned22(const uint8_t* data, int startByte) {
    bool neg = (data[startByte + 2] & 0x80) >> 7;  // bit 21 (sign bit)
    
    // Extract the 21-bit unsigned value spread across the bytes
    // Byte 0 (8): bits 2-7 contain bits 0-5 of value
    // Byte 1 (9): bits 0-1 contain bits 6-7, bits 2-7 contain bits 8-13
    // Byte 2 (10): bits 0-1 contain bits 14-15, bits 2-6 contain bits 16-20
    int32_t value = ((data[startByte] & 0xFC) >> 2) |                           // bits 0-5
                   (((data[startByte + 1] & 0x03) << 6)) |                       // bits 6-7
                   (((data[startByte + 1] & 0xFC) >> 2) << 8) |                  // bits 8-13
                   (((data[startByte + 2] & 0x03) << 14)) |                      // bits 14-15
                   (((data[startByte + 2] & 0x7C) >> 2) << 16);                  // bits 16-20
    
    if (neg) value = value - 2097152;  // 2's complement = val - 2^(b-1), b=22
    return value;
}

// Extract unsigned 20-bit value (spread across 3 bytes, little-endian)
// Consumed Ah for SmartShunt: bytes 11-13 (lower 4 bits of byte 13)
uint32_t VictronBLE::extractUnsigned20(const uint8_t* data, int startByte) {
    return data[startByte] |                     // bits 0-7
          (data[startByte + 1] << 8) |           // bits 8-15
          ((data[startByte + 2] & 0x0F) << 16);  // bits 16-19
}

// Extract unsigned 10-bit value (spread across 2 bytes)
// State of Charge for SmartShunt: bytes 13-14 (upper 4 bits of byte 13, bits 0-5 of byte 14)
uint16_t VictronBLE::extractUnsigned10(const uint8_t* data, int startByte) {
    // Byte 13: bits 4-7 contain bits 0-3 of value (extract with 0xF0, shift right by 4)
    // Byte 14: bits 0-3 contain bits 4-7 of value (extract with 0x0F, shift left by 4)
    // Byte 14: bits 4-5 contain bits 8-9 of value (extract with 0x30, shift left by 4 more)
    return ((data[startByte] & 0xF0) >> 4) |       // bits 0-3
           ((data[startByte + 1] & 0x0F) << 4) |   // bits 4-7
           ((data[startByte + 1] & 0x30) << 4);    // bits 8-9 (bits 4-5 of byte shifted to 8-9)
}

// Validate voltage reading with sanity check
// Returns true if voltage is valid (within MIN_VALID_VOLTAGE to MAX_VALID_VOLTAGE), false if invalid
// If invalid, sets device.dataValid to false and logs an error
bool VictronBLE::validateVoltage(float voltage, const char* source, VictronDeviceData& device) {
    // Sanity check: discard packet if voltage > MAX_VALID_VOLTAGE or < MIN_VALID_VOLTAGE (clearly incorrect data)
    if (voltage > MAX_VALID_VOLTAGE || voltage < MIN_VALID_VOLTAGE) {
        device.dataValid = false;
        // Build error message using snprintf for better performance
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "Invalid voltage reading (%.2fV, valid range: %.0fV to %.0fV) - packet discarded", 
                 voltage, MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        device.errorMessage = errorMsg;
        Serial.printf("ERROR: Invalid voltage %.2fV detected in %s packet (valid range: %.0fV to %.0fV) - discarding\n", 
                     voltage, source, MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        return false;
    }
    return true;
}

// Validate temperature reading with sanity check
// Returns true if temperature is valid (between valid range and not exceeding MAX_VALID_TEMPERATURE), false if invalid
// If invalid, sets device.dataValid to false and logs an error
bool VictronBLE::validateTemperature(float temperature, const char* source, VictronDeviceData& device) {
    // Sanity check: discard packet if temperature > MAX_VALID_TEMPERATURE (clearly incorrect data)
    if (temperature > MAX_VALID_TEMPERATURE) {
        device.dataValid = false;
        // Build error message using snprintf for better performance
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "Invalid temperature reading (%.1f°C, max: %.0f°C) - packet discarded", 
                 temperature, MAX_VALID_TEMPERATURE);
        device.errorMessage = errorMsg;
        Serial.printf("ERROR: Invalid temperature %.1f°C detected in %s packet (max: %.0f°C) - discarding\n", 
                     temperature, source, MAX_VALID_TEMPERATURE);
        return false;
    }
    return true;
}

// Parse SmartShunt data (15-byte fixed structure, but handle partial data)
// Based on VBM.cpp from reference implementation
void VictronBLE::parseSmartShuntData(const uint8_t* output, size_t length, VictronDeviceData& device) {
    // Parse whatever fields are available based on actual data length
    // Each field is only parsed if we have enough bytes for it
    
    // Time To Go (bytes 0-1, little-endian, unsigned 16-bit, units: minutes)
    if (length >= 2) {
        uint16_t ttgMinutes = extractUnsigned16(output, 0);
        if (ttgMinutes != 0xFFFF) {
            device.timeToGo = ttgMinutes;
        }
    }
    
    // Battery Voltage (bytes 2-3, signed 16-bit, units: 10mV)
    if (length >= 4) {
        int16_t battMv10 = extractSigned16(output, 2);
        if (battMv10 != 0x7FFF) {
            float voltage = battMv10 / 100.0f;  // convert 10mV to V
            if (!validateVoltage(voltage, "SmartShunt", device)) {
                return;
            }
            device.voltage = voltage;
            device.hasVoltage = true;
        }
    }
    
    // Alarm bits (bytes 4-5)
    if (length >= 6) {
        device.alarmState = extractUnsigned16(output, 4);
    }
    
    // Auxiliary input (bytes 6-7) and aux mode (byte 8)
    // The type is determined by bits 0-1 of byte 8
    if (length >= 9) {
        device.auxMode = output[8] & 0x03;
        
        if (device.auxMode == 0) {
            // Aux voltage (signed 16-bit, units: 10mV)
            int16_t auxMv10 = extractSigned16(output, 6);
            if (auxMv10 != 0x7FFF) {
                device.auxVoltage = auxMv10 / 100.0f;
            }
        } else if (device.auxMode == 1) {
            // Mid-point voltage (unsigned 16-bit, units: 10mV)
            uint16_t midMv10 = extractUnsigned16(output, 6);
            if (midMv10 != 0xFFFF) {
                device.midVoltage = midMv10 / 100.0f;
            }
        } else if (device.auxMode == 2) {
            // Temperature (unsigned 16-bit, units: 10mK = 0.01K)
            uint16_t tempK01 = extractUnsigned16(output, 6);
            if (tempK01 != 0xFFFF) {
                float temperature = (tempK01 / 100.0f) - 273.15;  // convert 0.01K to Celsius
                if (!validateTemperature(temperature, "SmartShunt", device)) {
                    return;
                }
                device.temperature = temperature;
                device.hasTemperature = true;
            }
        }
    }
    
    // Battery Current (bytes 8-10, signed 22-bit, units: mA)
    if (length >= 11) {
        int32_t battMa = extractSigned22(output, 8);
        if (battMa != 0x1FFFFF) {
            device.current = battMa / 1000.0f;  // convert mA to A
            device.hasCurrent = true;
            
            // Calculate power if we have both voltage and current
            if (device.hasVoltage) {
                device.power = device.voltage * device.current;
                device.hasPower = true;
            }
        }
    }
    
    // Consumed Ah (bytes 11-13, unsigned 20-bit, units: 100mAh = 0.1Ah)
    if (length >= 14) {
        uint32_t consumedAh01 = extractUnsigned20(output, 11);
        if (consumedAh01 != 0xFFFFF) {
            device.consumedAh = consumedAh01 / 10.0f;  // convert 0.1Ah to Ah
        }
    }
    
    // State of Charge (bytes 13-14, unsigned 10-bit, units: 0.1%)
    if (length >= 15) {
        uint16_t soc01 = extractUnsigned10(output, 13);
        if (soc01 != 0x3FF) {
            device.batterySOC = soc01 / 10.0f;  // convert 0.1% to %
            device.hasSOC = true;
        }
    }
    
    Serial.printf("SmartShunt parsed: V=%.2f, A=%.3f, SOC=%.1f%%, Ah=%.1f\n", 
                 device.voltage, device.current, device.batterySOC, device.consumedAh);
}

// Parse Solar Controller data (16-byte fixed structure, but handle partial data)
// Blue Smart Chargers also use this same format
// Based on VSC.cpp from reference implementation
void VictronBLE::parseSolarControllerData(const uint8_t* output, size_t length, VictronDeviceData& device) {
    // Parse whatever fields are available based on actual data length
    // Each field is only parsed if we have enough bytes for it
    
    // Device State (byte 0)
    if (length >= 1) {
        device.deviceState = output[0];
    }
    
    // Charger Error (byte 1)
    if (length >= 2) {
        device.chargerError = output[1];
    }
    
    // Battery Voltage (bytes 2-3, signed 16-bit, units: 10mV)
    if (length >= 4) {
        int16_t battMv10 = extractSigned16(output, 2);
        if (battMv10 != 0x7FFF) {
            float voltage = battMv10 / 100.0f;  // convert 10mV to V
            if (!validateVoltage(voltage, "SolarController", device)) {
                return;
            }
            device.voltage = voltage;
            device.hasVoltage = true;
        }
    }
    
    // Battery Current (bytes 4-5, signed 16-bit, units: 100mA)
    if (length >= 6) {
        int16_t battMa100 = extractSigned16(output, 4);
        if (battMa100 != 0x7FFF) {
            device.current = battMa100 / 10.0f;  // convert 100mA to A
            device.hasCurrent = true;
            
            // Calculate power if we have both voltage and current
            if (device.hasVoltage) {
                device.power = device.voltage * device.current;
                device.hasPower = true;
            }
        }
    }
    
    // Yield Today (bytes 6-7, unsigned 16-bit, units: 10Wh = 0.01kWh)
    if (length >= 8) {
        uint16_t yieldWh10 = extractUnsigned16(output, 6);
        if (yieldWh10 != 0xFFFF) {
            device.yieldToday = yieldWh10 / 100.0f;  // convert 10Wh to kWh
        }
    }
    
    // PV Power (bytes 8-9, unsigned 16-bit, units: W)
    if (length >= 10) {
        uint16_t pvW = extractUnsigned16(output, 8);
        if (pvW != 0xFFFF) {
            device.pvPower = pvW;
        }
    }
    
    // Load Current (bytes 10-11, partially used, units: 100mA)
    // Only 9 bits are used: bit 0 of byte 11 + all 8 bits of byte 10
    if (length >= 12) {
        uint16_t loadMa100 = ((output[11] & 0x01) << 8) | output[10];
        if (loadMa100 != 0x1FF) {
            device.loadCurrent = loadMa100 / 10.0f;  // convert 100mA to A
        }
    }
    
    Serial.printf("%s parsed: V=%.2f, A=%.2f, PV=%.0fW, Yield=%.2fkWh, State=%d, Error=%d\n",
                 (device.type == DEVICE_BLUE_SMART_CHARGER) ? "BlueSmartCharger" : "SolarController",
                 device.voltage, device.current, device.pvPower, device.yieldToday, 
                 device.deviceState, device.chargerError);
}

// Parse DC-DC Converter data (16-byte fixed structure, but handle partial data)
// Based on reference implementation: mp-se/victron-receiver
// Data layout: state(8), error(8), inputV(16), outputV(16), offReason(32)
void VictronBLE::parseDCDCConverterData(const uint8_t* output, size_t length, VictronDeviceData& device) {
    // Parse whatever fields are available based on actual data length
    // Each field is only parsed if we have enough bytes for it
    
    // Device State (byte 0)
    if (length >= 1) {
        device.deviceState = output[0];
    }
    
    // Error code (byte 1)
    if (length >= 2) {
        device.chargerError = output[1];
    }
    
    // Input Voltage (bytes 2-3, unsigned 16-bit, units: 10mV)
    if (length >= 4) {
        uint16_t inputMv10 = extractUnsigned16(output, 2);
        if (inputMv10 != 0xFFFF) {
            float inputVoltage = inputMv10 / 100.0f;
            if (!validateVoltage(inputVoltage, "DCDC input", device)) {
                return;
            }
            device.inputVoltage = inputVoltage;
            device.hasInputVoltage = true;
        }
    }
    
    // Output Voltage (bytes 4-5, signed 16-bit, units: 10mV)
    if (length >= 6) {
        int16_t outputMv10 = extractSigned16(output, 4);
        if (outputMv10 != 0x7FFF) {
            float outputVoltage = outputMv10 / 100.0f;
            if (!validateVoltage(outputVoltage, "DCDC output", device)) {
                return;
            }
            device.outputVoltage = outputVoltage;
            device.hasOutputVoltage = true;
            // Also set as general voltage for display
            device.voltage = device.outputVoltage;
            device.hasVoltage = true;
        }
    }
    
    // Off Reason (bytes 6-9, unsigned 32-bit)
    if (length >= 10) {
        device.offReason = (uint32_t)output[6] | 
                          ((uint32_t)output[7] << 8) | 
                          ((uint32_t)output[8] << 16) | 
                          ((uint32_t)output[9] << 24);
    }
    
    Serial.printf("DCDC parsed: In=%.2fV, Out=%.2fV, State=%d, Error=%d, OffReason=0x%08X\n", 
                 device.inputVoltage, device.outputVoltage, device.deviceState, 
                 device.chargerError, device.offReason);
}

// Parse TLV records (fallback for unknown device types or unencrypted instant readout)
// This provides backwards compatibility
void VictronBLE::parseTLVRecords(const uint8_t* data, size_t length, size_t startPos, VictronDeviceData& device) {
    size_t pos = startPos;
    
    Serial.println("Parsing TLV records (fallback mode)");
    
    while (pos < length) {
        if (pos + 1 >= length) break;
        
        uint8_t recordType = data[pos];
        uint8_t recordLen = data[pos + 1];
        
        if (pos + 2 + recordLen > length) break;
        
        const uint8_t* recordData = &data[pos + 2];
        
        // Store parsed record for debug purposes
        VictronRecord rec;
        rec.type = recordType;
        rec.length = recordLen;
        if (recordLen <= sizeof(rec.data)) {
            memcpy(rec.data, recordData, recordLen);
            device.parsedRecords.push_back(rec);
        }
        
        switch (recordType) {
            case BATTERY_VOLTAGE:
            case SOLAR_CHARGER_VOLTAGE:
            case CHARGER_VOLTAGE:
                {
                    float voltage = decodeValue(recordData, recordLen, 0.01); // 10mV resolution
                    if (!validateVoltage(voltage, "TLV", device)) {
                        return;
                    }
                    device.voltage = voltage;
                    device.hasVoltage = true;
                }
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
                {
                    float temperature = decodeValue(recordData, recordLen, 0.01) - 273.15; // Kelvin to Celsius
                    if (!validateTemperature(temperature, "TLV", device)) {
                        return;
                    }
                    device.temperature = temperature;
                    device.hasTemperature = true;
                }
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
                
            case AC_OUT_VOLTAGE:
                device.acOutVoltage = decodeValue(recordData, recordLen, 0.01);
                device.hasAcOut = true;
                break;
                
            case AC_OUT_CURRENT:
                device.acOutCurrent = decodeValue(recordData, recordLen, 0.1);
                device.hasAcOut = true;
                break;
                
            case AC_OUT_POWER:
                device.acOutPower = decodeValue(recordData, recordLen, 1.0);
                device.hasAcOut = true;
                break;
                
            case INPUT_VOLTAGE:
                device.inputVoltage = decodeValue(recordData, recordLen, 0.01);
                device.hasInputVoltage = true;
                break;
                
            case OUTPUT_VOLTAGE:
                device.outputVoltage = decodeValue(recordData, recordLen, 0.01);
                device.hasOutputVoltage = true;
                break;
                
            case OFF_REASON:
                device.offReason = (int)decodeValue(recordData, recordLen, 1.0);
                break;
        }
        
        pos += 2 + recordLen;
    }
}

// Merge new device data with existing data
// This preserves the last good values when new parsing fails or returns invalid data
void VictronBLE::mergeDeviceData(const VictronDeviceData& newData, VictronDeviceData& existingData) {
    // Update name only if new name is not empty (sometimes BLE advertisements don't include the name)
    if (!newData.name.isEmpty()) {
        existingData.name = newData.name;
    } else {
        Serial.print("Retaining existing device name '");
        Serial.print(existingData.name);
        Serial.print("' for ");
        Serial.print(existingData.address);
        Serial.println(" (new name empty)");
    }
    
    // Update type only if new type is not DEVICE_UNKNOWN (type is derived from name)
    if (newData.type != DEVICE_UNKNOWN) {
        existingData.type = newData.type;
    } else {
        Serial.print("Retaining existing device type for ");
        Serial.print(existingData.address);
        Serial.println(" (new type unknown)");
    }
    
    // Always update these fields regardless of parsing success
    existingData.rssi = newData.rssi;
    existingData.lastUpdate = newData.lastUpdate;
    existingData.manufacturerId = newData.manufacturerId;
    existingData.modelId = newData.modelId;
    existingData.encrypted = newData.encrypted;
    
    // Safely copy raw data with bounds checking
    existingData.rawDataLength = newData.rawDataLength > sizeof(existingData.rawManufacturerData) 
                                 ? sizeof(existingData.rawManufacturerData) 
                                 : newData.rawDataLength;
    memcpy(existingData.rawManufacturerData, newData.rawManufacturerData, existingData.rawDataLength);
    
    existingData.parsedRecords = newData.parsedRecords;
    
    // If new data is valid, update all the measurement fields
    if (newData.dataValid) {
        existingData.dataValid = true;
        existingData.errorMessage = newData.errorMessage;
        
        // Update voltage if newly available
        if (newData.hasVoltage) {
            existingData.voltage = newData.voltage;
            existingData.hasVoltage = true;
        }
        
        // Update current if newly available
        if (newData.hasCurrent) {
            existingData.current = newData.current;
            existingData.hasCurrent = true;
        }
        
        // Update power if newly available
        if (newData.hasPower) {
            existingData.power = newData.power;
            existingData.hasPower = true;
        }
        
        // Update SOC if newly available
        if (newData.hasSOC) {
            existingData.batterySOC = newData.batterySOC;
            existingData.hasSOC = true;
        }
        
        // Update temperature if newly available
        if (newData.hasTemperature) {
            existingData.temperature = newData.temperature;
            existingData.hasTemperature = true;
        }
        
        // Update AC output if newly available
        if (newData.hasAcOut) {
            existingData.acOutVoltage = newData.acOutVoltage;
            existingData.acOutCurrent = newData.acOutCurrent;
            existingData.acOutPower = newData.acOutPower;
            existingData.hasAcOut = true;
        }
        
        // Update input voltage if newly available
        if (newData.hasInputVoltage) {
            existingData.inputVoltage = newData.inputVoltage;
            existingData.hasInputVoltage = true;
        }
        
        // Update output voltage if newly available
        if (newData.hasOutputVoltage) {
            existingData.outputVoltage = newData.outputVoltage;
            existingData.hasOutputVoltage = true;
        }
        
        // Update all device-specific fields when data is valid
        // These fields don't have corresponding 'has' flags because they're always
        // present in the data structure (even if zero/default). Since we're in the
        // dataValid block, we know parsing succeeded and these values are meaningful.
        existingData.consumedAh = newData.consumedAh;
        existingData.timeToGo = newData.timeToGo;
        existingData.auxVoltage = newData.auxVoltage;
        existingData.midVoltage = newData.midVoltage;
        existingData.auxMode = newData.auxMode;
        existingData.yieldToday = newData.yieldToday;
        existingData.pvPower = newData.pvPower;
        existingData.loadCurrent = newData.loadCurrent;
        existingData.deviceState = newData.deviceState;
        existingData.chargerError = newData.chargerError;
        existingData.alarmState = newData.alarmState;
        existingData.offReason = newData.offReason;
    } else {
        // New data is invalid - keep existing valid data but update error message
        if (!newData.errorMessage.isEmpty()) {
            existingData.errorMessage = newData.errorMessage;
        }
        Serial.printf("Retaining last good data for %s (new data invalid)\n", existingData.address.c_str());
    }
}

void VictronBLE::setRetainLastData(bool retain) {
    retainLastData = retain;
    Serial.printf("Retain last data: %s\n", retain ? "enabled" : "disabled");
}

bool VictronBLE::getRetainLastData() const {
    return retainLastData;
}

// Helper function to convert device state to human-readable string
String VictronBLE::deviceStateToString(int state) {
    switch (state) {
        case STATE_OFF: return "Off";
        case STATE_LOW_POWER: return "Low Power";
        case STATE_FAULT: return "Fault";
        case STATE_BULK: return "Bulk";
        case STATE_ABSORPTION: return "Absorption";
        case STATE_FLOAT: return "Float";
        case STATE_STORAGE: return "Storage";
        case STATE_EQUALIZE_MANUAL: return "Equalize Manual";
        case STATE_PASSTHRU: return "Pass Through";
        case STATE_INVERTING: return "Inverting";
        case STATE_ASSISTING: return "Assisting";
        case STATE_POWER_SUPPLY: return "Power Supply";
        case STATE_SUSTAIN: return "Sustain";
        case STATE_STARTING_UP: return "Starting Up";
        case STATE_REPEATED_ABSORPTION: return "Repeated Absorption";
        case STATE_AUTO_EQUALIZE: return "Auto Equalize";
        case STATE_BATTERY_SAFE: return "Battery Safe";
        case STATE_LOAD_DETECT: return "Load Detect";
        case STATE_BLOCKED: return "Blocked";
        case STATE_TEST: return "Test";
        case STATE_EXTERNAL_CONTROL: return "External Control";
        case STATE_NOT_AVAILABLE: return "N/A";
        default: return "Unknown (" + String(state) + ")";
    }
}

// Helper function to convert charger error to human-readable string
String VictronBLE::chargerErrorToString(int error) {
    switch (error) {
        case ERR_NO_ERROR: return "No error";
        case ERR_TEMPERATURE_BATTERY_HIGH: return "Battery temp too high";
        case ERR_VOLTAGE_HIGH: return "Battery voltage too high";
        case ERR_REMOTE_TEMPERATURE_A: return "Remote temp sensor failure";
        case ERR_REMOTE_BATTERY_A: return "Remote battery sense failure";
        case ERR_HIGH_RIPPLE: return "Battery high ripple";
        case ERR_TEMPERATURE_BATTERY_LOW: return "Battery temp too low";
        case ERR_TEMPERATURE_CHARGER: return "Charger temp too high";
        case ERR_OVER_CURRENT: return "Charger over current";
        case ERR_POLARITY: return "Current polarity reversed";
        case ERR_BULK_TIME: return "Bulk time limit exceeded";
        case ERR_CURRENT_SENSOR: return "Current sensor issue";
        case ERR_INTERNAL_TEMPERATURE: return "Internal temp sensor failure";
        case ERR_FAN: return "Fan failure";
        case ERR_OVERHEATED: return "Terminals overheated";
        case ERR_SHORT_CIRCUIT: return "Charger short circuit";
        case ERR_CONVERTER_ISSUE: return "Power stage issue";
        case ERR_OVER_CHARGE: return "Over-Charge protection";
        case ERR_INPUT_VOLTAGE: return "PV over-voltage";
        case ERR_INPUT_CURRENT: return "PV over-current";
        case ERR_INPUT_POWER: return "PV over-power";
        case ERR_INPUT_SHUTDOWN_VOLTAGE: return "Input shutdown (voltage)";
        case ERR_INVERTER_SHUTDOWN: return "Inverter shutdown";
        case ERR_INVERTER_OVERLOAD: return "Inverter overload";
        case ERR_INVERTER_TEMPERATURE: return "Inverter temperature";
        case ERR_BMS: return "BMS connection lost";
        case ERR_NETWORK: return "Network misconfigured";
        case ERR_CPU_TEMPERATURE: return "CPU temperature too high";
        case ERR_CALIBRATION_LOST: return "Calibration data lost";
        case ERR_FIRMWARE: return "Invalid firmware";
        case ERR_SETTINGS: return "Settings data lost";
        case ERR_NOT_AVAILABLE: return "N/A";
        default: return "Error " + String(error);
    }
}

// Helper function to convert off reason to human-readable string
String VictronBLE::offReasonToString(uint32_t offReason) {
    if (offReason == 0) {
        return "Active";
    }
    
    String reasons = "";
    if (offReason & 0x00000001) reasons += "No input power; ";
    if (offReason & 0x00000002) reasons += "Switched off; ";
    if (offReason & 0x00000004) reasons += "Switched off (register); ";
    if (offReason & 0x00000008) reasons += "Remote input; ";
    if (offReason & 0x00000010) reasons += "Protection active; ";
    if (offReason & 0x00000020) reasons += "Pay-as-you-go; ";
    if (offReason & 0x00000040) reasons += "BMS; ";
    if (offReason & 0x00000080) reasons += "Engine shutdown; ";
    if (offReason & 0x00000100) reasons += "Analysing input; ";
    
    // Remove trailing "; "
    if (reasons.length() > 2) {
        reasons = reasons.substring(0, reasons.length() - 2);
    }
    
    return reasons;
}

// Helper function to convert alarm reason to human-readable string
String VictronBLE::alarmReasonToString(uint16_t alarm) {
    if (alarm == 0) {
        return "No alarm";
    }
    
    String alarms = "";
    if (alarm & 0x0001) alarms += "Low voltage; ";
    if (alarm & 0x0002) alarms += "High voltage; ";
    if (alarm & 0x0004) alarms += "Low SOC; ";
    if (alarm & 0x0008) alarms += "Low starter voltage; ";
    if (alarm & 0x0010) alarms += "High starter voltage; ";
    if (alarm & 0x0020) alarms += "Low temperature; ";
    if (alarm & 0x0040) alarms += "High temperature; ";
    if (alarm & 0x0080) alarms += "Mid voltage; ";
    if (alarm & 0x0100) alarms += "Overload; ";
    if (alarm & 0x0200) alarms += "DC ripple; ";
    if (alarm & 0x0400) alarms += "Low VAC out; ";
    if (alarm & 0x0800) alarms += "High VAC out; ";
    if (alarm & 0x1000) alarms += "Short circuit; ";
    if (alarm & 0x2000) alarms += "BMS lockout; ";
    
    // Remove trailing "; "
    if (alarms.length() > 2) {
        alarms = alarms.substring(0, alarms.length() - 2);
    }
    
    return alarms;
}
