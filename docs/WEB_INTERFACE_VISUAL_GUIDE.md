# Web Configuration Interface - Visual Guide

This document provides a visual description of the web configuration interface for the ESP32-Victron project.

## Interface Overview

The web interface is a modern, responsive single-page application with a purple gradient theme that matches the Victron brand colors.

### Color Scheme
- **Primary**: Purple gradient (#667eea to #764ba2)
- **Background**: White cards on gradient background
- **Accent**: Cyan for headers, Yellow for highlights
- **Text**: Dark gray on white, White on colored backgrounds

## Main Dashboard

When you first access the web interface, you see:

### Header Section
```
┌───────────────────────────────────────────────┐
│  [Purple Gradient Background]                 │
│                                               │
│      Victron BLE Configuration              │
│     Configure your Victron devices and        │
│     encryption keys                           │
│                                               │
└───────────────────────────────────────────────┘
```

### Device List Section
```
┌───────────────────────────────────────────────┐
│   Configured Devices                         │
│  ─────────────────────────────────────────    │
│                                               │
│  Note: Add your Victron devices with their │
│  BLE MAC addresses. If your device uses       │
│  encryption, enter the 32-character           │
│  encryption key from the VictronConnect app.  │
│                                               │
│  ┌─────────────────────────────────────────┐ │
│  │ SmartShunt 500A                         │ │
│  │ AA:BB:CC:DD:EE:FF                       │ │
│  │  Instant Readout                      │ │
│  │                         [Edit] [Delete] │ │
│  ├─────────────────────────────────────────┤ │
│  │ SmartSolar MPPT 100/20                  │ │
│  │ 11:22:33:44:55:66                       │ │
│  │  Encrypted                            │ │
│  │                         [Edit] [Delete] │ │
│  └─────────────────────────────────────────┘ │
│                                               │
│  [Add Device]                               │
│                                               │
└───────────────────────────────────────────────┘
```

### WiFi Configuration Section
```
┌───────────────────────────────────────────────┐
│   WiFi Configuration                         │
│  ─────────────────────────────────────────    │
│                                               │
│  Note: Current Mode: Access Point                │
│     IP Address: 192.168.4.1                   │
│                                               │
│  [Configure WiFi]                           │
│                                               │
└───────────────────────────────────────────────┘
```

## Add/Edit Device Modal

When clicking "Add Device" or "Edit", a modal dialog appears:

```
╔═════════════════════════════════════════════╗
║  Add Device                                 ║
║  ─────────────────────────────────────      ║
║                                             ║
║  Device Name                                ║
║  ┌─────────────────────────────────────┐   ║
║  │ e.g., SmartShunt 500A               │   ║
║  └─────────────────────────────────────┘   ║
║  Friendly name for the device              ║
║                                             ║
║  BLE MAC Address                            ║
║  ┌─────────────────────────────────────┐   ║
║  │ e.g., AA:BB:CC:DD:EE:FF             │   ║
║  └─────────────────────────────────────┘   ║
║  MAC address from VictronConnect app       ║
║                                             ║
║  Encryption Key (Optional)                  ║
║  ┌─────────────────────────────────────┐   ║
║  │ e.g., 0123456789ABCDEF...           │   ║
║  └─────────────────────────────────────┘   ║
║  32-character hex key (leave empty for     ║
║  instant readout mode)                     ║
║                                             ║
║               [Cancel]  [Save]              ║
║                                             ║
╚═════════════════════════════════════════════╝
```

## WiFi Configuration Modal

When clicking "Configure WiFi", you see:

```
╔═════════════════════════════════════════════╗
║  WiFi Configuration                         ║
║  ─────────────────────────────────────      ║
║                                             ║
║  Mode                                       ║
║  ┌─────────────────────────────────────┐   ║
║  │ ▼ Access Point (AP)                │   ║
║  │   Station (Connect to WiFi)         │   ║
║  └─────────────────────────────────────┘   ║
║                                             ║
║  AP Password                                ║
║  ┌─────────────────────────────────────┐   ║
║  │ •••••••                             │   ║
║  └─────────────────────────────────────┘   ║
║  Minimum 8 characters                      ║
║                                             ║
║  Warning: Changing WiFi settings         ║
║  requires a restart to take effect.        ║
║                                             ║
║         [Cancel]  [Save & Restart]          ║
║                                             ║
╚═════════════════════════════════════════════╝
```

For Station mode, the form changes to show:

```
╔═════════════════════════════════════════════╗
║  WiFi Configuration                         ║
║  ─────────────────────────────────────      ║
║                                             ║
║  Mode                                       ║
║  ┌─────────────────────────────────────┐   ║
║  │ ▼ Station (Connect to WiFi)        │   ║
║  │   Access Point (AP)                 │   ║
║  └─────────────────────────────────────┘   ║
║                                             ║
║  WiFi SSID                                  ║
║  ┌─────────────────────────────────────┐   ║
║  │ Your WiFi network name              │   ║
║  └─────────────────────────────────────┘   ║
║                                             ║
║  WiFi Password                              ║
║  ┌─────────────────────────────────────┐   ║
║  │ •••••••                             │   ║
║  └─────────────────────────────────────┘   ║
║                                             ║
║  Warning: Changing WiFi settings         ║
║  requires a restart to take effect.        ║
║                                             ║
║         [Cancel]  [Save & Restart]          ║
║                                             ║
╚═════════════════════════════════════════════╝
```

## Empty State

When no devices are configured:

```
┌───────────────────────────────────────────────┐
│   Configured Devices                         │
│  ─────────────────────────────────────────    │
│                                               │
│  Note: Add your Victron devices...         │
│                                               │
│  ┌─────────────────────────────────────────┐ │
│  │                                         │ │
│  │            [Document Icon]              │ │
│  │                                         │ │
│  │      No devices configured yet          │ │
│  │                                         │ │
│  └─────────────────────────────────────────┘ │
│                                               │
│  [Add Device]                               │
│                                               │
└───────────────────────────────────────────────┘
```

## Responsive Design

The interface is fully responsive and works on:
-  Desktop browsers (Chrome, Firefox, Safari, Edge)
-  Tablet devices (iPad, Android tablets)
-  Mobile phones (iPhone, Android)

On mobile, the layout adapts:
- Device cards stack vertically
- Buttons are touch-friendly
- Forms are optimized for mobile input

## Button States

### Primary Buttons
- **Normal**: Purple background (#667eea)
- **Hover**: Darker purple (#5568d3)
- **Active**: Even darker purple with slight scale

### Secondary Buttons (Cancel, Delete)
- **Normal**: Gray or red background
- **Hover**: Darker shade
- **Active**: Further darkened

### Edit/Delete Buttons
- **Edit**: Purple with white text
- **Delete**: Red with white text
- Both have hover effects

## Interactive Elements

### Form Validation
- MAC address field validates pattern: `XX:XX:XX:XX:XX:XX`
- Encryption key validates 32 hex characters
- Required fields show error if empty

### Success/Error Messages
- Success: Green checkmark or notification
- Error: Red error message with details
- Info: Blue information boxes

## Accessibility Features

-  High contrast text
-  Clear visual hierarchy
-  Touch-friendly tap targets (44px minimum)
-  Keyboard navigation support
-  Screen reader compatible labels
-  Color is not the only indicator (icons + text)

## Technical Details

### Technologies Used
- Pure HTML5, CSS3, JavaScript (no frameworks)
- Embedded directly in ESP32 firmware
- ~30KB total size (compressed)
- No external dependencies

### Browser Support
- Chrome/Edge:  Full support
- Firefox:  Full support
- Safari:  Full support
- Mobile browsers:  Full support
- IE11:  Not supported

## API Integration

The interface communicates with the ESP32 via REST API:
- `GET /api/devices` - Fetch device list
- `POST /api/devices` - Add device
- `POST /api/devices/update` - Update device
- `POST /api/devices/delete` - Delete device
- `GET /api/wifi` - Get WiFi config
- `POST /api/wifi` - Update WiFi config
- `POST /api/restart` - Restart device

All responses are JSON formatted.

## Future Enhancements

Planned visual improvements:
- Real-time device status indicators
- Live data preview
- Dark mode toggle
- Custom themes
- Device icons based on type
- Data visualization graphs
- Offline indicator
- Connection status badge
