# SmartClock Installation Instructions

## Requirements

### Software
- PlatformIO Core or PlatformIO IDE (VS Code extension)
- Python 3.7+
- Git

### Hardware
- ESP8266 NodeMCU v2
- ST7789V 240x240 TFT display
- USB cable (micro-USB)

## Step 1: Install PlatformIO

### Option A: VS Code + PlatformIO Extension
```bash
# Install VS Code
# Open VS Code → Extensions → search for "PlatformIO IDE" → Install
```

### Option B: PlatformIO Core (CLI)
```bash
pip install platformio
```

## Step 2: Download the Project

```bash
cd /path/to/your/projects
# If you have a git repo:
git clone <repo-url>
cd smartclock-arduino

# Or copy the smartclock-arduino folder
```

## Step 3: Connect the Hardware

### Pin Mapping

| ESP8266 Pin | Display Pin | Function |
|-------------|-------------|----------|
| GPIO13 (D7) | SDA/MOSI    | Data     |
| GPIO14 (D5) | SCL/SCLK    | Clock    |
| GPIO0 (D3)  | DC          | Data/Command |
| GPIO2 (D4)  | RST         | Reset    |
| GPIO5 (D1)  | BL          | Backlight (PWM) |
| 3.3V        | VCC         | Power    |
| GND         | GND         | Ground   |

**NOTE:**
- Use 3.3V, NOT 5V!
- Some displays have a CS pin - leave it floating or connect it to GND

## Step 4: Compile and Upload

### Via PlatformIO CLI
```bash
cd smartclock-arduino

# Download dependencies and compile
pio run

# Upload via USB
pio run -t upload

# Monitor serial output
pio device monitor
```

### Via VS Code
1. Open smartclock-arduino folder in VS Code
2. PlatformIO sidebar → Open
3. Project Tasks → nodemcuv2 → General → Build
4. Project Tasks → nodemcuv2 → General → Upload
5. Project Tasks → nodemcuv2 → General → Monitor

## Step 5: First Boot

After upload:
1. Device reboots automatically
2. Display shows "SmartClock Initializing..."
3. Display shows "WiFi Setup..."
4. Device starts WiFi AP "SmartClock-Setup"

## Step 6: Configure WiFi

### Via Smartphone/Laptop
1. Connect to WiFi "SmartClock-Setup"
2. Password: `smartclock123`
3. Captive portal opens automatically (if not: go to 192.168.4.1)
4. Click "Configure WiFi"
5. Select your WiFi network
6. Enter WiFi password
7. Click "Save"
8. Device reboots and connects

### Troubleshooting WiFi Setup
- Timeout after 3 minutes → device reboots → try again
- Connection failed → device reboots in AP mode → repeat step 6

## Step 7: Find the Device IP

### Option A: Serial Monitor
```bash
pio device monitor
# Look for: "WiFi Connected! IP: 192.168.x.x"
```

### Option B: mDNS (if your network supports it)
```bash
ping smartclock.local
# Or in browser: http://smartclock.local
```

### Option C: Router admin panel
Look for a device with hostname "smartclock"

## Step 8: Testing

```bash
# Use test script
./test-api.sh smartclock.local
# Or with IP:
./test-api.sh 192.168.1.100

# Or manually:
curl http://smartclock.local/app.json
```

## Step 9: Home Assistant Integration (Optional)

1. Install the GeekMagic HACS integration in Home Assistant
2. Configuration → Integrations → Add Integration → GeekMagic
3. Host: `smartclock.local` or IP address
4. Configure display via HACS panel

## OTA Updates (after first installation)

### Via Arduino IDE
1. Tools → Port → smartclock (network port)
2. Upload new firmware

### Via Web Interface
1. Browse to http://smartclock.local/update
2. Select firmware.bin
3. Upload
4. Wait for reboot

### Via PlatformIO
```bash
# Set in platformio.ini:
upload_protocol = espota
upload_port = smartclock.local

# Upload:
pio run -t upload
```

## Troubleshooting

### Display shows nothing
- Check 3.3V power
- Verify pin connections
- Check that TFT_eSPI build flags in platformio.ini match your display

### Compilation errors
```bash
# Clean and rebuild
pio run -t clean
pio run
```

### Upload failed
- Check USB cable
- Verify COM port: `pio device list`
- Check driver (CH340/CP2102)
- Reset device during upload (GPIO0 to GND at boot)

### WiFi does not connect
- Check SSID and password
- 2.4GHz WiFi required (no 5GHz)
- Reset: upload firmware again

### OTA doesn't work
- Verify network connection
- Check firewall (port 3232 for ArduinoOTA)
- Ping test: `ping smartclock.local`

### Out of memory during compilation
```bash
# Lower debug level in platformio.ini
build_flags =
    ...
    -D NDEBUG
```

### Filesystem errors
```bash
# Format filesystem via serial monitor
# Add to setup():
# LittleFS.format();
```

## Advanced Configuration

### Custom hostname
Edit `src/config.h`:
```cpp
#define MDNS_HOSTNAME "myclock"
#define OTA_HOSTNAME "myclock"
```

### Custom WiFi AP credentials
Edit `src/config.h`:
```cpp
#define WIFI_AP_NAME "MyClock"
#define WIFI_AP_PASSWORD "mypassword"
```

### Adjust brightness range
Edit `src/display.cpp` in `displaySetBrightness()` function

### Scroll speed
Edit `src/config.h`:
```cpp
#define SCROLL_INTERVAL 20  // faster (was 30)
```

## Next Steps

- Test all API endpoints with test-api.sh
- Upload first image via GeekMagic HACS integration
- Configure Home Assistant dashboard
- Setup automations for live updates
- View logs: `pio device monitor`
