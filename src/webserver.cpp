#include "webserver.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>   // For WiFi.softAPIP()
#include <WiFiManager.h> // Include WiFiManager here

ESP8266WebServer server(WEB_SERVER_PORT);

int currentBrightness = 70;
int currentTheme = 0;
char currentImage[DISPLAY_PATH_BUFFER_SIZE];

extern Settings appSettings;

// File upload buffer
File uploadFile;

const char* index_html = R"rawliteral(<!DOCTYPE html><html><head><title>SmartClock Control</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>:root{--primary-color:#007bff;--primary-hover-color:#0056b3;--danger-color:#dc3545;--danger-hover-color:#c82333;--background-color:#f8f9fa;--card-bg-color:#ffffff;--border-color:#dee2e6;--text-color:#212529;--light-text-color:#6c757d;--shadow-color:rgba(0,0,0,0.05);--font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif}body{font-family:var(--font-family);margin:0;padding:20px;background-color:var(--background-color);color:var(--text-color);line-height:1.5}.container{max-width:600px;margin:20px auto;background-color:var(--card-bg-color);border-radius:8px;box-shadow:0 4px 12px var(--shadow-color);overflow:hidden}h1{background-color:var(--primary-color);color:white;padding:15px 20px;margin:0;font-size:1.8em;text-align:center;border-bottom:1px solid var(--primary-hover-color)}.card{background-color:var(--card-bg-color);border:1px solid var(--border-color);border-radius:8px;margin:20px;overflow:hidden;box-shadow:0 2px 5px var(--shadow-color)}.card-header{background-color:#e9ecef;padding:10px 15px;font-size:1.2em;font-weight:bold;color:var(--primary-color);border-bottom:1px solid var(--border-color)}.card-body{padding:15px}.input-group{margin-bottom:15px}.input-group label{display:block;margin-bottom:5px;font-weight:bold;color:var(--text-color)}.input-group input[type="number"],.input-group input[type="text"],.input-group input[type="password"],.input-group select{width:calc(100% - 22px);padding:10px;border:1px solid var(--border-color);border-radius:4px;box-sizing:border-box;font-size:1em;margin-top:3px}.input-group input[type="file"]{margin-top:5px}.button{display:inline-block;background-color:var(--primary-color);color:white;padding:10px 15px;border:none;border-radius:5px;cursor:pointer;text-decoration:none;font-size:1em;margin-top:10px;transition:background-color 0.2s ease}.button:hover{background-color:var(--primary-hover-color)}.button.red{background-color:var(--danger-color)}.button.red:hover{background-color:var(--danger-hover-color)}.button-group{display:flex;flex-wrap:wrap;gap:10px;margin-top:10px}.button-group .button{margin-top:0}.info{background-color:#e0f7fa;color:#006064;border:1px solid #b2ebf2;padding:10px;border-radius:5px;margin-top:15px;font-size:0.9em}#scanStatus{margin-top:10px;font-style:italic;color:var(--light-text-color)}form{margin-bottom:0}.spacer{height:10px}</style></head><body><div class="container"><h1>SmartClock Control</h1><div class="card"><div class="card-header">Status & Info</div><div class="card-body button-group"><a href="/app.json" class="button">App JSON</a><a href="/space.json" class="button">Storage Info</a><a href="/brt.json" class="button">Brightness JSON</a><a href="/log" class="button">View Logs</a></div></div><div class="card"><div class="card-header">WiFi Configuration</div><div class="card-body"><button class="button" onclick="scanWiFi()">Scan Networks</button><div id="scanStatus"></div><div id="wifiSection" style="display: none; margin-top: 15px;"><div class="input-group"><label for="wifiNetwork">Select Network:</label><select id="wifiNetwork"></select></div><div class="input-group"><label for="wifiPassword">Password:</label><input type="password" id="wifiPassword" placeholder="Enter WiFi password"></div><button class="button" onclick="connectWiFi()">Connect</button></div><div class="spacer"></div><button class="button" onclick="reconfigureWiFi()">Reconfigure WiFi (Portal)</button></div></div><div class="card"><div class="card-header">Settings</div><div class="card-body"><div class="input-group"><label for="brightness">Brightness (0-100):</label><input type="number" id="brightness" min="0" max="100"><button class="button" onclick="setBrightness()">Set Brightness</button></div><div class="input-group"><label for="gmtOffset">GMT Offset (seconds):</label><input type="number" id="gmtOffset"><button class="button" onclick="setTimezone()">Set Timezone</button></div></div></div><div class="card"><div class="card-header">Image Management</div><div class="card-body"><form action="/doUpload?dir=/image/" method="POST" enctype="multipart/form-data"><div class="input-group"><label for="fileUpload">Upload JPEG Image:</label><input type="file" name="file" accept="image/jpeg" id="fileUpload"><input type="submit" value="Upload" class="button"></div></form><div class="info">Uploaded images are stored on LittleFS and cleared on reboot unless set as default.</div><div class="input-group"><button class="button" onclick="displayTestImage()">Display Test Image</button></div><div class="input-group"><label for="imagePath">Display Image Path (e.g., /image/my_image.jpg):</label><input type="text" id="imagePath"><button class="button" onclick="displayImage()">Display Image</button></div></div></div><div class="card"><div class="card-header">Advanced Actions</div><div class="card-body button-group"><a href="/update" class="button">Firmware Update (OTA)</a><button class="button red" onclick="factoryReset()">Factory Reset</button></div></div></div><script>function setBrightness(){var brightness=document.getElementById("brightness").value;fetch('/set?brt='+brightness).then(response=>response.text()).then(data=>alert('Brightness set: '+data)).catch(error=>console.error('Error:',error));} function reconfigureWiFi(){if (confirm("Are you sure you want to reconfigure WiFi? This will restart the device into AP mode.")){fetch('/reconfigurewifi').then(response=>response.text()).then(data=>alert('WiFi Reconfiguration triggered: '+data)).catch(error=>console.error('Error:',error));}} function setTimezone(){var gmtOffset=document.getElementById("gmtOffset").value;fetch('/set?gmt='+gmtOffset).then(response=>response.text()).then(data=>alert('Timezone set: '+data)).catch(error=>console.error('Error:',error));} function displayImage(){var imagePath=document.getElementById("imagePath").value;fetch('/set?img='+imagePath).then(response=>response.text()).then(data=>alert('Image display triggered: '+data)).catch(error=>console.error('Error:',error));} function displayTestImage(){fetch('/test').then(response=>response.text()).then(data=>alert('Test Image display triggered: '+data)).catch(error=>console.error('Error:',error));} function factoryReset(){if (confirm("WARNING: Are you sure you want to perform a factory reset? This will erase all settings and files and restart the device.")){fetch('/factoryreset').then(response=>response.text()).then(data=>alert('Factory Reset triggered: '+data)).catch(error=>console.error('Error:',error));}} function scanWiFi(){document.getElementById('scanStatus').textContent='Scanning for networks...';document.getElementById('wifiSection').style.display='none';fetch('/scan').then(response=>response.json()).then(data=>{const select=document.getElementById('wifiNetwork');select.innerHTML='<option value="">--Select a network--</option>';if (data.length===0){document.getElementById('scanStatus').textContent='No networks found';return;}data.sort((a,b)=>b.rssi-a.rssi);data.forEach(network=>{if (network.ssid&&network.ssid.trim()!==''){const option=document.createElement('option');option.value=network.ssid;let signalBars='';if (network.rssi>-60) signalBars='++++';else if (network.rssi>-70) signalBars='+++';else if (network.rssi>-80) signalBars='++';else signalBars='+';const encryption=network.encryption===7 ? '[Open]' : '[Secure]';option.textContent=`${network.ssid} ${signalBars} ${encryption}`;select.appendChild(option);}});document.getElementById('scanStatus').textContent=`Found ${data.length} network(s)`;document.getElementById('wifiSection').style.display='block';}).catch(error=>{console.error('Error:',error);document.getElementById('scanStatus').textContent='Scan failed';});} function connectWiFi(){const ssid=document.getElementById('wifiNetwork').value;const password=document.getElementById('wifiPassword').value;if (!ssid){alert('Please select a network');return;} if (confirm(`Connect to ${ssid}? The device will restart if the connection is successful.`)){document.getElementById('scanStatus').textContent='Connecting...';fetch(`/connect?ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`).then(response=>response.text()).then(data=>{alert(data);document.getElementById('scanStatus').textContent='Connection attempt sent. Please wait for device to restart...';}).catch(error=>{console.error('Error:',error);document.getElementById('scanStatus').textContent='Connection failed';});}}window.onload=function(){fetch('/app.json').then(response=>response.json()).then(data=>{document.getElementById('brightness').value=data.brt;document.getElementById('gmtOffset').value=data.gmtOffset;document.getElementById('imagePath').value=data.img;}).catch(error=>console.error('Error fetching app data:',error));};</script></body></html>)rawliteral";

