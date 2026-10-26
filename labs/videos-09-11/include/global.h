#pragma once
#include <Arduino.h>
#include "freertos/semphr.h"
struct SharedState {
  uint8_t ledMode = 0; // 0: deu, 1: loe mot lan, 2: loe hai lan
  uint8_t neoMode = 0; // 0: RGB, 1: do, 2: xanh la, 3: xanh duong, 4: xanh la mo
  bool automatic = false;
  bool valid = false;
  float temperature = NAN, humidity = NAN;
  float threshold = 30.0f;
  uint32_t sequence = 0;
};
extern SharedState sharedState;
extern SemaphoreHandle_t stateMutex, logMutex;
SharedState snapshot();
void logLine(const char *format, ...);
