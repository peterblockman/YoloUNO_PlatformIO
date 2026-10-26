#include "neo_blinky.h"
#include <Adafruit_NeoPixel.h>
void neoTask(void *) {
  Adafruit_NeoPixel pixel(1, 45, NEO_GRB + NEO_KHZ800);
  pixel.begin(); pixel.clear(); pixel.show();
  uint8_t phase = 0;
  for (;;) {
    uint8_t mode = snapshot().neoMode;
    uint8_t color = mode == 0 ? phase++ % 3 : mode == 4 ? 1 : mode - 1;
    uint8_t level = mode == 4 ? 8 : 64;
    uint8_t r = color == 0 ? level : 0, g = color == 1 ? level : 0, b = color == 2 ? level : 0;
    pixel.setPixelColor(0, pixel.Color(r,g,b)); pixel.show();
    logLine("NEO mode=%u state=ON rgb=%u,%u,%u hold_ms=500",mode,r,g,b);
    vTaskDelay(pdMS_TO_TICKS(500));
    pixel.clear(); pixel.show();
    logLine("NEO mode=%u state=OFF hold_ms=500",mode);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
