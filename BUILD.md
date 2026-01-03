# SmartClock Build Instructions

## Building the Firmware

### Method 1: PlatformIO CLI

```bash
cd smartclock-arduino

# Build firmware
pio run

# Or
platformio run
```

**Output location:**
```
.pio/build/nodemcuv2/firmware.bin
```

### Method 2: VS Code + PlatformIO IDE

1. Open VS Code
2. Open folder `smartclock-arduino`
3. PlatformIO sidebar → Project Tasks
4. nodemcuv2 → General → Build
5. Firmware in `.pio/build/nodemcuv2/firmware.bin`

### Method 3: Arduino IDE (not recommended)

Convert to Arduino sketch structure - complex, use PlatformIO.

---

## Installing PlatformIO

### Via pip (Python)
```bash
pip install platformio
```

### Via VS Code
1. Install "PlatformIO IDE" extension
2. Reload VS Code
3. PlatformIO icon appears in the sidebar

---

## Build Options

### Clean build
```bash
pio run -t clean
pio run
```

### Verbose output
```bash
pio run -v
```

### Specific environment
```bash
pio run -e nodemcuv2
```

---

## Build Output

After a successful build:

```
RAM:   [====      ]  45% (used ~37KB of 81KB)
Flash: [====      ]  38% (used ~395KB of 1MB)

Building .pio/build/nodemcuv2/firmware.bin
========================= [SUCCESS] Took 12.34 seconds
```

**Generated files:**
```
.pio/build/nodemcuv2/
├── firmware.bin     # Flash this to ESP8266
├── firmware.elf     # Debug symbols
└── firmware.map     # Memory map
```

---

## Uploading Firmware

### First time (USB/Serial)

```bash
# Auto-detect port
pio run -t upload

# Or specific port
pio run -t upload --upload-port /dev/ttyUSB0
```

### After first flash (OTA)

**Option A: Web interface**
```bash
# Via browser
firefox http://smartclock.local/update
# Upload .pio/build/nodemcuv2/firmware.bin
```

**Option B: curl**
```bash
curl -F "update=@.pio/build/nodemcuv2/firmware.bin" \
  http://smartclock.local/update
```

**Option C: PlatformIO OTA**

Add to `platformio.ini`:
```ini
upload_protocol = espota
upload_port = smartclock.local
```

Then:
```bash
pio run -t upload
```

---

## Troubleshooting

### pio: command not found

**Solution:**
```bash
# Install PlatformIO
pip install platformio

# Add to PATH (Linux/Mac)
export PATH=$PATH:~/.platformio/penv/bin

# Or use full path
~/.platformio/penv/bin/platformio run
```

### Build errors

**Library download fails:**
```bash
# Re-download dependencies
pio pkg update
pio run
```

**Out of memory:**
```bash
# Clean first
pio run -t clean
pio run
```

### Upload fails (USB)

**Check port:**
```bash
# List devices
pio device list

# Use correct port
pio run -t upload --upload-port /dev/ttyUSB0  # Linux
pio run -t upload --upload-port COM3          # Windows
```

**Driver issues (Windows):**
- Install CH340 driver (NodeMCU v2)
- Or CP2102 driver (other boards)

### OTA upload fails

**Test connection:**
```bash
ping smartclock.local
```

**Check port 3232:**
```bash
# Linux/Mac
nc -zv smartclock.local 3232

# Windows
Test-NetConnection smartclock.local -Port 3232
```

---

## Build Configuration

### platformio.ini

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino

# Dependencies
lib_deps =
    me-no-dev/ESPAsyncTCP@^1.2.2
    ...

# Compiler flags
build_flags =
    -D USER_SETUP_LOADED=1
    -D ST7789_DRIVER=1
    ...

# Serial monitor
monitor_speed = 115200

# OTA (optional)
upload_protocol = espota
upload_port = smartclock.local
```

### Adjusting build flags

For debugging:
```ini
build_flags =
    ${env.build_flags}
    -D DEBUG_ESP_PORT=Serial
    -D DEBUG_ESP_CORE
```

For smaller binary:
```ini
build_flags =
    ${env.build_flags}
    -D NDEBUG
    -O2
```

---

## Monitor Serial Output

During development:

```bash
# Build, upload, and monitor at once
pio run -t upload && pio device monitor

# Or just monitor
pio device monitor

# Custom baud rate
pio device monitor -b 115200

# Exit: Ctrl+C
```

---

## CI/CD Build

### GitHub Actions example

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.x'
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build firmware
        run: |
          cd smartclock-arduino
          pio run
      - name: Upload artifact
        uses: actions/upload-artifact@v2
        with:
          name: firmware
          path: smartclock-arduino/.pio/build/nodemcuv2/firmware.bin
```

---

## Size Optimization

Current size: ~395KB of 1MB

**Possible reductions:**

1. **Disable features:**
```cpp
// config.h
// #define ENABLE_OTA  // Save ~30KB
```

2. **Smaller fonts:**
```ini
build_flags =
    # Remove unused fonts
    # -D LOAD_FONT7
```

3. **Optimize libraries:**
```ini
lib_deps =
    # Use specific versions
    bodmer/TFT_eSPI@2.5.43  # Not @^2.5.43
```

---

## Quick Reference

```bash
# Full workflow
cd smartclock-arduino
pio run -t clean        # Clean
pio run                 # Build
pio run -t upload       # Upload via USB
pio device monitor      # Monitor serial

# OTA workflow
pio run                                    # Build
curl -F "update=@.pio/build/nodemcuv2/firmware.bin" \
  http://smartclock.local/update          # Upload OTA

# Check build
ls -lh .pio/build/nodemcuv2/firmware.bin  # Verify binary exists
```

**Firmware location:** `.pio/build/nodemcuv2/firmware.bin`

**Typical size:** ~350-400KB

---

## Platform Details

### ESP8266 Memory Layout

```
Flash: 4MB total
├── 0x000000 - Bootloader
├── 0x001000 - Firmware (max ~1MB)
├── 0x100000 - SPIFFS/LittleFS (~1MB)
└── 0x3FB000 - WiFi config, EEPROM

RAM: 80KB
├── Heap: ~40KB free (runtime)
├── Stack: ~4KB
└── System: ~36KB
```

### Firmware Sections

```bash
# Analyze memory usage
pio run -v 2>&1 | grep "section"

# Size report
pio run -t size
```

Typical output:
```
.text   : 250KB  (code)
.rodata : 100KB  (constants)
.data   : 5KB    (initialized data)
.bss    : 30KB   (uninitialized data)
```

---

## Further Reading

- [PlatformIO Docs](https://docs.platformio.org/)
- [ESP8266 Arduino Core](https://arduino-esp8266.readthedocs.io/)
- ARCHITECTURE.md - Memory management details
- INSTALL.md - First time setup
