#include "EcoWorthyBMS.h"

EcoWorthyBMS::EcoWorthyBMS() : 
    pClient(nullptr),
    pService(nullptr),
    pRxCharacteristic(nullptr),
    isConnected(false),
    isScanning(false),
    dataBufferLen(0),
    dataReceived(false),
    dataA1Len(0),
    dataA2Len(0),
    hasDataA1(false),
    hasDataA2(false) {
    memset(dataBuffer, 0, sizeof(dataBuffer));
    memset(macBytes, 0, sizeof(macBytes));
    memset(dataA1, 0, sizeof(dataA1));
    memset(dataA2, 0, sizeof(dataA2));
}

EcoWorthyBMS::~EcoWorthyBMS() {
    disconnect();
}

bool EcoWorthyBMS::begin() {
    Serial.println("Initializing Eco Worthy BMS...");
    // NimBLE should already be initialized by VictronBLE
    return true;
}

bool EcoWorthyBMS::isEcoWorthyDevice(const String& name) {
    if (name.length() == 0) return false;
    
    // Check for Eco Worthy naming patterns
    return (name.startsWith("ECO-WORTHY") || 
            name.startsWith("DCHOUSE") ||
            name.indexOf("ECO-WORTHY 02_") >= 0);
}

bool EcoWorthyBMS::isEcoWorthyDevice(NimBLEAdvertisedDevice* device) {
    if (!device) return false;
    
    String name = device->getName().c_str();
    if (isEcoWorthyDevice(name)) {
        return true;
    }
    
    // Also check for service UUID
    if (device->isAdvertisingService(NimBLEUUID(ECOWORTHY_SERVICE_UUID))) {
        return true;
    }
    
    return false;
}

uint16_t EcoWorthyBMS::calculateModbusCRC(const uint8_t* data, size_t length) {
    // Modbus CRC-16 calculation
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool EcoWorthyBMS::connectToDevice(NimBLEAdvertisedDevice* device) {
    if (!device) return false;
    return connectToAddress(String(device->getAddress().toString().c_str()));
}

bool EcoWorthyBMS::connectToAddress(const String& address) {
    if (isConnected) {
        Serial.println("Already connected to a device");
        return true;
    }
    
    Serial.printf("Connecting to Eco Worthy BMS: %s\n", address.c_str());
    
    // Parse MAC address into bytes
    int values[6];
    if (sscanf(address.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            macBytes[i] = (uint8_t)values[i];
        }
    }
    
    // Create client
    pClient = NimBLEDevice::createClient();
    if (!pClient) {
        Serial.println("Failed to create BLE client");
        return false;
    }
    
    // Connect to device
    NimBLEAddress bleAddress(address.c_str());
    if (!pClient->connect(bleAddress)) {
        Serial.println("Failed to connect to device");
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        return false;
    }
    
    Serial.println("Connected to device");
    
    // Get service
    pService = pClient->getService(ECOWORTHY_SERVICE_UUID);
    if (!pService) {
        Serial.println("Failed to find service");
        disconnect();
        return false;
    }
    
    // Get RX characteristic
    pRxCharacteristic = pService->getCharacteristic(ECOWORTHY_RX_UUID);
    if (!pRxCharacteristic) {
        Serial.println("Failed to find RX characteristic");
        disconnect();
        return false;
    }
    
    // Subscribe to notifications
    if (pRxCharacteristic->canNotify()) {
        pRxCharacteristic->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
            // Store received data
            if (length > 0 && length < sizeof(this->dataBuffer)) {
                memcpy(this->dataBuffer, pData, length);
                this->dataBufferLen = length;
                this->dataReceived = true;
                this->parseResponse(pData, length);
            }
        });
    }
    
    isConnected = true;
    currentData.address = address;
    currentData.connected = true;
    
    Serial.println("Successfully connected and subscribed to notifications");
    return true;
}

