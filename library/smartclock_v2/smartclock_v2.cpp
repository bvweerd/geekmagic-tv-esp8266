#include "smartclock_v2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <ArduinoJson.h>

#ifdef USE_ESP8266
#include <FS.h>
#include <LittleFS.h>
#endif

// Include TJpg_Decoder only in .cpp to avoid SD.h dependency issues in header
#include <TJpg_Decoder.h>

namespace esphome {
namespace smartclock_v2 {

static const char *const TAG = "smartclock_v2";

// Static instance pointer voor TJpgDec callback
SmartClockV2Component *SmartClockV2Component::instance_ = nullptr;

void SmartClockV2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SmartClock V2...");

  // Set static instance pointer for TJpgDec callback
  instance_ = this;

  // Initialize TJpgDec
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(SmartClockV2Component::tjpg_output_callback);
  ESP_LOGI(TAG, "TJpgDec initialized");

#ifdef USE_ESP8266
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    ESP_LOGW(TAG, "LittleFS mount failed. Formatting LittleFS...");

    // Format LittleFS if mounting fails (just like original code)
    LittleFS.format();
    ESP_LOGI(TAG, "LittleFS formatted. Restarting...");

    delay(3000);
    App.safe_reboot();
    return;
  }
  ESP_LOGI(TAG, "LittleFS mounted successfully");

  // Create /image directory if it doesn't exist
  if (!LittleFS.exists("/image")) {
    LittleFS.mkdir("/image");
    ESP_LOGI(TAG, "Created /image directory");
  }

  // Image clearing on startup is disabled to allow for persistent images.
  // Images can be cleared manually via the /set?clear=image endpoint.
  // ESP_LOGI(TAG, "Image persistence is enabled.");
#endif

  // Load settings from flash
  // Use a fixed hash for the preferences storage
  this->pref_ = global_preferences->make_preference<SmartClockSettings>(fnv1_hash("smartclock_v2_settings"));
  this->load_settings();

  // Apply saved brightness to backlight (defer to after components are ready)
  this->defer([this]() {
    if (this->backlight_ != nullptr) {
      ESP_LOGI(TAG, "Applying saved brightness: %d", this->settings_.brightness);
      auto call = this->backlight_->make_call();
      call.set_brightness(this->settings_.brightness / 100.0f);
      call.perform();
    }
  });

  // Get the global web server base
  auto *server = web_server_base::global_web_server_base;

  if (server == nullptr) {
    ESP_LOGE(TAG, "Web server base not available! Add web_server_base to your config.");
    this->mark_failed();
    return;
  }

  this->setup_handlers();

  ESP_LOGCONFIG(TAG, "SmartClock V2 setup complete");
}

void SmartClockV2Component::load_settings() {
  if (!this->pref_.load(&this->settings_)) {
    ESP_LOGI(TAG, "No saved settings, using defaults");
  } else {
    ESP_LOGI(TAG, "Loaded settings: brightness=%d, theme=%d, gmt_offset=%d",
             this->settings_.brightness, this->settings_.theme, this->settings_.gmt_offset);
  }
}

void SmartClockV2Component::save_settings() {
  this->pref_.save(&this->settings_);
  ESP_LOGD(TAG, "Settings saved");
}

void SmartClockV2Component::log(const std::string &message) {
  // Add to circular log buffer
  if (this->log_buffer_.size() >= MAX_LOG_ENTRIES) {
    this->log_buffer_.erase(this->log_buffer_.begin());
  }
  this->log_buffer_.push_back(message);
  ESP_LOGI(TAG, "%s", message.c_str());
}

void SmartClockV2Component::set_brightness(int brightness) {
  brightness = std::max(0, std::min(100, brightness));  // Constrain 0-100
  this->settings_.brightness = brightness;
  this->save_settings();

  // Update the actual light entity if available
  if (this->backlight_ != nullptr) {
    auto call = this->backlight_->make_call();
    call.set_brightness(brightness / 100.0f);
    call.perform();
  }

  this->log("Brightness set to " + std::to_string(brightness));
}

void SmartClockV2Component::set_image_path(const std::string &path) {
  strncpy(this->settings_.image_path, path.c_str(), sizeof(this->settings_.image_path) - 1);
  this->settings_.image_path[sizeof(this->settings_.image_path) - 1] = '\0';
  this->show_image_ = true;
  this->image_decoded_ = false;  // Reset flag to force re-decode
  this->save_settings();

  // Trigger display update directly
  if (this->display_) {
    this->display_->update();
  }

  this->log("Image path set to: " + path);
}

