#include "WebConfigServer.h"
#include "VictronBLE.h"
#include "MQTTPublisher.h"

WebConfigServer::WebConfigServer() : server(nullptr), serverStarted(false), filesystemMounted(false), victronBLE(nullptr), mqttPublisher(nullptr) {
}

WebConfigServer::~WebConfigServer() {
    if (server) {
        delete server;
    }
}

void WebConfigServer::setVictronBLE(VictronBLE* vble) {
    victronBLE = vble;
    // Sync any loaded encryption keys to the VictronBLE instance
    syncEncryptionKeys();
}

void WebConfigServer::setMQTTPublisher(MQTTPublisher* mqtt) {
    mqttPublisher = mqtt;
}

void WebConfigServer::begin() {
    Serial.println("Initializing Web Configuration Server...");
    
    // Initialize LittleFS
    filesystemMounted = LittleFS.begin(true);
    if (!filesystemMounted) {
        Serial.println("ERROR: Failed to mount LittleFS!");
        Serial.println("Web interface will not work properly.");
        Serial.println("Please upload filesystem: pio run --target uploadfs");
    } else {
        Serial.println("LittleFS mounted successfully");
    }
    
    // Load configurations
    loadWiFiConfig();
    loadDeviceConfigs();
    
    // Start WiFi
    startWiFi();
    
    // Start web server
    startServer();
}

void WebConfigServer::startWiFi() {
    Serial.println("Starting WiFi...");
    
    if (wifiConfig.apMode) {
        // Access Point mode
        Serial.println("Starting in AP mode...");
        WiFi.mode(WIFI_AP);
        
        // Configure and start SoftAP
        bool apStarted = WiFi.softAP("Victron-Config", wifiConfig.apPassword.c_str());
        
        if (apStarted) {
            Serial.println("SoftAP started successfully");
            // Give the AP a moment to fully initialize
            delay(100);
            
            Serial.print("AP IP address: ");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("ERROR: Failed to start SoftAP!");
        }
    } else {
        // Station mode
        Serial.println("Connecting to WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nWiFi connection failed, falling back to AP mode");
            wifiConfig.apMode = true;
            WiFi.mode(WIFI_AP);
            
            bool apStarted = WiFi.softAP("Victron-Config", wifiConfig.apPassword.c_str());
            
            if (apStarted) {
                Serial.println("Fallback SoftAP started successfully");
                delay(100);
                Serial.print("AP IP address: ");
                Serial.println(WiFi.softAPIP());
            } else {
                Serial.println("ERROR: Failed to start fallback SoftAP!");
            }
        }
    }
}

void WebConfigServer::startServer() {
    if (serverStarted) return;
    
    server = new AsyncWebServer(80);
    
    // Serve main page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });
    
    // Serve monitor page
    server->on("/monitor", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleMonitor(request);
    });
    
    // Serve debug page
    server->on("/debug", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleDebug(request);
    });
    
    // API endpoints
    server->on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetDevices(request);
    });
    
    server->on("/api/devices/live", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLiveData(request);
    });
    
    server->on("/api/debug", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetDebugData(request);
    });
    
    server->on("/api/devices", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleAddDevice(request);
    });
    
    server->on("/api/devices/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleUpdateDevice(request);
    });
    
    server->on("/api/devices/delete", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleDeleteDevice(request);
    });
    
    server->on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWiFiConfig(request);
    });
    
    server->on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetWiFiConfig(request);
    });
    
    server->on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetMQTTConfig(request);
    });
    
    server->on("/api/mqtt", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetMQTTConfig(request);
    });
    
    server->on("/api/buzzer", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetBuzzerConfig(request);
    });
    
    server->on("/api/buzzer", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetBuzzerConfig(request);
    });
    
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleRestart(request);
    });
    
    server->begin();
    serverStarted = true;
    Serial.println("Web server started");
}