void handleAppJson() {
    String json = "{";
    json += "\"theme\":" + String(currentTheme) + ",";
    json += "\"brt\":" + String(currentBrightness) + ",";
    json += "\"img\":\"";
    json += currentImage;
    json += "\",";
    json += "\"gmtOffset\":" + String(appSettings.gmtOffset);
    json += "}";
    server.send(200, "application/json", json);
}

void handleSpaceJson() {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    String json = "{";
    json += "\"total\":" + String(fs_info.totalBytes) + ",";
    json += "\"free\":" + String(fs_info.totalBytes - fs_info.usedBytes);
    json += "}";
    server.send(200, "application/json", json);
}

void handleBrtJson() {
    String json = "{\"brt\":\"" + String(currentBrightness) + "\"}";
    server.send(200, "application/json", json);
}

void handleSet() {
    bool updated = false;

    if (server.hasArg("brt")) {
        currentBrightness = server.arg("brt").toInt();
        currentBrightness = constrain(currentBrightness, 0, 100);
        displaySetBrightness(currentBrightness);
        appSettings.brightness = currentBrightness;
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("theme")) {
        currentTheme = server.arg("theme").toInt();
        appSettings.theme = currentTheme;
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("img")) {
        strncpy(currentImage, server.arg("img").c_str(), sizeof(currentImage));
        currentImage[sizeof(currentImage) - 1] = '\0'; // Ensure null-termination
        strncpy(displayState.imagePath, currentImage, sizeof(displayState.imagePath));
        displayState.imagePath[sizeof(displayState.imagePath) - 1] = '\0'; // Ensure null-termination
        displayState.showImage = true;
        displayUpdate();
        strncpy(appSettings.lastImage, currentImage, sizeof(appSettings.lastImage)); // Use sizeof for appSettings.lastImage
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("gmt")) {
        appSettings.gmtOffset = server.arg("gmt").toInt();
        timeClient.setTimeOffset(appSettings.gmtOffset);
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("clear")) {
        if (server.arg("clear") == "image") {
            Dir dir = LittleFS.openDir(IMAGE_DIR);
            while (dir.next()) {
                LittleFS.remove(dir.fileName());
            }
            updated = true;
        }
    }

    server.send(200, "text/plain", updated ? "OK" : "No action");
}

void handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        Serial.printf("Upload start: %s\n", filename.c_str());

        String dir = IMAGE_DIR;
        if (server.hasArg("dir")) {
            dir = server.arg("dir");
        }

        String filepath = dir + filename;
        uploadFile = LittleFS.open(filepath, "w");

        if (!uploadFile) {
            Serial.println(F("Failed to open file for writing"));
            logPrintf("ERROR: Failed to open file %s for writing!", filepath.c_str());
        } else {
            logPrintf("INFO: Opened file %s for writing.", filepath.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
            if (bytesWritten != upload.currentSize) {
                logPrintf("WARNING: Only %u of %u bytes written to file!", bytesWritten, upload.currentSize);
            } else {
                logPrintf("INFO: Wrote %u bytes to file.", bytesWritten);
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            logPrintf("INFO: File %s closed. Total size: %u bytes", upload.filename.c_str(), upload.totalSize);
            Serial.printf("Upload complete: %s (%u bytes)\n",
                         upload.filename.c_str(), upload.totalSize);
        }
    }
}

void handleUploadDone() {
    server.send(200, "text/plain", "OK");

    // After upload, verify file size on LittleFS
    String filename = server.upload().filename;
    String dir = IMAGE_DIR;
    if (server.hasArg("dir")) {
        dir = server.arg("dir");
    }
    String filepath = dir + filename;

    File uploadedFile = LittleFS.open(filepath, "r");
    if (uploadedFile) {
        logPrintf("INFO: Actual file size on LittleFS for %s: %u bytes", filepath.c_str(), uploadedFile.size());
        uploadedFile.close();
    } else {
        logPrintf("ERROR: Could not open %s after upload to check size.", filepath.c_str());
    }
}

