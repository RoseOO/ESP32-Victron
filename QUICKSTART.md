# Quick Start Guide

Get your M5StickC PLUS2 displaying Victron data in under 10 minutes!

## What You Need

- M5StickC PLUS2 board  
- USB-C cable  
- Computer with Arduino IDE or PlatformIO  
- Victron device with Bluetooth (Smart Shunt, Smart Solar, or Blue Smart Charger)  

## 5-Step Setup

### Step 1: Prepare Your Victron Device (2 minutes)

1. Power on your Victron device
2. Using the VictronConnect app on your phone:
   - Connect to your device
   - Go to Settings → Product Info
   - **For unencrypted mode**: Enable **"Instant Readout"** (if not already enabled)
   - **For encrypted mode**: Note down the encryption key (see web configuration below)
3. Close the VictronConnect app

> **Note**: This monitor supports both instant readout (unencrypted) and encrypted modes. For encrypted devices, you'll need to configure the encryption key via the web interface.

### Step 2: Choose Your Development Environment

Pick one:

#### Option A: Arduino IDE (Easiest) - Recommended for Beginners

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. Install libraries:
   - Sketch → Include Library → Manage Libraries
   - Install: **M5StickCPlus2** and **NimBLE-Arduino**

#### Option B: PlatformIO (Advanced)

