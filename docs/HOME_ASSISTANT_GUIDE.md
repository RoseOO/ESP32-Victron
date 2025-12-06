# Home Assistant / MQTT Integration Guide

## Overview

This guide explains how to integrate your ESP32-Victron BLE Monitor with Home Assistant using MQTT.

## Prerequisites

1. Home Assistant installed and running
2. MQTT broker (Mosquitto) configured in Home Assistant
3. ESP32-Victron device connected to your WiFi network
4. At least one Victron device being monitored

## Quick Setup

### Step 1: Configure MQTT Broker in Home Assistant

If you haven't already set up an MQTT broker:

1. Go to **Settings** → **Add-ons**
2. Install **Mosquitto broker** add-on
3. Start the Mosquitto broker
4. Go to **Settings** → **Devices & Services**
5. Add **MQTT** integration
6. Configure with localhost and default port 1883

### Step 2: Configure MQTT in ESP32-Victron

1. Access the web interface at `http://{device-ip}`
2. Click **"⚙️ Configure MQTT"** in the Home Assistant / MQTT section
3. Enter your settings:
   - **Enable MQTT**: Set to "Enabled"
   - **MQTT Broker Address**: Your Home Assistant IP (e.g., `192.168.1.100`)
   - **MQTT Port**: `1883` (default)
   - **Username**: MQTT username (if authentication is enabled)
   - **Password**: MQTT password (if authentication is enabled)
   - **Base Topic**: `victron` (default, can be changed)
   - **Home Assistant Auto-Discovery**: "Enabled" (recommended)
   - **Publish Interval**: `30` seconds (default, range: 5-300)
4. Click **"Save"**

### Step 3: Verify Connection

1. Check the web interface - Connection status should show "✓ Connected"
2. In Home Assistant, go to **Settings** → **Devices & Services** → **MQTT**
3. Click **"Configure"** → **"Listen to a topic"**
4. Enter topic: `victron/#`
5. You should see data being published

### Step 4: Discover Devices

Within a few seconds, new devices should appear in Home Assistant:

1. Go to **Settings** → **Devices & Services** → **MQTT**
2. Look for devices under "Victron Energy" manufacturer
3. Each Victron device will be listed by name or MAC address

## MQTT Topics

### Topic Structure

All data is published to topics following this pattern:
```
{baseTopic}/{deviceId}/{sensor}
```

Example topics:
```
victron/aa_bb_cc_dd_ee_ff/voltage
victron/aa_bb_cc_dd_ee_ff/current
victron/aa_bb_cc_dd_ee_ff/power
victron/aa_bb_cc_dd_ee_ff/battery_soc
victron/aa_bb_cc_dd_ee_ff/temperature
```

### Discovery Topics

Home Assistant auto-discovery messages are published to:
```
homeassistant/sensor/{deviceId}_{sensor}/config
```

These messages are sent once when the device connects and create sensor entities automatically.

## Sensors Created

For each Victron device, the following sensors are created (if available):

### Common Sensors
- **Voltage** - Battery/Solar voltage in Volts (V)
- **Current** - Current flow in Amperes (A)
- **Power** - Power in Watts (W)
- **Temperature** - Device temperature in Celsius (°C)
- **RSSI** - Signal strength in dBm

### Smart Shunt Specific
- **Battery SOC** - State of Charge in percentage (%)

### Inverter Specific
- **AC Output Voltage** - AC output voltage in Volts (V)
- **AC Output Power** - AC output power in Watts (W)

### DC-DC Converter Specific
- **Input Voltage** - Input voltage in Volts (V)
- **Output Voltage** - Output voltage in Volts (V)

## Entity IDs

Entities are created with IDs following this format:
```
sensor.{device_id}_{sensor}
```

Examples:
```
sensor.aa_bb_cc_dd_ee_ff_voltage
sensor.aa_bb_cc_dd_ee_ff_current
sensor.aa_bb_cc_dd_ee_ff_power
sensor.aa_bb_cc_dd_ee_ff_battery_soc
```

## Creating Dashboards

### Simple Dashboard Example

1. Go to **Overview** in Home Assistant
2. Click **"Edit Dashboard"** → **"Add Card"**
3. Choose **"Entities"** card
4. Add sensors:
   - `sensor.aa_bb_cc_dd_ee_ff_voltage`
   - `sensor.aa_bb_cc_dd_ee_ff_current`
   - `sensor.aa_bb_cc_dd_ee_ff_power`
   - `sensor.aa_bb_cc_dd_ee_ff_battery_soc`

### Energy Dashboard

For Smart Shunt devices:

1. Go to **Settings** → **Dashboards** → **Energy**
2. Click **"Add Consumption"**
3. Select the power sensor: `sensor.aa_bb_cc_dd_ee_ff_power`

### YAML Dashboard Example

