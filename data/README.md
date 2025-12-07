# Data Directory - LittleFS Filesystem

This directory contains files that will be uploaded to the ESP32's LittleFS filesystem.

## Files

- **index.html** - Main web configuration interface
- **monitor.html** - Live monitoring dashboard

## Uploading to ESP32

### Using PlatformIO

To upload the filesystem image to your ESP32:

```bash
pio run --target uploadfs
```

Or use the PlatformIO IDE:
1. Open PlatformIO sidebar
2. Click on "Platform" → "Upload Filesystem Image"

### Using Arduino IDE

1. Install the [ESP32 Filesystem Uploader](https://github.com/me-no-dev/arduino-esp32fs-plugin)
2. Place the `data` folder in your sketch directory
3. Select "Tools" → "ESP32 Sketch Data Upload"

## Important Notes

- These files are served from flash memory, not RAM
- This significantly reduces memory usage and prevents boot loops
- The filesystem must be uploaded separately from the firmware
- Total filesystem size is limited by the partition scheme (usually 1-4 MB)
- Files are automatically mounted at boot via `LittleFS.begin()`

## Modifying HTML

To modify the web interface:
1. Edit the HTML files in this directory
2. Re-upload the filesystem using the command above
3. No firmware recompilation needed if only HTML/CSS/JS changed