void SmartClockV2Component::setup_handlers() {
  auto *server = web_server_base::global_web_server_base->get_server();

  // GET /app.json - App status (like original webserver.cpp)
  server->on("/app.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["theme"] = this->settings_.theme;
    doc["brt"] = this->settings_.brightness;
    doc["img"] = this->settings_.image_path;
    doc["gmtOffset"] = this->settings_.gmt_offset;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // GET /space.json - LittleFS storage info
  server->on("/space.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
#ifdef USE_ESP8266
    FSInfo fs_info;
    LittleFS.info(fs_info);

    JsonDocument doc;
    doc["total"] = fs_info.totalBytes;
    doc["free"] = fs_info.totalBytes - fs_info.usedBytes;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
#else
    request->send(501, "text/plain", "Not implemented on this platform");
#endif
  });

  // GET /brt.json - Brightness info
  server->on("/brt.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["brt"] = String(this->settings_.brightness).c_str();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // GET /set - Set parameters (brt, img, gmt, theme, clear)
  server->on("/set", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (this->is_uploading_) {
      request->send(503, "text/plain", "Busy uploading");
      return;
    }

    bool updated = false;

    // Set brightness
    if (request->hasParam("brt")) {
      int brightness = request->getParam("brt")->value().toInt();
      this->set_brightness(brightness);
      updated = true;
    }

    // Set theme
    if (request->hasParam("theme")) {
      int theme = request->getParam("theme")->value().toInt();
      this->settings_.theme = theme;
      this->save_settings();
      updated = true;
    }

    // Set image path
    if (request->hasParam("img")) {
      String img_path = request->getParam("img")->value();
      this->set_image_path(img_path.c_str());
      updated = true;
    }

    // Set GMT offset
    if (request->hasParam("gmt")) {
      int32_t gmt = request->getParam("gmt")->value().toInt();
      this->set_gmt_offset(gmt);
      updated = true;
    }

    // Clear images
    if (request->hasParam("clear")) {
      String clear_type = request->getParam("clear")->value();
      if (clear_type == "image") {
#ifdef USE_ESP8266
        Dir dir = LittleFS.openDir("/image");
        int cleared = 0;
        while (dir.next()) {
          String path = "/image/" + dir.fileName();
          if (LittleFS.remove(path)) {
            cleared++;
          }
        }
        this->log("Cleared " + std::to_string(cleared) + " image(s)");
        updated = true;
#endif
      }
    }

    request->send(200, "text/plain", updated ? "OK" : "No action");
  });

  // GET /delete - Delete specific file
  server->on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
#ifdef USE_ESP8266
      String filepath = request->getParam("file")->value();
      if (LittleFS.remove(filepath)) {
        this->log("Deleted file: " + std::string(filepath.c_str()));
        request->send(200, "text/plain", "Deleted");
      } else {
        request->send(404, "text/plain", "Not found");
      }
#else
      request->send(501, "text/plain", "Not implemented");
#endif
    } else {
      request->send(400, "text/plain", "Missing file parameter");
    }
  });

  // GET /log - View logs
  server->on("/log", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String log_output = "";
    for (const auto &entry : this->log_buffer_) {
      log_output += entry.c_str();
      log_output += "\n";
    }
    request->send(200, "text/plain", log_output);
  });

  // POST /api/update - JSON API update (for display updates)
  server->on("/api/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("plain", true)) {
        String body = request->getParam("plain", true)->value();
        JsonDocument doc;
        deserializeJson(doc, body);

        // Update custom message (line1 from API -> custom_message_)
        if (!doc["line1"].isNull()) {
            this->custom_message_ = doc["line1"].as<std::string>();
            ESP_LOGD(TAG, "Custom message: %s", this->custom_message_.c_str());
        } else {
            this->custom_message_ = "";
        }

        // Clear image display when API update is received
        this->show_image_ = false;
        this->image_decoded_ = false;  // Reset decoded flag

        // Trigger display update directly
        if (this->display_) {
            this->display_->update();
        }

        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "No JSON body");
    }
  });

  // GET /api/status - Get current status
  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["time"] = this->time_string_;
    doc["message"] = this->custom_message_;
    doc["image"] = this->settings_.image_path;
    doc["show_image"] = this->show_image_;
    doc["brightness"] = this->settings_.brightness;
    doc["gmt_offset"] = this->settings_.gmt_offset;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

    // File upload handler (/doUpload for backwards compatibility)

    server->on(

        "/doUpload", HTTP_POST,

        [this](AsyncWebServerRequest *request) {

          // The request is handled by the body handler, which responds when the upload is complete.

        },

        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len,

           bool final) {

  #ifdef USE_ESP8266

          if (index == 0) {

            this->is_uploading_ = true;

            this->upload_filename_ = filename;

            

            String target_dir = "/image/"; // Default with trailing slash

            if (request->hasParam("dir", true)) { // Also check POST parameters

                target_dir = request->getParam("dir", true)->value();

                if (!target_dir.startsWith("/")) {

                    target_dir = "/" + target_dir;

                }

                if (!target_dir.endsWith("/")) {

                    target_dir += "/";

                }

            }

  

            if (!LittleFS.exists(target_dir)) {

              LittleFS.mkdir(target_dir);

              ESP_LOGI(TAG, "Created directory: %s", target_dir.c_str());

            }

  

            this->upload_filepath_ = target_dir + filename;

            ESP_LOGI(TAG, "Upload start: %s to %s", filename.c_str(), this->upload_filepath_.c_str());

            this->upload_file_ = LittleFS.open(this->upload_filepath_, "w");

            if (!this->upload_file_) {

              ESP_LOGE(TAG, "Failed to open file for writing: %s", this->upload_filepath_.c_str());

              this->is_uploading_ = false; // Reset flag on failure

              return;

            }

          }

  

          if (this->upload_file_) {

            if (this->upload_file_.write(data, len) != len) {

              ESP_LOGE(TAG, "Error writing chunk to file");

            }

          }

  

          if (final) {

            if (this->upload_file_) {

              ESP_LOGI(TAG, "Upload finished: %s, total size=%u bytes", this->upload_filepath_.c_str(), index + len);

              this->upload_file_.close();

              this->upload_filepath_ = ""; // Clear path

              request->send(200, "text/plain", "OK"); // Respond only when completely finished

            } else {

              if (!request->isSent()) {

                request->send(500, "text/plain", "Upload failed, no file handle");

              }

            }

            this->is_uploading_ = false; // Reset flag when done

          }

  #endif

        });

  ESP_LOGI(TAG, "API endpoints registered");
}

