# ESP32-Victron BLE Monitor

A M5StickC PLUS2 based Bluetooth Low Energy (BLE) monitor for Victron Energy devices. This project allows you to wirelessly monitor your Victron Smart Shunt, Smart Solar MPPT controllers, and Blue Smart Chargers in real-time.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-blue.svg)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Arduino-compatible-green.svg)](https://www.arduino.cc/)

## Documentation

- **[Quick Start Guide](QUICKSTART.md)** - Get up and running in 10 minutes
- **[Filesystem Upload Guide](docs/FILESYSTEM_UPLOAD.md)** - Upload web interface files to ESP32 flash
- **[Web Configuration Guide](docs/WEB_CONFIGURATION.md)** - Configure devices and encryption keys via web interface
- **[Hardware Setup](docs/HARDWARE_SETUP.md)** - Detailed hardware installation guide
- **[Victron BLE Protocol](docs/VICTRON_BLE_PROTOCOL.md)** - Technical protocol documentation
- **[Data Parsing Implementation](docs/DATA_PARSING.md)** - Detailed guide to Victron data parsing (NEW!)
- **[Configuration Examples](examples/CONFIGURATION.md)** - Customization and advanced features
- **[Arduino IDE Guide](arduino/README.md)** - Arduino IDE specific instructions
- **[Contributing](CONTRIBUTING.md)** - How to contribute to this project
- **[Changelog](CHANGELOG.md)** - Version history and release notes

## Getting Started

### Quick Install (5 minutes)

1. **Hardware**: Get a M5StickC PLUS2 and a Victron device with BLE
2. **Software**: Install Arduino IDE or PlatformIO
3. **Upload**: Flash the firmware to your M5StickC PLUS2
4. **Enjoy**: Watch your Victron data in real-time!

For detailed instructions, see the **[Quick Start Guide](QUICKSTART.md)**.

## Features

- **Web Configuration Interface**: Configure devices and encryption keys via web browser
  - WiFi Access Point or Station mode
  - Add/edit/delete Victron devices
  - Store encryption keys securely
  - User-friendly responsive interface
- **Real-Time Web Monitoring**: View live data from all devices in web browser
  - Auto-refreshing dashboard
  - Multiple device cards with real-time updates
  - Signal strength indicators
  - Works on any device with a web browser
- **Home Assistant Integration**: Push data to Home Assistant via MQTT
  - Automatic device discovery
  - Configure MQTT broker via web interface
  - Supports authentication
  - Configurable publish intervals
- **Multi-Device Support**: Connect to and monitor multiple Victron devices:
  - Smart Shunt (Battery Monitor)
  - Smart Solar MPPT Controllers
  - Blue Smart Chargers
  - Inverters (Phoenix, MultiPlus, Quattro)
  - DC-DC Converters (Orion, etc.)
- **Real-Time Monitoring**: Display live data including:
  - Battery/Solar Voltage (V)
  - Current Draw/Charge (A)
  - Power (W)
  - Battery State of Charge (%) for Smart Shunt
  - Temperature (°C)
  - AC Output (for Inverters)
  - Input/Output Voltage (for DC-DC Converters)
  - Signal Strength (RSSI)
- **Encryption Support**: Store and manage encryption keys for Victron devices
- **Device Switching**: Easily switch between multiple connected Victron devices using the M5 button
- **Automatic Scanning**: Periodically scans for new devices and updates connections
- **Compact Display**: Optimized UI for the M5StickC PLUS2's 1.14" LCD screen

## Hardware Requirements

- **M5StickC PLUS2** - ESP32-based development board with built-in display
- **Victron Energy Devices** with BLE support:
  - SmartShunt 500A/500A with Bluetooth
  - SmartSolar MPPT Controllers (75/10 to 250/100)
  - Blue Smart IP65 Chargers
  - Blue Smart IP22 Chargers
  - Phoenix Inverters with VE.Direct Bluetooth Smart dongle
  - MultiPlus/Quattro Inverters with VE.Direct Bluetooth Smart dongle
  - Orion DC-DC Converters with Bluetooth

## Victron BLE Protocol

This project decodes Victron's BLE advertisement packets which contain:
- Manufacturer ID: 0x02E1 (Victron Energy)
- Support for both instant readout (unencrypted) and encrypted modes
- AES-128-CTR decryption for encrypted devices
- Various record types for voltage, current, power, SOC, temperature, etc.

## Software Requirements

- **PlatformIO** - Cross-platform build system and library manager
- **Arduino Framework** for ESP32
- **Libraries** (automatically installed via PlatformIO):
  - M5StickCPlus2 (^0.1.1)
  - NimBLE-Arduino (^1.4.1)
  - ESPAsyncWebServer (^1.2.3)
  - AsyncTCP (^1.1.1)
  - PubSubClient (^2.8)

## Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/RoseOO/ESP32-Victron.git
   cd ESP32-Victron
   ```

2. Open the project in PlatformIO (VS Code with PlatformIO extension recommended)

3. Build and upload firmware to your M5StickC PLUS2:
   ```bash
   pio run --target upload
   ```

4. Upload web interface files to filesystem (required for web interface):
   ```bash
   pio run --target uploadfs
   ```

5. Monitor serial output (optional):
   ```bash
   pio device monitor
   ```

> **Note:** Both firmware and filesystem uploads are required. The filesystem contains the web interface HTML files. See [Filesystem Upload Guide](docs/FILESYSTEM_UPLOAD.md) for details.

## Usage

### Basic Operation

1. **Power On**: Turn on your M5StickC PLUS2
2. **WiFi Setup**: On first boot, device creates WiFi AP "Victron-Config"
3. **Web Configuration** (optional): 
   - Connect to "Victron-Config" WiFi (password: `victron123`)
   - Open browser to `http://192.168.4.1`
   - Add your Victron devices with encryption keys if needed
4. **Initial Scan**: The device will automatically scan for Victron devices (5 seconds)
5. **View Data**: Live data from the first detected device will be displayed
6. **Switch Devices**: Press the **M5 Button (front button)** to cycle through detected devices
7. **Auto-Update**: The display updates every second, and scans for new devices every 30 seconds

### Web Configuration Interface

The web interface allows you to:
- Configure WiFi (AP or Station mode)
- Add Victron devices with MAC addresses
- Store encryption keys for encrypted devices
- Enable/disable individual devices
- View current configuration
- Monitor live data from all devices
- Configure MQTT/Home Assistant integration

**Access the web interface:**
- **Short press** M5 button: Navigate between devices
- **Long press** M5 button (1 second): View WiFi info and IP address
- Open web browser to the displayed IP address
- Click "Live Monitor" to see real-time data from all devices

### Home Assistant Integration

To integrate with Home Assistant:
1. Access the web interface
2. Click "Configure MQTT" in the Home Assistant / MQTT section
3. Enter your MQTT broker details:
   - Broker address (e.g., `192.168.1.100`)
   - Port (default: `1883`)
   - Username/Password (if required)
4. Enable "Home Assistant Auto-Discovery"
5. Set publish interval (default: 30 seconds)
6. Save settings

The device will automatically create sensors in Home Assistant for all discovered Victron devices.

For detailed instructions, see the **[Web Configuration Guide](docs/WEB_CONFIGURATION.md)**.

## Display Layout

```
┌─────────────────────────────┐
│ Device Name            1/3  │
│ Device Type                 │
├─────────────────────────────┤
│ Voltage:        12.85 V     │
│ Current:         3.45 A     │
│ Power:          44.3 W      │
│ Battery:        87.5 %      │
│ Temp:           25.3 C      │
│                             │
├─────────────────────────────┤
│ M5: Next Device  RSSI: -65  │
└─────────────────────────────┘
```

## Troubleshooting

### No Devices Found
- Ensure your Victron device is powered on
- Check that Bluetooth is enabled on the Victron device
- Make sure you're within BLE range (typically 5-10 meters)
- Verify the device is in "instant readout" mode (not encrypted)

### Data Not Updating
- Check signal strength (RSSI) - move closer to the device
- Ensure the Victron device is actively measuring (connected to battery/solar)
- Try rescanning by restarting the M5StickC PLUS2

### Build Errors
- Ensure you have the latest PlatformIO Core
- Check that all dependencies are installed
- Try cleaning the build: `pio run --target clean`

## Technical Details

### Victron BLE Advertisement Format

The Victron BLE advertisement packet structure:
- Bytes 0-1: Manufacturer ID (0x02E1)
- Byte 2: Record Type
- Bytes 3-4: Model ID
- Byte 5: Encryption key (0x00 for instant readout)
- Byte 6+: Data records (Type-Length-Value format)

### Supported Record Types

| Record Type | Description | Resolution |
|------------|-------------|------------|
| 0x01 | Solar Charger Voltage | 10 mV |
| 0x02 | Solar Charger Current | 1 mA |
| 0x03 | Battery Voltage | 10 mV |
| 0x04 | Battery Current | 1 mA |
| 0x05 | Battery Power | 1 W |
| 0x06 | Battery SOC | 0.01 % |
| 0x07 | Battery Temperature | 0.01 K |
| 0x09 | Charger Voltage | 10 mV |
| 0x0A | Charger Current | 1 mA |
| 0x0D | Consumed Ah | 0.1 Ah |

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is open source and available under the MIT License.

## Acknowledgments

- Victron Energy for their excellent products and BLE protocol
- M5Stack for the M5StickC PLUS2 hardware
- NimBLE-Arduino library developers
- All contributors to this project

## Star History

If you find this project useful, please consider giving it a star on GitHub!

## Disclaimer

This is an unofficial project and is not affiliated with or endorsed by Victron Energy. Use at your own risk.

---

**Made with care for the Victron and ESP32 community**