void handleDelete() {
    if (server.hasArg("file")) {
        String filepath = server.arg("file");
        if (LittleFS.remove(filepath)) {
            server.send(200, "text/plain", "Deleted");
        } else {
            server.send(404, "text/plain", "Not found");
        }
    } else {
        server.send(400, "text/plain", "Missing file parameter");
    }
}

void handleApiUpdate() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        JsonDocument doc;
        deserializeJson(doc, body);

        // line1 from API now goes to displayState.line2 for custom messages
        if (!doc["line1"].isNull()) {
            strncpy(displayState.line2, doc["line1"].as<const char*>(), sizeof(displayState.line2));
            displayState.line2[sizeof(displayState.line2) - 1] = '\0'; // Ensure null-termination
        } else {
            // If line1 is not provided, clear the custom message
            displayState.line2[0] = '\0';
        }
        
        // Clear other fields as they are not used in simplified clock mode

        displayState.showImage = false;
        displayUpdate();

        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No JSON body");
    }
}

void handleReconfigureWiFi() {
    server.send(200, "text/plain", "WiFi Reconfiguration triggered. Device restarting to AP mode.");
    delay(100); // Give time for response to send

    // Set failsafe mode and display AP credentials on screen
    wifiFailsafeMode = true;
    displayShowAPScreen(WIFI_AP_NAME, apPassword.c_str(), WiFi.softAPIP().toString().c_str());
    delay(1000); // Give time for display update

    wifiManager.startConfigPortal(WIFI_AP_NAME, apPassword.c_str()); // Restart into AP mode with random password

    // After config portal exits, check if connected
    if (WiFi.status() == WL_CONNECTED) {
        wifiFailsafeMode = false;
        displayShowMessage("WiFi OK\n" + WiFi.localIP().toString());
        delay(2000);
    }
    // ESP.restart(); // WiFiManager.startConfigPortal() usually reboots itself
}

// Function to handle factory reset
void handleFactoryReset() {
    server.send(200, "text/plain", "Factory Reset triggered. Clearing data and restarting...");
    delay(100); // Give time for response to send

    logPrint(F("Performing factory reset..."));

    // Clear WiFi credentials FIRST (most important)
    logPrint(F("Clearing WiFi credentials..."));
    WiFi.disconnect(true);  // Disconnect and erase WiFi credentials
    delay(100);

    // Use WiFiManager to reset settings (clears WiFi config sector)
    wifiManager.resetSettings();
    logPrint(F("WiFiManager settings cleared."));
    delay(100);

    // Also erase ESP8266 WiFi config sector for complete wipe
    ESP.eraseConfig();
    logPrint(F("ESP WiFi config erased."));
    delay(100);

    // Clear EEPROM settings (our custom settings)
    settingsInit(); // Ensure EEPROM is ready
    Settings defaultSettings;
    settingsReset(defaultSettings);  // Use the proper reset function
    settingsSave(defaultSettings);
    logPrint(F("EEPROM settings cleared/reset."));

    // Clear boot counter
    bootCounterReset();
    logPrint(F("Boot counter reset."));

    // Format LittleFS (delete all files)
    logPrint(F("Formatting LittleFS..."));
    LittleFS.format();
    logPrint(F("LittleFS formatted."));

    logPrint(F("Factory reset complete. Restarting..."));
    delay(1000);

    ESP.restart(); // Restart the device
}

