#include <M5StickCPlus2.h>
#include <Arduino.h>
#include <vector>
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

bool scanning = false;
bool webConfigMode = false;  // Toggle between normal mode and web config display

// Forward declarations
void updateDeviceList();
void drawDisplay();

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("STARTUP: Serial ready");

    Serial.println("STARTUP: calling M5.begin()");
    M5.begin();
    Serial.println("STARTUP: M5.begin() returned");

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
    
    // Button A: Switch to next device or toggle config display
    if (M5.BtnA.wasPressed() && (currentTime - lastButtonPressTime > BUTTON_DEBOUNCE)) {
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
    
    // Long press button A: toggle web config display
    if (M5.BtnA.pressedFor(1000) && (currentTime - lastButtonPressTime > BUTTON_DEBOUNCE)) {
        lastButtonPressTime = currentTime;
        webConfigMode = !webConfigMode;
        drawDisplay();
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
    
    // Handle MQTT publishing
    mqttPublisher->loop();
    
    delay(10);
}
