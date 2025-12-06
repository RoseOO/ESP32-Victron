# Hardware Setup Guide

## Required Hardware

### M5StickC PLUS

The M5StickC PLUS is a compact ESP32-based development board featuring:
- **ESP32-PICO-D4**: Dual-core processor with integrated WiFi and BLE
- **Display**: 1.14 inch TFT LCD (135×240 pixels)
- **Battery**: Built-in 120mAh rechargeable lithium battery
- **Buttons**: Power button and programmable M5 button
- **Grove Port**: For additional sensors and modules
- **IMU**: 6-axis MPU6886 (optional - not used in this project)
- **Microphone**: SPM1423 (optional - not used in this project)

**Purchase**: Available from M5Stack official store, Adafruit, SparkFun, and other electronics retailers.

### Victron Energy Devices

This project supports the following Victron Energy devices with BLE:

#### 1. SmartShunt 500A/500A
- Battery monitor with Bluetooth
- Measures DC voltage, current, and power
- Calculates battery state of charge (SOC)
- Tracks consumed amp-hours and time remaining
- **Price**: ~$130-150 USD
- **Manual**: [SmartShunt Manual](https://www.victronenergy.com/battery-monitors/smartshunt)

#### 2. SmartSolar MPPT Controllers
- Solar charge controllers with Bluetooth
- Various models: 75/10, 75/15, 100/20, 100/30, 100/50, 150/35, 150/45, 150/60, 150/70, 250/60, 250/70, 250/100
- Monitors PV voltage, current, and battery charging
- **Price**: ~$100-600 USD depending on model
- **Manual**: [SmartSolar Manual](https://www.victronenergy.com/solar-charge-controllers)

#### 3. Blue Smart IP65/IP22 Chargers
- AC to DC battery chargers with Bluetooth
- Various models for different battery voltages and capacities
- Monitors charger voltage, current, and charging state
- **Price**: ~$100-400 USD depending on model
- **Manual**: [Blue Smart Charger Manual](https://www.victronenergy.com/battery-chargers)

## Victron Device Setup

### Enable Instant Readout Mode

For the monitor to work, Victron devices must be in "Instant Readout" mode (unencrypted BLE):

1. **Using VictronConnect App**:
   - Download VictronConnect for iOS or Android
   - Connect to your Victron device via Bluetooth
   - Go to Settings → Product Info
   - Enable "Instant Readout" or ensure encryption is disabled
   - Some devices have this enabled by default

2. **Factory Default**:
   - Most newer Victron devices have Instant Readout enabled by default
   - If you've set a PIN code for BLE, you may need to disable it for this project to work

### Verify BLE is Working

1. On your smartphone, enable Bluetooth
2. Scan for BLE devices (using nRF Connect app or similar)
3. Look for devices with names like:
   - "SmartShunt 500A HQ..."
   - "SmartSolar HQ..."
   - "Blue Smart Charger HQ..."
4. The device should be visible without needing to pair

## M5StickC PLUS Setup

### Initial Programming

1. **Install PlatformIO**:
   - Install [Visual Studio Code](https://code.visualstudio.com/)
   - Add the PlatformIO extension from the VS Code marketplace
   - Restart VS Code

2. **Clone and Open Project**:
   ```bash
   git clone https://github.com/RoseOO/ESP32-Victron.git
   cd ESP32-Victron
   code .
   ```

3. **Connect M5StickC PLUS**:
   - Connect your M5StickC PLUS via USB-C cable
   - Press the power button to turn it on (if not already on)

4. **Upload Firmware**:
   - Click the PlatformIO icon in VS Code
   - Select "Upload" under the m5stick-c-plus environment
   - Wait for compilation and upload to complete
   - The M5StickC PLUS should automatically restart and run the program

### Alternative: Using Arduino IDE

If you prefer Arduino IDE:

1. Install Arduino IDE 2.0 or later
2. Install ESP32 board support:
   - File → Preferences
   - Add to "Additional Board Manager URLs": 
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. Install required libraries:
   - Sketch → Include Library → Manage Libraries
   - Search and install: "M5StickCPlus", "NimBLE-Arduino"
4. Select board: Tools → Board → ESP32 Arduino → M5Stick-C
5. Select port: Tools → Port → (your M5StickC PLUS port)
6. Copy code from `src/main.cpp` to Arduino IDE
7. Click Upload

## Physical Placement

### Optimal Positioning

For best BLE signal reception:

1. **Distance**: Keep M5StickC PLUS within 5-10 meters of Victron devices
2. **Obstacles**: Minimize metal objects and walls between devices
3. **Interference**: Keep away from:
   - WiFi routers (especially 2.4 GHz)
   - Microwave ovens
   - Other strong BLE/WiFi devices

### Mounting Options

The M5StickC PLUS has multiple mounting options:

1. **Magnetic Mount**: Use the built-in magnetic mount on the back
2. **Wall Mount**: Use the Grove port with M5Stack mounting accessories
3. **Handheld**: Built-in battery allows portable operation (2-3 hours typical)
4. **USB Powered**: Can run continuously when connected to USB power

### Battery Considerations

- **Runtime**: 2-3 hours on internal battery (with display active and BLE scanning)
- **Charging**: Charges via USB-C while in use
- **Power Saving**: Consider reducing display brightness or scan frequency for longer battery life

## Troubleshooting Hardware Issues

### M5StickC PLUS Not Powering On
- Charge the device (red LED indicates charging)
- Long press the power button (2-3 seconds)
- Try connecting to USB power

### No BLE Connection
- Verify Victron device has BLE enabled
- Check Instant Readout mode is enabled on Victron device
- Ensure devices are within range (try moving closer)
- Check for interference from other wireless devices
- Restart both M5StickC PLUS and Victron device

### Display Issues
- Screen too dim: Adjust brightness in code
- Screen orientation wrong: Check `M5.Lcd.setRotation()` value
- Gibberish on screen: Verify correct board selected and uploaded successfully

### Upload Failures
- Check USB cable (use a data cable, not just charging cable)
- Verify correct COM port selected
- Hold M5 button while plugging in USB to enter bootloader mode
- Try a different USB port or cable
- Ensure no other serial monitor is open on the same port

## Power Management

### Battery Life Optimization

To extend battery life, consider these modifications in `main.cpp`:

1. **Reduce scan frequency**:
   ```cpp
   const unsigned long SCAN_INTERVAL = 60000;  // Scan every 60 seconds instead of 30
   ```

2. **Reduce display updates**:
   ```cpp
   const unsigned long DISPLAY_UPDATE_INTERVAL = 5000;  // Update every 5 seconds
   ```

3. **Dim display**:
   ```cpp
   M5.Axp.ScreenBreath(8);  // Range: 7-12, lower is dimmer
   ```

4. **Sleep mode**: Implement deep sleep between readings (advanced)

### Continuous Operation

For 24/7 monitoring, keep the M5StickC PLUS connected to USB power:
- Use any USB power adapter (5V, min 500mA)
- Can use a powerbank for outdoor/portable installations
- Battery will stay charged while connected

## Safety Considerations

- Do not expose M5StickC PLUS to water (it is not waterproof)
- Keep away from extreme temperatures (operating range: 0°C to 50°C)
- Do not short circuit or damage the battery
- Use proper cable management in vehicle/marine installations
- Ensure proper ventilation if mounting in enclosed spaces

## Next Steps

After hardware setup:
1. Test with your Victron devices
2. Verify all data fields are displaying correctly
3. Adjust scan intervals and display preferences as needed
4. Consider creating a custom enclosure for permanent installation