void SmartClockV2Component::loop() {
  // No scroll animation needed for simple clock display
  // Time updates will come from YAML time component
}

void SmartClockV2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SmartClock V2:");
  ESP_LOGCONFIG(TAG, "  Brightness: %d", this->settings_.brightness);
  ESP_LOGCONFIG(TAG, "  Theme: %d", this->settings_.theme);
  ESP_LOGCONFIG(TAG, "  GMT Offset: %d", this->settings_.gmt_offset);
  ESP_LOGCONFIG(TAG, "  Image Path: %s", this->settings_.image_path);
  ESP_LOGCONFIG(TAG, "API Endpoints:");
  ESP_LOGCONFIG(TAG, "  GET  /app.json");
  ESP_LOGCONFIG(TAG, "  GET  /space.json");
  ESP_LOGCONFIG(TAG, "  GET  /brt.json");
  ESP_LOGCONFIG(TAG, "  GET  /set?brt=<0-100>&img=<path>&gmt=<seconds>&clear=image");
  ESP_LOGCONFIG(TAG, "  GET  /delete?file=<path>");
  ESP_LOGCONFIG(TAG, "  GET  /log");
  ESP_LOGCONFIG(TAG, "  POST /api/update (JSON: {\"line1\":\"message\"})");
  ESP_LOGCONFIG(TAG, "  GET  /api/status");
  ESP_LOGCONFIG(TAG, "  POST /doUpload");
}

// TJpgDec callback - called by decoder with chunks of decoded image data
bool SmartClockV2Component::tjpg_output_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (instance_ == nullptr || instance_->display_ == nullptr) {
    return false;
  }

  // Push RGB565 pixels to display
  // Process in chunks and yield to prevent watchdog
  const uint16_t PIXELS_PER_YIELD = 240;  // Yield every ~1 line on 240px display
  uint16_t pixel_count = 0;

  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint16_t pixel = bitmap[row * w + col];

      // Convert RGB565 to RGB888
      // Note: TJpgDec uses BGR order due to setSwapBytes(true)
      uint8_t b = ((pixel >> 11) & 0x1F) << 3;  // 5 bits -> 8 bits
      uint8_t g = ((pixel >> 5) & 0x3F) << 2;   // 6 bits -> 8 bits
      uint8_t r = (pixel & 0x1F) << 3;          // 5 bits -> 8 bits

      // Color constructor expects RGB, but display needs G and B swapped
      instance_->display_->draw_pixel_at(x + col, y + row, Color(r, b, g));

      // Yield to watchdog periodically
      pixel_count++;
      if (pixel_count >= PIXELS_PER_YIELD) {
        yield();
        pixel_count = 0;
      }
    }
  }

  return true;
}

bool SmartClockV2Component::render_jpeg_image(display::Display &it, const char *path) {
#ifdef USE_ESP8266
  if (path == nullptr || strlen(path) == 0) {
    ESP_LOGW(TAG, "No image path provided");
    return false;
  }

  // Check if file exists
  if (!LittleFS.exists(path)) {
    ESP_LOGW(TAG, "Image file not found: %s", path);
    return false;
  }

  // Open file
  File jpgFile = LittleFS.open(path, "r");
  if (!jpgFile) {
    ESP_LOGE(TAG, "Failed to open image file: %s", path);
    return false;
  }

  ESP_LOGI(TAG, "Decoding JPEG: %s (%u bytes)", path, jpgFile.size());

  // Store display reference for callback
  this->display_ = &it;

  // Feed watchdog before decode
  ESP.wdtFeed();
  yield();

  // Decode JPEG from file
  uint8_t result = TJpgDec.drawFsJpg(0, 0, jpgFile);

  jpgFile.close();

  // Feed watchdog after decode
  ESP.wdtFeed();
  yield();

  if (result != 0) {
    ESP_LOGE(TAG, "JPEG decode failed with error: %u", result);
    return false;
  }

  ESP_LOGI(TAG, "JPEG decoded successfully");
  return true;
#else
  ESP_LOGW(TAG, "JPEG rendering only supported on ESP8266");
  return false;
#endif
}

}  // namespace smartclock_v2
}  // namespace esphome
