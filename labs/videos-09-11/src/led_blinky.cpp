#include "led_blinky.h"
namespace {
void edge(bool on, uint8_t mode, uint32_t hold) {
  digitalWrite(48, on ? HIGH : LOW);
  logLine("LED mode=%u state=%s hold_ms=%lu", mode, on ? "ON" : "OFF", (unsigned long)hold);
  vTaskDelay(pdMS_TO_TICKS(hold));
}
}
void ledTask(void *) {
  pinMode(48, OUTPUT);
  for (;;) {
    uint8_t mode = snapshot().ledMode;
    if (mode == 0) { edge(true, mode, 1000); edge(false, mode, 1000); }
    else if (mode == 1) { edge(true, mode, 100); edge(false, mode, 900); }
    else { edge(true, mode, 100); edge(false, mode, 100); edge(true, mode, 100); edge(false, mode, 1700); }
  }
}
