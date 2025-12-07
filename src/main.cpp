#include <M5StickCPlus2.h>
#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include "NoDebug.h"

// Add the project headers that define the classes/types used below.
// Adjust filenames if your headers use different names/paths.
#include "VictronBLE.h"
#include "WebConfigServer.h"
#include "MQTTPublisher.h"

// change globals to pointers to avoid constructor-side effects
VictronBLE *victron = nullptr;
WebConfigServer *webServer = nullptr;
MQTTPublisher *mqttPublisher = nullptr;

std::vector<String> deviceAddresses;
int currentDeviceIndex = 0;
unsigned long lastScanTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonPressTime = 0;  // For debouncing
const unsigned long SCAN_INTERVAL = 30000;  // Scan every 30 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;  // Update display every second
const unsigned long BUTTON_DEBOUNCE = 500;  // Debounce period in ms
const unsigned long LONG_PRESS_DURATION = 1000;  // Long press duration in ms

bool scanning = false;
bool webConfigMode = false;  // Toggle between normal mode and web config display

// Buzzer alarm configuration
Preferences buzzerPreferences;
bool buzzerEnabled = true;
float buzzerThreshold = 10.0;  // Default 10% battery SOC
bool buzzerAlarmActive = false;
unsigned long lastBuzzerCheck = 0;
unsigned long lastBuzzerBeep = 0;
const unsigned long BUZZER_CHECK_INTERVAL = 5000;  // Check every 5 seconds
const unsigned long BUZZER_BEEP_INTERVAL = 200;  // Beep duration/interval in ms
const int BUZZER_FREQUENCY = 2000;  // Buzzer frequency in Hz
int buzzerBeepCount = 0;
bool longPressHandled = false;  // For button long press detection

// Data retention configuration
Preferences dataPreferences;
bool retainLastData = true;  // Default to retaining last good data

// Forward declarations
void updateDeviceList();
void drawDisplay();
void loadBuzzerConfig();
void saveBuzzerConfig();
void loadDataRetentionConfig();
void saveDataRetentionConfig();
void checkBatteryAlarm();
void handleBuzzerBeep();

void loadBuzzerConfig() {
    buzzerPreferences.begin("buzzer", true);  // read-only
    buzzerEnabled = buzzerPreferences.getBool("enabled", true);
    buzzerThreshold = buzzerPreferences.getFloat("threshold", 10.0);
    buzzerPreferences.end();
    Serial.printf("Buzzer config loaded: enabled=%d, threshold=%.1f%%\n", buzzerEnabled, buzzerThreshold);
}

void saveBuzzerConfig() {
    buzzerPreferences.begin("buzzer", false);  // read-write
    buzzerPreferences.putBool("enabled", buzzerEnabled);
    buzzerPreferences.putFloat("threshold", buzzerThreshold);
    buzzerPreferences.end();
    Serial.printf("Buzzer config saved: enabled=%d, threshold=%.1f%%\n", buzzerEnabled, buzzerThreshold);
}

void loadDataRetentionConfig() {
    dataPreferences.begin("victron-data", true);  // read-only
    retainLastData = dataPreferences.getBool("retainLast", true);
    dataPreferences.end();
    Serial.printf("Data retention config loaded: retainLastData=%d\n", retainLastData);
}

void saveDataRetentionConfig() {
    dataPreferences.begin("victron-data", false);  // read-write
    dataPreferences.putBool("retainLast", retainLastData);
    dataPreferences.end();
    Serial.printf("Data retention config saved: retainLastData=%d\n", retainLastData);
}

void checkBatteryAlarm() {
    if (!buzzerEnabled) {
        buzzerAlarmActive = false;
        buzzerBeepCount = 0;
        return;
    }
    
    // Check all devices with battery SOC data
    bool alarmCondition = false;
    for (const auto& addr : deviceAddresses) {
        VictronDeviceData* device = victron->getDevice(addr);
        if (device && device->hasSOC && device->dataValid) {
            if (device->batterySOC < buzzerThreshold && device->batterySOC >= 0) {
                alarmCondition = true;
                Serial.printf("Battery alarm triggered: %s at %.1f%% (threshold: %.1f%%)\n", 
                    device->name.c_str(), device->batterySOC, buzzerThreshold);
                break;
            }
        }
    }
    
    // Trigger alarm if condition met and not already active
    if (alarmCondition && !buzzerAlarmActive) {
        buzzerAlarmActive = true;
        buzzerBeepCount = 0;  // Start beep sequence
        Serial.println("Battery alarm activated");
    } else if (!alarmCondition) {
        buzzerAlarmActive = false;
        buzzerBeepCount = 0;
    }
}

