#include "global.h"
#include <stdarg.h>
SharedState sharedState;
SemaphoreHandle_t stateMutex, logMutex;
SharedState snapshot() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  SharedState copy = sharedState;
  xSemaphoreGive(stateMutex);
  return copy;
}
void logLine(const char *format, ...) {
  char buffer[240]; va_list args; va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
  xSemaphoreTake(logMutex, portMAX_DELAY);
  Serial.printf("[%8lu ms] %s\n", (unsigned long)millis(), buffer);
  xSemaphoreGive(logMutex);
}