void WebConfigServer::handleRoot(AsyncWebServerRequest *request) {
    if (!filesystemMounted) {
        request->send(500, "text/plain", "ERROR: Filesystem not mounted. Please upload filesystem: pio run --target uploadfs");
        return;
    }
    
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
    } else {
        request->send(500, "text/plain", "ERROR: index.html not found in filesystem. Please upload filesystem: pio run --target uploadfs");
    }
}

void WebConfigServer::handleGetDevices(AsyncWebServerRequest *request) {
    String json = "[";
    
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (i > 0) json += ",";
        
        json += "{";
        json += "\"name\":\"" + deviceConfigs[i].name + "\",";
        json += "\"address\":\"" + deviceConfigs[i].address + "\",";
        json += "\"encryptionKey\":\"" + deviceConfigs[i].encryptionKey + "\",";
        json += "\"enabled\":" + String(deviceConfigs[i].enabled ? "true" : "false");
        json += "}";
    }
    
    json += "]";
    request->send(200, "application/json", json);
}

void WebConfigServer::handleGetLiveData(AsyncWebServerRequest *request) {
    if (!victronBLE) {
        request->send(500, "application/json", "{\"error\":\"VictronBLE not initialized\"}");
        return;
    }
    
    String json = "[";
    auto& devices = victronBLE->getDevices();
    bool first = true;
    
    for (auto& pair : devices) {
        if (!first) json += ",";
        first = false;
        
        VictronDeviceData* device = &pair.second;
        json += "{";
        json += "\"name\":\"" + device->name + "\",";
        json += "\"address\":\"" + device->address + "\",";
        json += "\"type\":" + String((int)device->type) + ",";
        json += "\"typeName\":\"";
        
        switch (device->type) {
            case DEVICE_SMART_SHUNT:
                json += "Smart Shunt";
                break;
            case DEVICE_SMART_SOLAR:
                json += "Smart Solar";
                break;
            case DEVICE_BLUE_SMART_CHARGER:
                json += "Blue Smart Charger";
                break;
            case DEVICE_INVERTER:
                json += "Inverter";
                break;
            case DEVICE_DCDC_CONVERTER:
                json += "DC-DC Converter";
                break;
            default:
                json += "Unknown";
                break;
        }
        
        json += "\",";
        json += "\"rssi\":" + String(device->rssi) + ",";
        json += "\"voltage\":" + String(device->voltage, 2) + ",";
        json += "\"current\":" + String(device->current, 3) + ",";
        json += "\"power\":" + String(device->power, 1) + ",";
        json += "\"batterySOC\":" + String(device->batterySOC, 1) + ",";
        json += "\"temperature\":" + String(device->temperature, 1) + ",";
        json += "\"acOutVoltage\":" + String(device->acOutVoltage, 2) + ",";
        json += "\"acOutCurrent\":" + String(device->acOutCurrent, 2) + ",";
        json += "\"acOutPower\":" + String(device->acOutPower, 1) + ",";
        json += "\"inputVoltage\":" + String(device->inputVoltage, 2) + ",";
        json += "\"outputVoltage\":" + String(device->outputVoltage, 2) + ",";
        json += "\"lastUpdate\":" + String(device->lastUpdate) + ",";
        json += "\"dataValid\":" + String(device->dataValid ? "true" : "false") + ",";
        json += "\"hasVoltage\":" + String(device->hasVoltage ? "true" : "false") + ",";
        json += "\"hasCurrent\":" + String(device->hasCurrent ? "true" : "false") + ",";
        json += "\"hasPower\":" + String(device->hasPower ? "true" : "false") + ",";
        json += "\"hasSOC\":" + String(device->hasSOC ? "true" : "false") + ",";
        json += "\"hasTemperature\":" + String(device->hasTemperature ? "true" : "false") + ",";
        json += "\"hasAcOut\":" + String(device->hasAcOut ? "true" : "false") + ",";
        json += "\"hasInputVoltage\":" + String(device->hasInputVoltage ? "true" : "false") + ",";
        json += "\"hasOutputVoltage\":" + String(device->hasOutputVoltage ? "true" : "false");
        json += "}";
    }
    
    json += "]";
    request->send(200, "application/json", json);
}