void handleBuzzerBeep() {
    if (!buzzerAlarmActive || buzzerBeepCount >= 6) {
        return;  // Not active or already completed 3 beeps (6 states: on/off x 3)
    }
    
    unsigned long currentTime = millis();
    
    // Non-blocking beep pattern: 200ms on, 100ms off, repeat 3 times
    if (currentTime - lastBuzzerBeep >= BUZZER_BEEP_INTERVAL) {
        if (buzzerBeepCount % 2 == 0) {
            // Even count: start beep
            M5.Speaker.tone(BUZZER_FREQUENCY, BUZZER_BEEP_INTERVAL);
            Serial.printf("Beep %d/3\n", (buzzerBeepCount / 2) + 1);
        }
        buzzerBeepCount++;
        lastBuzzerBeep = currentTime;
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("STARTUP: Serial ready");

    Serial.println("STARTUP: calling M5.begin()");
    M5.begin();
    Serial.println("STARTUP: M5.begin() returned");
    
    // Load buzzer configuration
    Serial.println("STARTUP: loading buzzer config");
    loadBuzzerConfig();
    
    // Load data retention configuration
    Serial.println("STARTUP: loading data retention config");
    loadDataRetentionConfig();

    // instantiate objects (no heavy init in constructors)
    Serial.println("STARTUP: new VictronBLE/WebConfigServer/MQTTPublisher");
    victron = new VictronBLE();
    webServer = new WebConfigServer();
    mqttPublisher = new MQTTPublisher();
    Serial.println("STARTUP: allocations done");

    // Basic display sanity test
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Victron Monitor");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 40);
    M5.Lcd.println("Startup OK");
    delay(500);

    // Try enabling Victron BLE first (needed by other components)
    Serial.println("STARTUP: attempting victron->begin()");
    victron->begin();
    Serial.println("STARTUP: victron->begin() returned");
    
    // Apply data retention setting to VictronBLE
    victron->setRetainLastData(retainLastData);
    
    // Initialize MQTT publisher with VictronBLE reference
    Serial.println("STARTUP: attempting mqttPublisher->begin()");
    mqttPublisher->begin(victron);
    Serial.println("STARTUP: mqttPublisher->begin() returned");
    
    // Initialize web server with references to other components
    Serial.println("STARTUP: setting up webServer references");
    webServer->setVictronBLE(victron);
    webServer->setMQTTPublisher(mqttPublisher);
    
    // Initialize web server (WiFi + HTTP server)
    Serial.println("STARTUP: attempting webServer->begin()");
    webServer->begin();
    Serial.println("STARTUP: webServer->begin() returned");

    // Initialize ArduinoOTA for over-the-air firmware updates
    Serial.println("STARTUP: initializing ArduinoOTA");
    ArduinoOTA.setHostname("ESP32-Victron");
    ArduinoOTA.setPassword("victron123");  // Set OTA password for security
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {  // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("OTA: Start updating " + type);
        // Display OTA progress on screen
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(CYAN, BLACK);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.println("OTA Update");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(10, 40);
        M5.Lcd.println("Starting...");
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: Update complete");
        M5.Lcd.setCursor(10, 70);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.println("Update Complete!");
        M5.Lcd.setCursor(10, 85);
        M5.Lcd.println("Rebooting...");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        unsigned int percent = (progress / (total / 100));
        Serial.printf("OTA Progress: %u%%\r", percent);
        // Update progress bar on screen
        M5.Lcd.fillRect(10, 55, 220, 10, BLACK);
        M5.Lcd.drawRect(10, 55, 220, 10, WHITE);
        M5.Lcd.fillRect(11, 56, (percent * 218) / 100, 8, GREEN);
        M5.Lcd.setCursor(10, 70);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.printf("Progress: %u%%  ", percent);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        M5.Lcd.fillRect(0, 70, 240, 60, BLACK);
        M5.Lcd.setCursor(10, 70);
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("Update Failed!");
        M5.Lcd.setCursor(10, 85);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
            M5.Lcd.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
            M5.Lcd.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
            M5.Lcd.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
            M5.Lcd.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
            M5.Lcd.println("End Failed");
        }
    });
    
    ArduinoOTA.begin();
    Serial.println("STARTUP: ArduinoOTA initialized");
    Serial.println("OTA: Ready for updates");
    Serial.print("OTA: Hostname: ESP32-Victron");
    if (webServer->isAPMode()) {
        Serial.print(" IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.print(" IP: ");
        Serial.println(WiFi.localIP());
    }

    Serial.println("STARTUP: doing a short scan to populate devices");
    victron->scan(2);            // short scan
    updateDeviceList();

    if (!deviceAddresses.empty()) {
        drawDisplay();
    } else {
        Serial.println("STARTUP: no devices found yet - showing basic screen");
    }

    // Leave setup so loop() runs normally (do NOT block here)
}

