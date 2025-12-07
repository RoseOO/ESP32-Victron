#include "MQTTPublisher.h"

MQTTPublisher::MQTTPublisher() : 
    mqttClient(wifiClient),
    victronBLE(nullptr),
    lastPublishTime(0),
    lastReconnectAttempt(0),
    discoveryPublished(false) {
}

void MQTTPublisher::begin(VictronBLE* vble) {
    victronBLE = vble;
    loadConfig();
    
    if (config.enabled && !config.broker.isEmpty()) {
        mqttClient.setServer(config.broker.c_str(), config.port);
        Serial.printf("MQTT configured: %s:%d\n", config.broker.c_str(), config.port);
    }
}

void MQTTPublisher::loop() {
    if (!config.enabled || !victronBLE) {
        return;
    }
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    // Reconnect if needed
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            reconnect();
        }
        return;
    }
    
    mqttClient.loop();
    
    // Publish device data at configured interval
    unsigned long now = millis();
    if (now - lastPublishTime > (config.publishInterval * 1000)) {
        lastPublishTime = now;
        publishAll();
    }
}

void MQTTPublisher::reconnect() {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP32-Victron-" + String(ESP.getEfuseMac(), HEX);
    
    bool connected;
    if (config.username.isEmpty()) {
        connected = mqttClient.connect(clientId.c_str());
    } else {
        connected = mqttClient.connect(
            clientId.c_str(), 
            config.username.c_str(), 
            config.password.c_str()
        );
    }
    
    if (connected) {
        Serial.println(" connected!");
        discoveryPublished = false;  // Re-publish discovery on reconnect
    } else {
        Serial.print(" failed, rc=");
        Serial.println(mqttClient.state());
    }
}

void MQTTPublisher::publishAll() {
    if (!mqttClient.connected() || !victronBLE) {
        return;
    }
    
    auto& devices = victronBLE->getDevices();
    
    for (auto& pair : devices) {
        VictronDeviceData* device = &pair.second;
        
        // Publish Home Assistant discovery if enabled and not yet published
        if (config.homeAssistant && !discoveryPublished) {
            publishDiscovery(device);
        }
        
        // Publish device data
        publishDeviceData(device);
    }
    
    discoveryPublished = true;
}