```yaml
type: entities
title: Victron Battery Monitor
entities:
  - entity: sensor.aa_bb_cc_dd_ee_ff_voltage
    name: Battery Voltage
  - entity: sensor.aa_bb_cc_dd_ee_ff_current
    name: Current
  - entity: sensor.aa_bb_cc_dd_ee_ff_power
    name: Power
  - entity: sensor.aa_bb_cc_dd_ee_ff_battery_soc
    name: State of Charge
  - entity: sensor.aa_bb_cc_dd_ee_ff_temperature
    name: Temperature
  - entity: sensor.aa_bb_cc_dd_ee_ff_rssi
    name: Signal Strength
```

## Advanced Configuration

### Custom Base Topic

If you have multiple ESP32-Victron devices:

1. Set different base topics for each device
2. Example: `victron_van`, `victron_boat`, etc.
3. Topics will be: `victron_van/device_id/sensor`

### Adjust Publish Interval

- **Fast updates (5-10 seconds)**: Good for monitoring, higher network traffic
- **Normal updates (30 seconds)**: Balanced between freshness and efficiency
- **Slow updates (60-120 seconds)**: Lower network traffic, less current drain

### Authentication

If your MQTT broker requires authentication:

1. Create a user in Mosquitto configuration
2. Enter username and password in ESP32-Victron MQTT config
3. Leave blank if not using authentication

## Troubleshooting

### Device Not Connecting to MQTT

**Check connection status:**
1. Web interface shows "✗ Disconnected"
2. Verify broker IP address is correct
3. Check broker is running: `mosquitto -v` or check Home Assistant add-on
4. Verify username/password if using authentication
5. Check firewall rules (port 1883 must be open)

**Serial Monitor Debugging:**
1. Connect USB cable
2. Monitor serial output: `pio device monitor` or Arduino Serial Monitor
3. Look for "MQTT connected!" or error messages

### Sensors Not Appearing in Home Assistant

1. **Check MQTT integration is configured**
   - Settings → Devices & Services → MQTT
   
2. **Verify auto-discovery is enabled**
   - ESP32-Victron web interface → MQTT config
   
3. **Listen to discovery topic:**
   - MQTT → Configure → Listen to topic: `homeassistant/#`
   - Should see discovery messages
   
4. **Restart Home Assistant**
   - Sometimes needed to pick up new discovery messages
   
5. **Check device is scanning and finding Victron devices**
   - Web interface → Live Monitor
   - Should show devices with data

### Data Not Updating

1. **Check publish interval** - Set to 30 seconds by default
2. **Verify ESP32 has WiFi connection** - Check web interface accessibility
3. **Check Victron device is in range** - RSSI should be > -80 dBm
4. **Monitor MQTT topics** - Listen to `victron/#` in Home Assistant

### Permission Denied Errors

If MQTT broker has ACL (Access Control List) configured:

1. Ensure MQTT user has publish permission for `victron/#`
2. Ensure MQTT user has publish permission for `homeassistant/#`
3. Check Mosquitto configuration for ACL settings

## Performance Considerations

### Network Traffic

With default settings (30 second interval):
- Each sensor update: ~50 bytes
- 10 sensors per device: ~500 bytes per update
- Network traffic: ~1 KB/minute per device

### Memory Usage

- MQTT client: ~4 KB RAM
- Message buffer: ~512 bytes per message
- Total overhead: ~8-10 KB RAM

### Battery Life

For M5StickC PLUS running on battery:
- WiFi and MQTT active: ~2-3 hours
- Consider USB power or external battery for continuous operation

## Example Automations

### Battery Low Alert

```yaml
automation:
  - alias: "Battery Low Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.aa_bb_cc_dd_ee_ff_battery_soc
        below: 20
    action:
      - service: notify.mobile_app
        data:
          message: "Battery SOC is low: {{ states('sensor.aa_bb_cc_dd_ee_ff_battery_soc') }}%"
```

### High Power Consumption Alert

```yaml
automation:
  - alias: "High Power Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.aa_bb_cc_dd_ee_ff_power
        above: 1000
        for:
          minutes: 5
    action:
      - service: notify.mobile_app
        data:
          message: "Power consumption high: {{ states('sensor.aa_bb_cc_dd_ee_ff_power') }}W"
```

### Solar Charging Notification

```yaml
automation:
  - alias: "Solar Charging Started"
    trigger:
      - platform: numeric_state
        entity_id: sensor.solar_mppt_current
        above: 0.5
    action:
      - service: notify.mobile_app
        data:
          message: "Solar panels are charging at {{ states('sensor.solar_mppt_current') }}A"
```

## Support

For issues or questions:
1. Check the main README.md for general information
2. Review CHANGELOG.md for version-specific notes
3. Open an issue on GitHub: https://github.com/RoseOO/ESP32-Victron/issues

## References

- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
- [Mosquitto MQTT Broker](https://mosquitto.org/)
- [MQTT Protocol Specification](https://mqtt.org/)
