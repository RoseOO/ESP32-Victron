# Boot Loop Fix - Visual Diagrams

## Problem: Boot Loop

```
┌─────────────────────────────────────────────────────┐
│                  ESP32 Boot Sequence                │
│                    (BEFORE FIX)                     │
└─────────────────────────────────────────────────────┘

Power On
   │
   ▼
┌──────────────┐
│ Bootloader   │ Load firmware from flash
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  setup()     │
│  begins...   │
└──────┬───────┘
       │
       ├─► M5.begin()                        OK (~25 KB RAM)
       │
       ├─► victron = new VictronBLE()        OK (~45 KB RAM)
       │
       ├─► webServer = new WebConfigServer() OK (~5 KB RAM)
       │
       ├─► webServer->begin()                FAIL!
       │      │
       │      ├─► Load INDEX_HTML (~25 KB)   Out of memory!
       │      └─► Load MONITOR_HTML (~9 KB)  Out of memory!
       │
       ▼
   ┌────────────────┐
   │ WATCHDOG TIMER │ System crashed!
   │   TRIGGERED    │
   └────────┬───────┘
            │
            ▼
   ┌────────────────┐
   │   SW_RESET     │ rst:0x3
   └────────┬───────┘
            │
            └──────► BOOT LOOP (repeats forever)
```

## Solution: LittleFS Filesystem

```
┌─────────────────────────────────────────────────────┐
│                  ESP32 Boot Sequence                │
│                     (AFTER FIX)                     │
└─────────────────────────────────────────────────────┘

Power On
   │
   ▼
┌──────────────┐
│ Bootloader   │ Load firmware from flash
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  setup()     │
│  begins...   │
└──────┬───────┘
       │
       ├─► M5.begin()                        OK (~25 KB RAM)
       │
       ├─► victron = new VictronBLE()        OK (~45 KB RAM)
       │
       ├─► webServer = new WebConfigServer() OK (~5 KB RAM)
       │
       ├─► webServer->begin()                OK
       │      │
       │      ├─► LittleFS.begin()            Mount filesystem
       │      │   (HTML stays in flash)      (0 KB RAM used!)
       │      │
       │      └─► startServer()               Setup routes
       │
       ├─► mqttPublisher->begin()            OK (~15 KB RAM)
       │
       ▼
   ┌────────────────┐
   │   loop()       │ System running!
   │   started      │ ~155 KB free RAM
   └────────────────┘

When user requests web page:
   │
   ▼
┌──────────────────────────────────────────┐
│  HTTP Request: GET /                     │
└──────────────┬───────────────────────────┘
               │
               ▼
   ┌────────────────────────────┐
   │ Read /index.html from      │
   │ LittleFS (flash)           │ Only ~4 KB buffer needed
   │ Stream directly to client  │ (not 25 KB!)
   └────────────────────────────┘
               │
               ▼
   ┌────────────────────────────┐
   │ Send to web browser        │
   └────────────────────────────┘
```

## Memory Layout Comparison

### Before Fix (Boot Loop)

```
┌─────────────────────────────────────────────────┐
│            ESP32 RAM (320 KB Total)             │
├─────────────────────────────────────────────────┤
│                                                 │
│  Arduino Framework          ~50 KB              │
│  ├─ Core libraries                              │
│  ├─ WiFi stack                                  │
│  └─ Network buffers                             │
│                                                 │
│  BLE Stack (NimBLE)         ~45 KB              │
│  ├─ Connection management                       │
│  ├─ Advertisement parsing                       │
│  └─ Scan buffers                                │
│                                                 │
│  M5StickCPlus2 Libraries    ~25 KB              │
│  ├─ Display driver                              │
│  ├─ Button handling                             │
│  └─ Power management                            │
│                                                 │
│  MQTT Client                ~15 KB              │
│  ├─ Connection buffers                          │
│  └─ Message queues                              │
│                                                 │
│  ╔═══════════════════════════════════════╗      │
│  ║ INDEX_HTML (~25 KB)    PROBLEM!     ║      │
│  ║ MONITOR_HTML (~9 KB)   PROBLEM!     ║      │
│  ╚═══════════════════════════════════════╝      │
│                                                 │
│  Other allocations          ~30 KB              │
│  ├─ Heap overhead                               │
│  ├─ Stack space                                 │
│  └─ Static variables                            │
│                                                 │
│  ╔═══════════════════════════════════════╗      │
│  ║ Free RAM: ~121 KB     INSUFFICIENT! ║      │
│  ╚═══════════════════════════════════════╝      │
│                                                 │
│  TOTAL: 320 KB = CRASH!                         │
└─────────────────────────────────────────────────┘
```

### After Fix (Working)

