# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-12-06

### Added
- **Web Configuration Interface**: Complete web-based configuration system
  - WiFi Access Point mode (default) and Station mode
  - User-friendly HTML/CSS/JavaScript interface
  - Add, edit, and delete Victron devices
  - Store device names and BLE MAC addresses
  - Configure and store encryption keys for devices
  - Persistent configuration using ESP32 Preferences
- **Encryption Key Support**: Infrastructure for handling encrypted Victron devices
  - Store 128-bit AES encryption keys (32 hex characters)
  - Encryption key storage and retrieval per device
  - Placeholder for AES-128-CTR decryption (to be fully implemented)
- **WiFi Connectivity**: 
  - AsyncWebServer for handling HTTP requests
  - AsyncTCP library for async network operations
  - Configurable WiFi SSID and password
  - Automatic fallback to AP mode if WiFi connection fails
- **Display Enhancements**:
  - WiFi status and IP address display on screen
  - Toggle between monitor mode and config info display
  - Web config accessible via long-press M5 button
- **Documentation**:
  - Complete Web Configuration Guide
  - API reference for web endpoints
  - Troubleshooting section for web interface
  - Security considerations

### Changed
- Updated platformio.ini to include ESPAsyncWebServer and AsyncTCP libraries
- Modified main.cpp to integrate web server initialization
- Enhanced VictronBLE class with encryption key management methods
- Updated README with web configuration features

### Technical Details
- RESTful API endpoints for device and WiFi configuration
- JSON responses for all API calls
- Responsive web design with modern CSS
- Persistent storage using ESP32 Preferences library
- Modular WebConfigServer class for clean separation

### Known Limitations
- Full AES-128-CTR decryption not yet implemented (infrastructure in place)
- Web interface uses HTTP (not HTTPS)
- No authentication/login system (planned for future release)
- Encryption currently requires "Instant Readout" mode on devices

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
- Full AES-128-CTR decryption implementation for encrypted devices
- MQTT publishing for home automation integration
- Data logging to SD card
- Historical data graphs
- HTTPS support for web interface
- Authentication/login system for web interface
- OTA (Over-The-Air) firmware updates via web interface
- Real-time data viewing in web interface
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
