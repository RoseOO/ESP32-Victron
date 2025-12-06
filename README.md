# ESP32-Victron BLE Monitor

A M5StickC PLUS based Bluetooth Low Energy (BLE) monitor for Victron Energy devices. This project allows you to wirelessly monitor your Victron Smart Shunt, Smart Solar MPPT controllers, and Blue Smart Chargers in real-time.

## Features

- **Multi-Device Support**: Connect to and monitor multiple Victron devices:
  - Smart Shunt (Battery Monitor)
  - Smart Solar MPPT Controllers
  - Blue Smart Chargers
- **Real-Time Monitoring**: Display live data including:
  - Battery/Solar Voltage (V)
  - Current Draw/Charge (A)
  - Power (W)
  - Battery State of Charge (%) for Smart Shunt
  - Temperature (°C)
  - Signal Strength (RSSI)
- **Device Switching**: Easily switch between multiple connected Victron devices using the M5 button
- **Automatic Scanning**: Periodically scans for new devices and updates connections
- **Compact Display**: Optimized UI for the M5StickC PLUS's 1.14" LCD screen

## Hardware Requirements

- **M5StickC PLUS** - ESP32-based development board with built-in display
- **Victron Energy Devices** with BLE support:
  - SmartShunt 500A/500A with Bluetooth
  - SmartSolar MPPT Controllers (75/10 to 250/100)
  - Blue Smart IP65 Chargers
  - Blue Smart IP22 Chargers

## Victron BLE Protocol

This project decodes Victron's BLE advertisement packets which contain:
- Manufacturer ID: 0x02E1 (Victron Energy)
- Instant readout data (unencrypted mode)
- Various record types for voltage, current, power, SOC, temperature, etc.

## Software Requirements

- **PlatformIO** - Cross-platform build system and library manager
- **Arduino Framework** for ESP32
- **Libraries** (automatically installed via PlatformIO):
  - M5StickCPlus (^0.1.0)
  - NimBLE-Arduino (^1.4.1)

## Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/RoseOO/ESP32-Victron.git
   cd ESP32-Victron
   ```

2. Open the project in PlatformIO (VS Code with PlatformIO extension recommended)

3. Build and upload to your M5StickC PLUS:
   ```bash
   pio run --target upload
   ```

4. Monitor serial output (optional):
   ```bash
   pio device monitor
   ```

## Usage

1. **Power On**: Turn on your M5StickC PLUS
2. **Initial Scan**: The device will automatically scan for Victron devices (5 seconds)
3. **View Data**: Live data from the first detected device will be displayed
4. **Switch Devices**: Press the **M5 Button (front button)** to cycle through detected devices
5. **Auto-Update**: The display updates every second, and scans for new devices every 30 seconds

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
- Try rescanning by restarting the M5StickC PLUS

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
- M5Stack for the M5StickC PLUS hardware
- NimBLE-Arduino library developers

## Disclaimer

This is an unofficial project and is not affiliated with or endorsed by Victron Energy. Use at your own risk.