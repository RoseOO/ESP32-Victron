# Victron BLE Data Parsing Implementation

This document describes how the ESP32-Victron project parses data from Victron Energy devices via Bluetooth Low Energy (BLE).

## Overview

Victron devices advertise their data using BLE manufacturer-specific data packets. The data can be either unencrypted (instant readout mode) or encrypted (default mode). After decryption (if needed), the payload contains a fixed 16-byte structure that varies by device type.

## Packet Structure

### BLE Advertisement Packet Layout

```
Byte 0-1:   Manufacturer ID (0x02E1 for Victron, little-endian)
Byte 2-3:   Model ID (little-endian)
Byte 4:     Readout Type / Encryption Indicator (0x00 = instant readout, non-zero = encrypted)

For Encrypted Packets:
Byte 5-6:   Additional flags/padding
Byte 7-8:   IV/Counter (nonce) for AES-CTR decryption (little-endian)
Byte 9:     Encryption Key Match Byte (must match first byte of decryption key)
Byte 10-25: Encrypted payload (16 bytes)

For Unencrypted Packets:
Byte 5-20:  Unencrypted payload (16 bytes)
```

## Device-Specific Parsing

### SmartShunt / Battery Monitor

The SmartShunt uses a fixed 16-byte structure with complex bit packing:

#### Structure Layout

```
Byte 0-1:   Time To Go (unsigned 16-bit, little-endian, units: minutes)
            0xFFFF = infinite/unavailable
            
Byte 2-3:   Battery Voltage (signed 16-bit, units: 10mV)
            Byte 2: LSB (bits 0-7)
            Byte 3: bits 0-6 = MSB (bits 8-14), bit 7 = sign bit
            0x7FFF = unavailable
            Formula: voltage_V = value * 0.01
            
Byte 4-5:   Alarm Status (unsigned 16-bit bitmap)
            Byte 4: Battery Monitor alarms
            Byte 5: Inverter alarms
            
Byte 6-7:   Auxiliary Input (depends on aux mode in byte 8)
            Mode 0 (Aux Voltage): signed 16-bit, units: 10mV
            Mode 1 (Mid Voltage): unsigned 16-bit, units: 10mV
            Mode 2 (Temperature): unsigned 16-bit, units: 10mK (0.01 Kelvin)
            0x7FFF (mode 0) or 0xFFFF (modes 1-2) = unavailable
            
Byte 8:     Auxiliary Mode (bits 0-1)
            0 = Auxiliary voltage input
            1 = Mid-point voltage
            2 = Temperature sensor
            3 = None/Off
            Bits 2-7: Part of battery current (see below)
            
Byte 8-10:  Battery Current (signed 22-bit, units: mA)
            Complex bit packing across 3 bytes:
            Byte 8 bits 2-7:    bits 0-5 of value (6 bits)
            Byte 9 bits 0-1:    bits 6-7 of value (2 bits)
            Byte 9 bits 2-7:    bits 8-13 of value (6 bits)
            Byte 10 bits 0-1:   bits 14-15 of value (2 bits)
            Byte 10 bits 2-6:   bits 16-20 of value (5 bits)
            Byte 10 bit 7:      sign bit (bit 21)
            0x1FFFFF = unavailable
            Formula: current_A = value * 0.001
            
Byte 11-13: Consumed Ah (unsigned 20-bit, units: 100mAh = 0.1Ah)
            Byte 11:           bits 0-7 (8 bits)
            Byte 12:           bits 8-15 (8 bits)
            Byte 13 bits 0-3:  bits 16-19 (4 bits)
            0xFFFFF = unavailable
            Formula: consumed_Ah = value * 0.1
            
Byte 13-14: State of Charge (unsigned 10-bit, units: 0.1%)
            Byte 13 bits 4-7:  bits 0-3 of SOC (4 bits)
            Byte 14 bits 0-3:  bits 4-7 of SOC (4 bits)
            Byte 14 bits 4-5:  bits 8-9 of SOC (2 bits)
            0x3FF = unavailable
            Formula: soc_percent = value * 0.1
            
Byte 14-15: Unused
```

#### Alarm Codes

Byte 4 (Battery Monitor alarms):
- 0x0001: Low voltage
- 0x0002: High voltage
- 0x0004: Low SOC
- 0x0008: Low starter voltage
- 0x0010: High starter voltage
- 0x0020: Low temperature
- 0x0040: High temperature
- 0x0080: Mid voltage alarm

Byte 5 (Inverter alarms):
- 0x0100: Overload
- 0x0200: DC ripple
- 0x0400: Low AC out voltage
- 0x0800: High AC out voltage
- 0x1000: Short circuit
- 0x2000: BMS lockout

### Solar Controller / MPPT

The Solar Controller uses a simpler fixed 16-byte structure:

#### Structure Layout