void handleOTAForm() {
    server.send(200, "text/html",
        "<!DOCTYPE html><html><body>"
        "<h1>SmartClock OTA Update</h1>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='update'><br><br>"
        "<input type='submit' value='Update Firmware'>"
        "</form></body></html>");
}

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA Update Start: %s\n", upload.filename.c_str());
        displayShowMessage("OTA Update...");

        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        } else {
            int percent = (Update.progress() * 100) / Update.size();
            Serial.printf("Progress: %d%%\n", percent);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
            displayShowMessage("Success!");
        } else {
            Update.printError(Serial);
            displayShowMessage("OTA Failed!");
        }
    }
}

void handleOTADone() {
    bool shouldReboot = !Update.hasError();
    server.send(200, "text/plain", shouldReboot ? "OK - Rebooting..." : "FAIL");

    if (shouldReboot) {
        delay(1000);
        ESP.restart();
    }
}

void handleLog() {
    String log = logGetAll();
    server.send(200, "text/plain", log);
}

void handleWiFiScan() {
    logPrint(F("Starting WiFi scan..."));

    // Scan for networks (async scan to avoid blocking AP mode)
    int numNetworks = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

    String json = "[";
    for (int i = 0; i < numNetworks; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }
    json += "]";

    WiFi.scanDelete(); // Clear scan results
    logPrintf("WiFi scan complete. Found %d networks", numNetworks);

    server.send(200, "application/json", json);
}

void handleWiFiConnect() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "Missing SSID parameter");
        return;
    }

    String ssid = server.arg("ssid");
    String password = server.hasArg("password") ? server.arg("password") : "";

    logPrintf("Attempting to connect to WiFi: %s", ssid.c_str());
    server.send(200, "text/plain", "Connecting to " + ssid + "... Device will restart if successful.");
    delay(100);

    // Enable persistent WiFi credentials storage
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);

    // Disconnect from AP mode and switch to STA mode
    WiFi.softAPdisconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    delay(100);

    // Connect to new WiFi with credentials
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait up to 20 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        attempts++;
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        logPrintf("Successfully connected to %s", ssid.c_str());
        logPrintf("IP address: %s", WiFi.localIP().toString().c_str());

        // Credentials are now saved, restart to apply changes
        delay(1000);
        ESP.restart();
    } else {
        logPrintf("Failed to connect to %s", ssid.c_str());
        // Restart AP mode
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_NAME, apPassword.c_str());
        logPrint(F("Connection failed, AP mode restarted"));
    }
}

void handleRoot() {
    server.send(200, "text/html", index_html);
}

void webserverInit() {
    // GET endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/app.json", HTTP_GET, handleAppJson);
    server.on("/space.json", HTTP_GET, handleSpaceJson);
    server.on("/brt.json", HTTP_GET, handleBrtJson);
    server.on("/set", HTTP_GET, handleSet);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/log", HTTP_GET, handleLog);
    server.on("/reconfigurewifi", HTTP_GET, handleReconfigureWiFi);
    server.on("/factoryreset", HTTP_GET, handleFactoryReset);
    server.on("/scan", HTTP_GET, handleWiFiScan);
    server.on("/connect", HTTP_GET, handleWiFiConnect);

    // POST endpoints
    server.on("/api/update", HTTP_POST, handleApiUpdate);

    // File upload
    server.on("/doUpload", HTTP_POST, handleUploadDone, handleFileUpload);

    // OTA
    server.on("/update", HTTP_GET, handleOTAForm);
    server.on("/update", HTTP_POST, handleOTADone, handleOTAUpload);

    currentImage[0] = '\0'; // Initialize currentImage as empty

    server.begin();
    Serial.println(F("Web server started"));
}

void webserverHandle() {
    server.handleClient();
}
