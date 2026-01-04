#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/display/display.h"
#include <vector>

#ifdef USE_ESP8266
#include <FS.h>
#include <LittleFS.h>
#endif

namespace esphome {
namespace smartclock_v2 {

// Settings structure (persistent storage)
struct SmartClockSettings {
  int brightness = 70;
  int theme = 0;
  char image_path[64] = "";
  int32_t gmt_offset = 0;
};

class SmartClockV2Component : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Set light entity for brightness control
  void set_backlight(light::LightState *backlight) { this->backlight_ = backlight; }

  // Set display for JPEG rendering and direct updates
  void set_display(display::Display *display) { this->display_ = display; }

  // Render JPEG image from LittleFS to display (called from YAML)
  bool render_jpeg_image(display::Display &it, const char *path);

  // Getters for YAML lambda to access state
  std::string get_time_string() const { return time_string_; }
  std::string get_custom_message() const { return custom_message_; }
  std::string get_image_path() const { return settings_.image_path; }
  bool get_show_image() const { return show_image_; }
  bool get_image_decoded() const { return image_decoded_; }
  void set_image_decoded(bool decoded) { image_decoded_ = decoded; }
  int get_brightness() const { return settings_.brightness; }
  int32_t get_gmt_offset() const { return settings_.gmt_offset; }

  // Setters
  void set_time_string(const std::string &time) { time_string_ = time; }
  void set_brightness(int brightness);
  void set_image_path(const std::string &path);
  void set_gmt_offset(int32_t offset) { settings_.gmt_offset = offset; save_settings(); }

 protected:
  void setup_handlers();
  void load_settings();
  void save_settings();
  void log(const std::string &message);

  light::LightState *backlight_{nullptr};
  display::Display *display_{nullptr};
  ESPPreferenceObject pref_;

  // TJpgDec callback - static omdat TJpgDec C callback verwacht
  static SmartClockV2Component *instance_;
  static bool tjpg_output_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

  // Settings
  SmartClockSettings settings_;

  // Display state
  std::string time_string_{""};
  std::string custom_message_{""};
  bool show_image_{false};
  bool image_decoded_{false};  // Track if current image has been decoded

  // Log buffer (circular buffer, last 50 messages)
  std::vector<std::string> log_buffer_;
  const size_t MAX_LOG_ENTRIES = 50;

#ifdef USE_ESP8266
  // File upload state
  File upload_file_;
  String upload_filename_;
  String upload_filepath_;
  bool is_uploading_{false};
#endif
};

}  // namespace smartclock_v2
}  // namespace esphome