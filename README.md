# SmartClock Arduino

ESP8266 SmartClock with GeekMagic API compatibility. Works with: https://github.com/adrienbrault/geekmagic-hacs

This firmware is made for the GeekMagic SmallTV Smart Weather Clock as can be found in online stores. 

The Smalltv and Smalltv-Ultra are based on an ESP8266, while the Smalltv-Pro uses an ESP32 with more memory and processing power.
This code has only been tested on the basic Smalltv (ESP8266) and may require modifications for other variants.

## ⚠️ OTA Update Safety

This firmware implements **robust OTA update protection** with multiple failsafe mechanisms.
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

### User-Initiated Factory Reset (Power Cycle Method)

The firmware provides a **manual factory reset mechanism** that users can trigger without needing the web interface or serial console:

**How it works:**
1. Power cycle the device **5 times in quick succession** (within ~10 seconds total)
2. On the 5th boot, the device automatically performs a complete factory reset
3. All settings, WiFi credentials, and filesystem data are erased
4. Device restarts in AP mode ready for initial setup

**Reset Process:**
- **Power Cycle Counter**: Tracked in EEPROM, increments on each boot
- **Timeout Window**: Counter resets to 0 after 10 seconds of successful uptime
- **Factory Reset Trigger**: When counter reaches 5, performs full factory reset
- **Reset Actions**: Erases WiFi config, resets EEPROM settings, formats filesystem, restarts device

**When to use this:**
- Device is stuck in a boot loop and web interface is inaccessible
- Forgot WiFi credentials and can't access AP mode
- Need to completely reset device to factory defaults
- Preparing device for a new owner or network

**Usage Example:**
1. Unplug device from power
2. Wait 2 seconds
3. Plug in device, wait for it to start booting (3-5 seconds)
4. Unplug device again
5. Repeat steps 2-4 until you've done this 5 times total
6. On the 5th boot, device will display "Factory Reset" message and restart in AP mode

**Serial Console Output:**
```
========================================
USER RESET: 5 quick power cycles detected!
Performing factory reset...
========================================
Factory reset complete. System will restart in 5 seconds...
```

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
- Device runs as Access Point (SSID: `SmartClock-Setup`)
- **AP credentials displayed on device screen** (SSID, password, and IP address)
- Random password generated for security (8-digit numeric)
- Web interface remains accessible via AP IP (typically 192.168.4.1)
- Retries WiFi connection every 5 minutes automatically
- If connection succeeds: restarts to restore full functionality
- mDNS and OTA temporarily disabled to conserve resources

#### Runtime WiFi Monitoring
During normal operation:
- Checks connection status every 60 seconds
- If connection lost: 3 quick reconnection attempts
- If reconnection fails: switches to Failsafe AP Mode

### Configuration

Key settings in `src/config.h` and `src/settings.cpp`:
```cpp
#define WIFI_RETRY_ATTEMPTS 5          // Initial connection attempts
#define WIFI_RETRY_DELAY_MS 2000        // Base delay for exponential backoff
#define WIFI_CONNECTION_TIMEOUT 30000   // 30s timeout per attempt
#define WIFI_MONITOR_INTERVAL 60000     // Check WiFi every 60s
#define WIFI_RECONNECT_INTERVAL 300000  // Retry in AP mode every 5min
#define BOOT_FAILURE_THRESHOLD 5        // Emergency reset after 5 failures
#define POWER_CYCLE_THRESHOLD 5         // Factory reset after 5 quick power cycles
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
| Need factory reset | ❌ Not automatic | **✅ 5 quick power cycles** |
| Forgot WiFi credentials | ✅ Fallback to AP mode | **✅ 5 quick power cycles** or connect to AP |
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
2. **Look at the device display** to see the randomly generated AP password
   - Alternatively, connect via serial console (115200 baud) to see the password
3. Connect to the "SmartClock-Setup" WiFi network using the displayed password
4. Captive portal opens automatically
5. Configure WiFi credentials
6. Device reboots and connects to the network
7. The device display will now show the IP address at the top of the clock screen

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

## Display Modes & Information

### Clock Mode (Normal Operation)
- **Time Display**: Large, centered time (HH:MM) from NTP synchronization
- **Date Display**: Below time, shows DD-MM-YYYY format
- **IP Address**: Small text at top of screen showing current IP address (allows easy access without serial monitor)
- **Auto-Updates**: Display refreshes every 5 seconds

### Clock Mode (AP Failsafe Mode)
When the device is in AP/failsafe mode, the display automatically shows:
- **"AP Mode Active"** message
- **SSID**: The access point name (SmartClock-Setup)
- **Password**: The randomly generated 8-digit password
- **IP Address**: The AP IP address (typically 192.168.4.1) shown at top

This ensures you can always see the connection credentials on the device screen without needing serial access.

### Image Mode
- JPEG rendering (240x240)
- Images are uploaded via `/doUpload` and are temporary (cleared on reboot)

## Security Features

### Random AP Password Generation
For enhanced security, the device generates a **unique random 8-digit numeric password** on each boot when AP mode is activated. This prevents unauthorized access to your device's configuration portal.

**Password Characteristics:**
- Length: 8 digits
- Character set: 0-9 (numbers only)
- Generated using hardware random number generator (ESP.getCycleCount() ^ micros() ^ ESP.getChipId())
- Displayed on device screen in AP mode
- Logged to serial console at boot

**How to find your AP password:**
1. **On the device display**: When in AP mode, the password is shown on screen
2. **Via serial console**: Connect to serial port (115200 baud) and look for "Generated AP Password: ..."
3. **After first boot**: The password persists for the session but changes after reboot

## Troubleshooting

### Display remains black
- Check pin connections
- Verify TFT_eSPI build flags in platformio.ini

### WiFi doesn't connect
- Check the device display for the current AP password (randomly generated)
- Use the "Reconfigure WiFi" option in the Web Control Panel
- Check WiFi credentials
- If in AP mode, the SSID, password, and IP address are shown on the display

### Can't find device IP address
- **Look at the device display**: The IP address is shown at the top of the clock screen in small text
- Alternatively, use mDNS: `smartclock.local`
- Check your router's DHCP client list

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

### Device is stuck or needs factory reset
- **Quick Solution**: Power cycle the device **5 times in quick succession** (within ~10 seconds total)
- On the 5th boot, the device will automatically perform a complete factory reset
- This will erase all settings, WiFi credentials, and filesystem data
- Device will restart in AP mode ready for initial setup
- See "User-Initiated Factory Reset (Power Cycle Method)" section for detailed instructions

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

- **WiFi AP SSID**: SmartClock-Setup
- **WiFi AP Password**: Random 8-digit numeric (see device display or serial console)
- **OTA Password**: admin
- **mDNS Hostname**: smartclock.local

**Note**: The AP password is randomly generated on each boot for security. Always check the device display when connecting in AP mode.

## License

MIT
