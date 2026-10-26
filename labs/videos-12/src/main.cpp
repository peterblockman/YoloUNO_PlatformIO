#include "global.h"
#include "led_blinky.h"
#include "mainserver.h"
#if LAB_STAGE >= 10
#include "neo_blinky.h"
#endif
#if LAB_STAGE >= 11
#include "temp_humi_monitor.h"
#endif
void setup() {
  Serial.begin(115200);delay(1500);
  stateMutex=xSemaphoreCreateMutex();logMutex=xSemaphoreCreateMutex();
  if(!stateMutex||!logMutex){while(true)delay(1000);}
  logLine("VIDEO%u_READY LED=48 NEO=45 SDA=11 SCL=12",LAB_STAGE);
  sharedState.webControl=true;
  bool ok=xTaskCreate(ledTask,"LED",3072,nullptr,1,nullptr)==pdPASS;
#if LAB_STAGE >= 10
  ok &= xTaskCreate(neoTask,"NeoPixel",4096,nullptr,1,nullptr)==pdPASS;
#endif
#if LAB_STAGE >= 11
  ok &= xTaskCreate(sensorTask,"DHT20",4096,nullptr,1,nullptr)==pdPASS;
#endif

  ok &= xTaskCreate(serverTask,"HTTP",8192,nullptr,2,nullptr)==pdPASS;
  if(!ok){logLine("INIT_ERROR task");while(true)delay(1000);}
}
void loop(){vTaskDelay(pdMS_TO_TICKS(1000));}