void MQTTPublisher::publishDiscovery(VictronDeviceData* device) {
    if (!config.homeAssistant) return;
    
    String deviceId = sanitizeTopicName(device->address);
    String deviceName = device->name;
    if (deviceName.isEmpty()) {
        deviceName = device->address;
    }
    
    // Publish sensor discoveries for available fields
    struct SensorConfig {
        const char* name;
        const char* unit;
        const char* deviceClass;
        const char* stateClass;
        bool available;
    };
    
    SensorConfig sensors[] = {
        {"Voltage", "V", "voltage", "measurement", device->hasVoltage},
        {"Current", "A", "current", "measurement", device->hasCurrent},
        {"Power", "W", "power", "measurement", device->hasPower},
        {"Battery SOC", "%", "battery", "measurement", device->hasSOC},
        {"Temperature", "°C", "temperature", "measurement", device->hasTemperature},
        {"Consumed Ah", "Ah", "energy", "total_increasing", device->consumedAh > 0},
        {"Time to Go", "min", "", "measurement", device->timeToGo > 0 && device->timeToGo < 65535},
        {"Aux Voltage", "V", "voltage", "measurement", device->auxMode == 0 && device->auxVoltage > 0},
        {"Mid Voltage", "V", "voltage", "measurement", device->auxMode == 1 && device->midVoltage > 0},
        {"Yield Today", "kWh", "energy", "total_increasing", device->yieldToday > 0},
        {"PV Power", "W", "power", "measurement", device->pvPower > 0},
        {"Load Current", "A", "current", "measurement", device->loadCurrent > 0},
        {"Device State", "", "", "measurement", device->deviceState >= 0},
        {"Charger Error", "", "", "measurement", device->chargerError > 0},
        {"Alarm State", "", "", "measurement", device->alarmState > 0},
        {"AC Output Voltage", "V", "voltage", "measurement", device->hasAcOut},
        {"AC Output Power", "W", "power", "measurement", device->hasAcOut},
        {"Input Voltage", "V", "voltage", "measurement", device->hasInputVoltage},
        {"Output Voltage", "V", "voltage", "measurement", device->hasOutputVoltage},
        {"RSSI", "dBm", "signal_strength", "measurement", true}
    };
    
    for (const auto& sensor : sensors) {
        if (!sensor.available) continue;
        
        String sensorId = sanitizeTopicName(String(sensor.name));
        String discoveryTopic = "homeassistant/sensor/" + deviceId + "_" + sensorId + "/config";
        String stateTopic = config.baseTopic + "/" + deviceId + "/" + sensorId;
        
        String payload = "{";
        payload += "\"name\":\"" + deviceName + " " + String(sensor.name) + "\",";
        payload += "\"unique_id\":\"" + deviceId + "_" + sensorId + "\",";
        payload += "\"state_topic\":\"" + stateTopic + "\",";
        payload += "\"unit_of_measurement\":\"" + String(sensor.unit) + "\",";
        payload += "\"device_class\":\"" + String(sensor.deviceClass) + "\",";
        payload += "\"state_class\":\"" + String(sensor.stateClass) + "\",";
        payload += "\"device\":{";
        payload += "\"identifiers\":[\"" + deviceId + "\"],";
        payload += "\"name\":\"" + deviceName + "\",";
        payload += "\"manufacturer\":\"Victron Energy\",";
        payload += "\"model\":\"";
        
        switch (device->type) {
            case DEVICE_SMART_SHUNT:
                payload += "Smart Shunt";
                break;
            case DEVICE_SMART_SOLAR:
                payload += "Smart Solar";
                break;
            case DEVICE_BLUE_SMART_CHARGER:
                payload += "Blue Smart Charger";
                break;
            case DEVICE_INVERTER:
                payload += "Inverter";
                break;
            case DEVICE_DCDC_CONVERTER:
                payload += "DC-DC Converter";
                break;
            default:
                payload += "Unknown";
                break;
        }
        
        payload += "\"}}";
        
        mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);
        // Note: Discovery messages are sent once on connect, not frequently
        // MQTT client handles queueing internally, no delay needed
    }
}