void WebConfigServer::handleAddDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("name", true) && request->hasParam("address", true)) {
        String name = request->getParam("name", true)->value();
        String address = request->getParam("address", true)->value();
        String key = request->hasParam("encryptionKey", true) ? 
                     request->getParam("encryptionKey", true)->value() : "";
        
        DeviceConfig config(name, address, key, true);
        addDeviceConfig(config);
        
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    }
}

void WebConfigServer::handleUpdateDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("address", true) && request->hasParam("name", true)) {
        String address = request->getParam("address", true)->value();
        String name = request->getParam("name", true)->value();
        String key = request->hasParam("encryptionKey", true) ? 
                     request->getParam("encryptionKey", true)->value() : "";
        bool enabled = request->hasParam("enabled", true) ? 
                       request->getParam("enabled", true)->value() == "true" : true;
        
        DeviceConfig config(name, address, key, enabled);
        updateDeviceConfig(address, config);
        
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    }
}

void WebConfigServer::handleDeleteDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("address", true)) {
        String address = request->getParam("address", true)->value();
        removeDeviceConfig(address);
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing address\"}");
    }
}

void WebConfigServer::handleGetWiFiConfig(AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"ssid\":\"" + wifiConfig.ssid + "\",";
    json += "\"apMode\":" + String(wifiConfig.apMode ? "true" : "false") + ",";
    json += "\"apPassword\":\"" + wifiConfig.apPassword + "\"";
    json += "}";
    
    request->send(200, "application/json", json);
}

void WebConfigServer::handleSetWiFiConfig(AsyncWebServerRequest *request) {
    bool changed = false;
    
    if (request->hasParam("ssid", true)) {
        wifiConfig.ssid = request->getParam("ssid", true)->value();
        changed = true;
    }
    
    if (request->hasParam("password", true)) {
        wifiConfig.password = request->getParam("password", true)->value();
        changed = true;
    }
    
    if (request->hasParam("apMode", true)) {
        wifiConfig.apMode = request->getParam("apMode", true)->value() == "true";
        changed = true;
    }
    
    if (request->hasParam("apPassword", true)) {
        wifiConfig.apPassword = request->getParam("apPassword", true)->value();
        changed = true;
    }
    
    if (changed) {
        saveWiFiConfig();
        request->send(200, "application/json", 
            "{\"success\":true,\"message\":\"Restart required for changes to take effect\"}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"No parameters provided\"}");
    }
}

void WebConfigServer::handleMonitor(AsyncWebServerRequest *request) {
    if (!filesystemMounted) {
        request->send(500, "text/plain", "ERROR: Filesystem not mounted. Please upload filesystem: pio run --target uploadfs");
        return;
    }
    
    if (LittleFS.exists("/monitor.html")) {
        request->send(LittleFS, "/monitor.html", "text/html");
    } else {
        request->send(500, "text/plain", "ERROR: monitor.html not found in filesystem. Please upload filesystem: pio run --target uploadfs");
    }
}

void WebConfigServer::handleDebug(AsyncWebServerRequest *request) {
    if (!filesystemMounted) {
        request->send(500, "text/plain", "ERROR: Filesystem not mounted. Please upload filesystem: pio run --target uploadfs");
        return;
    }
    
    if (LittleFS.exists("/debug.html")) {
        request->send(LittleFS, "/debug.html", "text/html");
    } else {
        request->send(500, "text/plain", "ERROR: debug.html not found in filesystem. Please upload filesystem: pio run --target uploadfs");
    }
}