1. Install [VS Code](https://code.visualstudio.com/)
2. Install PlatformIO extension from VS Code marketplace
3. Clone this repository and open in VS Code

### Step 3: Load the Code (1 minute)

#### For Arduino IDE:
1. Download `arduino/ESP32-Victron/ESP32-Victron.ino` from this repo
2. Open it in Arduino IDE
3. Select **Tools → Board → M5Stick-C**
4. Select **Tools → Port** → (your device's port)

#### For PlatformIO:
1. Open the project folder in VS Code
2. PlatformIO will automatically detect configuration

### Step 4: Upload Firmware & Filesystem (3 minutes)

#### For PlatformIO (Recommended):
1. Connect M5StickC PLUS2 via USB-C cable
2. Press power button on M5StickC to turn it on
3. Upload firmware:
   ```bash
   pio run --target upload
   ```
4. Upload web interface files (required):
   ```bash
   pio run --target uploadfs
   ```
5. Wait for both uploads to complete

#### For Arduino IDE:
1. Connect M5StickC PLUS2 via USB-C cable
2. Press power button on M5StickC to turn it on
3. Click **Upload** button for firmware
4. For filesystem upload, see [Filesystem Upload Guide](docs/FILESYSTEM_UPLOAD.md)

> **Important**: Both firmware and filesystem uploads are required. The filesystem contains the web interface HTML files.

> **Troubleshooting**: If upload fails, try holding the M5 button while connecting USB

### Step 5: First Run & Web Configuration (2 minutes)

1. **Initial Boot**:
   - M5StickC PLUS2 will restart automatically
   - You'll see "Starting WiFi..." then "Starting BLE..."
   - Device creates WiFi access point "Victron-Config"

2. **Optional: Configure via Web Interface** (Recommended for encrypted devices):
   - On your phone/computer, connect to WiFi: **Victron-Config**
   - Password: `victron123`
   - Open browser and go to: `http://192.168.4.1`
   - You'll see the web configuration page
   - Click **"Add Device"** to add your Victron devices with encryption keys

3. **First Scan**:
   - After 5 seconds, the device will scan for Victron devices
   - Your device data should appear!
   - Press the **M5 button** to switch between multiple devices

4. **Toggle Display**:
   - **Short press** M5 button: Switch devices
   - **Long press** M5 button (1 second): View WiFi info and IP address

## What You Should See

```
┌─────────────────────────────┐
│ SmartShunt 500A HQ...  1/1  │
│ Smart Shunt                 │
├─────────────────────────────┤
│ Voltage:        13.24 V     │
│ Current:         2.15 A     │
│ Power:          28.5 W      │
│ Battery:        85.3 %      │
│                             │
├─────────────────────────────┤
│ M5: Next Device  RSSI: -55  │
└─────────────────────────────┘
```

## Button Controls

- **M5 Button** (front button): 
  - Short press: Switch between devices
  - Long press (1s): Toggle web config info display
- **Power Button** (side button): 
  - Short press: Toggle screen on/off
  - Long press (6s): Power off device

## Web Configuration Interface

The web interface allows you to:
- **Add devices** with their BLE MAC addresses
- **Store encryption keys** for encrypted Victron devices
- **Configure WiFi** (Access Point or connect to your network)
- **Manage devices** (edit, enable/disable, delete)

**To access**:
1. Connect to "Victron-Config" WiFi (password: `victron123`)
2. Open browser: `http://192.168.4.1`
3. Or long-press M5 button to see current IP address

**For detailed guide**, see: [Web Configuration Documentation](docs/WEB_CONFIGURATION.md)

## Common Issues & Solutions

### "No devices found!"

**Solutions**:
- Ensure Victron device is ON and close to M5StickC (within 5 meters)
- Close VictronConnect app on your phone (it may block BLE access)
- Enable "Instant Readout" in Victron settings
- Try restarting both devices
- **New**: Use web interface to manually add device with MAC address

### Device is encrypted / Cannot read data

**Solutions**:
- Option 1: Enable "Instant Readout" mode in VictronConnect app (unencrypted)
- Option 2: Use web interface to configure encryption key (recommended):
  1. Get encryption key from VictronConnect app (Settings → Product Info → Show Encryption Data)
  2. Copy the 32-character hex key
  3. Connect to web interface (http://192.168.4.1 or device IP)
  4. Add device with MAC address and paste the encryption key
  5. The device will automatically decrypt the BLE data using AES-128-CTR
- **Note**: Full AES-128-CTR encryption is now supported!

### "Upload failed" / "Could not connect to ESP32"

**Solutions**:
- Check USB cable (must be a data cable, not charge-only)
- Press and hold M5 button while connecting USB
- Try different USB port
- Lower upload speed: Tools → Upload Speed → 115200

### Data shows "-- V" or "-- A"

**Solutions**:
- Device may be in standby (not connected to load/battery)
- Move closer to Victron device (check RSSI value)
- Wait for next scan cycle (30 seconds)
- Ensure device is actively measuring

### Display is blank

**Solutions**:
- Long press power button to wake up
- Check if upload completed successfully
- Verify M5StickC PLUS2 is charged (connect to USB)

## Next Steps

### Customize Your Monitor

Edit these values in the code:

```cpp
// Scan more frequently (battery drains faster)
const unsigned long SCAN_INTERVAL = 10000;  // 10 seconds

// Update display more often
const unsigned long DISPLAY_UPDATE_INTERVAL = 500;  // 0.5 seconds
```

See [examples/CONFIGURATION.md](examples/CONFIGURATION.md) for more options.

### Add More Features

- Enable WiFi data logging
- Add battery low alarm
- Implement SD card logging
- Send data to MQTT broker

See [examples/CONFIGURATION.md](examples/CONFIGURATION.md) for code examples.

### Monitor Multiple Locations

If you have multiple M5StickC PLUS2 devices, you can:
- Place one near solar panels (for Smart Solar)
- Place one near batteries (for Smart Shunt)
- Place one near AC charger (for Blue Smart Charger)

## Understanding the Display

| Item | Meaning |
|------|---------|
| **Voltage** | Battery or solar panel voltage |
| **Current** | Current flow (+ charging, - discharging) |
| **Power** | Real-time power in watts |
| **Battery** | State of charge (Smart Shunt only) |
| **RSSI** | Signal strength (-60 = excellent, -80 = poor) |

### Battery % Color Coding

- **Green** (>50%): Good
- **Yellow** (20-50%): Medium
- **Red** (<20%): Low - charge soon!

## Tips for Best Results

1. **Placement**: Keep M5StickC within 5-10 meters of Victron device
2. **Charging**: Connect to USB for continuous monitoring
3. **Battery Life**: On internal battery: ~2-3 hours (adjust scan interval for longer life)
4. **Multiple Devices**: M5 button cycles through all detected devices
5. **Signal Quality**: Keep RSSI above -70 for reliable readings

## Getting Help

- [Full Documentation](README.md)
- [Hardware Setup Guide](docs/HARDWARE_SETUP.md)
- [Protocol Details](docs/VICTRON_BLE_PROTOCOL.md)
- [Configuration Examples](examples/CONFIGURATION.md)
- [Open an Issue](https://github.com/RoseOO/ESP32-Victron/issues)

## Success Checklist

- [ ] M5StickC PLUS2 powered on and displays content
- [ ] At least one Victron device detected
- [ ] Voltage and current values displaying (not "-- V")
- [ ] RSSI signal strength shown (around -40 to -80)
- [ ] M5 button switches between devices (if you have multiple)
- [ ] Display updates approximately every second

If all boxes are checked, you're good to go! Enjoy monitoring your Victron devices!

## What's Happening Behind the Scenes?

1. **Scanning**: M5StickC scans for BLE devices every 30 seconds
2. **Filtering**: Only Victron devices (manufacturer ID 0x02E1) are processed
3. **Parsing**: BLE advertisement data is decoded according to Victron protocol
4. **Display**: Data is formatted and shown on the LCD screen
5. **Updates**: Display refreshes every second with latest data

---

**Estimated Total Setup Time**: 5-10 minutes  
**Skill Level**: Beginner to Intermediate  
**Cost**: ~$40 (M5StickC PLUS2) + Victron device (already owned)

Happy Monitoring!
