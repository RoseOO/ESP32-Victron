#ifndef WEB_CONFIG_SERVER_H
#define WEB_CONFIG_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <vector>

// Structure to store device configuration
struct DeviceConfig {
    String name;
    String address;        // BLE MAC address
    String encryptionKey;  // 32-character hex string (128-bit key)
    bool enabled;
    
    DeviceConfig() : enabled(true) {}
    
    DeviceConfig(const String& n, const String& addr, const String& key = "", bool en = true)
        : name(n), address(addr), encryptionKey(key), enabled(en) {}
};

// WiFi configuration structure
struct WiFiConfig {
    String ssid;
    String password;
    bool apMode;           // true = Access Point mode, false = Station mode
    String apPassword;     // Password for AP mode
    
    WiFiConfig() : apMode(true), apPassword("victron123") {}
};

class WebConfigServer {
private:
    AsyncWebServer* server;
    Preferences preferences;
    std::vector<DeviceConfig> deviceConfigs;
    WiFiConfig wifiConfig;
    bool serverStarted;
    bool filesystemMounted;
    
    // Configuration persistence
    void saveWiFiConfig();
    void loadWiFiConfig();
    void saveDeviceConfigs();
    void loadDeviceConfigs();
    
    // Request handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleMonitor(AsyncWebServerRequest *request);
    void handleDebug(AsyncWebServerRequest *request);
    void handleGetDevices(AsyncWebServerRequest *request);
    void handleGetLiveData(AsyncWebServerRequest *request);
    void handleGetDebugData(AsyncWebServerRequest *request);
    void handleAddDevice(AsyncWebServerRequest *request);
    void handleUpdateDevice(AsyncWebServerRequest *request);
    void handleDeleteDevice(AsyncWebServerRequest *request);
    void handleGetWiFiConfig(AsyncWebServerRequest *request);
    void handleSetWiFiConfig(AsyncWebServerRequest *request);
    void handleGetMQTTConfig(AsyncWebServerRequest *request);
    void handleSetMQTTConfig(AsyncWebServerRequest *request);
    void handleGetBuzzerConfig(AsyncWebServerRequest *request);
    void handleSetBuzzerConfig(AsyncWebServerRequest *request);
    void handleRestart(AsyncWebServerRequest *request);
    
    // Pointer to VictronBLE instance for live data
    class VictronBLE* victronBLE;
    
    // Pointer to MQTTPublisher instance for MQTT config
    class MQTTPublisher* mqttPublisher;
    
public:
    WebConfigServer();
    ~WebConfigServer();
    
    void begin();
    void startWiFi();
    void startServer();
    
    // Set VictronBLE instance for live data
    void setVictronBLE(class VictronBLE* vble);
    
    // Set MQTTPublisher instance for MQTT config
    void setMQTTPublisher(class MQTTPublisher* mqtt);
    
    // Device configuration access
    std::vector<DeviceConfig>& getDeviceConfigs();
    DeviceConfig* getDeviceConfig(const String& address);
    void addDeviceConfig(const DeviceConfig& config);
    void updateDeviceConfig(const String& address, const DeviceConfig& config);
    void removeDeviceConfig(const String& address);
    
    // WiFi info
    String getIPAddress();
    bool isWiFiConnected();
    bool isAPMode();
};

#endif // WEB_CONFIG_SERVER_H
