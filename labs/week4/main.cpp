// Week 4: physical-button tests and explicitly labelled synthetic stress tests.
#include <Arduino.h>
#include <Wire.h>
#include <DHT20.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

constexpr uint8_t LED_PIN = 48;
constexpr uint8_t BUTTON_PIN = 0;
struct ButtonMessage { uint32_t pressCount; uint32_t durationMs; };
struct TemperatureSample { uint32_t seq; float temp; float hum; };
enum Mode { BINARY, COUNTING, QUEUE, STRUCT };
Mode mode = STRUCT;
uint32_t sendTimeoutMs = 0;
SemaphoreHandle_t binarySem, countingSem, logMutex, controlMutex;
QueueHandle_t pressQueue, temperatureQueue;
DHT20 sensor;
bool sensorReady = false;
bool paused = false;
bool busy = false;
uint32_t sent = 0, dropped = 0, received = 0, blinkCount = 0, sequence = 0;

const char *modeName(Mode m) {
  return m == BINARY ? "binary" : m == COUNTING ? "counting" : m == QUEUE ? "queue" : "struct";
}
void logLine(const char *fmt, ...) {
  char buf[240]; va_list args; va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);
  xSemaphoreTake(logMutex, portMAX_DELAY);
  Serial.printf("[%8lu ms] %s\n", (unsigned long)millis(), buf);
  xSemaphoreGive(logMutex);
}
bool pending() {
  return uxSemaphoreGetCount(binarySem) || uxSemaphoreGetCount(countingSem) || uxQueueMessagesWaiting(pressQueue);
}
void sendPress(uint32_t duration, const char *source) {
  // Single producer task owns all calls, including serial-generated test input.
  ButtonMessage msg = {++sequence, duration};
  BaseType_t ok;
  if (mode == BINARY) ok = xSemaphoreGive(binarySem);
  else if (mode == COUNTING) ok = xSemaphoreGive(countingSem);
  else ok = xQueueSend(pressQueue, &msg, pdMS_TO_TICKS(sendTimeoutMs));
  xSemaphoreTake(controlMutex, portMAX_DELAY);
  if (ok == pdTRUE) ++sent; else ++dropped;
  xSemaphoreGive(controlMutex);
  logLine("TX source=%s mode=%s press=%lu duration=%lu result=%s", source, modeName(mode),
    (unsigned long)msg.pressCount, (unsigned long)duration, ok == pdTRUE ? "OK" : "FULL/DROPPED");
}
void printStats() {
  xSemaphoreTake(controlMutex, portMAX_DELAY);
  logLine("STATS mode=%s accepted=%lu dropped=%lu processed=%lu blinks=%lu busy=%u paused=%u pending_bin=%u pending_count=%u pending_queue=%u timeout_ms=%lu",
    modeName(mode), (unsigned long)sent, (unsigned long)dropped, (unsigned long)received,
    (unsigned long)blinkCount, busy, paused, (unsigned)uxSemaphoreGetCount(binarySem),
    (unsigned)uxSemaphoreGetCount(countingSem), (unsigned)uxQueueMessagesWaiting(pressQueue), (unsigned long)sendTimeoutMs);
  xSemaphoreGive(controlMutex);
}
void help() {
  logLine("COMMANDS: mode binary|counting|queue|struct; timeout 0|1000; pause; run; stats; help");
  logLine("SYNTHETIC ONLY: burst N (1..20), press MS. Physical BOOT button remains the normal input.");
  logLine("Mode change requires no work pending. pause freezes dequeue for controlled saturation tests.");
}
void command(String s) {
  s.trim();
  if (s == "stats") { printStats(); return; }
  if (s == "help") { help(); return; }
  if (s == "pause" || s == "run") {
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    if (s == "pause" && busy) logLine("REJECT pause: wait until current LED action finishes");
    else { paused = s == "pause"; logLine("WORKER paused=%u (controlled synthetic test setting)", paused); }
    xSemaphoreGive(controlMutex); return;
  }
  if (s.startsWith("mode ")) {
    String v = s.substring(5); Mode next;
    if (v == "binary") next=BINARY; else if(v=="counting") next=COUNTING;
    else if(v=="queue") next=QUEUE; else if(v=="struct") next=STRUCT;
    else { logLine("Invalid mode"); return; }
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    if (busy || pending()) logLine("REJECT mode: run/drain pending work first");
    else { mode=next; sequence=sent=dropped=received=blinkCount=0; logLine("MODE %s; counters reset", modeName(mode)); }
    xSemaphoreGive(controlMutex); return;
  }
  if (s.startsWith("timeout ")) {
    uint32_t n=s.substring(8).toInt();
    if(n==0 || n==1000) {sendTimeoutMs=n; logLine("QUEUE send timeout=%lu ms",(unsigned long)n);}
    return;
  }
  if (s.startsWith("burst ")) {
    int n=s.substring(6).toInt();
    if(n<1 || n>20) {logLine("burst range: 1..20");return;}
    logLine("BEGIN SYNTHETIC burst=%d (not physical button evidence)", n);
    for(int i=0;i<n;++i) sendPress(100,"SYNTHETIC");
    logLine("END SYNTHETIC burst"); return;
  }
  if (s.startsWith("press ")) {
    int n=s.substring(6).toInt();
    if(n<0 || n>10000) return;
    sendPress(n,"SYNTHETIC"); return;
  }
  if(s.length()) logLine("Unknown command; type help");
}
void producer(void *) {
  String input; input.reserve(100);
  bool down=false; uint32_t started=0;
  for(;;) {
    // Debounce both edges; send once on release to measure duration.
    if (!down && digitalRead(BUTTON_PIN)==LOW) {
      uint32_t start=millis(); vTaskDelay(pdMS_TO_TICKS(50));
      if(digitalRead(BUTTON_PIN)==LOW) {down=true;started=start;}
    } else if(down && digitalRead(BUTTON_PIN)==HIGH) {
      uint32_t ended=millis(); vTaskDelay(pdMS_TO_TICKS(20));
      if(digitalRead(BUTTON_PIN)==HIGH) {down=false;sendPress(ended-started,"BUTTON");}
    }
    while(Serial.available()) {
      char c=Serial.read();
      if(c=='\n') {command(input);input="";}
      else if(c!='\r' && input.length()<100) input+=c;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void consumer(void *) {
  for(;;) {
    ButtonMessage msg={0,0}; BaseType_t ok=pdFALSE; Mode current;
    xSemaphoreTake(controlMutex,portMAX_DELAY);
    current=mode;
    if(!paused) {
      if(current==BINARY) ok=xSemaphoreTake(binarySem,pdMS_TO_TICKS(20));
      else if(current==COUNTING) ok=xSemaphoreTake(countingSem,pdMS_TO_TICKS(20));
      else ok=xQueueReceive(pressQueue,&msg,pdMS_TO_TICKS(20));
    }
    if(ok==pdTRUE) {busy=true;++received;}
    xSemaphoreGive(controlMutex);
    if(ok!=pdTRUE) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
    uint32_t n=current==BINARY ? 3 : current==COUNTING ? 1 : msg.pressCount;
    bool hold=current==STRUCT && msg.durationMs>1000;
    logLine("RX mode=%s press=%lu duration=%lu action=%s count=%lu",modeName(current),
      (unsigned long)msg.pressCount,(unsigned long)msg.durationMs,hold?"HOLD_2000MS":"BLINK",(unsigned long)(hold?1:n));
    if(hold) {
      digitalWrite(LED_PIN,HIGH);vTaskDelay(pdMS_TO_TICKS(2000));digitalWrite(LED_PIN,LOW);
      logLine("LED hold complete (2000 ms)");
    } else {
      for(uint32_t i=0;i<n;++i) {
        digitalWrite(LED_PIN,HIGH);vTaskDelay(pdMS_TO_TICKS(200));
        digitalWrite(LED_PIN,LOW);vTaskDelay(pdMS_TO_TICKS(200));
        xSemaphoreTake(controlMutex,portMAX_DELAY);++blinkCount;xSemaphoreGive(controlMutex);
      }
    }
    xSemaphoreTake(controlMutex,portMAX_DELAY);busy=false;xSemaphoreGive(controlMutex);
    logLine("ACTION complete");
  }
}
void readTemperature(void *) {
  vTaskDelay(pdMS_TO_TICKS(1500)); uint32_t seq=0;
  for(;;) {
    if(!sensorReady) sensorReady=sensor.begin(11,12);
    int status=sensorReady?sensor.read():DHT20_ERROR_CONNECT;
    if(status==DHT20_OK) {
      TemperatureSample t={++seq,sensor.getTemperature(),sensor.getHumidity()};
      if(isfinite(t.temp)&&isfinite(t.hum)) {
        auto ok=xQueueSend(temperatureQueue,&t,pdMS_TO_TICKS(100));
        logLine("TEMP_TX seq=%lu temp=%.2f hum=%.2f result=%s",(unsigned long)t.seq,t.temp,t.hum,ok==pdPASS?"OK":"FULL");
      }
    } else logLine("DHT20_ERROR code=%d; check red I2C port SDA11/SCL12",status);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
void receiveTemperature(void *) {
  TemperatureSample t;
  for(;;) if(xQueueReceive(temperatureQueue,&t,portMAX_DELAY)==pdPASS)
    logLine("TEMP_RX seq=%lu temp=%.2f C hum=%.2f %%",(unsigned long)t.seq,t.temp,t.hum);
}
void setup() {
  Serial.begin(115200); pinMode(LED_PIN,OUTPUT);digitalWrite(LED_PIN,LOW);pinMode(BUTTON_PIN,INPUT_PULLUP);
  logMutex=xSemaphoreCreateMutex();controlMutex=xSemaphoreCreateMutex();
  binarySem=xSemaphoreCreateBinary();countingSem=xSemaphoreCreateCounting(10,0);
  pressQueue=xQueueCreate(5,sizeof(ButtonMessage));temperatureQueue=xQueueCreate(5,sizeof(TemperatureSample));
  if(!logMutex||!controlMutex||!binarySem||!countingSem||!pressQueue||!temperatureQueue) {while(true)delay(1000);}
  sensorReady=sensor.begin(11,12);
  delay(1500);
  logLine("WEEK4 firmware ready; LED48 BOOT0 DHT20 SDA11 SCL12 sensor_present=%u",sensorReady);
  help();
  xTaskCreate(producer,"ButtonSerial",4096,NULL,10,NULL);
  xTaskCreate(consumer,"LED",4096,NULL,5,NULL);
  xTaskCreate(readTemperature,"DHT20",4096,NULL,3,NULL);
  xTaskCreate(receiveTemperature,"TempReceiver",4096,NULL,3,NULL);
}
void loop(){vTaskDelay(pdMS_TO_TICKS(1000));}