void updateDeviceList() {
    deviceAddresses.clear();
    auto& devices = victron->getDevices();
    for (auto& pair : devices) {
        deviceAddresses.push_back(pair.first);
    }
    
    if (currentDeviceIndex >= deviceAddresses.size()) {
        currentDeviceIndex = 0;
    }
}

void drawDisplay() {
    if (webConfigMode) {
        // Show web configuration screen
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(CYAN, BLACK);
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.println("Web Configuration");
        
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(5, 25);
        if (webServer->isAPMode()) {
            M5.Lcd.println("Mode: Access Point");
            M5.Lcd.setCursor(5, 40);
            M5.Lcd.println("SSID: Victron-Config");
        } else {
            M5.Lcd.println("Mode: WiFi Client");
        }
        
        M5.Lcd.setCursor(5, 55);
        M5.Lcd.print("IP: ");
        M5.Lcd.println(webServer->getIPAddress());
        
        M5.Lcd.setCursor(5, 75);
        M5.Lcd.setTextColor(YELLOW, BLACK);
        M5.Lcd.println("Open in web browser:");
        M5.Lcd.setCursor(5, 90);
        M5.Lcd.print("http://");
        M5.Lcd.println(webServer->getIPAddress());
        
        M5.Lcd.drawLine(0, 110, 240, 110, DARKGREY);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(DARKGREY, BLACK);
        M5.Lcd.setCursor(5, 118);
        M5.Lcd.print("M5: Back to Monitor");
        return;
    }
    
    if (deviceAddresses.empty()) {
        return;
    }
    
    VictronDeviceData* device = victron->getDevice(deviceAddresses[currentDeviceIndex]);
    
    if (!device) {
        return;
    }
    
    M5.Lcd.fillScreen(BLACK);
    
    // Device name header
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setCursor(0, 0);
    
    String deviceName = device->name;
    if (deviceName.length() > 26) {
        deviceName = deviceName.substring(0, 23) + "...";
    }
    M5.Lcd.println(deviceName);
    
    // Device type indicator
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(0, 12);
    switch (device->type) {
        case DEVICE_SMART_SHUNT:
            M5.Lcd.print("Smart Shunt");
            break;
        case DEVICE_SMART_SOLAR:
            M5.Lcd.print("Smart Solar");
            break;
        case DEVICE_BLUE_SMART_CHARGER:
            M5.Lcd.print("Blue Smart Charger");
            break;
        case DEVICE_INVERTER:
            M5.Lcd.print("Inverter");
            break;
        case DEVICE_DCDC_CONVERTER:
            M5.Lcd.print("DC-DC Converter");
            break;
        default:
            M5.Lcd.print("Victron Device");
            break;
    }
    
    // Device counter
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(220, 12);
    M5.Lcd.printf("%d/%d", currentDeviceIndex + 1, deviceAddresses.size());
    
    // Draw separator line
    M5.Lcd.drawLine(0, 24, 240, 24, DARKGREY);
    
    // Main data display
    M5.Lcd.setTextSize(1);
    int y = 30;
    
    // Voltage
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Voltage:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80, y);
    if (device->dataValid) {
        M5.Lcd.printf("%.2f V", device->voltage);
    } else {
        M5.Lcd.print("-- V");
    }
    y += 15;
    
    // Current
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Current:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80, y);
    if (device->dataValid) {
        M5.Lcd.printf("%.2f A", device->current);
    } else {
        M5.Lcd.print("-- A");
    }
    y += 15;
    
    // Power
    if (device->hasPower) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Power:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f W", device->power);
        y += 15;
    }
    
    // Battery SOC (State of Charge) - for Smart Shunt
    if (device->hasSOC) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Battery:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        uint16_t color = WHITE;
        if (device->batterySOC <= 20) {
            color = RED;
        } else if (device->batterySOC <= 50) {
            color = YELLOW;
        } else {
            color = GREEN;
        }
        M5.Lcd.setTextColor(color, BLACK);
        M5.Lcd.printf("%.1f %%", device->batterySOC);
        M5.Lcd.setTextColor(WHITE, BLACK);
        y += 15;
    }
    
    // Temperature
    if (device->hasTemperature) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Temp:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f C", device->temperature);
        y += 15;
    }
    
    // Inverter AC Output
    if (device->hasAcOut) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("AC Out:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f V", device->acOutVoltage);
        y += 15;
        
        if (device->acOutCurrent != 0 || device->acOutPower != 0) {
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.setCursor(5, y);
            M5.Lcd.print("AC Pwr:");
            M5.Lcd.setTextColor(WHITE, BLACK);
            M5.Lcd.setCursor(80, y);
            M5.Lcd.printf("%.0f W", device->acOutPower);
            y += 15;
        }
    }
    
    // DC-DC Converter voltages
    if (device->hasInputVoltage) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("In:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.2f V", device->inputVoltage);
        y += 15;
    }
    
    if (device->hasOutputVoltage) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Out:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.2f V", device->outputVoltage);
        y += 15;
    }
    
    // Draw bottom instructions
    M5.Lcd.drawLine(0, 110, 240, 110, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(DARKGREY, BLACK);
    M5.Lcd.setCursor(5, 118);
    M5.Lcd.print("M5: Next Device");
    
    // Signal strength indicator
    M5.Lcd.setCursor(180, 118);
    if (device->rssi > -60) {
        M5.Lcd.setTextColor(GREEN, BLACK);
    } else if (device->rssi > -80) {
        M5.Lcd.setTextColor(YELLOW, BLACK);
    } else {
        M5.Lcd.setTextColor(RED, BLACK);
    }
    M5.Lcd.printf("RSSI:%d", device->rssi);
}

