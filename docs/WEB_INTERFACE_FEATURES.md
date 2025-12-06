# Web Interface Features - Visual Guide

## Main Configuration Page

**URL:** `http://{device-ip}/`

### Header
- Title: "🔧 Victron BLE Configuration"
- Description: "Configure your Victron devices and encryption keys"

### Sections

#### 1. Configured Devices
- Shows list of all configured Victron devices
- Each device card displays:
  - Device name
  - BLE MAC address
  - Encryption status (🔒 Encrypted / 🔓 Instant Readout)
  - Edit and Delete buttons
- "➕ Add Device" button to add new devices
- **NEW:** "📊 Live Monitor" button (top right) - links to real-time monitoring page

#### 2. WiFi Configuration
- Current Mode display (Access Point / Station)
- IP Address display
- "⚙️ Configure WiFi" button

#### 3. Home Assistant / MQTT (NEW)
- Status display (Enabled/Disabled)
- Broker address display
- Connection status (✓ Connected / ✗ Disconnected)
- "⚙️ Configure MQTT" button

### Modals

#### Add/Edit Device Modal
- Device Name field
- BLE MAC Address field (with validation)
- Encryption Key field (optional, 32 hex characters)
- Cancel and Save buttons

#### WiFi Configuration Modal
- Mode selector (AP / Station)
- SSID field (for Station mode)
- Password field (for Station mode)
- AP Password field (for AP mode)
- Warning about restart requirement
- Cancel and "Save & Restart" buttons

#### MQTT Configuration Modal (NEW)
- Enable MQTT toggle
- MQTT Broker Address field
- MQTT Port field (default: 1883)
- Username field (optional)
- Password field (optional)
- Base Topic field (default: "victron")
- Home Assistant Auto-Discovery toggle
- Publish Interval field (5-300 seconds)
- Info box explaining auto-discovery
- Cancel and Save buttons

---

## Live Monitor Page (NEW)

**URL:** `http://{device-ip}/monitor`

### Header
- Title: "📊 Live Monitor"
- "⚙️ Configuration" button (returns to config page)

### Device Grid
- Responsive card layout (auto-fills based on screen width)
- Each device card shows:
  - **Header:**
    - Device name
    - Device type (Smart Shunt, Smart Solar, Inverter, etc.)
    - Signal indicator (colored circle: green/yellow/red)
    - RSSI value in dBm
  - **Data Rows:** (shown only if available)
    - Voltage (V)
    - Current (A)
    - Power (W)
    - Battery SOC (%)
    - Temperature (°C)
    - AC Output (V) - for inverters
    - AC Power (W) - for inverters
    - Input Voltage (V) - for DC-DC converters
    - Output Voltage (V) - for DC-DC converters

### Features
- Auto-refresh every 2 seconds
- Shows "Loading devices..." initially
- Shows "No devices found. Scanning..." if no devices
- Last update timestamp at bottom
- Hover effect on device cards
- Clean, modern gradient design

### Color Coding
- **Signal Strength:**
  - Green: RSSI > -60 dBm (Excellent)
  - Yellow: RSSI -60 to -80 dBm (Good)
  - Red: RSSI < -80 dBm (Poor)

---

## Design Features

### Overall Theme
- Purple gradient background (#667eea to #764ba2)
- White content cards with rounded corners
- Modern, clean aesthetic
- Fully responsive (mobile-friendly)

### Interactions
- Hover effects on buttons and cards
- Smooth transitions
- Clear visual feedback
- Loading states for data fetching

### Typography
- Clear hierarchy with color-coded text
- Monospace font for MAC addresses
- Icons for visual clarity (emojis)

---

## API Endpoints (for developers)

### Configuration APIs
- `GET /api/devices` - Get configured devices
- `POST /api/devices` - Add new device
- `POST /api/devices/update` - Update device
- `POST /api/devices/delete` - Delete device
- `GET /api/wifi` - Get WiFi configuration
- `POST /api/wifi` - Update WiFi configuration

### Live Data APIs (NEW)
- `GET /api/devices/live` - Get live data from all discovered devices

### MQTT APIs (NEW)
- `GET /api/mqtt` - Get MQTT configuration
- `POST /api/mqtt` - Update MQTT configuration

### System APIs
- `POST /api/restart` - Restart device

---

## Home Assistant Integration

### Automatic Discovery
When MQTT is enabled with Home Assistant auto-discovery:

1. Device connects to MQTT broker
2. Publishes discovery messages to `homeassistant/sensor/{device}_{sensor}/config`
3. Home Assistant automatically creates sensor entities
4. Entities appear under "Victron Energy" manufacturer

### Example Entities Created
For a SmartShunt device with MAC `AA:BB:CC:DD:EE:FF`:
- `sensor.aa_bb_cc_dd_ee_ff_voltage`
- `sensor.aa_bb_cc_dd_ee_ff_current`
- `sensor.aa_bb_cc_dd_ee_ff_power`
- `sensor.aa_bb_cc_dd_ee_ff_battery_soc`
- `sensor.aa_bb_cc_dd_ee_ff_temperature`
- `sensor.aa_bb_cc_dd_ee_ff_rssi`

### MQTT Topic Structure
- State topics: `victron/{device_id}/{sensor}`
- Example: `victron/aa_bb_cc_dd_ee_ff/voltage`
- Retained: No (regular updates)
- QoS: 0 (at most once delivery)

---

## User Workflow

### Initial Setup
1. Power on M5StickC PLUS
2. Connect to "Victron-Config" WiFi (password: victron123)
3. Navigate to http://192.168.4.1
4. Add Victron devices with MAC addresses
5. (Optional) Configure WiFi to connect to home network
6. (Optional) Configure MQTT for Home Assistant

### Regular Use
1. View live data at http://{device-ip}/monitor
2. Or use M5StickC PLUS display to cycle through devices
3. Data automatically published to Home Assistant via MQTT

### Home Assistant Setup
1. Ensure MQTT broker is configured in Home Assistant
2. In device web interface, configure MQTT settings
3. Enable Home Assistant Auto-Discovery
4. Save settings
5. Check Home Assistant - sensors appear automatically
6. Add sensors to dashboards as desired
