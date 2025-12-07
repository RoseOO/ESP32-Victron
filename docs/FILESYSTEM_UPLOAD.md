# LittleFS Filesystem Upload Guide

## Overview

The ESP32-Victron project uses LittleFS to store web interface files (HTML, CSS, JavaScript) in the ESP32's flash memory. This approach prevents boot loops caused by large HTML strings consuming too much RAM.

## Why LittleFS?

- **Memory Efficiency**: HTML files are stored in flash, not RAM
- **Prevents Boot Loops**: Avoids memory exhaustion during initialization
- **Easy Updates**: Web interface can be updated without recompiling firmware
- **Separation of Concerns**: Web assets are separate from application code

## Files in data/ Directory

```
data/
├── README.md          # This file
├── index.html         # Main configuration interface (~25 KB)
└── monitor.html       # Live monitoring dashboard (~9 KB)
```

## Upload Methods

### Method 1: PlatformIO (Recommended)

#### Command Line

```bash
# Upload filesystem only
pio run --target uploadfs

# Upload both firmware and filesystem
pio run --target upload
pio run --target uploadfs
```

#### PlatformIO IDE (VS Code)

1. Open the project in VS Code with PlatformIO extension
2. Click on the PlatformIO icon in the left sidebar
3. Expand "PROJECT TASKS" → "m5stick-c-plus2"
4. Under "Platform", click "Upload Filesystem Image"

### Method 2: Arduino IDE

#### Prerequisites

Install the ESP32 filesystem uploader plugin:

1. Download [ESP32FS Plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases)
2. Extract to `<Arduino>/tools/ESP32FS/tool/esp32fs.jar`
3. Restart Arduino IDE

#### Upload Steps

1. Open the sketch in Arduino IDE
2. Ensure the `data` folder is in your sketch directory
3. Select your board: "Tools" → "Board" → "ESP32 Dev Module"
4. Select the correct port: "Tools" → "Port"
5. Click "Tools" → "ESP32 Sketch Data Upload"
6. Wait for upload to complete

### Method 3: esptool.py (Advanced)

For manual uploading using esptool:

```bash
# Generate filesystem image
mklittlefs -c data -s 0x100000 littlefs.bin

# Upload to ESP32 (adjust offset for your partition scheme)
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x290000 littlefs.bin
```

## First-Time Setup

When flashing a new ESP32 or after erasing flash:

1. **Upload Firmware First**
   ```bash
   pio run --target upload
   ```

2. **Upload Filesystem**
   ```bash
   pio run --target uploadfs
   ```

3. **Verify** - The device should boot without loops and serve web interface

## Troubleshooting

### "LittleFS mount failed" Error

**Symptoms**: Serial output shows "ERROR: Failed to mount LittleFS!"

**Solutions**:
1. Ensure filesystem was uploaded: `pio run --target uploadfs`
2. Try erasing flash completely: `pio run --target erase`
3. Re-upload firmware and filesystem

### Web Interface Shows 404/500 Errors

**Symptoms**: Cannot access web interface, or pages return errors

**Solutions**:
1. Verify files exist in data/ directory
2. Re-upload filesystem image
3. Check serial output for LittleFS mount status
4. Ensure filesystem partition is large enough (1-4 MB recommended)

### Out of Memory / Boot Loop

**Symptoms**: ESP32 continuously reboots, shows SW_RESET

**Solutions**:
1. This should be fixed by using LittleFS instead of PROGMEM
2. If still occurring, check for other memory-heavy operations
3. Reduce CORE_DEBUG_LEVEL in platformio.ini
4. Use PSRAM for large allocations (already enabled)

## Partition Scheme

The default partition scheme allocates space for:
- **Firmware** (~1.5 MB)
- **LittleFS** (~1-2 MB)
- **OTA Updates** (future use)

To customize partitions, create `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x20000,
```

Then add to `platformio.ini`:
```ini
board_build.partitions = partitions.csv
```

## Modifying Web Interface

To update the web interface:

1. Edit HTML files in `data/` directory
2. Re-upload filesystem: `pio run --target uploadfs`
3. No firmware recompilation needed
4. Refresh browser to see changes

## File Size Considerations

- **index.html**: ~25 KB (configuration interface)
- **monitor.html**: ~9 KB (live monitoring)
- **Total**: ~34 KB of ~1-2 MB available

Plenty of room for future additions:
- CSS/JS libraries
- Images and icons
- Additional pages
- Configuration files

## Security Note

Currently, the web interface uses HTTP (not HTTPS) and has no authentication. Consider:
- Use only on trusted networks
- Add authentication in future releases
- Implement HTTPS for production use

## References

- [LittleFS Documentation](https://github.com/littlefs-project/littlefs)
- [ESP32 LittleFS](https://github.com/lorol/LITTLEFS)
- [PlatformIO Filesystem](https://docs.platformio.org/en/latest/platforms/espressif32.html#uploading-files-to-file-system)
