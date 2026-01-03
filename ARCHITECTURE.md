# SmartClock Architecture

## Project Structure

```
smartclock-arduino/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Entry point, setup & loop
│   ├── config.h           # Constants and pin definitions
│   ├── display.cpp/h      # TFT rendering, JPEG decode
│   ├── webserver.cpp/h    # HTTP API endpoints
│   └── settings.cpp/h     # EEPROM persistence
├── README.md              # User documentation
├── INSTALL.md             # Installation instructions
├── ARCHITECTURE.md        # This document
└── test-api.sh           # API test script

Generated:
├── .pio/                  # PlatformIO build artifacts
└── .vscode/               # VS Code settings (optional)
```

## Module Overview

### main.cpp
**Responsible for:**
- System initialization
- WiFi setup (WiFiManager)
- mDNS advertising
- ArduinoOTA setup
- LittleFS filesystem mount
- Main loop (OTA, mDNS, display updates)

**Dependencies:**
- WiFiManager (captive portal)
- ESP8266mDNS
- ArduinoOTA
- LittleFS
- display module
- webserver module
- settings module

### display.cpp/h
**Responsible for:**
- TFT_eSPI display initialization
- Clock mode rendering (time, WiFi, power bar, icons)
- Image mode rendering (JPEG decode)
- Brightness control (PWM backlight)
- Scroll text animation
- Message popups

**Key structures:**
```cpp
struct DisplayState {
    String line1;      // Main time
    String line2;      // Power label
    String lineMedia;  // Scrolling text
    String line3-6;    // Icon values
    float barValue;    // Power bar (0.0-1.0)
    bool showImage;
    String imagePath;
};
```

**Functions:**
- `displayInit()` - Setup TFT en JPEG decoder
- `displayUpdate()` - Render current mode
- `displayRenderClock()` - Clock layout
- `displayRenderImage()` - JPEG rendering
- `displaySetBrightness()` - PWM backlight
- `displayUpdateScroll()` - Scroll animation
- `displayShowMessage()` - Popup message

### webserver.cpp/h
**Responsible for:**
- AsyncWebServer setup
- HTTP API endpoints
- Multipart file upload
- OTA firmware upload
- JSON responses

**Endpoints:**

| Path | Method | Function |
|------|--------|----------|
| /app.json | GET | Device status (theme, brightness, image) |
| /space.json | GET | Filesystem info (total, free) |
| /brt.json | GET | Current brightness |
| /set | GET | Control (brt, theme, img, clear) |
| /doUpload | POST | File upload (multipart) |
| /delete | GET | Delete file |
| /api/update | POST | Live display update (JSON) |
| /update | GET | OTA upload form |
| /update | POST | OTA firmware upload |

**Global state:**
- `currentBrightness` - Last set brightness
- `currentTheme` - Last set theme
- `currentImage` - Last displayed image path

### settings.cpp/h
**Responsible for:**
- EEPROM initialization
- Settings save/load
- Magic number validation

**Settings struct:**
```cpp
struct Settings {
    int brightness;     // 0-100
    int theme;          // 0-6
    char lastImage[64]; // Last displayed image path
    bool valid;         // Struct validity flag
};
```

**Storage:**
- EEPROM address 0: Magic number (0xCAFE)
- EEPROM address 2: Settings struct
- Total: ~70 bytes

### config.h
**Defines:**
- Pin assignments
- Display dimensions
- WiFi defaults
- mDNS hostname
- OTA credentials
- Update intervals
- Filesystem paths

## Data Flow

### Display Update Flow
```
HTTP Request → webserver.cpp
    ↓
Update DisplayState → display.cpp
    ↓
displayUpdate() → displayRenderClock/Image()
    ↓
TFT_eSPI → ST7789V Display
```

### Image Upload Flow
```
POST /doUpload → webserver.cpp handleFileUpload()
    ↓
Multipart parser → LittleFS.open()
    ↓
Write chunks → /image/filename.jpg
    ↓
GET /set?img=/image/filename.jpg
    ↓
displayState.imagePath = path
displayState.showImage = true
    ↓
displayRenderImage() → TJpgDec.drawFsJpg()
    ↓
Display shows image
```

### Live Update Flow
```
POST /api/update (JSON)
    ↓
ArduinoJson deserialize
    ↓
Update displayState fields
    ↓
displayState.showImage = false
    ↓
displayUpdate() → displayRenderClock()
    ↓
Display shows clock with new data
```

