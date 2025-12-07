# Over-The-Air (OTA) Updates

This guide explains how to use the Over-The-Air (OTA) update feature to wirelessly update your ESP32-Victron firmware without needing a USB cable.

## Overview

ArduinoOTA allows you to upload new firmware to your ESP32 device over WiFi. This is particularly useful when your M5StickC PLUS2 is installed in a location that's difficult to access physically.

## Features

- **Wireless Updates**: Upload firmware via WiFi network
- **Password Protected**: Secure updates with password authentication
- **Visual Feedback**: Progress bar and status displayed on M5 screen
- **Error Handling**: Clear error messages if update fails
- **Dual Mode Support**: Works in both AP mode and WiFi client mode

## Prerequisites

Before using OTA updates, ensure:

1. Your device is powered on and running
2. WiFi is configured and connected (either AP mode or Station mode)
3. You know the device's IP address
4. You have the OTA password (default: `victron123`)

## Finding Your Device's IP Address

### Method 1: Serial Monitor
Connect via USB and check the serial output during startup:
```
STARTUP: ArduinoOTA initialized
OTA: Ready for updates
OTA: Hostname: ESP32-Victron, IP: 192.168.1.100
```

### Method 2: Web Configuration Screen
Long-press the M5 button to view the web configuration screen, which displays the current IP address.

### Method 3: Network Scan
Use mDNS discovery to find the device on your network:
```bash
# Linux/Mac
avahi-browse -r -t _arduino._tcp

# Windows (with Bonjour)
dns-sd -B _arduino._tcp
```

The device broadcasts as: `ESP32-Victron.local`

## Uploading Firmware

### Using PlatformIO

#### Command Line:
```bash
# Upload to specific IP address
pio run --target upload --upload-port 192.168.1.100

# Or use the hostname (if mDNS is working)
pio run --target upload --upload-port ESP32-Victron.local
```

#### VS Code:
1. Open PlatformIO sidebar
2. Click on "PROJECT TASKS"
3. Expand your environment (e.g., m5stick-c-plus2)
4. Click "Upload" under "Platform"
5. When prompted, enter the IP address or hostname

#### platformio.ini Configuration (Optional):
You can add OTA configuration to your `platformio.ini`:
```ini
[env:m5stick-c-plus2]
; ... existing configuration ...

; OTA Configuration
upload_protocol = espota
upload_port = 192.168.1.100  ; or ESP32-Victron.local
upload_flags = 
    --auth=victron123
```

### Using Arduino IDE

1. **Ensure Arduino is configured for ESP32**:
   - Install ESP32 board support if not already installed
   - Select "ESP32 Dev Module" as the board

2. **Find the Network Port**:
   - Go to **Tools → Port**
   - Look for "ESP32-Victron at [IP_ADDRESS]" in the port list
   - This should appear after the device has been on WiFi for a few seconds

3. **Select the Network Port**:
   - Click on "ESP32-Victron at [IP_ADDRESS]"

4. **Upload**:
   - Click the Upload button as usual
   - When prompted, enter the password: `victron123`

5. **Monitor Progress**:
   - The M5 screen will show upload progress
   - Arduino IDE will show upload status

## OTA Update Process

When an OTA update starts, the device:

1. **Displays "OTA Update" screen**: The M5 display clears and shows the update status
2. **Shows Progress**: A progress bar displays the upload percentage
3. **Completes**: Shows "Update Complete!" message
4. **Reboots**: Automatically restarts with new firmware

### Progress Display

```
┌─────────────────────────────┐
│ OTA Update                  │
│                             │
│ Starting...                 │
│                             │
│ [████████░░░░░░░░░░░░]      │
│ Progress: 45%               │
└─────────────────────────────┘
```

## Troubleshooting

### Device Not Found in Port List

**Possible causes:**
- Device is not connected to WiFi
- mDNS is not working on your network
- Firewall is blocking OTA port (default: 3232)

**Solutions:**
1. Check serial monitor to confirm WiFi connection
2. Use IP address instead of hostname
3. Ensure device and computer are on same network
4. Temporarily disable firewall to test

### Authentication Failed

**Error:** `Auth Failed` displayed on screen

