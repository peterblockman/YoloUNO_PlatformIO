#include "global.h"
#include "led_blinky.h"
#if LAB_STAGE >= 10
#include "neo_blinky.h"
#endif
#if LAB_STAGE >= 11
#include "temp_humi_monitor.h"
#endif
void controlTask(void *) {
  String line;
  for (;;) {
    while (Serial.available()) {
      char c=Serial.read();
      if(c=='\n') {
        int led,neo;float threshold;
        if(sscanf(line.c_str(),"mode %d %d",&led,&neo)==2&&led>=0&&led<=2&&neo>=0&&neo<=4) {
          xSemaphoreTake(stateMutex,portMAX_DELAY);
          sharedState.automatic=false;sharedState.ledMode=led;sharedState.neoMode=neo;
          xSemaphoreGive(stateMutex);
          logLine("CONTROL manual led=%d neo=%d",led,neo);
        } else if(sscanf(line.c_str(),"auto %f",&threshold)==1&&isfinite(threshold)&&threshold>=-40&&threshold<=80) {
          xSemaphoreTake(stateMutex,portMAX_DELAY);
          sharedState.threshold=threshold;sharedState.automatic=true;
          xSemaphoreGive(stateMutex);logLine("CONTROL auto threshold=%.2f",threshold);
        } else logLine("CONTROL use: mode <0..2> <0..4> or auto <-40..80>");
        line="";
      } else if(c!='\r') {if(line.length()<80)line+=c;else line="";}
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void setup() {
  Serial.begin(115200);delay(1500);
  stateMutex=xSemaphoreCreateMutex();logMutex=xSemaphoreCreateMutex();
  if(!stateMutex||!logMutex){while(true)delay(1000);}
  logLine("VIDEO%u_READY LED=48 NEO=45 SDA=11 SCL=12",LAB_STAGE);
  bool ok=xTaskCreate(ledTask,"LED",3072,nullptr,1,nullptr)==pdPASS;
#if LAB_STAGE >= 10
  ok &= xTaskCreate(neoTask,"NeoPixel",4096,nullptr,1,nullptr)==pdPASS;
#endif
#if LAB_STAGE >= 11
  ok &= xTaskCreate(sensorTask,"DHT20",4096,nullptr,1,nullptr)==pdPASS;
#endif
  ok &= xTaskCreate(controlTask,"Control",3072,nullptr,1,nullptr)==pdPASS;
  if(!ok){logLine("INIT_ERROR task");while(true)delay(1000);}
}
void loop(){vTaskDelay(pdMS_TO_TICKS(1000));}