```
Byte 0:     Device State
            0 = Off
            1 = Low power
            2 = Fault
            3 = Bulk charging
            4 = Absorption
            5 = Float
            6 = Storage
            7 = Equalize (manual)
            
Byte 1:     Charger Error
            0 = No error
            1 = Battery temperature too high
            2 = Battery voltage too high
            3 = Remote temperature sensor failure (auto)
            4-8 = Various remote sensor failures
            
Byte 2-3:   Battery Voltage (signed 16-bit, units: 10mV)
            Same format as SmartShunt
            0x7FFF = unavailable
            Formula: voltage_V = value * 0.01
            
Byte 4-5:   Battery Current (signed 16-bit, units: 100mA)
            Byte 4: LSB, Byte 5 bits 0-6: MSB, Byte 5 bit 7: sign
            0x7FFF = unavailable
            Formula: current_A = value * 0.1
            
Byte 6-7:   Yield Today (unsigned 16-bit, units: 10Wh = 0.01kWh)
            Little-endian
            0xFFFF = unavailable
            Formula: yield_kWh = value * 0.01
            
Byte 8-9:   PV Power (unsigned 16-bit, units: Watts)
            Little-endian
            0xFFFF = unavailable
            
Byte 10-11: Load Current (9-bit, units: 100mA)
            Byte 10: bits 0-7 (8 bits)
            Byte 11 bit 0: bit 8 (1 bit)
            Bytes 11 bits 1-7: unused
            0x1FF = unavailable
            Formula: load_A = value * 0.1
            Note: Many users report this value is not accurate
            
Byte 12-15: Unused
```

### DC-DC Converter

The DC-DC Converter format is not officially documented. Our implementation makes best-effort assumptions:

```
Byte 0:     Device State (assumed similar to Solar Controller)
Byte 1:     Error Code
Byte 2-3:   Input Voltage (signed 16-bit, units: 10mV)
Byte 4-5:   Output Voltage (signed 16-bit, units: 10mV)
Byte 6:     Off Reason
Byte 7-15:  Unknown/unused
```

## Implementation Details

### Multi-Byte Value Extraction

The implementation provides helper functions for extracting multi-byte values:

1. **extractSigned16()**: Standard signed 16-bit little-endian with sign bit
2. **extractUnsigned16()**: Standard unsigned 16-bit little-endian
3. **extractSigned22()**: Complex 22-bit signed value spread across 3 bytes
4. **extractUnsigned20()**: 20-bit unsigned value spread across 3 bytes
5. **extractUnsigned10()**: 10-bit unsigned value spread across 2 bytes

### Parsing Flow

1. **Decrypt** (if encrypted): Use AES-128-CTR with IV from bytes 7-8
2. **Identify device type**: Based on device name and model ID
3. **Select parser**: Route to device-specific parsing function
4. **Extract values**: Use helper functions for multi-byte extraction
5. **Convert units**: Apply scaling factors to get final values
6. **Validate**: Check for special "unavailable" values (0xFF...)

### Fallback TLV Parsing

For unknown device types or instant readout mode, the code falls back to TLV (Type-Length-Value) record parsing. This provides backwards compatibility but doesn't support all device-specific features.

## Testing and Validation

The parsing implementation has been validated against:
- Reference implementation: chrisj7903/Read-Victron-advertised-data
- Real device data from SmartShunt and Solar Controller
- Bit-level verification of multi-byte value extraction

### Known Limitations

1. **Load Current on Solar Controllers**: The reference implementation notes this value may not be accurate
2. **DC-DC Converter**: Format is based on assumptions, not official documentation
3. **Inverter/MultiPlus**: Not yet implemented, would need similar device-specific parser

## Data Display

### Web Interface

The web monitor displays all available fields:
- Common: Voltage, Current, Power, Temperature
- SmartShunt: SOC, Consumed Ah, Time to Go, Aux/Mid voltage, Alarms
- Solar Controller: Yield Today, PV Power, Load Current, State, Errors
- DC-DC: Input/Output voltages

### MQTT/Home Assistant

All fields are published via MQTT with automatic Home Assistant discovery:
- Individual topics for each sensor
- Proper units of measurement
- Device classification for Home Assistant UI
- State classes for energy monitoring

## References

- [Reference Implementation](https://github.com/chrisj7903/Read-Victron-advertised-data)
- [Victron BLE Protocol Documentation](VICTRON_BLE_PROTOCOL.md)
- Victron VE.Direct protocol documentation
- Community reverse engineering efforts

## Version History

- v1.1.0 (2025-12-07): Implemented fixed-structure parsing for SmartShunt and Solar Controller
- v1.0.0 (2025-12-06): Initial TLV-based parsing implementation

---

**Note**: This parsing implementation is based on community reverse engineering and the referenced open-source project. It is not officially endorsed by Victron Energy.
