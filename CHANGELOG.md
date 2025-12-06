# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-12-06

### Added
- Initial release of ESP32-Victron BLE Monitor
- Support for M5StickC PLUS hardware platform
- BLE scanning and monitoring of Victron Energy devices
- Support for three device types:
  - SmartShunt 500A battery monitor
  - SmartSolar MPPT charge controllers
  - Blue Smart IP65/IP22 chargers
- Real-time display of key metrics:
  - Voltage (V)
  - Current (A)
  - Power (W)
  - Battery State of Charge - SOC (%)
  - Temperature (°C)
  - Signal strength (RSSI)
- Multi-device support with button-based switching
- Automatic periodic BLE scanning (every 30 seconds)
- Device type auto-detection from BLE advertisement
- Color-coded battery status (green/yellow/red based on SOC)
- PlatformIO project structure
- Arduino IDE compatible sketch
- Comprehensive documentation:
  - Quick Start Guide
  - Hardware Setup Guide
  - Victron BLE Protocol Documentation
  - Configuration Examples
  - Contributing Guidelines
- MIT License

### Technical Details
- Uses NimBLE-Arduino library for efficient BLE operations
- Parses Victron BLE advertisement data (manufacturer ID 0x02E1)
- Supports Instant Readout mode (unencrypted advertisements)
- Decodes TLV (Type-Length-Value) record format
- Implements proper bounds checking for security
- Little-endian signed integer decoding with sign extension

### Known Limitations
- Only supports Instant Readout mode (no encryption support)
- Cannot parse encrypted Victron BLE data
- Requires devices to be within BLE range (5-10 meters typical)
- Display updates limited by screen refresh rate
- Battery life ~2-3 hours on internal battery with default settings

## [Unreleased]

### Planned Features
- WiFi connectivity and MQTT publishing
- Data logging to SD card
- Historical data graphs
- Configuration menu on device
- Battery low alarm functionality
- OTA (Over-The-Air) firmware updates
- Support for additional Victron devices (Inverters, DC-DC converters)
- Deep sleep mode for extended battery life
- Web interface for configuration

### Under Consideration
- Multiple language support
- Custom display themes
- Integration with Home Assistant
- Support for VE.Direct protocol
- Encrypted BLE support (requires device pairing)

---

## Version History

### Format
- **[Major.Minor.Patch]** - Date
- **Major**: Breaking changes, major feature additions
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes, minor improvements

### Release Notes

#### v1.0.0 (2025-12-06)
First stable release. Provides complete basic functionality for monitoring Victron devices via BLE on M5StickC PLUS hardware. Includes comprehensive documentation and both PlatformIO and Arduino IDE support.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute to this project.

## Reporting Issues

Please report bugs and feature requests through [GitHub Issues](https://github.com/RoseOO/ESP32-Victron/issues).