### OTA Update Flow
```
POST /update (firmware binary)
    ↓
Update.begin()
    ↓
Update.write() chunks
    ↓
displayShowMessage("Progress...")
    ↓
Update.end() → ESP.restart()
```

### Settings Persistence Flow
```
Power on → settingsLoad()
    ↓
Check magic (0xCAFE)
    ↓
Valid? Load from EEPROM : Use defaults
    ↓
Apply to currentBrightness, currentTheme
    ↓
On change → settingsSave()
    ↓
EEPROM.put() → EEPROM.commit()
```

## Memory Management

### ESP8266 Constraints
- Flash: 4MB
- RAM: ~80KB (heap + stack)
- EEPROM: 512 bytes allocated

### Optimization Strategies
1. **Image buffering:** JPEG decoded streaming to display (no RAM buffer)
2. **Async operations:** ESPAsyncWebServer (non-blocking)
3. **String usage:** Arduino String for convenience (stack allocated where possible)
4. **Display updates:** Only on change (5s interval for clock)
5. **Scroll animation:** Minimal memory (single int position)

### Typical Memory Usage
- Firmware: ~350KB
- LittleFS: ~1MB reserved
- Heap free (runtime): ~30-40KB
- Stack: ~4KB

## Threading Model

ESP8266 is single-threaded cooperative multitasking:

### Main Loop
```cpp
loop() {
    ArduinoOTA.handle();     // Check OTA requests
    MDNS.update();           // mDNS responder
    displayUpdateScroll();   // Scroll animation

    if (time for update) {
        displayUpdate();     // Render display
    }

    yield();                 // Give time to WiFi stack
}
```

### Async Web Server
- Runs in background via ESP8266 event loop
- Callbacks triggered on HTTP requests
- Non-blocking file uploads

### Critical Sections
- Display updates: ~50ms (TFT rendering)
- JPEG decode: ~200-500ms (streaming)
- File uploads: Chunked (non-blocking)

## Configuration

### Compile-time (config.h)
- Pin assignments
- WiFi AP credentials
- mDNS hostname
- Update intervals

### Runtime (EEPROM)
- Brightness
- Theme
- Last image

### WiFi (WiFiManager)
- SSID
- Password
- Stored in ESP8266 WiFi config

## Dependencies

### Core Libraries
- Arduino Core for ESP8266
- ESP8266WiFi
- ESP8266mDNS
- LittleFS

### External Libraries (PlatformIO)
- TFT_eSPI: Display driver
- ESPAsyncTCP: Async TCP/IP
- ESPAsyncWebServer: HTTP server
- WiFiManager: Captive portal
- ArduinoJson: JSON parsing
- TJpg_Decoder: JPEG decoding
- ArduinoOTA: OTA updates

## Build Process

```bash
pio run
```

1. Download dependencies (lib_deps)
2. Apply build_flags (TFT_eSPI config)
3. Compile src/*.cpp
4. Link libraries
5. Generate firmware.bin (~350KB)
6. Generate filesystem.bin (if data/ exists)

## Deployment

### Initial (Serial)
```bash
pio run -t upload        # Upload firmware
pio run -t uploadfs      # Upload filesystem (optional)
```

### Updates (OTA)
```bash
# Via network
upload_protocol = espota
upload_port = smartclock.local
pio run -t upload
```

## API Compatibility

### GeekMagic API
Fully compatible with GeekMagic HACS integration:
- All required endpoints implemented
- Multipart upload support
- JSON responses match format
- Theme/brightness control
- Image display

### Extensions
Additional endpoints not in original:
- `/api/update` - Live JSON updates (no image render)
- `/update` - Web-based OTA

## Security Considerations

### WiFi
- AP mode password protected
- WPA2 for station mode

### OTA
- Password protected (default: "admin")
- Change in config.h before deployment

### HTTP
- No authentication on API endpoints
- Use on trusted network only
- Consider adding HTTP Basic Auth for production

### Recommendations
1. Change OTA_PASSWORD
2. Change WIFI_AP_PASSWORD
3. Enable HTTPS (requires ESP8266 >= 2.4.0, more complexity)
4. Implement API token authentication

## Future Enhancements

### Possible Additions
- MQTT support (async client)
- NTP time sync
- Timezone configuration
- Multiple screen/layouts
- Touch input (if hardware supports)
- Animations/transitions
- Weather widget
- Chart/graph widgets
- HTTP authentication
- HTTPS support
- Prometheus metrics endpoint