void WebConfigServer::handleGetDebugData(AsyncWebServerRequest *request) {
    if (!victronBLE) {
        request->send(500, "application/json", "{\"error\":\"VictronBLE not initialized\"}");
        return;
    }
    
    String json = "{\"devices\":[";
    auto& devices = victronBLE->getDevices();
    bool first = true;
    
    for (auto& pair : devices) {
        if (!first) json += ",";
        first = false;
        
        VictronDeviceData* device = &pair.second;
        json += "{";
        json += "\"name\":\"" + device->name + "\",";
        json += "\"address\":\"" + device->address + "\",";
        json += "\"type\":" + String((int)device->type) + ",";
        json += "\"typeName\":\"";
        
        switch (device->type) {
            case DEVICE_SMART_SHUNT:
                json += "Smart Shunt";
                break;
            case DEVICE_SMART_SOLAR:
                json += "Smart Solar";
                break;
            case DEVICE_BLUE_SMART_CHARGER:
                json += "Blue Smart Charger";
                break;
            case DEVICE_INVERTER:
                json += "Inverter";
                break;
            case DEVICE_DCDC_CONVERTER:
                json += "DC-DC Converter";
                break;
            default:
                json += "Unknown";
                break;
        }
        
        json += "\",";
        json += "\"rssi\":" + String(device->rssi) + ",";
        json += "\"dataValid\":" + String(device->dataValid ? "true" : "false") + ",";
        json += "\"encrypted\":" + String(device->encrypted ? "true" : "false") + ",";
        json += "\"errorMessage\":\"" + device->errorMessage + "\",";
        String mfgIdHex = String(device->manufacturerId, HEX);
        mfgIdHex.toUpperCase();
        json += "\"manufacturerId\":\"0x" + mfgIdHex + "\",";
        String modelIdHex = String(device->modelId, HEX);
        modelIdHex.toUpperCase();
        json += "\"modelId\":\"0x" + modelIdHex + "\",";
        json += "\"rawDataLength\":" + String(device->rawDataLength) + ",";
        json += "\"lastUpdate\":" + String(millis() - device->lastUpdate) + ",";
        
        // Raw manufacturer data as byte array
        json += "\"rawData\":[";
        for (size_t i = 0; i < device->rawDataLength; i++) {
            if (i > 0) json += ",";
            json += String(device->rawManufacturerData[i]);
        }
        json += "],";
        
        // Parsed records
        json += "\"records\":[";
        for (size_t i = 0; i < device->parsedRecords.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"type\":" + String(device->parsedRecords[i].type) + ",";
            json += "\"length\":" + String(device->parsedRecords[i].length) + ",";
            json += "\"data\":[";
            for (size_t j = 0; j < device->parsedRecords[i].length; j++) {
                if (j > 0) json += ",";
                json += String(device->parsedRecords[i].data[j]);
            }
            json += "]";
            json += "}";
        }
        json += "]";
        
        json += "}";
    }
    
    json += "]}";
    request->send(200, "application/json", json);
}

void WebConfigServer::handleGetMQTTConfig(AsyncWebServerRequest *request) {
    if (!mqttPublisher) {
        request->send(500, "application/json", "{\"error\":\"MQTT not initialized\"}");
        return;
    }
    
    MQTTConfig& config = mqttPublisher->getConfig();
    String json = "{";
    json += "\"broker\":\"" + config.broker + "\",";
    json += "\"port\":" + String(config.port) + ",";
    json += "\"username\":\"" + config.username + "\",";
    json += "\"baseTopic\":\"" + config.baseTopic + "\",";
    json += "\"enabled\":" + String(config.enabled ? "true" : "false") + ",";
    json += "\"homeAssistant\":" + String(config.homeAssistant ? "true" : "false") + ",";
    json += "\"publishInterval\":" + String(config.publishInterval) + ",";
    json += "\"connected\":" + String(mqttPublisher->isConnected() ? "true" : "false");
    json += "}";
    
    request->send(200, "application/json", json);
}

