#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "VictronBLE.h"

// MQTT Configuration structure
struct MQTTConfig {
    String broker;              // MQTT broker address
    uint16_t port;              // MQTT broker port
    String username;            // MQTT username (optional)
    String password;            // MQTT password (optional)
    String baseTopic;           // Base topic for all devices
    bool enabled;               // Enable/disable MQTT
    bool homeAssistant;         // Enable Home Assistant auto-discovery
    uint16_t publishInterval;   // Publish interval in seconds
    
    MQTTConfig() : 
        broker(""), 
        port(1883), 
        username(""), 
        password(""), 
        baseTopic("victron"),
        enabled(false),
        homeAssistant(true),
        publishInterval(30) {}
};

class MQTTPublisher {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    Preferences preferences;
    MQTTConfig config;
    VictronBLE* victronBLE;
    
    unsigned long lastPublishTime;
    unsigned long lastReconnectAttempt;
    bool discoveryPublished;
    
    void reconnect();
    void publishDiscovery(VictronDeviceData* device);
    void publishDeviceData(VictronDeviceData* device);
    String sanitizeTopicName(const String& name);
    String getDeviceClass(VictronRecordType type);
    
public:
    MQTTPublisher();
    
    void begin(VictronBLE* vble);
    void loop();
    
    // Configuration management
    void loadConfig();
    void saveConfig();
    MQTTConfig& getConfig();
    void setConfig(const MQTTConfig& cfg);
    
    // MQTT operations
    bool isConnected();
    void connect();
    void disconnect();
    void publishAll();
};

#endif // MQTT_PUBLISHER_H
