# Eco Worthy Battery BMS Integration

This document describes the integration of Eco Worthy Battery BMS support into the ESP32-Victron project.

## Overview

Eco Worthy Battery BMS devices use a different communication protocol than Victron devices. While Victron devices broadcast data via BLE advertisements (passive scanning), Eco Worthy devices require an active GATT connection to read battery data.

## Supported Devices

- **Eco Worthy batteries** with BW02 BLE adapter (show up as `ECO-WORTHY*`)
- **DCHOUSE batteries** (show up as `DCHOUSE*`, use same protocol)

## Technical Details

### Communication Protocol

- **Service UUID**: `0000fff0-0000-1000-8000-00805f9b34fb`
- **RX Characteristic**: `0000fff1-0000-1000-8000-00805f9b34fb` (notifications)
- **Data Format**: Two packet types (A1 and A2) with Modbus CRC validation
- **Connection**: Active GATT connection required (not passive like Victron)

### Packet Types

#### A1 Packet (Battery Information)
- Battery level (SOC) - percentage (0-100%)
- Battery health - percentage (0-100%)
- Voltage - in 0.01V resolution
- Current - signed, in 0.01A or 0.1A (version dependent)
- Design capacity - in 0.01Ah
- Problem code - error/warning codes

#### A2 Packet (Cell & Temperature Information)
- Cell count - number of battery cells
- Cell voltages - individual voltage for each cell (in mV)
- Temperature sensor count
- Temperature readings - for each sensor (in 0.1°C)

### Data Flow

1. **Scanning**: Eco Worthy devices are detected during BLE scan by name pattern
2. **Connection**: When an Eco Worthy device is selected, a GATT connection is established
3. **Data Reading**: Subscribe to notifications on RX characteristic
4. **Parsing**: Parse A1 and A2 packets with CRC validation
5. **Display**: Data is synchronized to `VictronDeviceData` structure for unified display

## Implementation

### Classes

- **EcoWorthyBMS**: Main class for Eco Worthy BMS communication
  - Handles GATT connection and disconnection
  - Subscribes to notifications
  - Parses A1 and A2 packets
  - Validates Modbus CRC

- **EcoWorthyBMSData**: Data structure for Eco Worthy battery data
  - Voltage, current, power
  - Battery level (SOC) and health
  - Cell voltages (up to 16 cells)
  - Temperatures (up to 4 sensors)
  - Problem codes

### Integration Points

1. **VictronBLE.h**: Added `DEVICE_ECO_WORTHY_BMS` device type enum
2. **VictronBLE.cpp**: Enhanced scan() to detect Eco Worthy devices by name
3. **main.cpp**: 
   - Initialize EcoWorthyBMS instance
   - Connect to Eco Worthy devices during scan
   - Read and synchronize data to VictronDeviceData
   - Display Eco Worthy data using existing UI

## Display Features

When monitoring an Eco Worthy BMS, the following data is displayed:

- **Device Name**: "Eco Worthy BMS"
- **Voltage**: Battery voltage in V
- **Current**: Charge/discharge current in A (positive = charging)
- **Power**: Calculated power in W
- **Battery**: State of charge (SOC) in %
- **Temp**: Battery temperature (if available)

## Configuration

1. **Automatic Detection**: Eco Worthy devices are automatically detected during BLE scan
2. **Web Configuration**: Add device MAC address and enable via web interface
3. **Connection**: Device will auto-connect when selected for display

## Limitations

- Only one Eco Worthy device can be actively connected at a time
- Requires active connection (slightly higher power consumption than Victron passive scanning)
- Connection must be established before data can be read

## Reference Implementation

This implementation is based on the protocol used in:
- [BMS_BLE-HA](https://github.com/patman15/BMS_BLE-HA) - Home Assistant integration
- [aiobmsble](https://github.com/patman15/aiobmsble) - Python BMS library

## Future Enhancements

Potential improvements for future versions:

- Display individual cell voltages in a detailed view
- Show all temperature sensors
- Display battery health percentage
- Show problem codes with descriptions
- Support for multiple concurrent Eco Worthy connections
- Cell balancing status indication

## Troubleshooting

### Device Not Found
- Ensure the Eco Worthy battery is powered on
- Verify the BW02 adapter is properly connected
- Check that the device shows up as `ECO-WORTHY*` or `DCHOUSE*` in BLE scan
- Make sure you're within BLE range (5-10 meters)

### Connection Fails
- Try power cycling the battery
- Reset the BW02 adapter if available
- Check for BLE interference from other devices
- Ensure no other app is connected to the BMS

### No Data Updates
- Verify the connection is established (check serial monitor)
- Check for CRC errors in serial output
- Ensure battery is actively charging or discharging
- Try reconnecting the device

## Testing

To test Eco Worthy BMS support:

1. Power on an Eco Worthy battery with BW02 adapter
2. Flash the firmware to M5StickC PLUS2
3. Wait for device scan
4. Add device via web configuration
5. Select device using M5 button
6. Verify voltage, current, and SOC are displayed

## Protocol Notes

### CRC Validation
- Uses Modbus CRC-16 algorithm
- Calculated over entire packet except last 2 bytes
- Polynomial: 0xA001
- Initial value: 0xFFFF

### Byte Order
- Little-endian for multi-byte values
- Signed integers for current (can be negative)
- Unsigned integers for voltage, capacity

### Version Detection
- V1 protocol: Packet starts with 0xA1, current in 0.01A
- V2 protocol: Packet starts with MAC address, current in 0.1A
