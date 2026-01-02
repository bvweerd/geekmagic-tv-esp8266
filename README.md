# SmartClock Arduino

ESP8266 SmartClock with GeekMagic API compatibility.

## ⚠️ OTA Update Safety

This firmware implements **robust OTA update protection** with multiple failsafe mechanisms:
- ✅ Automatic EEPROM validation with CRC32 checksums
- ✅ Boot failure detection and automatic recovery
- ✅ WiFi connection retry with exponential backoff
- ✅ Automatic fallback to AP mode when WiFi fails
- ✅ Emergency EEPROM reset after repeated boot failures

**While OTA updates are now significantly safer, always keep a USB cable available for emergency recovery via UART if needed.**

## Features

- ✅ Web-based User Interface (UI) for settings and control
- ✅ ST7789V 240x240 display
- ✅ WiFi configuration via captive portal
- ✅ mDNS discovery (smartclock.local)
- ✅ ArduinoOTA updates
- ✅ Web-based OTA updates (/update)
- ✅ LittleFS filesystem
- ✅ Persistent settings (Brightness, Timezone Offset)
- ✅ NTP time synchronization with configurable GMT offset
- ✅ Live update API
- ✅ Simplified clock display (time and date only)
- ✅ Temporary image upload and rendering (images cleared on boot)

## 🛡️ OTA Stability & Recovery Features

### EEPROM Validation System

The firmware implements a three-layer validation system to prevent corruption from affecting operations:

1. **Magic Number Check** (`0xCAFE`): Quick validation that settings exist
2. **Firmware Version Check**: Ensures settings structure matches current firmware version
3. **CRC32 Checksum**: Validates data integrity of all settings

When any validation fails, the system automatically:
- Logs the specific failure reason
- Resets settings to factory defaults
- Saves validated defaults back to EEPROM
- Continues boot with safe defaults

### Boot Failure Detection

The firmware tracks consecutive boot failures to detect crash loops:

- **Boot Counter**: Incremented at start of each boot attempt
- **Reset on Success**: Counter cleared when boot completes successfully
- **Failsafe Threshold**: After 5 consecutive failed boots, triggers emergency recovery
- **Emergency Recovery**: Performs complete EEPROM reset and restarts device

This prevents infinite crash loops after problematic OTA updates.

### WiFi Connection Resilience

Enhanced WiFi connection handling with multiple fallback levels:

#### Initial Connection (5 attempts with exponential backoff)
1. Attempt 1: Immediate connection try (30s timeout)
2. Attempt 2: Retry after 2 seconds
3. Attempt 3: Retry after 4 seconds
4. Attempt 4: Retry after 8 seconds
5. Attempt 5: Retry after 16 seconds

#### Fallback to AP Mode
If all connection attempts fail:
- Starts WiFiManager captive portal (3 minute timeout)
- If user doesn't configure: enters **Failsafe AP Mode**

#### Failsafe AP Mode
When in failsafe mode:
- Device runs as Access Point (`SmartClock-Setup` / `smartclock123`)
- Web interface remains accessible via AP IP
- Retries WiFi connection every 5 minutes automatically
- If connection succeeds: restarts to restore full functionality
- mDNS and OTA temporarily disabled to conserve resources

#### Runtime WiFi Monitoring
During normal operation:
- Checks connection status every 60 seconds
- If connection lost: 3 quick reconnection attempts
- If reconnection fails: switches to Failsafe AP Mode

### Configuration

Key settings in `src/config.h`:
```cpp
#define WIFI_RETRY_ATTEMPTS 5          // Initial connection attempts
#define WIFI_RETRY_DELAY_MS 2000        // Base delay for exponential backoff
#define WIFI_CONNECTION_TIMEOUT 30000   // 30s timeout per attempt
#define WIFI_MONITOR_INTERVAL 60000     // Check WiFi every 60s
#define WIFI_RECONNECT_INTERVAL 300000  // Retry in AP mode every 5min
#define BOOT_FAILURE_THRESHOLD 5        // Emergency reset after 5 failures
#define FIRMWARE_VERSION 2              // Increment when Settings struct changes
```

### Best Practices for OTA Updates

1. **Always test new firmware on a development device first**
2. **Increment `FIRMWARE_VERSION` when changing the `Settings` struct**
3. **Monitor serial output during first boot after OTA update**
4. **Keep credentials to your WiFi network accessible**
5. **Document any EEPROM layout changes in release notes**

### Recovery Scenarios

| Scenario | Automatic Recovery | Manual Recovery |
|----------|-------------------|-----------------|
| Corrupted EEPROM data | ✅ Automatic reset to defaults | Not needed |
| Invalid firmware version | ✅ Automatic reset to defaults | Not needed |
| WiFi network unavailable | ✅ Fallback to AP mode, periodic retry | Connect to AP and reconfigure |
| 5+ consecutive boot failures | ✅ Emergency EEPROM reset + restart | Not needed |
| Complete crash/brick | ❌ Not possible | Flash via USB/UART |

## Hardware

- ESP8266 NodeMCU v2
- ST7789V 240x240 TFT display
- Pins:
  - MOSI: GPIO13
  - SCLK: GPIO14
  - DC: GPIO0
  - RST: GPIO2
  - Backlight: GPIO5 (PWM)