bool EcoWorthyBMS::disconnect() {
    if (pClient) {
        if (isConnected) {
            pClient->disconnect();
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }
    
    pService = nullptr;
    pRxCharacteristic = nullptr;
    isConnected = false;
    currentData.connected = false;
    
    return true;
}

bool EcoWorthyBMS::isDeviceConnected() const {
    return isConnected && pClient && pClient->isConnected();
}

bool EcoWorthyBMS::parseResponse(const uint8_t* data, size_t length) {
    if (length < 3) return false;
    
    // Check for valid header
    uint8_t header = data[0];
    bool hasMacPrefix = false;
    size_t dataStart = 0;
    
    // Check if this starts with MAC address (6 bytes) + header
    if (length >= 7) {
        // Check if bytes match our MAC
        bool macMatch = true;
        for (int i = 0; i < 6; i++) {
            if (data[i] != macBytes[i]) {
                macMatch = false;
                break;
            }
        }
        
        if (macMatch) {
            hasMacPrefix = true;
            header = data[6];
            dataStart = 7;
        }
    }
    
    // If no MAC prefix, check if first byte is header
    if (!hasMacPrefix) {
        if (data[0] == HEAD_A1 || data[0] == HEAD_A2) {
            header = data[0];
            dataStart = 1;
        } else {
            Serial.println("Invalid header byte");
            return false;
        }
    }
    
    // Verify CRC (last 2 bytes)
    if (length < dataStart + 2) return false;
    
    size_t dataLen = length - dataStart - 2;
    uint16_t receivedCRC = data[length - 2] | (data[length - 1] << 8);
    uint16_t calculatedCRC = calculateModbusCRC(data, length - 2);
    
    if (receivedCRC != calculatedCRC) {
        Serial.printf("CRC mismatch: received 0x%04X, calculated 0x%04X\n", receivedCRC, calculatedCRC);
        return false;
    }
    
    // Store data based on header type
    if (header == HEAD_A1) {
        memcpy(dataA1, data + dataStart, dataLen);
        dataA1Len = dataLen;
        hasDataA1 = true;
        return parseDataA1(data + dataStart, dataLen);
    } else if (header == HEAD_A2) {
        memcpy(dataA2, data + dataStart, dataLen);
        dataA2Len = dataLen;
        hasDataA2 = true;
        return parseDataA2(data + dataStart, dataLen);
    }
    
    return false;
}

bool EcoWorthyBMS::parseDataA1(const uint8_t* data, size_t length) {
    // A1 packet contains: battery level, health, voltage, current, capacity, problem code
    // Based on reference implementation offsets
    if (length < 52) return false;
    
    // Battery level (offset 16, 2 bytes)
    if (length >= 18) {
        currentData.batteryLevel = (data[16] | (data[17] << 8)) / 1.0;
    }
    
    // Battery health (offset 18, 2 bytes)
    if (length >= 20) {
        currentData.batteryHealth = (data[18] | (data[19] << 8)) / 1.0;
    }
    
    // Voltage (offset 20, 2 bytes, in 0.01V)
    if (length >= 22) {
        uint16_t voltageRaw = data[20] | (data[21] << 8);
        currentData.voltage = voltageRaw / 100.0;
    }
    
    // Current (offset 22, 2 bytes, signed, in 0.01A or 0.1A depending on version)
    if (length >= 24) {
        int16_t currentRaw = (int16_t)(data[22] | (data[23] << 8));
        // Note: Version detection would require header info from parent parseResponse()
        // For now, use 0.01A as default (V1 protocol)
        currentData.current = currentRaw / 100.0;
    }
    
    // Design capacity (offset 26, 2 bytes, in 0.01Ah, convert to Ah)
    if (length >= 28) {
        uint16_t capacityRaw = data[26] | (data[27] << 8);
        currentData.designCapacity = capacityRaw / 100.0;
    }
    
    // Problem code (offset 51, 2 bytes)
    if (length > 52) {
        currentData.problemCode = data[51] | (data[52] << 8);
    }
    
    // Calculate power
    currentData.power = currentData.voltage * currentData.current;
    
    currentData.dataValid = true;
    currentData.lastUpdate = millis();
    
    return true;
}

bool EcoWorthyBMS::parseDataA2(const uint8_t* data, size_t length) {
    // A2 packet contains: cell count, cell voltages, temp sensor count, temperatures
    if (length < 16) return false;
    
    // Cell count (offset 14, 2 bytes)
    if (length >= 16) {
        currentData.cellCount = data[14] | (data[15] << 8);
        if (currentData.cellCount > 16) currentData.cellCount = 16; // Safety limit
    }
    
    // Extract cell voltages
    extractCellVoltages(data, length);
    
    // Extract temperatures
    extractTemperatures(data, length);
    
    return true;
}

void EcoWorthyBMS::extractCellVoltages(const uint8_t* data, size_t length) {
    // Cell voltages start at offset 16 (after cell count at offset 14)
    const int CELL_VOLTAGE_START = 16;
    
    for (int i = 0; i < currentData.cellCount && i < 16; i++) {
        int offset = CELL_VOLTAGE_START + (i * 2);
        if (offset + 1 < length) {
            uint16_t cellVoltage = data[offset] | (data[offset + 1] << 8);
            currentData.cellVoltages[i] = cellVoltage / 1000.0; // Convert mV to V
        }
    }
}

void EcoWorthyBMS::extractTemperatures(const uint8_t* data, size_t length) {
    // Temperature sensor count is at offset 80
    const int TEMP_COUNT_OFFSET = 80;
    const int TEMP_START_OFFSET = 82;
    
    if (length < TEMP_COUNT_OFFSET + 2) return;
    
    currentData.tempSensorCount = data[TEMP_COUNT_OFFSET] | (data[TEMP_COUNT_OFFSET + 1] << 8);
    if (currentData.tempSensorCount > 4) currentData.tempSensorCount = 4; // Safety limit
    
    for (int i = 0; i < currentData.tempSensorCount && i < 4; i++) {
        int offset = TEMP_START_OFFSET + (i * 2);
        if (offset + 1 < length) {
            int16_t tempRaw = (int16_t)(data[offset] | (data[offset + 1] << 8));
            currentData.temperatures[i] = tempRaw / 10.0; // Convert 0.1°C to °C
        }
    }
}

bool EcoWorthyBMS::updateData() {
    if (!isDeviceConnected()) {
        Serial.println("Not connected to device");
        return false;
    }
    
    // Reset flags
    hasDataA1 = false;
    hasDataA2 = false;
    dataReceived = false;
    
    // Wait for notifications (data is received via callbacks)
    // The BMS automatically sends notifications when connected
    unsigned long startTime = millis();
    const unsigned long timeout = 5000; // 5 second timeout
    
    while ((!hasDataA1 || !hasDataA2) && (millis() - startTime < timeout)) {
        delay(100);
        
        // Keep connection alive
        if (!pClient->isConnected()) {
            Serial.println("Connection lost during update");
            isConnected = false;
            currentData.connected = false;
            return false;
        }
    }
    
    if (!hasDataA1 || !hasDataA2) {
        Serial.printf("Timeout waiting for data (A1: %d, A2: %d)\n", hasDataA1, hasDataA2);
        return false;
    }
    
    Serial.println("Successfully updated Eco Worthy BMS data");
    return true;
}

EcoWorthyBMSData* EcoWorthyBMS::getData() {
    return &currentData;
}
