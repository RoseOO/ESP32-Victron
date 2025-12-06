# Victron BLE Protocol Documentation

## Overview

Victron Energy devices with Bluetooth Low Energy (BLE) support broadcast their measurement data via BLE advertisements. This allows for instant readout without pairing or connection establishment.

## BLE Advertisement Structure

Victron devices use the Manufacturer Specific Data field in BLE advertisements with the following structure:

```
Offset | Length | Description
-------|--------|-------------
0-1    | 2      | Manufacturer ID (0x02E1 = Victron Energy)
2      | 1      | Record Type
3-4    | 2      | Model ID
5      | 1      | Encryption Key (0x00 = instant readout mode, unencrypted)
6+     | varies | Data Records (TLV format: Type, Length, Value)
```

## Encryption

Victron devices support two modes:
- **Instant Readout** (key = 0x00): Data is unencrypted and can be read by anyone
- **Encrypted** (key != 0x00): Data is encrypted with a device-specific key

This implementation only supports Instant Readout mode for simplicity.

## Data Records Format

Starting from byte 6, data is encoded in TLV (Type-Length-Value) format:

```
[Type (1 byte)][Length (1 byte)][Value (Length bytes)]
```

Multiple records can be present in a single advertisement.

## Record Types

### Common Records

| Type | Name | Description | Resolution | Unit |
|------|------|-------------|------------|------|
| 0x01 | Solar Charger Voltage | PV voltage on solar charger | 10 mV | Volts |
| 0x02 | Solar Charger Current | PV current on solar charger | 1 mA | Amperes |
| 0x03 | Battery Voltage | Battery voltage | 10 mV | Volts |
| 0x04 | Battery Current | Battery current (+ charging, - discharging) | 1 mA | Amperes |
| 0x05 | Battery Power | Battery power | 1 W | Watts |
| 0x06 | Battery SOC | Battery State of Charge | 0.01 % | Percent |
| 0x07 | Battery Temperature | Battery temperature | 0.01 K | Kelvin |
| 0x08 | External Temperature | External temperature sensor | 0.01 K | Kelvin |
| 0x09 | Charger Voltage | Charger output voltage | 10 mV | Volts |
| 0x0A | Charger Current | Charger output current | 1 mA | Amperes |
| 0x0B | Device State | Operating state of device | 1 | - |
| 0x0C | Charger Error | Error code | 1 | - |
| 0x0D | Consumed Ah | Consumed amp-hours | 0.1 Ah | Amp-hours |
| 0x0E | Time to Go | Estimated time remaining | 1 min | Minutes |
| 0x0F | Alarm | Alarm status | 1 | Bitfield |
| 0x10 | Relay State | Relay on/off state | 1 | Boolean |

## Value Encoding

Values are encoded as little-endian signed integers with the following byte lengths:
- 1 byte: -128 to 127
- 2 bytes: -32768 to 32767
- 3 bytes: -8388608 to 8388607
- 4 bytes: -2147483648 to 2147483647

The raw integer value must be multiplied by the resolution to get the actual value in the specified unit.

### Example

Battery Voltage (type 0x03):
```
Raw bytes: [03 02 F4 04]
Type: 0x03 (Battery Voltage)
Length: 0x02 (2 bytes)
Value: [F4 04] = 0x04F4 = 1268 (little-endian)
Actual voltage: 1268 × 0.01V = 12.68V
```

## Device Types

Different Victron devices broadcast different sets of records:

### Smart Shunt
- Battery Voltage (0x03)
- Battery Current (0x04)
- Battery Power (0x05)
- Battery SOC (0x06)
- Consumed Ah (0x0D)
- Time to Go (0x0E)
- Alarm (0x0F)

### Smart Solar MPPT
- Solar Charger Voltage (0x01)
- Solar Charger Current (0x02)
- Battery Voltage (0x03)
- Device State (0x0B)
- Charger Error (0x0C)

### Blue Smart Charger
- Charger Voltage (0x09)
- Charger Current (0x0A)
- Battery Voltage (0x03)
- Device State (0x0B)
- Charger Error (0x0C)
- External Temperature (0x08)

## Device Identification

Devices can be identified by:
1. **Manufacturer ID**: 0x02E1 (Victron Energy)
2. **Device Name**: BLE advertised name typically contains:
   - "SmartShunt" or "Shunt"
   - "SmartSolar" or "MPPT"
   - "Blue" or "Charger"
3. **Model ID**: Bytes 3-4 contain a unique model identifier
4. **Record Types**: The presence of specific record types indicates device type

## Implementation Notes

### Parsing Strategy
1. Scan for BLE advertisements with manufacturer ID 0x02E1
2. Check encryption key (byte 5) - only process if 0x00
3. Parse data records starting from byte 6
4. Iterate through TLV records until end of data
5. Store decoded values with appropriate scaling

### Refresh Rate
Victron devices typically broadcast advertisements every 1-2 seconds. However, the scan interval should be balanced between:
- Data freshness (shorter interval)
- Battery life (longer interval)
- BLE congestion (avoid continuous scanning)

A recommended scan strategy:
- Initial scan: 5 seconds to discover all devices
- Periodic scan: 2 seconds every 30 seconds to refresh data

### Signal Strength
RSSI (Received Signal Strength Indicator) values:
- > -60 dBm: Excellent
- -60 to -80 dBm: Good
- < -80 dBm: Poor (consider moving closer)

## References

- Victron Energy BLE Protocol: https://community.victronenergy.com/
- BLE Core Specification: https://www.bluetooth.com/specifications/bluetooth-core-specification/

## Troubleshooting

### No Data Received
1. Ensure device is in Instant Readout mode (not encrypted)
2. Check if device is actively measuring (connected to battery/load)
3. Verify BLE is enabled on the device
4. Ensure device is powered on

### Incorrect Values
1. Verify correct byte order (little-endian)
2. Check resolution multiplier for the record type
3. Ensure proper sign extension for negative values
4. Validate record length matches expected size

### Missing Records
Different devices broadcast different sets of records based on their function. Not all devices will have all record types.
