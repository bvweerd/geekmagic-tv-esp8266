// Stub SD.h - TJpg_Decoder requires this header but we don't use SD cards
// We only use LittleFS for JPEG storage
#pragma once

#include <Arduino.h>
#include <FS.h>

// FILE_READ constant
#define FILE_READ "r"

// Stub SD class that does nothing (we use LittleFS instead)
class SDClass {
 public:
  bool begin(uint8_t csPin = 0, uint32_t frequency = 4000000) { return false; }
  bool exists(const char* path) { return false; }
  bool exists(const String& path) { return false; }
  File open(const char* path, const char* mode = FILE_READ) { return File(); }
  File open(const String& path, const char* mode = FILE_READ) { return File(); }
  bool remove(const char* path) { return false; }
  bool remove(const String& path) { return false; }
};

// Global SD object
extern SDClass SD;