```
┌─────────────────────────────────────────────────┐
│            ESP32 RAM (320 KB Total)             │
├─────────────────────────────────────────────────┤
│                                                 │
│  Arduino Framework          ~50 KB              │
│  BLE Stack (NimBLE)         ~45 KB              │
│  M5StickCPlus2 Libraries    ~25 KB              │
│  MQTT Client                ~15 KB              │
│  LittleFS buffers            ~4 KB              │
│  Other allocations          ~26 KB              │
│                                                 │
│  ╔═══════════════════════════════════════╗      │
│  ║ Free RAM: ~155 KB     SUFFICIENT!   ║      │
│  ╚═══════════════════════════════════════╝      │
│                                                 │
│  TOTAL: 165 KB used, 155 KB free              │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│          ESP32 Flash (4 MB Total)               │
├─────────────────────────────────────────────────┤
│                                                 │
│  Firmware                  ~1.2 MB              │
│  ├─ Application code                            │
│  ├─ Libraries                                   │
│  └─ Constants                                   │
│                                                 │
│  ╔═══════════════════════════════════════╗      │
│  ║ LittleFS Filesystem    ~50 KB         ║      │
│  ║ ├─ index.html    25 KB              ║      │
│  ║ ├─ monitor.html   9 KB              ║      │
│  ║ └─ overhead      16 KB                ║      │
│  ╚═══════════════════════════════════════╝      │
│                                                 │
│  Free Flash                ~2.7 MB              │
│  (Available for future use)                     │
│                                                 │
└─────────────────────────────────────────────────┘
```

## Filesystem Architecture

```
┌──────────────────────────────────────────────────┐
│          ESP32 Flash Partition Layout            │
├──────────────────────────────────────────────────┤
│                                                  │
│  0x1000    Bootloader                            │
│            ├─ First stage bootloader             │
│            └─ Partition table                    │
│                                                  │
│  0x8000    NVS (Non-Volatile Storage)            │
│            ├─ WiFi credentials                   │
│            ├─ Device configs                     │
│            └─ MQTT settings                      │
│                                                  │
│  0x10000   Application (app0)                    │
│            ├─ main.cpp                           │
│            ├─ WebConfigServer.cpp                │
│            ├─ VictronBLE.cpp                     │
│            ├─ MQTTPublisher.cpp                  │
│            └─ Libraries                          │
│                                                  │
│  0x290000  LittleFS Filesystem  ◄── NEW!        │
│            ├─ index.html     (25 KB)            │
│            ├─ monitor.html   (9 KB)             │
│            └─ Free space     (~1.9 MB)          │
│               (for future assets)                │
│                                                  │
│  0x3D0000  OTA (app1) - Reserved                 │
│            └─ For future OTA updates             │
│                                                  │
└──────────────────────────────────────────────────┘
```

## Upload Process

### Before Fix (1 step)

```
   Developer's Computer              ESP32
   
   ┌─────────────────┐
   │  pio run        │
   │  --target       │──────────►  Upload firmware
   │  upload         │              (includes HTML)
   └─────────────────┘
                                     Done!
```

### After Fix (2 steps)

```
   Developer's Computer              ESP32
   
   Step 1:
   ┌─────────────────┐
   │  pio run        │
   │  --target       │──────────►  Upload firmware
   │  upload         │              (no HTML)
   └─────────────────┘
                                     Firmware uploaded
   
   Step 2:
   ┌─────────────────┐
   │  pio run        │
   │  --target       │──────────►  Upload filesystem
   │  uploadfs       │              (HTML files)
   └─────────────────┘
                                     Filesystem uploaded
                                    
                                     Ready to use!
```

## Benefits Summary

```
┌─────────────────────────────────────────────────┐
│                   Benefits                      │
├─────────────────────────────────────────────────┤
│                                                 │
│   No more boot loops                           │
│    └─ Stable, reliable booting                  │
│                                                 │
│   Reduced RAM usage by ~34 KB                  │
│    └─ More memory for future features           │
│                                                 │
│   Smaller firmware size by ~34 KB              │
│    └─ Faster uploads, more code space           │
│                                                 │
│   Better maintainability                       │
│    └─ HTML can be updated without recompiling   │
│                                                 │
│   Separation of concerns                       │
│    └─ Web assets separate from code             │
│                                                 │
│   Scalability                                  │
│    └─ Can add more web files (images, CSS, JS)  │
│                                                 │
│   Clear error messages                         │
│    └─ Users know if filesystem not uploaded     │
│                                                 │
└─────────────────────────────────────────────────┘
```

## Key Takeaways

1. **Root Cause**: Large HTML strings in RAM caused memory exhaustion
2. **Solution**: Move HTML to flash filesystem (LittleFS)
3. **Impact**: Fixed boot loop, freed 34 KB RAM, reduced firmware size
4. **Trade-off**: Requires 2-step upload process
5. **Result**: Stable, maintainable, scalable web interface
