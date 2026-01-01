# SmartClock HTTP API Reference

Base URL: `http://smartclock.local` of `http://<device-ip>`

## Endpoints

### GET /

Access the web-based user interface.

**Response:**
HTML web page for device control and settings.

**Example:**
```bash
# Open in browser
firefox http://smartclock.local/
```

---


### GET /app.json

Device status information.

**Response:**
```json
{
  "theme": 0,       // Theme is currently unused, will always be 0
  "brt": 70,
  "img": "/image/current.jpg", // Empty string if no image is set
  "gmtOffset": 3600 // Current GMT offset in seconds
}
```

**Fields:**
- `theme` (int): Current theme number (currently always 0 for simplified clock)
- `brt` (int): Brightness level (0-100)
- `img` (string): Currently displayed image path (empty if clock mode active)
- `gmtOffset` (long): Current GMT offset in seconds

**Example:**
```bash
curl http://smartclock.local/app.json
```

---


### GET /space.json

Filesystem storage information.

**Response:**
```json
{
  "total": 1048576,
  "free": 800000
}
```

**Fields:**
- `total` (int): Total bytes
- `free` (int): Free bytes

**Example:**
```bash
curl http://smartclock.local/space.json
```

---


### GET /brt.json

Current brightness level.

**Response:**
```json
{
  "brt": "70"
}
```

**Fields:**
- `brt` (string): Brightness as string (0-100)

**Example:**
```bash
curl http://smartclock.local/brt.json
```

---


### GET /set

Control device settings.

**Query Parameters:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| brt | int | 0-100 | Set brightness |
| gmt | long | -   | Set GMT offset in seconds (e.g., 3600 for +1 hour) |
| img | string | - | Display image path (e.g., `/image/photo.jpg`) |

**Response:**
```
OK
```
or
```
No action
```

**Examples:**
```bash
# Set brightness to 50%
curl "http://smartclock.local/set?brt=50"

# Set GMT offset to +1 hour (3600 seconds)
curl "http://smartclock.local/set?gmt=3600"

# Display specific image (temporary, cleared on reboot)
curl "http://smartclock.local/set?img=/image/photo.jpg"

# Combine parameters
curl "http://smartclock.local/set?brt=80&gmt=7200&img=/image/photo.jpg"
```

---


### POST /doUpload

Upload file to device.

**Query Parameters:**
- `dir` (string): Target directory (default: `/image/`)

**Body:**
- Multipart form data
- Field name: `file`
- Content-Type: image/jpeg

**Response:**
```
OK
```

**Notes:**
- Only JPEG images are supported.
- Uploaded images are temporary and will be cleared from LittleFS on every device reboot.

**Examples:**
```bash
# Upload JPEG
curl -F "file=@photo.jpg" "http://smartclock.local/doUpload?dir=/image/"
```

---


### GET /delete

Delete file from device.

**Query Parameters:**
- `file` (string, required): Full file path

**Response:**
```
Deleted
```
or
```
Not found
```
or
```
Missing file parameter
```

**Example:**
```bash
curl "http://smartclock.local/delete?file=/image/old.jpg"
```

---


### POST /api/update

Live display update (JSON).

**Headers:**
- `Content-Type: application/json`

**Body:**
```json
{
  "line1": "12:45",
  "line2": "2500W",
  "lineMedia": "Now playing: Song Title",
  "line3": "25°C",
  "line4": "60%",
  "line5": "720W",
  "line6": "5.2kW",
  "bar": 0.5
}
```

**Fields (all optional):**
- `line1` (string): Main time/value (will replace current time display)
- `line2` (string): No longer displayed in simplified clock mode.
- `lineMedia` (string): No longer displayed in simplified clock mode.
- `line3` (string): No longer displayed in simplified clock mode.
- `line4` (string): No longer displayed in simplified clock mode.
- `line5` (string): No longer displayed in simplified clock mode.
- `line6` (string): No longer displayed in simplified clock mode.
- `bar` (float): No longer displayed in simplified clock mode.

**Response:**
```
OK
```

**Effect:**
- Switches to clock mode (`showImage=false`)
- Updates `displayState.line1` with provided value (overriding NTP time until next update)
- Note: Most other fields are ignored in the simplified clock mode.

**Example:**
```bash
curl -X POST http://smartclock.local/api/update \
  -H "Content-Type: application/json" \
  -d 
  {
    "line1": "Hello!"
  }
```

---


### GET /reconfigurewifi

Triggers the WiFiManager captive portal to allow reconfiguring WiFi credentials. The device will restart into Access Point (AP) mode.

**Response:**
```
WiFi Reconfiguration triggered. Device restarting to AP mode.
```

**Example:**
```bash
curl http://smartclock.local/reconfigurewifi
```

---


### GET /factoryreset

Erases all stored settings (EEPROM), formats the LittleFS filesystem, clears WiFi credentials, and reboots the device. This returns the SmartClock to its factory default state.

