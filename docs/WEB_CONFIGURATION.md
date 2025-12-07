# Web Configuration Interface

The ESP32-Victron project now includes a web-based configuration interface that allows you to configure WiFi settings and manage Victron devices with their encryption keys.

## Features

### 1. WiFi Configuration
- **Access Point Mode**: Creates a WiFi access point named "Victron-Config"
- **Station Mode**: Connects to your existing WiFi network
- Easy switching between modes
- Persistent configuration storage

### 2. Device Management
- Add Victron devices with their BLE MAC addresses
- Configure encryption keys for encrypted devices
- Enable/disable individual devices
- Delete devices from the configuration
- View all configured devices in a user-friendly interface

### 3. Encryption Key Support
- Store 128-bit AES encryption keys (32 hex characters)
- Support for both encrypted and instant readout modes
- Secure storage using ESP32 Preferences

## Quick Start

### First Time Setup

1. **Power on your M5StickC PLUS2** with the new firmware
2. **Connect to WiFi**: The device will create an Access Point named "Victron-Config"
   - Default password: `victron123`
3. **Open web browser** and navigate to: `http://192.168.4.1`
4. You'll see the web configuration interface

### Using the Web Interface

#### Main Dashboard

The web interface displays:
- List of configured devices
- WiFi configuration status
- Current IP address

#### Adding a Device

1. Click **"Add Device"** button
2. Fill in the form:
   - **Device Name**: Friendly name (e.g., "Solar MPPT")
   - **BLE MAC Address**: Device MAC from VictronConnect app (e.g., `AA:BB:CC:DD:EE:FF`)
   - **Encryption Key** (optional): 32-character hex key for encrypted devices
3. Click **"Save"**

The device will be saved to persistent storage and loaded on every boot.

#### Finding Device Information

To get your device's BLE MAC address and encryption key:

1. Open the **VictronConnect** app on your smartphone
2. Connect to your Victron device
3. Go to **Settings** → **Product Info**
   - The MAC address is shown as "Bluetooth Address"
4. For the encryption key:
   - Go to **Settings** → **BLE**
   - If encryption is enabled, you'll see the encryption key
   - If not enabled, leave the encryption key field empty (instant readout mode)

#### Editing a Device

1. Click **"Edit"** next to the device you want to modify
2. Update any fields
3. Click **"Save"**

#### Deleting a Device

1. Click **"Delete"** next to the device you want to remove
2. Confirm the deletion

#### WiFi Configuration

1. Click **"Configure WiFi"**
2. Choose mode:
   - **Access Point**: Device creates its own WiFi network
   - **Station**: Device connects to your WiFi network
3. For Station mode:
   - Enter your WiFi SSID
   - Enter your WiFi password
4. Click **"Save & Restart"**

The device will restart and apply the new WiFi settings.

## Display Interface

### Viewing Web Config Info on Screen

- **Short press** M5 button: Cycle through devices (normal mode)
- **Long press** M5 button (1 second): Toggle web config display

The web config screen shows:
- Current WiFi mode (AP or Station)
- IP address
- Instructions to open web browser

## WiFi Modes Explained

### Access Point Mode (Default)

**Pros:**
- No existing WiFi network required
- Always accessible
- Simple setup

**Cons:**
- Limited range
- Device not on your network
- Must connect to device's WiFi to configure

**Use case:** Initial setup, portable installations, when no WiFi available

### Station Mode

**Pros:**
- Device on your home/office network
- Can access from any device on network
- No need to switch WiFi networks

**Cons:**
- Requires existing WiFi network
- Must know WiFi credentials
- Device may be harder to find if network has issues

**Use case:** Permanent installations, when WiFi is available

## Security Considerations

### Encryption Keys

- Encryption keys are stored in ESP32 Preferences (flash memory)
- Keys are not transmitted over the network except during configuration
- Use strong AP password when in Access Point mode
- Consider changing default AP password

### Web Interface Security

The current implementation:
- No default admin credentials required
- Configuration changes require explicit save actions
- No HTTPS (uses HTTP)
- No authentication mechanism

**Recommendations:**
- Use only on trusted networks
- Change AP password from default
- Don't expose device to internet
- Consider enabling encryption on your Victron devices

## Troubleshooting

### Cannot Connect to Web Interface

1. **Check WiFi connection**:
   - In AP mode: Ensure you're connected to "Victron-Config" network
   - In Station mode: Check device is on same network as your computer

