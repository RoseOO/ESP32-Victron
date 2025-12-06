# Configuration Examples

## Basic Configuration

The default configuration in `src/main.cpp` is optimized for general use:

```cpp
// Scan every 30 seconds
const unsigned long SCAN_INTERVAL = 30000;

// Update display every second
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;
```

## Configuration Scenarios

### 1. Battery Life Optimized

For maximum battery life when running on internal battery:

```cpp
// Scan every 2 minutes
const unsigned long SCAN_INTERVAL = 120000;

// Update display every 5 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 5000;

// In setup(), add:
M5.Axp.ScreenBreath(8);  // Reduce screen brightness
```

Expected battery life: ~4-6 hours

### 2. Real-Time Monitoring

For USB-powered real-time monitoring with frequent updates:

```cpp
// Scan every 10 seconds
const unsigned long SCAN_INTERVAL = 10000;

// Update display every 500ms
const unsigned long DISPLAY_UPDATE_INTERVAL = 500;
```

Best for: Monitoring fast-changing values like solar charging or battery load testing.

### 3. Multiple Device Monitoring

If you have multiple Victron devices and want to cycle through them automatically:

```cpp
// Auto-switch device every 10 seconds
unsigned long lastDeviceSwitch = 0;
const unsigned long DEVICE_SWITCH_INTERVAL = 10000;

// Add to loop():
if (currentTime - lastDeviceSwitch > DEVICE_SWITCH_INTERVAL) {
    if (deviceAddresses.size() > 1) {
        currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
        drawDisplay();
        lastDeviceSwitch = currentTime;
    }
}
```

### 4. Low Signal Environment

For areas with poor BLE signal or many interfering devices:

```cpp
// Scan longer to ensure device discovery
victron.scan(10);  // 10 seconds instead of default 5

// Scan less frequently to reduce interference
const unsigned long SCAN_INTERVAL = 60000;
```

### 5. Single Device Only

If you only want to monitor one specific device:

```cpp
// Add filter in updateDeviceList():
void updateDeviceList() {
    deviceAddresses.clear();
    auto& devices = victron.getDevices();
    for (auto& pair : devices) {
        // Only add devices containing "SmartShunt" in the name
        if (pair.second.name.indexOf("SmartShunt") >= 0) {
            deviceAddresses.push_back(pair.first);
        }
    }
    
    if (currentDeviceIndex >= deviceAddresses.size()) {
        currentDeviceIndex = 0;
    }
}
```

### 6. Alarm on Low Battery

Add an alarm when battery SOC drops below threshold:

```cpp
// Add to drawDisplay() after displaying battery SOC:
if (device->batterySOC < 20 && device->batterySOC > 0) {
    // Visual alarm
    M5.Lcd.fillRect(0, 0, 240, 10, RED);
    
    // Audible alarm (if buzzer connected to Grove port)
    // tone(BUZZER_PIN, 1000, 500);  // 1000Hz for 500ms
}
```

## Display Customization

### Color Schemes

#### Dark Mode (Default)
```cpp
M5.Lcd.fillScreen(BLACK);
M5.Lcd.setTextColor(WHITE, BLACK);
```

#### High Contrast
```cpp
M5.Lcd.fillScreen(BLACK);
M5.Lcd.setTextColor(YELLOW, BLACK);  // All text in yellow
```

#### Light Mode
```cpp
M5.Lcd.fillScreen(WHITE);
M5.Lcd.setTextColor(BLACK, WHITE);
```

### Font Sizes

Adjust text size for readability:

```cpp
// Larger text for main values
M5.Lcd.setTextSize(2);  // Double size

// Smaller text for labels
M5.Lcd.setTextSize(1);  // Normal size
```

### Rotation

Change display orientation:

```cpp
M5.Lcd.setRotation(1);  // Landscape (default)
M5.Lcd.setRotation(0);  // Portrait
M5.Lcd.setRotation(3);  // Landscape flipped
```

## Advanced Features

### Data Logging to SD Card

If you have an M5Stack SD card module, you can log data:

```cpp
#include <SD.h>

void logData(VictronDeviceData* device) {
    File dataFile = SD.open("/victron.csv", FILE_APPEND);
    if (dataFile) {
        dataFile.printf("%lu,%s,%.2f,%.2f,%.2f,%.1f\n",
            millis(),
            device->name.c_str(),
            device->voltage,
            device->current,
            device->power,
            device->batterySOC);
        dataFile.close();
    }
}
```

