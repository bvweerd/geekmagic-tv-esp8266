#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <functional>

#ifdef USE_ESP8266
#include <FS.h>
#include <LittleFS.h>
#endif

namespace esphome {
namespace smartclock_v2 {

class SmartClockV2Component : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Set callback to trigger display update from YAML
  void set_update_callback(std::function<void()> callback) { this->update_callback_ = callback; }

  // Getters for YAML lambda to access state
  std::string get_line_1() const { return line_1_; }
  std::string get_line_2() const { return line_2_; }
  std::string get_line_3() const { return line_3_; }
  std::string get_line_4() const { return line_4_; }
  std::string get_line_5() const { return line_5_; }
  std::string get_line_6() const { return line_6_; }
  std::string get_line_media() const { return line_media_; }
  float get_bar_value() const { return bar_value_; }
  int get_scroll_pos() const { return scroll_pos_; }

 protected:
  void setup_handlers();

  std::function<void()> update_callback_{nullptr};

  // Display state (accessible from YAML lambda)
  std::string line_1_{""};
  std::string line_2_{""};
  std::string line_3_{""};
  std::string line_4_{""};
  std::string line_5_{""};
  std::string line_6_{""};
  std::string line_media_{""};
  float bar_value_{0.0f};
  int scroll_pos_{240};
  uint32_t last_scroll_{0};

#ifdef USE_ESP8266
  // File upload state
  File upload_file_;
  String upload_filename_;
#endif
};

}  // namespace smartclock_v2
}  // namespace esphome
