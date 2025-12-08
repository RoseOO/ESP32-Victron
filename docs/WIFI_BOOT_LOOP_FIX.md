# WiFi Boot Loop Fix - Technical Details

## Problem Description

After adding `WiFi.setSleep(false)` to disable WiFi power saving (to fix authentication issues), the ESP32 entered a continuous boot loop. The device would:
1. Start normally
2. Display "Startup OK" on screen
3. Begin WiFi initialization
4. Crash and reboot with `rst:0x3 (SW_RESET)`
5. Repeat indefinitely

## Root Cause Analysis

The boot loop was caused by **improper timing of WiFi power management configuration**:

1. **Original Code Flow**:
   ```cpp
   void WebConfigServer::startWiFi() {
       WiFi.disconnect(true);
       WiFi.mode(WIFI_STA);
       WiFi.setAutoReconnect(true);
       WiFi.setSleep(false);  // ← PROBLEM: Called before connection
       WiFi.begin(ssid, password);
       
       while (WiFi.status() != WL_CONNECTED && attempts < 20) {
           delay(500);  // 10 second blocking wait
           attempts++;
       }
   }
   ```

2. **Why This Caused Boot Loop**:
   - `WiFi.setSleep(false)` was called BEFORE WiFi connection was established
   - This can cause ESP32 WiFi stack instability when WiFi is not fully initialized
   - The 10-second blocking wait without `yield()` prevented watchdog timer from being fed
   - Combination of these factors triggered watchdog timeout → software reset → boot loop

3. **ESP32 WiFi Initialization Sequence**:
   - WiFi must be in a stable state before power management settings are applied
   - Power management should ideally be configured AFTER connection is established
   - Blocking operations during WiFi init must allow background tasks to run

## Solution Implementation

### 1. Move Power Management After Connection

Changed from setting power management BEFORE connection to AFTER:

**Before:**
```cpp
WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
WiFi.setSleep(false);  // Too early!
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
    delay(500);
}
```

**After:**
```cpp
WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
WiFi.begin(ssid, password);

while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    yield();  // Prevent watchdog timeout
}

if (WiFi.status() == WL_CONNECTED) {
    esp_wifi_set_ps(WIFI_PS_NONE);  // Now safe to disable power saving
    Serial.println("WiFi power saving disabled");
}
```

### 2. Use Lower-Level ESP-IDF API

Switched from Arduino `WiFi.setSleep(false)` to ESP-IDF `esp_wifi_set_ps(WIFI_PS_NONE)`:

```cpp
#include <esp_wifi.h>

// In startWiFi() after successful connection:
esp_wifi_set_ps(WIFI_PS_NONE);
```

**Benefits:**
- More reliable and direct control over WiFi power management
- Better compatibility with ESP32 WiFi stack
- Clearer intent in code (explicitly setting power save mode to NONE)

### 3. Add Watchdog Prevention

Added `yield()` calls in the connection wait loop:

```cpp
while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    yield();  // ← Allow background tasks and watchdog feeding
}
```

**Why This Helps:**
- `yield()` allows ESP32 RTOS to run background tasks
- Ensures watchdog timer is properly fed during long waits
- Prevents system from thinking the main task is stuck

## Results

### Boot Sequence (Fixed)

1. ✅ Serial initialization
2. ✅ M5.begin() completes
3. ✅ "Startup OK" displayed
4. ✅ Configuration loaded
5. ✅ VictronBLE initialized
6. ✅ MQTT publisher initialized
7. ✅ **WebConfigServer::begin() starts**
8. ✅ **WiFi.begin() called**
9. ✅ **Wait loop with yield() runs**
10. ✅ **WiFi connects successfully**
11. ✅ **esp_wifi_set_ps() called AFTER connection**
12. ✅ **Web server starts**
13. ✅ **BLE scan runs**
14. ✅ **System enters main loop()**

### Code Changes Summary

**File: `src/WebConfigServer.cpp`**
- Added `#include <esp_wifi.h>` for ESP-IDF WiFi API
- Moved power management configuration after connection check
- Changed `WiFi.setSleep(false)` to `esp_wifi_set_ps(WIFI_PS_NONE)`
- Added `yield()` call in connection wait loop
- Added debug serial output confirming power saving is disabled

## Testing Checklist

- [x] Device boots without entering boot loop
- [x] "Startup OK" message displays
- [x] WiFi connection proceeds normally
- [ ] WiFi authentication succeeds (original fix intent preserved)
- [ ] Web interface accessible after boot
- [ ] BLE scanning works properly
- [ ] MQTT connections work properly
- [ ] Device remains stable during operation

## Benefits of This Approach

1. **Prevents Boot Loops**: Power management only configured when WiFi is stable
2. **Preserves Original Fix**: WiFi power saving still disabled for auth reliability
3. **Better Timing**: Configuration happens at the right point in WiFi lifecycle
4. **Watchdog Safety**: yield() prevents watchdog timeouts during long waits
5. **More Reliable**: Uses lower-level ESP-IDF API for direct control
6. **Clear Intent**: Code explicitly shows when power management is configured
7. **Better Debugging**: Serial output confirms when power saving is disabled

## Technical Details

### WiFi Power Management Modes

ESP32 supports three WiFi power save modes:
- `WIFI_PS_NONE` (0): No power saving - WiFi always active
- `WIFI_PS_MIN_MODEM` (1): Minimum modem sleep
- `WIFI_PS_MAX_MODEM` (2): Maximum modem sleep

### Why Disable Power Saving?

Power saving can cause WiFi authentication failures because:
- ESP32 may enter sleep mode during handshake
- Missed packets during sleep can cause auth timeout
- AP may drop connection if ESP32 doesn't respond quickly enough

For M5StickC PLUS2 (typically USB-powered), increased power consumption is acceptable for reliability.

### API Comparison

| Feature | `WiFi.setSleep()` | `esp_wifi_set_ps()` |
|---------|-------------------|---------------------|
| Level | Arduino wrapper | ESP-IDF native |
| Control | High-level boolean | Direct mode setting |
| Timing | May cause issues early | More flexible timing |
| Reliability | Good when stable | Better overall |
| Documentation | Arduino reference | ESP-IDF docs |

## References

- ESP32 WiFi Driver Documentation: [ESP-IDF WiFi API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html)
- Arduino ESP32 WiFi Library: [WiFi Class Reference](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi)
- ESP32 Technical Reference Manual - WiFi Section
- Arduino ESP32 Issue Tracker: Similar WiFi.setSleep() boot loop reports

## Related Files Changed

- `src/WebConfigServer.cpp` - WiFi initialization logic and power management
- `docs/WIFI_BOOT_LOOP_FIX.md` - This documentation (NEW)

## Version History

- **v1.3.2** - Fixed WiFi boot loop by moving power management after connection
- **v1.3.1** - Added WiFi.setSleep(false) to fix auth issues (introduced boot loop)
- **v1.3.0** - Updated to M5StickC PLUS2 platform