**Response:**
```
Factory Reset triggered. Clearing data and restarting...
```

**Example:**
```bash
curl http://smartclock.local/factoryreset
```

---


### GET /update

OTA firmware update form.

**Response:**
HTML form for firmware upload.

**Example:**
```bash
# Open in browser
firefox http://smartclock.local/update
```

---


### POST /update

Upload firmware binary.

**Body:**
- Multipart form data
- Field name: `update`
- File: `firmware.bin`

**Response:**
```
OK - Rebooting...
```
or
```
FAIL
```

**Example:**
```bash
curl -F "update=@.pio/build/nodemcuv2/firmware.bin" \
  http://smartclock.local/update
```

---


## Error Handling

### HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (missing parameters) |
| 404 | Not found (file doesn't exist) |
| 500 | Internal server error |

### Common Errors

**Upload fails:**
- Filesystem full → check `/space.json`
- Invalid file → verify content type (only JPEG for images)
- Insufficient memory during decoding (for JPEG)

**Image not displaying:**
- File not found → verify path
- JPEG decode error → check image format (240x240 recommended, basic JPEG encoding)
- Images are temporary and cleared on boot.

**OTA fails:**
- Firmware too large → check binary size
- Connection lost → retry

**Time/Date Incorrect:**
- Ensure device is connected to WiFi.
- Check GMT Offset in Web Control Panel or via `/app.json`.

---


## Rate Limiting

No rate limiting implemented. Best practices:
- Max 10 requests/second for normal operations
- Max 1 upload/5 seconds for file uploads
- OTA updates: 1 at a time

---


## Display State Management

### Clock Mode (Default)

- Triggered by: Device boot, or when no image is explicitly set for display.
- Shows: Current time (HH:MM) and date (DD-YYYY), updated via NTP.
- Updates: Every `DISPLAY_UPDATE_INTERVAL` (5 seconds by default).

### Image Mode (Temporary)

- Triggered by: `/set?img=...` or Web UI.
- Shows: Full screen JPEG.
- Updates: Only on new image command.
- Images are temporary and will be cleared on device reboot.

**Switch modes:**
```bash
# To clock mode (default)
# Simply do not set an image, or reboot the device.
# Any /api/update call will also reset to clock mode.

# To image mode
curl "http://smartclock.local/set?img=/image/photo.jpg"
```

---


## Integration Examples

### Home Assistant Automation

```yaml
automation:
  - alias: "Update SmartClock Message"
    trigger:
      - platform: state
        entity_id: sensor.some_sensor_value
    action:
      - service: rest_command.smartclock_update_line1
        data:
          line1: "Sensor: {{ states('sensor.some_sensor_value') }}"

rest_command:
  smartclock_update_line1:
    url: http://smartclock.local/api/update
    method: POST
    headers:
      Content-Type: application/json
    payload: >
      {
        "line1": "{{ line1 }}"
      }
```

### Python Script

```python
import requests
import json

def update_clock_message(message):
    url = "http://smartclock.local/api/update"
    data = {
        "line1": message
    }
    response = requests.post(url, json=data)
    return response.text

# Usage
update_clock_message("Hello from Python!")

def set_brightness(level):
    url = f"http://smartclock.local/set?brt={level}"
    response = requests.get(url)
    return response.text

# Usage
set_brightness(75)
```

### Shell Script

```bash
#!/bin/bash
# Update SmartClock display with custom message

MESSAGE="Hello from Shell!"

curl -X POST http://smartclock.local/api/update \
  -H "Content-Type: application/json" \
  -d "{
    \"line1\": \"$MESSAGE\"
  }"
```

---


## GeekMagic API Compatibility

✅ **Fully compatible endpoints:**
- GET /app.json (updated fields)
- GET /space.json
- GET /brt.json
- GET /set (updated parameters)
- POST /doUpload (images are temporary)
- GET /delete

➕ **Extensions:**
- GET `/` (Web UI)
- GET `/reconfigurewifi` (new)
- GET `/factoryreset` (new)
- POST `/api/update` (live updates, most fields ignored in simplified clock)
- GET/POST `/update` (web OTA)

---


## Security Notes

⚠️ **No authentication** - API is open
⚠️ **HTTP only** - No HTTPS
⚠️ **OTA password** - Default: "admin" (change recommended)

**Recommendations:**
1. Use on trusted network only
2. Change OTA password in config.h
3. Consider firewall rules
4. For production: add HTTP Basic Auth

---


## mDNS Discovery

Device advertises:
- Hostname: `smartclock.local`
- Service: `_http._tcp`
- Port: 80
- TXT records:
  - `model=SmartClock`
  - `vendor=Custom`
  - `api=geekmagic`

**Discovery:**
```bash
# Linux/Mac
avahi-browse -rt _http._tcp

# Windows
dns-sd -B _http._tcp
```