void WebConfigServer::handleSetMQTTConfig(AsyncWebServerRequest *request) {
    if (!mqttPublisher) {
        request->send(500, "application/json", "{\"error\":\"MQTT not initialized\"}");
        return;
    }
    
    MQTTConfig config = mqttPublisher->getConfig();
    bool changed = false;
    
    if (request->hasParam("broker", true)) {
        config.broker = request->getParam("broker", true)->value();
        changed = true;
    }
    
    if (request->hasParam("port", true)) {
        config.port = request->getParam("port", true)->value().toInt();
        changed = true;
    }
    
    if (request->hasParam("username", true)) {
        config.username = request->getParam("username", true)->value();
        changed = true;
    }
    
    if (request->hasParam("password", true)) {
        String pwd = request->getParam("password", true)->value();
        if (!pwd.isEmpty()) {
            config.password = pwd;
            changed = true;
        }
    }
    
    if (request->hasParam("baseTopic", true)) {
        config.baseTopic = request->getParam("baseTopic", true)->value();
        changed = true;
    }
    
    if (request->hasParam("enabled", true)) {
        config.enabled = request->getParam("enabled", true)->value() == "true";
        changed = true;
    }
    
    if (request->hasParam("homeAssistant", true)) {
        config.homeAssistant = request->getParam("homeAssistant", true)->value() == "true";
        changed = true;
    }
    
    if (request->hasParam("publishInterval", true)) {
        config.publishInterval = request->getParam("publishInterval", true)->value().toInt();
        changed = true;
    }
    
    if (changed) {
        mqttPublisher->setConfig(config);
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"No parameters provided\"}");
    }
}

void WebConfigServer::handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
    // Delay is intentional here to allow the HTTP response to complete
    // before restart. This is acceptable for a restart endpoint.
    delay(1000);
    ESP.restart();
}


// Configuration persistence methods
void WebConfigServer::saveWiFiConfig() {
    preferences.begin("victron-wifi", false);
    preferences.putString("ssid", wifiConfig.ssid);
    preferences.putString("password", wifiConfig.password);
    preferences.putBool("apMode", wifiConfig.apMode);
    preferences.putString("apPassword", wifiConfig.apPassword);
    preferences.end();
    Serial.println("WiFi config saved");
}

void WebConfigServer::loadWiFiConfig() {
    preferences.begin("victron-wifi", true);
    wifiConfig.ssid = preferences.getString("ssid", "");
    wifiConfig.password = preferences.getString("password", "");
    wifiConfig.apMode = preferences.getBool("apMode", true);
    wifiConfig.apPassword = preferences.getString("apPassword", "victron123");
    preferences.end();
    Serial.println("WiFi config loaded");
}

void WebConfigServer::saveDeviceConfigs() {
    preferences.begin("victron-dev", false);
    preferences.putInt("count", deviceConfigs.size());
    
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        String prefix = "dev" + String(i) + "_";
        preferences.putString((prefix + "name").c_str(), deviceConfigs[i].name);
        preferences.putString((prefix + "addr").c_str(), deviceConfigs[i].address);
        preferences.putString((prefix + "key").c_str(), deviceConfigs[i].encryptionKey);
        preferences.putBool((prefix + "en").c_str(), deviceConfigs[i].enabled);
    }
    
    preferences.end();
    Serial.println("Device configs saved");
}

void WebConfigServer::loadDeviceConfigs() {
    preferences.begin("victron-dev", true);
    int count = preferences.getInt("count", 0);
    
    deviceConfigs.clear();
    for (int i = 0; i < count; i++) {
        String prefix = "dev" + String(i) + "_";
        DeviceConfig config;
        config.name = preferences.getString((prefix + "name").c_str(), "");
        config.address = preferences.getString((prefix + "addr").c_str(), "");
        config.encryptionKey = preferences.getString((prefix + "key").c_str(), "");
        config.enabled = preferences.getBool((prefix + "en").c_str(), true);
        
        if (!config.address.isEmpty()) {
            deviceConfigs.push_back(config);
        }
    }
    
    preferences.end();
    Serial.printf("Loaded %d device configs\n", deviceConfigs.size());
    
    // Sync encryption keys to VictronBLE instance if available
    syncEncryptionKeys();
}

// Device configuration access methods
std::vector<DeviceConfig>& WebConfigServer::getDeviceConfigs() {
    return deviceConfigs;
}

DeviceConfig* WebConfigServer::getDeviceConfig(const String& address) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            return &deviceConfigs[i];
        }
    }
    return nullptr;
}