### WiFi Data Upload

Send data to a server or MQTT broker:

```cpp
#include <WiFi.h>
#include <HTTPClient.h>

void uploadData(VictronDeviceData* device) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://your-server.com/api/victron");
        http.addHeader("Content-Type", "application/json");
        
        String json = String("{\"voltage\":") + device->voltage + 
                     ",\"current\":" + device->current +
                     ",\"soc\":" + device->batterySOC + "}";
        
        int httpCode = http.POST(json);
        http.end();
    }
}
```

### Button Functionality Extensions

Use other buttons for additional features:

```cpp
void loop() {
    M5.update();
    
    // Button A: Next device (default)
    if (M5.BtnA.wasPressed()) {
        currentDeviceIndex = (currentDeviceIndex + 1) % deviceAddresses.size();
        drawDisplay();
    }
    
    // Button B: Force rescan (if M5StickC Plus2 has button B)
    if (M5.BtnB.wasPressed()) {
        victron.scan(5);
        updateDeviceList();
        drawDisplay();
    }
    
    // Power button long press: Toggle display on/off
    if (M5.BtnPWR.pressedFor(2000)) {
        M5.Axp.SetLDO2(false);  // Turn off display
        delay(1000);
        M5.Axp.SetLDO2(true);   // Turn on display
    }
}
```

## BLE Configuration

### Scan Parameters

Adjust BLE scan parameters for better performance:

```cpp
void VictronBLE::begin() {
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisedDeviceCallbacks(this), false);
    
    // Active scanning (default)
    pBLEScan->setActiveScan(true);
    
    // Scan interval and window (in ms)
    pBLEScan->setInterval(100);  // How often to scan (100ms)
    pBLEScan->setWindow(99);     // How long each scan lasts (99ms)
    
    // For low power, use:
    // pBLEScan->setInterval(1000);
    // pBLEScan->setWindow(100);
}
```

## Compile-Time Configuration

### Conditional Features

Use preprocessor directives for optional features:

```cpp
// At top of main.cpp
#define ENABLE_LOGGING 1
#define ENABLE_WIFI 0
#define BATTERY_ALARM_THRESHOLD 20

// Later in code:
#if ENABLE_LOGGING
    logData(device);
#endif

#if ENABLE_WIFI
    uploadData(device);
#endif
```

### Platform-Specific Settings

```cpp
// Different settings for different boards
#ifdef ARDUINO_M5Stick_C
    #define DISPLAY_WIDTH 80
    #define DISPLAY_HEIGHT 160
#elif defined(ARDUINO_M5Stick_C_Plus)
    #define DISPLAY_WIDTH 135
    #define DISPLAY_HEIGHT 240
#endif
```

## Troubleshooting Configuration

### Common Issues

1. **Display too slow**: Increase `DISPLAY_UPDATE_INTERVAL`
2. **Missing devices**: Decrease `SCAN_INTERVAL` or increase scan duration
3. **Battery drains quickly**: Increase both intervals and reduce screen brightness
4. **Data not updating**: Check BLE is working and devices are in range

### Debug Mode

Enable verbose logging:

```cpp
// In setup():
Serial.begin(115200);
Serial.setDebugOutput(true);

// In VictronBLE.cpp, add more logging:
Serial.printf("Raw data: ");
for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
}
Serial.println();
```

## Performance Tips

1. Avoid calling `scan()` too frequently (minimum 10 seconds between scans)
2. Use `drawDisplay()` efficiently - only redraw when data changes
3. Consider using partial screen updates for better performance
4. Disable Serial output in production for slight speed improvement

## Saving Configurations

To save configurations across reboots, use EEPROM or preferences:

```cpp
#include <Preferences.h>

Preferences preferences;

void saveConfig() {
    preferences.begin("victron", false);
    preferences.putUInt("scanInt", SCAN_INTERVAL);
    preferences.putUInt("dispInt", DISPLAY_UPDATE_INTERVAL);
    preferences.end();
}

void loadConfig() {
    preferences.begin("victron", true);
    SCAN_INTERVAL = preferences.getUInt("scanInt", 30000);
    DISPLAY_UPDATE_INTERVAL = preferences.getUInt("dispInt", 1000);
    preferences.end();
}
```