void MQTTPublisher::publishDeviceData(VictronDeviceData* device) {
    String deviceId = sanitizeTopicName(device->address);
    String basePath = config.baseTopic + "/" + deviceId;
    
    // Publish available data - use same topic names as discovery
    if (device->hasVoltage) {
        String topic = basePath + "/" + sanitizeTopicName("Voltage");
        mqttClient.publish(topic.c_str(), String(device->voltage, 2).c_str());
    }
    
    if (device->hasCurrent) {
        String topic = basePath + "/" + sanitizeTopicName("Current");
        mqttClient.publish(topic.c_str(), String(device->current, 3).c_str());
    }
    
    if (device->hasPower) {
        String topic = basePath + "/" + sanitizeTopicName("Power");
        mqttClient.publish(topic.c_str(), String(device->power, 1).c_str());
    }
    
    if (device->hasSOC && device->batterySOC >= 0) {
        String topic = basePath + "/" + sanitizeTopicName("Battery SOC");
        mqttClient.publish(topic.c_str(), String(device->batterySOC, 1).c_str());
    }
    
    if (device->hasTemperature && device->temperature > -200) {
        String topic = basePath + "/" + sanitizeTopicName("Temperature");
        mqttClient.publish(topic.c_str(), String(device->temperature, 1).c_str());
    }
    
    // SmartShunt specific fields
    if (device->consumedAh > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Consumed Ah");
        mqttClient.publish(topic.c_str(), String(device->consumedAh, 1).c_str());
    }
    
    if (device->timeToGo > 0 && device->timeToGo < 65535) {
        String topic = basePath + "/" + sanitizeTopicName("Time to Go");
        mqttClient.publish(topic.c_str(), String(device->timeToGo).c_str());
    }
    
    if (device->auxMode == 0 && device->auxVoltage > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Aux Voltage");
        mqttClient.publish(topic.c_str(), String(device->auxVoltage, 2).c_str());
    }
    
    if (device->auxMode == 1 && device->midVoltage > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Mid Voltage");
        mqttClient.publish(topic.c_str(), String(device->midVoltage, 2).c_str());
    }
    
    // Solar Controller specific fields
    if (device->yieldToday > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Yield Today");
        mqttClient.publish(topic.c_str(), String(device->yieldToday, 2).c_str());
    }
    
    if (device->pvPower > 0) {
        String topic = basePath + "/" + sanitizeTopicName("PV Power");
        mqttClient.publish(topic.c_str(), String(device->pvPower, 0).c_str());
    }
    
    if (device->loadCurrent > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Load Current");
        mqttClient.publish(topic.c_str(), String(device->loadCurrent, 2).c_str());
    }
    
    if (device->deviceState >= 0) {
        String topic = basePath + "/" + sanitizeTopicName("Device State");
        mqttClient.publish(topic.c_str(), String(device->deviceState).c_str());
    }
    
    if (device->chargerError > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Charger Error");
        mqttClient.publish(topic.c_str(), String(device->chargerError).c_str());
    }
    
    if (device->alarmState > 0) {
        String topic = basePath + "/" + sanitizeTopicName("Alarm State");
        mqttClient.publish(topic.c_str(), String(device->alarmState).c_str());
    }
    
    // Inverter specific fields
    if (device->hasAcOut) {
        String topic1 = basePath + "/" + sanitizeTopicName("AC Output Voltage");
        mqttClient.publish(topic1.c_str(), String(device->acOutVoltage, 2).c_str());
        String topic2 = basePath + "/" + sanitizeTopicName("AC Output Power");
        mqttClient.publish(topic2.c_str(), String(device->acOutPower, 1).c_str());
    }
    
    if (device->hasInputVoltage) {
        String topic = basePath + "/" + sanitizeTopicName("Input Voltage");
        mqttClient.publish(topic.c_str(), String(device->inputVoltage, 2).c_str());
    }
    
    if (device->hasOutputVoltage) {
        String topic = basePath + "/" + sanitizeTopicName("Output Voltage");
        mqttClient.publish(topic.c_str(), String(device->outputVoltage, 2).c_str());
    }
    
    // Always publish RSSI
    String rssiTopic = basePath + "/" + sanitizeTopicName("RSSI");
    mqttClient.publish(rssiTopic.c_str(), String(device->rssi).c_str());
    
    Serial.printf("Published MQTT data for %s\n", device->name.c_str());
}

String MQTTPublisher::sanitizeTopicName(const String& name) {
    String result = name;
    result.replace(":", "_");
    result.replace(" ", "_");
    result.replace("-", "_");
    result.toLowerCase();
    return result;
}

void MQTTPublisher::loadConfig() {
    preferences.begin("mqtt-config", true);
    config.broker = preferences.getString("broker", "");
    config.port = preferences.getUShort("port", 1883);
    config.username = preferences.getString("username", "");
    config.password = preferences.getString("password", "");
    config.baseTopic = preferences.getString("baseTopic", "victron");
    config.enabled = preferences.getBool("enabled", false);
    config.homeAssistant = preferences.getBool("homeAssist", true);
    config.publishInterval = preferences.getUShort("interval", 30);
    preferences.end();
    
    Serial.println("MQTT config loaded");
}

void MQTTPublisher::saveConfig() {
    preferences.begin("mqtt-config", false);
    preferences.putString("broker", config.broker);
    preferences.putUShort("port", config.port);
    preferences.putString("username", config.username);
    preferences.putString("password", config.password);
    preferences.putString("baseTopic", config.baseTopic);
    preferences.putBool("enabled", config.enabled);
    preferences.putBool("homeAssist", config.homeAssistant);
    preferences.putUShort("interval", config.publishInterval);
    preferences.end();
    
    Serial.println("MQTT config saved");
}

MQTTConfig& MQTTPublisher::getConfig() {
    return config;
}

void MQTTPublisher::setConfig(const MQTTConfig& cfg) {
    config = cfg;
    saveConfig();
    
    // Reconfigure MQTT client
    if (config.enabled && !config.broker.isEmpty()) {
        disconnect();
        mqttClient.setServer(config.broker.c_str(), config.port);
        discoveryPublished = false;
    }
}

bool MQTTPublisher::isConnected() {
    return mqttClient.connected();
}

void MQTTPublisher::connect() {
    if (!config.enabled) return;
    reconnect();
}

void MQTTPublisher::disconnect() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }
}
