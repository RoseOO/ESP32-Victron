# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Full AES-128-CTR Decryption**: Complete implementation of encrypted device support
  - Decrypt Victron BLE advertisement data using AES-128-CTR mode
  - Uses ESP32's built-in `esp_aes` library for hardware-accelerated decryption
  - Validates encryption key format (32 hex characters = 16 bytes)
  - Verifies encryption key match byte for correctness
  - Extracts nonce/counter from BLE packet bytes 5-6
  - Properly decrypts payload starting from byte 8
  - Detailed error logging for troubleshooting

### Changed
- **Error Messages**: Updated decryption failure messages to reflect actual encryption support
  - Changed from "Encryption is not fully implemented" to "Decryption failed. Please verify the encryption key is correct."
  - Added specific error messages for invalid key format and decryption failures
  - Enhanced debug logging with encryption status and warnings

### Fixed
- **Encryption Key Address Matching**: Fixed issue where encryption keys were not being matched to devices
  - Added MAC address normalization to handle addresses with or without colons
  - Addresses are now normalized (colons removed, lowercase) before storing/comparing encryption keys
  - Fixes issue where devices remained encrypted despite adding encryption keys via web interface
  - Addresses can now be entered in any format: `AA:BB:CC:DD:EE:FF`, `aa:bb:cc:dd:ee:ff`, or `aabbccddeeff`

### Documentation
- Updated `VICTRON_BLE_PROTOCOL.md` with encryption technical details
- Updated `WEB_CONFIGURATION.md` to reflect that encryption is now fully supported
- Added AES-CTR implementation details and troubleshooting tips

## [1.3.1] - 2025-12-06

### Fixed
- **Critical: Boot Loop Issue**: Fixed continuous boot loop (SW_RESET) caused by web interface
  - Moved large HTML strings (~34 KB) from RAM to LittleFS filesystem in flash storage
  - Extracted `index.html` (25 KB) and `monitor.html` (9 KB) to `data/` directory
  - Updated `WebConfigServer` to serve files from filesystem instead of PROGMEM
  - Reduced `WebConfigServer.cpp` from 1381 to 567 lines (-814 lines, -59%)
  - Added proper error handling for filesystem mount failures
  - Added helpful error messages with upload instructions
  - This fix prevents memory exhaustion during initialization

### Added
- **LittleFS Filesystem Support**: Web interface files now stored in flash
  - Added `board_build.filesystem = littlefs` to `platformio.ini`
  - Created `data/` directory with web interface HTML files
  - Filesystem must be uploaded separately: `pio run --target uploadfs`
- **Documentation**: Comprehensive guides for filesystem usage
  - `docs/FILESYSTEM_UPLOAD.md` - Complete filesystem upload guide
  - `docs/BOOT_LOOP_FIX.md` - Technical analysis of the boot loop fix
  - `data/README.md` - Documentation for data directory
  - Updated `README.md` and `QUICKSTART.md` with filesystem upload instructions

### Changed
- **Memory Usage**: Significantly reduced RAM usage during initialization
  - HTML files no longer consume RAM (stored in flash instead)
  - Freed up ~34 KB of RAM for other allocations
  - Improved boot stability and reliability
- **Web Interface**: Now requires filesystem upload as separate step
  - Must run `pio run --target uploadfs` after firmware upload
  - Error pages provide clear instructions if filesystem not uploaded

### Technical Details
- RAM usage during setup reduced from ~199 KB to ~165 KB
- Firmware size reduced by ~34 KB
- Web interface can now be updated without recompiling firmware
- Better separation of concerns: web assets separate from application code

## [1.3.0] - 2025-12-06