void WebConfigServer::addDeviceConfig(const DeviceConfig& config) {
    // Check if device already exists
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(config.address)) {
            // Update existing
            deviceConfigs[i] = config;
            saveDeviceConfigs();
            // Sync encryption key to VictronBLE
            if (victronBLE && !config.encryptionKey.isEmpty()) {
                victronBLE->setEncryptionKey(config.address, config.encryptionKey);
            }
            return;
        }
    }
    
    // Add new
    deviceConfigs.push_back(config);
    saveDeviceConfigs();
    // Sync encryption key to VictronBLE
    if (victronBLE && !config.encryptionKey.isEmpty()) {
        victronBLE->setEncryptionKey(config.address, config.encryptionKey);
    }
}

void WebConfigServer::updateDeviceConfig(const String& address, const DeviceConfig& config) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            deviceConfigs[i] = config;
            saveDeviceConfigs();
            // Sync encryption key to VictronBLE
            if (victronBLE && !config.encryptionKey.isEmpty()) {
                victronBLE->setEncryptionKey(config.address, config.encryptionKey);
            }
            return;
        }
    }
}

void WebConfigServer::removeDeviceConfig(const String& address) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            deviceConfigs.erase(deviceConfigs.begin() + i);
            saveDeviceConfigs();
            // Note: We don't remove the encryption key from VictronBLE as it's harmless to keep it
            return;
        }
    }
}

void WebConfigServer::syncEncryptionKeys() {
    if (!victronBLE) {
        return;  // VictronBLE not set yet, will sync when it's set
    }
    
    // Sync all encryption keys from device configs to VictronBLE
    for (const auto& config : deviceConfigs) {
        if (!config.encryptionKey.isEmpty()) {
            victronBLE->setEncryptionKey(config.address, config.encryptionKey);
            Serial.printf("Synced encryption key for device %s\n", config.address.c_str());
        }
    }
}

// WiFi info methods
String WebConfigServer::getIPAddress() {
    if (wifiConfig.apMode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

bool WebConfigServer::isWiFiConnected() {
    return wifiConfig.apMode || WiFi.status() == WL_CONNECTED;
}

bool WebConfigServer::isAPMode() {
    return wifiConfig.apMode;
}

// External declarations for buzzer configuration (defined in main.cpp)
extern bool buzzerEnabled;
extern float buzzerThreshold;
extern void saveBuzzerConfig();

void WebConfigServer::handleGetBuzzerConfig(AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"enabled\":" + String(buzzerEnabled ? "true" : "false") + ",";
    json += "\"threshold\":" + String(buzzerThreshold, 1);
    json += "}";
    
    request->send(200, "application/json", json);
}

void WebConfigServer::handleSetBuzzerConfig(AsyncWebServerRequest *request) {
    if (!request->hasParam("enabled", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing enabled parameter\"}");
        return;
    }
    
    if (!request->hasParam("threshold", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing threshold parameter\"}");
        return;
    }
    
    String enabledStr = request->getParam("enabled", true)->value();
    String thresholdStr = request->getParam("threshold", true)->value();
    
    // Validate threshold string is numeric
    bool validNumber = true;
    if (thresholdStr.length() == 0) {
        validNumber = false;
    } else {
        for (size_t i = 0; i < thresholdStr.length(); i++) {
            char c = thresholdStr[i];
            if (c != '.' && c != '-' && (c < '0' || c > '9')) {
                validNumber = false;
                break;
            }
        }
    }
    
    if (!validNumber) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid threshold value\"}");
        return;
    }
    
    float newThreshold = thresholdStr.toFloat();
    
    // Validate threshold range
    if (newThreshold < 0 || newThreshold > 100) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Threshold must be between 0 and 100\"}");
        return;
    }
    
    buzzerEnabled = (enabledStr == "true");
    buzzerThreshold = newThreshold;
    
    saveBuzzerConfig();
    
    request->send(200, "application/json", "{\"success\":true}");
}
