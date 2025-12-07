# Data Retention Feature

## Overview

The Data Retention feature preserves the last valid readings from your Victron devices when BLE data parsing temporarily fails. This prevents the display from going blank or showing invalid data during brief connection issues or decoding problems.

## How It Works

### Without Data Retention (Disabled)
When a BLE scan fails to decode data properly:
1. Device data is completely replaced with the new (invalid) scan
2. All measurement values reset to defaults (0 or invalid markers)
3. The display shows "-- V", "-- A" or goes blank
4. Previous valid data is lost

### With Data Retention (Enabled - Default)
When a BLE scan fails to decode data properly:
1. Signal strength (RSSI) and timestamp are updated
2. Measurement values from the last successful scan are preserved
3. The display continues showing valid data
4. An error message is logged for troubleshooting

When a BLE scan succeeds:
- All fields are updated normally
- Any error messages are cleared
- Display shows fresh data

## Benefits

- **Better User Experience**: Display remains useful even during temporary issues
- **Easier Troubleshooting**: You can see the last known state while diagnosing problems
- **More Stable Display**: Reduces flickering and blank screens
- **Configurable**: Can be disabled if you prefer to see when data is stale

## Configuration

### Via Web Interface

1. Navigate to the Live Monitor page (`/monitor`)
2. Look for the "Display Settings" section at the top
3. Toggle "Retain Last Good Data" on or off
4. Setting is saved immediately and persists across reboots

### Default Setting

The feature is **enabled by default** as it provides the best user experience for most scenarios.

## Technical Details

### Storage
- Setting stored in ESP32 preferences namespace: `victron-data`
- Key: `retainLast` (boolean)
- Loaded on startup and applied to VictronBLE instance

### API Endpoints

**GET /api/data-retention**
```json
{
  "retainLastData": true
}
```

**POST /api/data-retention**
```
Content-Type: application/x-www-form-urlencoded
retainLastData=true
```

### Data Merging Logic

When `retainLastData` is enabled and a device is scanned:

**Always Updated:**
- Device name and type
- Signal strength (RSSI)
- Last update timestamp
- Raw BLE manufacturer data
- Encryption status

**Updated Only If New Data Is Valid:**
- Voltage, Current, Power
- Battery State of Charge (SOC)
- Temperature
- Consumed Ah, Time to Go
- Auxiliary measurements
- Solar controller data
- Inverter AC output
- DC-DC converter voltages
- Device state and errors

### Code Location

- Implementation: `src/VictronBLE.cpp` - `mergeDeviceData()` method
- Preferences: `src/main.cpp` - `loadDataRetentionConfig()`, `saveDataRetentionConfig()`
- API Handlers: `src/WebConfigServer.cpp` - `handleGetDataRetention()`, `handleSetDataRetention()`
- UI: `data/monitor.html` - Display Settings section

## Use Cases

### When to Keep Enabled (Recommended)
- Normal operation monitoring
- Devices with intermittent BLE connectivity
- Monitoring systems where continuous display is important
- Situations where brief data gaps are acceptable

### When to Disable
- Debugging BLE communication issues
- Need to see exactly when data stops updating
- Testing encryption keys
- Diagnosing real-time data parsing problems

## Troubleshooting

### "My device shows old data"
This is expected behavior when enabled. Check:
1. The timestamp in the "Last update" field shows when data was last received
2. Check signal strength (RSSI) - weak signal may cause intermittent updates
3. Look at the Debug page to see raw BLE data and error messages
4. Consider temporarily disabling retention to see real-time parsing status

### "Data keeps disappearing"
If retention is enabled but data still disappears:
1. The device might not have had any successful scans yet
2. Check encryption key configuration if device is encrypted
3. Verify device is powered on and within BLE range
4. Check serial logs for detailed error messages

### "Toggle doesn't stay enabled/disabled"
- Clear browser cache and reload the page
- Check browser console for JavaScript errors
- Verify the ESP32 has working preferences storage (not corrupted)

## Examples

### Scenario 1: Temporary Signal Loss
```
Time: 10:00:00 - Scan successful: 12.5V, 2.3A, 80% SOC ✓
Time: 10:00:30 - Scan successful: 12.4V, 2.2A, 79% SOC ✓
Time: 10:01:00 - Scan failed (weak signal)
  With Retention: Display shows 12.4V, 2.2A, 79% (last good)
  Without: Display shows --, --, --
Time: 10:01:30 - Scan successful: 12.3V, 2.1A, 79% SOC ✓
```

### Scenario 2: Encryption Key Issues
```
Time: 10:00:00 - Scan successful: 12.5V, 2.3A, 80% SOC ✓
Time: 10:00:30 - Device suddenly encrypted (key expired/changed)
  With Retention: Display shows 12.5V, 2.3A, 80% (last good)
                  Error: "Device is encrypted. Add encryption key..."
  Without: Display shows --, --, --
Time: 10:01:00 - Key updated, scan successful: 12.4V, 2.2A, 79% SOC ✓
```

## Related Documentation

- [Web Configuration Guide](WEB_CONFIGURATION.md) - General web interface usage
- [Victron BLE Protocol](VICTRON_BLE_PROTOCOL.md) - Technical protocol details
- [Data Parsing Implementation](DATA_PARSING.md) - How data is decoded