2. **Check IP address**:
   - Long-press M5 button to view IP address on screen
   - In AP mode: Usually `192.168.4.1`
   - In Station mode: Check your router's DHCP list

3. **Browser issues**:
   - Try clearing browser cache
   - Try a different browser
   - Ensure JavaScript is enabled

### Device Not Receiving Data After Configuration

1. **Verify MAC address**: Check that the BLE MAC address is correct
2. **Check encryption key**: 
   - If device is encrypted, ensure key is exactly 32 hex characters
   - If device uses instant readout, leave key empty
3. **Check device is in range**: BLE range is typically 5-10 meters
4. **Restart device**: Power cycle the M5StickC PLUS2

### WiFi Connection Failed

1. **Verify WiFi credentials**: Check SSID and password are correct
2. **Signal strength**: Ensure device is within range of WiFi router
3. **WiFi network**: Ensure network is 2.4 GHz (ESP32 doesn't support 5 GHz)
4. **Fallback to AP**: Device will automatically create AP if connection fails

### Encryption Not Working

Full AES-128-CTR decryption is now implemented and supported!

If you experience issues with encrypted devices:

1. **Verify the encryption key is correct**:
   - Get the key from VictronConnect app (Settings → Product Info → Show Encryption Data)
   - Ensure it's exactly 32 hexadecimal characters
   - Copy/paste carefully to avoid typos

2. **Check the key match byte**:
   - The device will log a warning if the encryption key match byte (byte 7 of the BLE packet) doesn't match the first byte of your key
   - This typically indicates an incorrect encryption key

3. **Alternative - Use Instant Readout mode**:
   If you prefer unencrypted data:
   - Open VictronConnect app
   - Connect to your device
   - Go to Settings → BLE
   - Enable "Instant Readout" mode
   - Leave encryption key field empty in web interface

## API Reference

For developers who want to integrate with the web interface programmatically:

### GET /api/devices
Returns list of configured devices in JSON format.

**Response:**
```json
[
  {
    "name": "SmartShunt 500A",
    "address": "AA:BB:CC:DD:EE:FF",
    "encryptionKey": "0123456789ABCDEF0123456789ABCDEF",
    "enabled": true
  }
]
```

### POST /api/devices
Add a new device.

**Parameters:**
- `name`: Device name
- `address`: BLE MAC address
- `encryptionKey`: Encryption key (optional)

**Response:**
```json
{"success": true}
```

### POST /api/devices/update
Update an existing device.

**Parameters:**
- `address`: Device address to update
- `name`: New device name
- `encryptionKey`: New encryption key
- `enabled`: true/false

### POST /api/devices/delete
Delete a device.

**Parameters:**
- `address`: Device address to delete

### GET /api/wifi
Get current WiFi configuration.

**Response:**
```json
{
  "ssid": "MyNetwork",
  "apMode": false,
  "apPassword": "victron123"
}
```

### POST /api/wifi
Update WiFi configuration.

**Parameters:**
- `ssid`: WiFi SSID (for station mode)
- `password`: WiFi password (for station mode)
- `apMode`: "true" or "false"
- `apPassword`: AP password (for AP mode)

### POST /api/restart
Restart the device.

## Advanced Configuration

### Changing Default AP Password

Edit `WebConfigServer.cpp` and change the default password:

```cpp
WiFiConfig() : apMode(true), apPassword("your-new-password") {}
```

### Customizing WiFi SSID

Edit `WebConfigServer.cpp`:

```cpp
WiFi.softAP("Your-Custom-SSID", wifiConfig.apPassword.c_str());
```

### Port Configuration

The web server runs on port 80 by default. To change:

```cpp
server = new AsyncWebServer(8080);  // Use port 8080
```

## Future Enhancements

Planned features for future releases:

- Web configuration interface (implemented)
- Encryption key storage (implemented)
- Full AES-128-CTR decryption (implemented)
- HTTPS support (planned)
- Authentication/login system (planned)
- OTA firmware updates via web interface (planned)
- Real-time data viewing in web interface (planned)
- Historical data graphs (planned)
- MQTT integration configuration (implemented)
- Mobile-responsive UI improvements (implemented)

## Support

For issues, questions, or contributions:
- GitHub Issues: https://github.com/RoseOO/ESP32-Victron/issues
- Pull Requests welcome!

## License

This feature is part of the ESP32-Victron project and is released under the MIT License.
