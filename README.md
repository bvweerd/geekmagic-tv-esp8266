# SmartClock Arduino
# WARNING: UPDATING VIA OTA MIGHT BRICK THE DEVICE. IF IT FAILS, USE A UART FLASH METHOD! 

ESP8266 SmartClock with GeekMagic API compatibility.

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
