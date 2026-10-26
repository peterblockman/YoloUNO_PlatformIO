#include <Arduino.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifndef ENABLE_DHT20
#define ENABLE_DHT20 1
#endif
#if ENABLE_DHT20
#include <Wire.h>
#include <DHT20.h>
#endif

constexpr uint8_t LED_PIN = 48;
constexpr uint32_t LED_STEP_MS = 2000;
constexpr uint32_t SENSOR_PERIOD_MS = 5000;
SemaphoreHandle_t logMutex;

void printLog(const char *format, ...) {
  char message[180];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  xSemaphoreTake(logMutex, portMAX_DELAY);
  Serial.printf("[%8lu ms] %s\n", (unsigned long)millis(), message);
  xSemaphoreGive(logMutex);
}

void ledTask(void *) {
  bool on = false;
  uint32_t edge = 0;
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  for (;;) {
    on = !on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    printLog("LED edge=%lu state=%s gpio=%u step_ms=%lu", (unsigned long)++edge,
             on ? "ON" : "OFF", LED_PIN, (unsigned long)LED_STEP_MS);
    vTaskDelay(pdMS_TO_TICKS(LED_STEP_MS));
  }
}

#if ENABLE_DHT20
void sensorTask(void *) {
  // Cam bien va bus I2C chi do tac vu nay truy cap.
  DHT20 sensor;
  bool ready = false;
  uint32_t sequence = 0;
  vTaskDelay(pdMS_TO_TICKS(1500));
  for (;;) {
    if (!ready) {
      ready = sensor.begin(11, 12);
      printLog("DHT20_INIT ready=%u sda=11 scl=12 library=0.2.2", ready);
    }
    int status = ready ? sensor.read() : DHT20_ERROR_CONNECT;
    float temperature = sensor.getTemperature();
    float humidity = sensor.getHumidity();
    if (status == DHT20_OK && isfinite(temperature) && isfinite(humidity)) {
      printLog("DHT20 sample=%lu temp=%.2f C hum=%.2f %% status=OK",
               (unsigned long)++sequence, temperature, humidity);
    } else {
      printLog("DHT20_ERROR status=%d sample_not_published=1", status);
      ready = false;
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1500);
  logMutex = xSemaphoreCreateMutex();
  if (!logMutex) {
    Serial.println("INIT_ERROR log mutex");
    while (true) delay(1000);
  }
  printLog("VIDEO_%u READY led_step_ms=%lu sensor_period_ms=%lu tick_ms=%u",
           ENABLE_DHT20 ? 7 : 6, (unsigned long)LED_STEP_MS,
           (unsigned long)SENSOR_PERIOD_MS, (unsigned)portTICK_PERIOD_MS);
  BaseType_t ledCreated = xTaskCreate(ledTask, "LED", 3072, nullptr, 2, nullptr);
  if (ledCreated != pdPASS) printLog("INIT_ERROR LED task");
#if ENABLE_DHT20
  BaseType_t sensorCreated = xTaskCreate(sensorTask, "DHT20", 4096, nullptr, 2, nullptr);
  if (sensorCreated != pdPASS) printLog("INIT_ERROR DHT20 task");
#endif
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