void loop() {
    M5.update();
    unsigned long currentTime = millis();
    
    // Handle ArduinoOTA updates
    ArduinoOTA.handle();
    
    // Long press button A: toggle web config display (check this first to prevent short press from firing)
    if (M5.BtnA.pressedFor(LONG_PRESS_DURATION) && !longPressHandled) {
        if (currentTime - lastButtonPressTime > BUTTON_DEBOUNCE) {
            lastButtonPressTime = currentTime;
            webConfigMode = !webConfigMode;
            drawDisplay();
            longPressHandled = true;
        }
    }
    
    // Reset long press flag when button is released
    if (M5.BtnA.wasReleased()) {
        longPressHandled = false;
    }
    
    // Button A: Switch to next device or go back from config mode (only if not a long press)
    if (M5.BtnA.wasPressed() && !longPressHandled && (currentTime - lastButtonPressTime > BUTTON_DEBOUNCE)) {
        lastButtonPressTime = currentTime;
        
        if (deviceAddresses.empty()) {
            // If no devices found, toggle web config display
            webConfigMode = !webConfigMode;
            drawDisplay();
        } else if (!webConfigMode) {
            // Normal mode: cycle through devices
            currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
            drawDisplay();
        } else {
            // Config mode: go back to normal mode
            webConfigMode = false;
            drawDisplay();
        }
    }
    
    // Periodic BLE scan (only in normal mode)
    if (!webConfigMode && currentTime - lastScanTime > SCAN_INTERVAL && !scanning) {
        scanning = true;
        Serial.println("Periodic scan...");
        victron->scan(2);  // Quick 2-second scan
        updateDeviceList();
        lastScanTime = currentTime;
        scanning = false;
        
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
    }
    
    // Update display periodically (only in normal mode with devices)
    if (!webConfigMode && currentTime - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
        lastDisplayUpdate = currentTime;
    }
    
    // Check battery alarm periodically
    if (currentTime - lastBuzzerCheck > BUZZER_CHECK_INTERVAL) {
        checkBatteryAlarm();
        lastBuzzerCheck = currentTime;
    }
    
    // Handle buzzer beeps (non-blocking)
    handleBuzzerBeep();
    
    // Handle MQTT publishing
    mqttPublisher->loop();
    
    delay(10);
}