**Solutions:**
- Verify you're using the correct password (default: `victron123`)
- If you changed the password in code, use the updated password
- Try uploading again

### Upload Timeout

**Symptoms:**
- Upload starts but never completes
- Connection times out

**Solutions:**
1. Check WiFi signal strength
2. Move device closer to WiFi router
3. Restart the device and try again
4. Reduce network traffic during upload

### Begin Failed / End Failed

**Symptoms:**
- Upload fails during initialization or finalization

**Solutions:**
1. Ensure sufficient free flash space
2. Try rebooting the device
3. Upload via USB first, then try OTA again

### Receive Failed

**Symptoms:**
- Upload fails during data transfer

**Solutions:**
1. Check WiFi stability
2. Try uploading smaller code changes
3. Upload via USB to reset OTA partition

## Security Considerations

### Default Password

The default OTA password is `victron123`. This is suitable for testing but should be changed for production use.

### Changing the OTA Password

Edit `src/main.cpp` and change this line:
```cpp
ArduinoOTA.setPassword("victron123");  // Change to your secure password
```

Replace `"victron123"` with your own secure password.

### Network Security

- OTA updates are not encrypted beyond the password
- Only use on trusted networks
- Consider using WPA2/WPA3 secured WiFi
- Do not expose device directly to the internet

## Advanced Configuration

### Custom Hostname

To change the device hostname, edit `src/main.cpp`:
```cpp
ArduinoOTA.setHostname("ESP32-Victron");  // Change to your preferred name
```

### Custom OTA Port

The default port is 3232. To change it:
```cpp
ArduinoOTA.setPort(8266);  // Custom port
```

### Disable OTA

If you want to disable OTA for any reason, comment out or remove the OTA initialization code in `setup()`:
```cpp
// ArduinoOTA.begin();  // Disabled
```

## Filesystem Updates

Currently, OTA supports firmware updates. To update the LittleFS filesystem (web interface files), you still need to use USB:
```bash
pio run --target uploadfs
```

**Note:** Future versions may support OTA filesystem updates.

## Best Practices

1. **Always test locally first**: Test firmware changes via USB before deploying via OTA
2. **Keep backup firmware**: Save a known-good version in case of issues
3. **Stable power supply**: Ensure device has stable power during OTA update
4. **Good WiFi signal**: Perform updates when device has strong WiFi connection
5. **Update during low usage**: Update when monitoring is less critical
6. **Monitor serial output**: Keep USB connected during first OTA tests to see debug messages

## Technical Details

### OTA Partition Layout

The ESP32 uses a dual-partition scheme for OTA:
- **app0**: Current running firmware
- **app1**: OTA update target partition
- **otadata**: Tracks which partition to boot from

After successful OTA update, the device boots from the new partition.

### Memory Requirements

- OTA requires enough free flash space for the new firmware
- Temporary buffer in RAM during upload
- Ensure your firmware size doesn't exceed partition size

### Network Requirements

- UDP port 3232 (default) must be accessible
- mDNS port 5353 for hostname resolution
- Device and upload computer must be on same network subnet (typically)

## Examples

### Example 1: Quick Update via PlatformIO
```bash
# First, find device IP
pio device list

# Upload firmware
pio run --target upload --upload-port 192.168.1.100
```

### Example 2: Update Multiple Devices
If you have multiple ESP32-Victron devices, specify each one:
```bash
# Device 1
pio run --target upload --upload-port 192.168.1.100

# Device 2  
pio run --target upload --upload-port 192.168.1.101

# Device 3
pio run --target upload --upload-port 192.168.1.102
```

### Example 3: Automated Build and Deploy Script
Create a script `deploy.sh`:
```bash
#!/bin/bash
DEVICE_IP="192.168.1.100"

echo "Building firmware..."
pio run

echo "Uploading to $DEVICE_IP..."
pio run --target upload --upload-port $DEVICE_IP

echo "Deployment complete!"
```

## Conclusion

OTA updates make it easy to keep your ESP32-Victron firmware up-to-date without physical access to the device. Just ensure you have a stable network connection and the device is accessible on your WiFi network.

For further assistance, see the [Troubleshooting](#troubleshooting) section or open an issue on GitHub.
