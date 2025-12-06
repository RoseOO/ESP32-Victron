#include <M5StickCPlus.h>
#include "VictronBLE.h"

VictronBLE victron;
std::vector<String> deviceAddresses;
int currentDeviceIndex = 0;
unsigned long lastScanTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long SCAN_INTERVAL = 30000;  // Scan every 30 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;  // Update display every second

bool scanning = false;

void setup() {
    // Initialize M5StickC PLUS
    M5.begin();
    M5.Lcd.setRotation(1);  // Landscape orientation
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("M5StickC PLUS - Victron BLE Monitor");
    
    // Display splash screen
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Victron BLE");
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Initializing...");
    
    // Initialize Victron BLE
    victron.begin();
    
    delay(1000);
    
    // Initial scan
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.println("Scanning for");
    M5.Lcd.setCursor(10, 35);
    M5.Lcd.println("Victron devices...");
    
    victron.scan(5);
    
    // Build device list
    updateDeviceList();
    
    lastScanTime = millis();
    
    if (deviceAddresses.empty()) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(10, 20);
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("No devices found!");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(10, 40);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Please ensure:");
        M5.Lcd.setCursor(10, 55);
        M5.Lcd.println("- Device is on");
        M5.Lcd.setCursor(10, 68);
        M5.Lcd.println("- BLE is enabled");
        M5.Lcd.setCursor(10, 81);
        M5.Lcd.println("- In range");
    } else {
        Serial.printf("Found %d device(s)\n", deviceAddresses.size());
    }
}

void updateDeviceList() {
    deviceAddresses.clear();
    auto& devices = victron.getDevices();
    for (auto& pair : devices) {
        deviceAddresses.push_back(pair.first);
    }
    
    if (currentDeviceIndex >= deviceAddresses.size()) {
        currentDeviceIndex = 0;
    }
}

void drawDisplay() {
    if (deviceAddresses.empty()) {
        return;
    }
    
    VictronDeviceData* device = victron.getDevice(deviceAddresses[currentDeviceIndex]);
    
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
    if (device->dataValid && device->voltage != 0) {
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
    if (device->dataValid && device->current != 0) {
        M5.Lcd.printf("%.2f A", device->current);
    } else {
        M5.Lcd.print("-- A");
    }
    y += 15;
    
    // Power
    if (device->power != 0 || device->dataValid) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Power:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        if (device->dataValid) {
            M5.Lcd.printf("%.1f W", device->power);
        } else {
            M5.Lcd.print("-- W");
        }
        y += 15;
    }
    
    // Battery SOC (State of Charge) - for Smart Shunt
    if (device->type == DEVICE_SMART_SHUNT && device->batterySOC >= 0) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Battery:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        if (device->dataValid) {
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
        } else {
            M5.Lcd.print("-- %");
        }
        y += 15;
    }
    
    // Temperature
    if (device->temperature != 0) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(5, y);
        M5.Lcd.print("Temp:");
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(80, y);
        M5.Lcd.printf("%.1f C", device->temperature);
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
    
    // Button A: Switch to next device
    if (M5.BtnA.wasPressed()) {
        if (!deviceAddresses.empty()) {
            currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
            drawDisplay();
        }
    }
    
    // Periodic BLE scan
    unsigned long currentTime = millis();
    if (currentTime - lastScanTime > SCAN_INTERVAL && !scanning) {
        scanning = true;
        Serial.println("Periodic scan...");
        victron.scan(2);  // Quick 2-second scan
        updateDeviceList();
        lastScanTime = currentTime;
        scanning = false;
        
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
    }
    
    // Update display periodically
    if (currentTime - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (!deviceAddresses.empty()) {
            drawDisplay();
        }
        lastDisplayUpdate = currentTime;
    }
    
    delay(10);
}
