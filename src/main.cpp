#include <M5StickCPlus2.h>
#include <Arduino.h>
#include <vector>
#include <Preferences.h>
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
unsigned long lastDeviceSwitch = 0;  // Track when device was last switched
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

// LCD display configuration
Preferences lcdPreferences;
int lcdFontSize = 1;  // Default font size (1x)
int lcdScrollRate = 5;  // Default scroll rate in seconds
String lcdOrientation = "landscape";  // Default orientation

// Forward declarations
void updateDeviceList();
void drawDisplay();
void loadBuzzerConfig();
void saveBuzzerConfig();
void loadDataRetentionConfig();
void saveDataRetentionConfig();
void loadLCDConfig();
void saveLCDConfig();
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

void loadLCDConfig() {
    lcdPreferences.begin("victron-lcd", true);  // read-only
    lcdFontSize = lcdPreferences.getInt("fontSize", 1);
    lcdScrollRate = lcdPreferences.getInt("scrollRate", 5);
    lcdOrientation = lcdPreferences.getString("orientation", "landscape");
    lcdPreferences.end();
    Serial.printf("LCD config loaded: fontSize=%d, scrollRate=%d, orientation=%s\n", lcdFontSize, lcdScrollRate, lcdOrientation.c_str());
}

void saveLCDConfig() {
    lcdPreferences.begin("victron-lcd", false);  // read-write
    lcdPreferences.putInt("fontSize", lcdFontSize);
    lcdPreferences.putInt("scrollRate", lcdScrollRate);
    lcdPreferences.putString("orientation", lcdOrientation);
    lcdPreferences.end();
    Serial.printf("LCD config saved: fontSize=%d, scrollRate=%d, orientation=%s\n", lcdFontSize, lcdScrollRate, lcdOrientation.c_str());
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
    
    // Load LCD display configuration
    Serial.println("STARTUP: loading LCD config");
    loadLCDConfig();

    // instantiate objects (no heavy init in constructors)
    Serial.println("STARTUP: new VictronBLE/WebConfigServer/MQTTPublisher");
    victron = new VictronBLE();
    webServer = new WebConfigServer();
    mqttPublisher = new MQTTPublisher();
    Serial.println("STARTUP: allocations done");

    // Basic display sanity test
    if (lcdOrientation == "portrait") {
        M5.Lcd.setRotation(0);  // Portrait mode
    } else {
        M5.Lcd.setRotation(1);  // Landscape mode (default)
    }
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
    
    // Get configured devices from webServer
    auto& configuredDevices = webServer->getDeviceConfigs();
    
    // Only add devices that are configured and enabled
    for (auto& pair : devices) {
        bool isConfigured = false;
        for (const auto& config : configuredDevices) {
            if (config.address.equalsIgnoreCase(pair.first) && config.enabled) {
                isConfigured = true;
                break;
            }
        }
        
        if (isConfigured) {
            deviceAddresses.push_back(pair.first);
        }
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
    M5.Lcd.setTextSize(lcdFontSize);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setCursor(0, 0);
    
    String deviceName = device->name;
    int maxChars = (lcdOrientation == "portrait") ? 10 : (26 / lcdFontSize);
    if (deviceName.length() > maxChars) {
        deviceName = deviceName.substring(0, maxChars - 3) + "...";
    }
    M5.Lcd.println(deviceName);
    
    // Device type indicator
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(0, 12 * lcdFontSize);
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
    int counterX = (lcdOrientation == "portrait") ? 100 : 220;
    M5.Lcd.setCursor(counterX, 12 * lcdFontSize);
    M5.Lcd.printf("%d/%d", currentDeviceIndex + 1, deviceAddresses.size());
    
    // Draw separator line
    M5.Lcd.drawLine(0, 24 * lcdFontSize, (lcdOrientation == "portrait") ? 135 : 240, 24 * lcdFontSize, DARKGREY);
    
    // Main data display
    M5.Lcd.setTextSize(lcdFontSize);
    int y = 30 * lcdFontSize;
    int lineSpacing = 15 * lcdFontSize;
    
    // Voltage
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Voltage:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80 * lcdFontSize, y);
    if (device->dataValid) {
        M5.Lcd.printf("%.2f V", device->voltage);
    } else {
        M5.Lcd.print("-- V");
    }
    y += lineSpacing;
    
    // Current
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(5, y);
    M5.Lcd.print("Current:");
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(80 * lcdFontSize, y);
    if (device->dataValid) {
        M5.Lcd.printf("%.2f A", device->current);
    } else {
        M5.Lcd.print("-- A");
    }
    y += lineSpacing;
    
    // Power
    if (device->hasPower) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Power:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
        M5.Lcd.printf("%.1f W", device->power);
        y += lineSpacing;
    }
    
    // Battery SOC (State of Charge) - for Smart Shunt
    if (device->hasSOC) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Battery:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
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
        y += lineSpacing;
    }
    
    // Temperature
    if (device->hasTemperature) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Temp:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
        M5.Lcd.printf("%.1f C", device->temperature);
        y += lineSpacing;
    }
    
    // Inverter AC Output
    if (device->hasAcOut) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("AC Out:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
        M5.Lcd.printf("%.1f V", device->acOutVoltage);
        y += lineSpacing;
        
        if (device->acOutCurrent != 0 || device->acOutPower != 0) {
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.setCursor(5, y);
            M5.Lcd.print("AC Pwr:");
            M5.Lcd.setTextColor(WHITE, BLACK);
            M5.Lcd.setCursor(80 * lcdFontSize, y);
            M5.Lcd.printf("%.0f W", device->acOutPower);
            y += lineSpacing;
        }
    }
    
    // DC-DC Converter voltages
    if (device->hasInputVoltage) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("In:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
        M5.Lcd.printf("%.2f V", device->inputVoltage);
        y += lineSpacing;
    }
    
    if (device->hasOutputVoltage) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Out:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80 * lcdFontSize, y);
        M5.Lcd.printf("%.2f V", device->outputVoltage);
        y += lineSpacing;
    }
    
    // Draw bottom instructions
    int bottomY = (lcdOrientation == "portrait") ? 220 : 110;
    M5.Lcd.drawLine(0, bottomY, (lcdOrientation == "portrait") ? 135 : 240, bottomY, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(DARKGREY, BLACK);
    M5.Lcd.setCursor(5, bottomY + 8);
    M5.Lcd.print("M5: Next Device");
    
    // Signal strength indicator
    M5.Lcd.setCursor((lcdOrientation == "portrait") ? 80 : 180, bottomY + 8);
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
            // Auto-scroll between devices based on scrollRate setting
            if (deviceAddresses.size() > 1 && currentTime - lastDeviceSwitch > (lcdScrollRate * 1000)) {
                currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
                lastDeviceSwitch = currentTime;
            }
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