### Changed
- **Hardware Platform Update**: Updated to target M5StickC PLUS2
  - Changed board configuration from `m5stick-c` to `m5stick-c-plus2`
  - Updated library from `M5StickCPlus@^0.1.0` to `M5StickCPlus2@^0.1.1`
  - Updated all source code includes from `M5StickCPlus.h` to `M5StickCPlus2.h`
  - Updated all documentation to reference M5StickC PLUS2
  - M5StickC PLUS2 features improved hardware:
    - ESP32-PICO-V3-02 processor (upgraded from PICO-D4)
    - 200mAh battery (upgraded from 120mAh)
    - 8MB Flash and 2MB PSRAM (double the storage)
    - Additional Button C
  - All existing features remain compatible

### Note
This version specifically targets the M5StickC PLUS2 hardware. If you are using the original M5StickC PLUS, please use version 1.2.0 or earlier.

## [1.2.0] - 2025-12-06

### Added
- **Additional Device Support**: Extended support for more Victron device types
  - Inverters (Phoenix, MultiPlus, Quattro)
  - DC-DC Converters (Orion series)
  - AC output voltage, current, and power monitoring for inverters
  - Input/output voltage monitoring for DC-DC converters
  - Enhanced device type identification
- **Real-Time Web Monitoring**: Live data viewing via web browser
  - `/monitor` endpoint with auto-refreshing dashboard
  - Display all devices in card layout with real-time updates
  - Signal strength indicators with color coding
  - Auto-refresh every 2 seconds
  - Responsive design for mobile and desktop
  - Direct link from configuration page to live monitor
- **Home Assistant Integration**: Full MQTT support with auto-discovery
  - MQTTPublisher class for managing MQTT connections
  - Configurable MQTT broker, port, credentials
  - Home Assistant MQTT Auto-Discovery protocol support
  - Automatic sensor entity creation in Home Assistant
  - Configurable publish intervals (5-300 seconds)
  - Support for authentication
  - Web interface for MQTT configuration
  - Real-time connection status display
- **New Record Types**: 
  - AC Output Voltage (0x11)
  - AC Output Current (0x12)
  - AC Output Power (0x13)
  - Input Voltage (0x14) for DC-DC converters
  - Output Voltage (0x15) for DC-DC converters
  - Off Reason (0x16)

### Changed
- Updated platformio.ini to include PubSubClient library for MQTT
- Enhanced VictronDeviceData structure with inverter and DC-DC specific fields
- Improved device type identification logic for inverters and converters
- Extended display logic to show device-specific measurements
- WebConfigServer now supports MQTTPublisher integration
- Added MQTT configuration section to web interface

### Technical Details
- RESTful API endpoint `/api/devices/live` for real-time device data
- RESTful API endpoints `/api/mqtt` (GET/POST) for MQTT configuration
- MQTT publisher with automatic reconnection handling
- Home Assistant discovery messages sent on connect
- Device-specific MQTT topics: `{baseTopic}/{deviceId}/{sensor}`
- Persistent MQTT configuration storage using ESP32 Preferences
- Field availability flags for optional measurements

### Known Limitations
- Inverter and DC-DC converter support requires instant readout mode
- MQTT uses unencrypted connections (no TLS support yet)
- Home Assistant discovery requires MQTT broker
- Web interface uses HTTP (not HTTPS)

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
- Support for M5StickC PLUS2 hardware platform
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
- Battery low alarm functionality
- Deep sleep mode for extended battery life

### Under Consideration
- Multiple language support
- Custom display themes
- Support for VE.Direct protocol
- Encrypted BLE support (requires device pairing)
- MQTT over TLS/SSL
- Authentication for web interface

---

## Version History

### Format
- **[Major.Minor.Patch]** - Date
- **Major**: Breaking changes, major feature additions
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes, minor improvements

### Release Notes

#### v1.0.0 (2025-12-06)
First stable release. Provides complete basic functionality for monitoring Victron devices via BLE on M5StickC PLUS2 hardware. Includes comprehensive documentation and both PlatformIO and Arduino IDE support.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute to this project.

## Reporting Issues

Please report bugs and feature requests through [GitHub Issues](https://github.com/RoseOO/ESP32-Victron/issues).
