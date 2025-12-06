# Arduino IDE Version

This folder contains a standalone Arduino sketch version of the ESP32-Victron project for users who prefer the Arduino IDE over PlatformIO.

## Installation

### 1. Install Arduino IDE

Download and install Arduino IDE 2.0 or later from [arduino.cc](https://www.arduino.cc/en/software)

### 2. Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Boards Manager**
6. Search for "ESP32"
7. Install "esp32 by Espressif Systems"

### 3. Install Required Libraries

1. Go to **Sketch → Include Library → Manage Libraries**
2. Search and install the following libraries:
   - **M5StickCPlus** by M5Stack
   - **NimBLE-Arduino** by h2zero

### 4. Open the Sketch

1. Navigate to the `arduino/ESP32-Victron/` folder
2. Open `ESP32-Victron.ino` in Arduino IDE

### 5. Configure Board Settings

1. Go to **Tools → Board** and select **M5Stick-C**
2. Go to **Tools → Port** and select the port your M5StickC PLUS is connected to
3. Other settings should be at default:
   - Upload Speed: 1500000
   - CPU Frequency: 240MHz
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB (32Mb)
   - Partition Scheme: Default

### 6. Upload

1. Connect your M5StickC PLUS via USB
2. Click the **Upload** button (→)
3. Wait for compilation and upload to complete
4. The M5StickC PLUS will automatically restart and run the program

## Usage

Same as the main project - see the main [README.md](../../README.md) for usage instructions.

## Features

This Arduino version includes all the same features as the PlatformIO version:
- BLE scanning for Victron devices
- Support for Smart Shunt, Smart Solar, and Blue Smart Chargers
- Real-time display of voltage, current, power, and battery state
- Device switching with M5 button
- Automatic periodic scanning

## Troubleshooting

### Compilation Errors

**"M5StickCPlus.h: No such file or directory"**
- Install the M5StickCPlus library via Library Manager

**"NimBLEDevice.h: No such file or directory"**
- Install the NimBLE-Arduino library via Library Manager

**"error: 'class M5Display' has no member named 'drawLine'"**
- Update M5StickCPlus library to the latest version

### Upload Errors

**"A fatal error occurred: Failed to connect to ESP32"**
- Check USB cable (must be a data cable, not charge-only)
- Try pressing and holding the M5 button while connecting USB
- Select the correct COM port under Tools → Port
- Try a different USB port

**"Timed out waiting for packet header"**
- Lower upload speed: Tools → Upload Speed → 921600 or 115200
- Reset M5StickC PLUS before uploading

### Runtime Issues

**"No devices found!"**
- Ensure Victron device is powered on
- Enable Instant Readout mode on Victron device
- Move M5StickC PLUS closer to the Victron device
- Check that BLE is enabled on Victron device

**Display is blank**
- Check if device is powered on (long press power button)
- Verify successful upload (check Serial Monitor for output)
- Try resetting by pressing power button

## Differences from PlatformIO Version

This Arduino sketch combines all code into a single `.ino` file for simplicity. The functionality is identical to the PlatformIO version, but:

- All code is in one file instead of separate header/source files
- Library versions are managed by Arduino IDE
- Board configuration is done through IDE menus instead of `platformio.ini`

## Customization

You can customize the behavior by editing these constants at the top of the sketch:

```cpp
const unsigned long SCAN_INTERVAL = 30000;  // Scan every 30 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;  // Update display every second
```

See [examples/CONFIGURATION.md](../../examples/CONFIGURATION.md) for more customization options.

## Converting to PlatformIO

If you want to use PlatformIO instead:

1. Use the main project folder (not this arduino folder)
2. Open in VS Code with PlatformIO extension
3. The code is organized into:
   - `include/VictronBLE.h` - Header file
   - `src/VictronBLE.cpp` - Implementation
   - `src/main.cpp` - Main application
   - `platformio.ini` - Project configuration

## Support

For issues and questions:
- Check the main [README.md](../../README.md)
- Review [docs/HARDWARE_SETUP.md](../../docs/HARDWARE_SETUP.md)
- See [docs/VICTRON_BLE_PROTOCOL.md](../../docs/VICTRON_BLE_PROTOCOL.md)
- Open an issue on GitHub

## License

MIT License - see main project for details.