## Installation

```bash
cd smartclock-arduino
pio run -t upload
pio device monitor
```

## First boot

1. Device starts in AP mode "SmartClock-Setup"
2. Connect with WiFi password "smartclock123"
3. Captive portal opens automatically
4. Configure WiFi credentials
5. Device reboots and connects to the network

## Usage

### Web Control Panel

Access the comprehensive web-based control panel by navigating to `http://smartclock.local/` (or your device's IP address) in a web browser.

**Main Sections:**

*   **Status & Info**: View device status, storage information, current brightness, and system logs.
*   **Settings**:
    *   **Brightness**: Adjust display brightness from 0% to 100%.
    *   **Timezone**: Configure the GMT offset (in seconds). *Note: Daylight Saving Time (DST) must be manually accounted for by adjusting this offset when changes occur.*
    *   **Reconfigure WiFi**: Trigger the WiFiManager captive portal to reconnect to a new network or update credentials.
*   **Image Upload**:
    *   **Upload JPEG**: Upload new JPEG images directly to the device. These images are temporary and will be cleared on the next reboot.
    *   **Display Image Path**: Manually specify a path to an uploaded image to display it.
*   **Advanced**:
    *   **Factory Reset**: Erase all saved settings (including WiFi credentials and timezone) and format the LittleFS filesystem, then restart the device. Use with caution.

### HTTP API (for advanced users/integrations)

```bash
# Upload image (temporary, cleared on reboot)
curl -F "file=@image.jpg" http://smartclock.local/doUpload?dir=/image/

# Show image (e.g., after upload, will be cleared on reboot)
curl http://smartclock.local/set?img=/image/image.jpg

# Set brightness (0-100)
curl http://smartclock.local/set?brt=50

# Set GMT Offset (e.g., for +1 hour)
curl http://smartclock.local/set?gmt=3600

# Live update (JSON - e.g., for custom text lines)
curl -X POST http://smartclock.local/api/update \
  -H "Content-Type: application/json" \
  -d '{"line1":"Custom","line2":"Text","bar":0.7}'

# Device status
curl http://smartclock.local/app.json
```

## OTA Updates

### Via Arduino IDE
1. Tools → Port → smartclock (network)
2. Upload firmware

### Via Web
1. Open `http://smartclock.local/update` (or your device's IP)
2. Select `firmware.bin`
3. Upload

## mDNS

Device advertises as:
- Hostname: `smartclock.local`
- HTTP service on port 80
- Metadata: model=SmartClock, api=geekmagic

## Settings

Stored in EEPROM:
- Brightness level
- GMT Offset (in seconds)
- Last displayed image (temporary, cleared on boot)

## Display Modes

### Clock Mode
- Shows current time (HH:MM) and date (DD-MM-YYYY)
- Updates regularly via NTP synchronization
- All other elements (WiFi signal, IP, power bar, media text, icon grid) have been removed for a cleaner, simplified view.

### Image Mode
- JPEG rendering (240x240)
- Images are uploaded via `/doUpload` and are temporary (cleared on reboot).

## Troubleshooting

### Display remains black
- Check pin connections
- Verify TFT_eSPI build flags in platformio.ini

### WiFi doesn't connect
- Use the "Reconfigure WiFi" option in the Web Control Panel.
- Check WiFi credentials.

### OTA doesn't work
- Verify device on network: `ping smartclock.local`
- Check firewall settings.

### Upload fails or image doesn't display correctly
- Ensure the image is a valid JPEG (240x240 resolution, relatively small file size < 100KB).
- Use the "Factory Reset" option in the Web Control Panel, then re-upload firmware and image.
- Check serial monitor for error messages during upload or display.

### Time or Date is incorrect
- Ensure the device is connected to WiFi.
- Check the GMT Offset setting in the Web Control Panel.

## Development

```bash
# Build
pio run

# Upload via serial
pio run -t upload

# Monitor serial
pio device monitor

# Clean
pio run -t clean

# Upload filesystem
pio run -t uploadfs
```

## API Endpoints (Updated)

| Endpoint | Method | Parameters | Description |
|----------|--------|------------|-------------|
| `/` | GET | - | Web-based User Interface (new) |
| `/app.json` | GET | - | Device status JSON (now includes `gmtOffset`) |
| `/space.json` | GET | - | Storage info JSON |
| `/brt.json` | GET | - | Brightness info JSON |
| `/set` | GET | `brt=<0-100>`, `gmt=<seconds>`, `img=<path>` | Control brightness, GMT offset, display image |
| `/doUpload` | POST | `dir=<path>`, `file` (multipart) | Upload file to LittleFS (`/image/` recommended) |
| `/delete` | GET | `file=<path>` | Delete a file from LittleFS |
| `/log` | GET | - | View system logs |
| `/reconfigurewifi` | GET | - | Trigger WiFi configuration portal (new) |
| `/factoryreset` | GET | - | Perform a factory reset (new) |
| `/api/update` | POST | JSON body | Live update display content (lines, bar) |
| `/update` | GET | - | OTA HTML form |
| `/update` | POST | firmware file | OTA firmware upload |

## Credentials

- WiFi AP: SmartClock-Setup / smartclock123
- OTA: admin
- mDNS: smartclock.local

## License

MIT
