#include "temp_humi_monitor.h"
#include <DHT20.h>
void sensorTask(void *) {
  DHT20 sensor; bool ready = false;
  vTaskDelay(pdMS_TO_TICKS(1500));
  for (;;) {
    if (!ready) ready = sensor.begin(11,12);
    int status = ready ? sensor.read() : DHT20_ERROR_CONNECT;
    float t = NAN, h = NAN;
    if (status == DHT20_OK) { t = sensor.getTemperature(); h = sensor.getHumidity(); }
    bool valid = status == DHT20_OK && isfinite(t) && isfinite(h);
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    sharedState.valid = valid;
    if (valid) {
      sharedState.temperature=t; sharedState.humidity=h; ++sharedState.sequence;
      if (sharedState.automatic && !sharedState.webControl) {
        bool hot=t>=sharedState.threshold;
        sharedState.ledMode=hot?2:0;
        sharedState.neoMode=hot?1:2;
      }
    }
    SharedState copy=sharedState;
    xSemaphoreGive(stateMutex);
    if (valid) logLine("DHT20 sample=%lu temp=%.2f hum=%.2f auto=%u threshold=%.2f led=%u neo=%u",(unsigned long)copy.sequence,t,h,copy.automatic,copy.threshold,copy.ledMode,copy.neoMode);
    else { ready=false; logLine("DHT20_ERROR status=%d",status); }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
