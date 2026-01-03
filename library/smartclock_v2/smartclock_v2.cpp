#include "smartclock_v2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <ArduinoJson.h>

#ifdef USE_ESP8266
#include <FS.h>
#include <LittleFS.h>
#endif

namespace esphome {
namespace smartclock_v2 {

static const char *const TAG = "smartclock_v2";

void SmartClockV2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SmartClock V2 (AsyncWebServer test)...");

#ifdef USE_ESP8266
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    ESP_LOGE(TAG, "LittleFS mount failed!");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "LittleFS mounted successfully");

  // Create /image directory if it doesn't exist
  if (!LittleFS.exists("/image")) {
    LittleFS.mkdir("/image");
    ESP_LOGI(TAG, "Created /image directory");
  }

  // Clear existing images (temporary storage)
  Dir dir = LittleFS.openDir("/image");
  int cleared = 0;
  while (dir.next()) {
    String path = "/image/" + dir.fileName();
    if (LittleFS.remove(path)) {
      cleared++;
    }
  }
  if (cleared > 0) {
    ESP_LOGI(TAG, "Cleared %d temporary image(s)", cleared);
  }
#endif

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

void SmartClockV2Component::setup_handlers() {
  auto *server = web_server_base::global_web_server_base->get_server();

  // Test handler: Simple GET endpoint
  server->on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP_LOGI(TAG, "Test endpoint called!");
    request->send(200, "text/plain", "SmartClock V2 - AsyncWebServer works!");
  });

  // File upload handler with multipart support (/doUpload voor backwards compatibility)
  server->on(
      "/doUpload", HTTP_POST,
      // Request handler (after upload completes)
      [this](AsyncWebServerRequest *request) {
        ESP_LOGI(TAG, "Upload complete: %s", this->upload_filename_.c_str());
        request->send(200, "text/plain", "OK");
      },
      // Upload handler (during upload)
      [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len,
         bool final) {
#ifdef USE_ESP8266
        if (index == 0) {
          // Start of upload
          this->upload_filename_ = filename;
          String filepath = "/image/" + filename;

          ESP_LOGI(TAG, "Upload start: %s", filename.c_str());

          // Open file for writing
          this->upload_file_ = LittleFS.open(filepath, "w");
          if (!this->upload_file_) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath.c_str());
            return;
          }
        }

        // Write chunk to file
        if (this->upload_file_) {
          size_t written = this->upload_file_.write(data, len);
          if (written != len) {
            ESP_LOGW(TAG, "Only wrote %u of %u bytes", written, len);
          }
        }

        if (final) {
          // End of upload
          if (this->upload_file_) {
            this->upload_file_.close();
            ESP_LOGI(TAG, "Upload finished: %s, total size=%u bytes",
                     filename.c_str(), index + len);

            // Verify file was written
            File f = LittleFS.open("/image/" + filename, "r");
            if (f) {
              ESP_LOGI(TAG, "Verified file size: %u bytes", f.size());
              f.close();
            }
          }
        }
#endif
      });

  // API update handler with JSON body (/api/update voor live display updates)
  server->on("/api/update", HTTP_POST, [this](AsyncWebServerRequest *request) {}, nullptr,
             [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
               // Parse JSON body
               JsonDocument doc;
               DeserializationError error = deserializeJson(doc, data, len);

               if (error) {
                 ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
                 request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                 return;
               }

               ESP_LOGI(TAG, "API update received");

               // Update state from JSON fields
               if (!doc["line1"].isNull()) {
                 this->line_1_ = doc["line1"].as<std::string>();
                 ESP_LOGI(TAG, "  line1: %s", this->line_1_.c_str());
               }
               if (!doc["line2"].isNull()) {
                 this->line_2_ = doc["line2"].as<std::string>();
                 ESP_LOGI(TAG, "  line2: %s", this->line_2_.c_str());
               }
               if (!doc["line3"].isNull()) {
                 this->line_3_ = doc["line3"].as<std::string>();
               }
               if (!doc["line4"].isNull()) {
                 this->line_4_ = doc["line4"].as<std::string>();
               }
               if (!doc["line5"].isNull()) {
                 this->line_5_ = doc["line5"].as<std::string>();
               }
               if (!doc["line6"].isNull()) {
                 this->line_6_ = doc["line6"].as<std::string>();
               }
               if (!doc["media"].isNull()) {
                 this->line_media_ = doc["media"].as<std::string>();
                 ESP_LOGI(TAG, "  media: %s", this->line_media_.c_str());
               }
               if (!doc["bar"].isNull()) {
                 this->bar_value_ = doc["bar"].as<float>();
                 ESP_LOGI(TAG, "  bar: %.2f", this->bar_value_);
               }

               // Trigger display update via callback
               if (this->update_callback_) {
                 this->update_callback_();
               }

               request->send(200, "application/json", "{\"status\":\"ok\"}");
             });

  // GET endpoint for status
  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["line1"] = this->line_1_;
    doc["line2"] = this->line_2_;
    doc["line3"] = this->line_3_;
    doc["line4"] = this->line_4_;
    doc["line5"] = this->line_5_;
    doc["line6"] = this->line_6_;
    doc["media"] = this->line_media_;
    doc["bar"] = this->bar_value_;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  ESP_LOGI(TAG, "Handlers registered on AsyncWebServer");
}

void SmartClockV2Component::loop() {
  // Scroll animation (30ms interval like in original YAML)
  uint32_t now = millis();
  if (now - this->last_scroll_ >= 30) {
    this->last_scroll_ = now;
    this->scroll_pos_ -= 3;
    if (this->scroll_pos_ < -400) {
      this->scroll_pos_ = 240;
    }

    // Trigger display update via callback if set
    if (this->update_callback_) {
      this->update_callback_();
    }
  }
}

void SmartClockV2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SmartClock V2:");
  ESP_LOGCONFIG(TAG, "  AsyncWebServer integration: YES");
  ESP_LOGCONFIG(TAG, "  Test endpoint: http://smartclock.local/test");
  ESP_LOGCONFIG(TAG, "  Upload endpoint: http://smartclock.local/doUpload");
  ESP_LOGCONFIG(TAG, "  API endpoint: http://smartclock.local/api/update");
}

}  // namespace smartclock_v2
}  // namespace esphome
