#include <Arduino.h>
#include <WiFi.h>
#include <DHT20.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <stdarg.h>
#include <atomic>
#include "freertos/semphr.h"
#include "freertos/queue.h"

constexpr uint8_t LED_PIN = 48;
constexpr uint32_t SENSOR_MS = 5000, TELEMETRY_MS = 10000;
struct Sample { float temperature, humidity; uint32_t sequence, at; bool valid; };
Sample latest{};
SemaphoreHandle_t dataMutex, configMutex, logMutex;
QueueHandle_t outgoing;
String wifiSsid, wifiPassword, accessToken;
uint32_t wifiRevision = 0, tokenRevision = 0;
std::atomic<bool> reconnectWifi{false}, reconnectMqtt{false};
WiFiClient transport;
PubSubClient mqtt(transport);
bool ledState = false;  // Chi tac vu MQTT truy cap trang thai LED.
bool attributesDirty = true;

void logLine(const char *format, ...) {
  char buffer[320]; va_list args; va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
  xSemaphoreTake(logMutex, portMAX_DELAY);
  Serial.printf("[%8lu ms] %s\n", (unsigned long)millis(), buffer);
  xSemaphoreGive(logMutex);
}

void rpcCallback(char *topic, byte *payload, unsigned length) {
  StaticJsonDocument<384> request;
  String path(topic), prefix = "v1/devices/me/rpc/request/";
  if (!path.startsWith(prefix) || deserializeJson(request, payload, length)) return;
  String method = request["method"] | "";
  String responseTopic = "v1/devices/me/rpc/response/" + path.substring(prefix.length());
  if (method == "getLedSwitchValue") {
    bool sent = mqtt.publish(responseTopic.c_str(), ledState ? "true" : "false");
    logLine("RPC_GET id=%s ledState=%u local_write_ok=%u", path.substring(prefix.length()).c_str(), ledState, sent);
  } else if (method == "setLedSwitchValue" && request["params"].is<bool>()) {
    ledState = request["params"].as<bool>();
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    attributesDirty = true;
    String response = String("{\"success\":true,\"ledState\":") + (ledState ? "true}" : "false}");
    bool sent = mqtt.publish(responseTopic.c_str(), response.c_str());
    logLine("RPC_SET id=%s ledState=%u gpio=%u local_write_ok=%u", path.substring(prefix.length()).c_str(), ledState, LED_PIN, sent);
  } else {
    mqtt.publish(responseTopic.c_str(), "{\"success\":false,\"error\":\"Unsupported method or non-boolean params\"}");
    logLine("RPC_REJECT method=%s", method.c_str());
  }
}

void sensorTask(void *) {
  DHT20 sensor; bool ready = false; uint32_t sequence = 0;
  vTaskDelay(pdMS_TO_TICKS(1500));
  for (;;) {
    if (!ready) ready = sensor.begin(11, 12);
    int status = ready ? sensor.read() : DHT20_ERROR_CONNECT;
    Sample sample{}; sample.at = millis();
    sample.valid = status == DHT20_OK;
    if (sample.valid) {
      sample.temperature = sensor.getTemperature(); sample.humidity = sensor.getHumidity();
      sample.valid = isfinite(sample.temperature) && isfinite(sample.humidity);
    }
    if (sample.valid) {
      sample.sequence = ++sequence;
      logLine("SENSOR sample=%lu temp=%.2f hum=%.2f status=OK", (unsigned long)sequence, sample.temperature, sample.humidity);
    } else { ready = false; logLine("SENSOR_ERROR status=%d", status); }
    xSemaphoreTake(dataMutex, portMAX_DELAY); latest = sample; xSemaphoreGive(dataMutex);
    vTaskDelay(pdMS_TO_TICKS(SENSOR_MS));
  }
}

void wifiTask(void *) {
  uint32_t revision = 0, lastAttempt = 0; bool connected = false, configured = false;
  String ssid, password;
  for (;;) {
    xSemaphoreTake(configMutex, portMAX_DELAY);
    if (wifiRevision != revision) {
      revision = wifiRevision; ssid = wifiSsid; password = wifiPassword;
      configured = !ssid.isEmpty(); lastAttempt = millis() - 10000;
      WiFi.disconnect(false, false);
    }
    xSemaphoreGive(configMutex);
    if (reconnectWifi.exchange(false)) {
      WiFi.disconnect(false, false); lastAttempt = millis() - 10000;
      logLine("WIFI_TEST disconnect requested");
    }
    bool nowConnected = WiFi.status() == WL_CONNECTED;
    if (nowConnected != connected) {
      connected = nowConnected;
      logLine("WIFI %s ip=%s", connected ? "CONNECTED" : "DISCONNECTED", connected ? WiFi.localIP().toString().c_str() : "none");
    }
    if (configured && !nowConnected && millis() - lastAttempt >= 10000) {
      lastAttempt = millis(); WiFi.begin(ssid.c_str(), password.c_str());
      logLine("WIFI_CONNECT_ATTEMPT retry_ms=10000");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void telemetryTask(void *) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(TELEMETRY_MS));
    Sample sample;
    xSemaphoreTake(dataMutex, portMAX_DELAY); sample = latest; xSemaphoreGive(dataMutex);
    if (sample.valid && millis() - sample.at <= 15000) {
      xQueueOverwrite(outgoing, &sample);
      logLine("TELEMETRY_READY sample=%lu period_ms=%lu", (unsigned long)sample.sequence, (unsigned long)TELEMETRY_MS);
    } else logLine("TELEMETRY_SKIP invalid_or_stale_sensor");
  }
}

bool publishAttributes() {
  StaticJsonDocument<512> attributes;
  attributes["ledState"] = ledState;
  attributes["macAddress"] = WiFi.macAddress(); attributes["localIp"] = WiFi.localIP().toString();
  attributes["ssid"] = WiFi.SSID(); attributes["rssi"] = WiFi.RSSI(); attributes["channel"] = WiFi.channel();
  attributes["firmware"] = "video8-rtos";
  String payload; serializeJson(attributes, payload);
  bool sent = mqtt.publish("v1/devices/me/attributes", payload.c_str());
  logLine("ATTRIBUTES ledState=%u local_write_ok=%u", ledState, sent);
  return sent;
}

void mqttTask(void *) {
  mqtt.setBufferSize(768); mqtt.setSocketTimeout(2); mqtt.setKeepAlive(15);
  mqtt.setServer("app.coreiot.io", 1883); mqtt.setCallback(rpcCallback);
  uint32_t revision = 0, lastAttempt = millis() - 5000, lastAttributes = 0;
  uint32_t lastSentSequence = 0; String token;
  for (;;) {
    xSemaphoreTake(configMutex, portMAX_DELAY);
    if (tokenRevision != revision) {
      revision = tokenRevision; token = accessToken; mqtt.disconnect(); lastAttempt = millis() - 5000;
    }
    xSemaphoreGive(configMutex);
    if (reconnectMqtt.exchange(false)) {
      mqtt.disconnect(); lastAttempt = millis() - 5000; logLine("MQTT_TEST disconnect requested");
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (mqtt.connected()) { mqtt.disconnect(); logLine("MQTT_PAUSED wifi_down"); }
    } else if (!token.isEmpty()) {
      if (!mqtt.connected() && millis() - lastAttempt >= 5000) {
        lastAttempt = millis(); String clientId = "YoloUNO-video8-" + WiFi.macAddress();
        if (mqtt.connect(clientId.c_str(), token.c_str(), "")) {
          bool subscribed = mqtt.subscribe("v1/devices/me/rpc/request/+");
          logLine("MQTT_CONNECTED RPC_subscribed=%u", subscribed);
          if (!subscribed) mqtt.disconnect();
          attributesDirty = true;
        } else logLine("MQTT_RETRY state=%d", mqtt.state());
      }
      if (mqtt.connected()) {
        mqtt.loop();
        if (attributesDirty || millis() - lastAttributes >= 10000) {
          if (publishAttributes()) { attributesDirty = false; lastAttributes = millis(); }
        }
        Sample sample;
        if (xQueueReceive(outgoing, &sample, 0) == pdTRUE) {
          if (millis() - sample.at <= 15000 && sample.sequence != lastSentSequence) {
            StaticJsonDocument<256> payload;
            payload["temperature"] = sample.temperature; payload["humidity"] = sample.humidity;
            payload["sampleSequence"] = sample.sequence; payload["source"] = "DHT20";
            String json; serializeJson(payload, json);
            bool sent = mqtt.publish("v1/devices/me/telemetry", json.c_str());
            if (sent) lastSentSequence = sample.sequence;
            logLine("MQTT_TX sample=%lu local_write_ok=%u payload=%s", (unsigned long)sample.sequence, sent, json.c_str());
          } else logLine("MQTT_SKIP stale_or_duplicate_sample");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void serialTask(void *) {
  String line;
  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        StaticJsonDocument<512> command;
        if (!deserializeJson(command, line)) {
          String action = command["cmd"] | "";
          if (action == "wifi") {
            xSemaphoreTake(configMutex, portMAX_DELAY);
            wifiSsid = command["ssid"] | ""; wifiPassword = command["pass"] | ""; ++wifiRevision;
            xSemaphoreGive(configMutex); logLine("CONFIG_WIFI received, credentials hidden");
          } else if (action == "mqtt") {
            xSemaphoreTake(configMutex, portMAX_DELAY); accessToken = command["token"] | ""; ++tokenRevision;
            xSemaphoreGive(configMutex); logLine("CONFIG_MQTT received, token hidden");
          } else if (action == "reconnect_wifi") reconnectWifi = true;
          else if (action == "reconnect_mqtt") reconnectMqtt = true;
          else if (action == "status") logLine("STATUS wifi=%u", WiFi.status() == WL_CONNECTED);
          else logLine("CONFIG unknown command");
        } else logLine("CONFIG invalid JSON, content hidden");
        line = "";
      } else if (c != '\r') { if (line.length() < 512) line += c; else line = ""; }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200); pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  dataMutex = xSemaphoreCreateMutex(); configMutex = xSemaphoreCreateMutex(); logMutex = xSemaphoreCreateMutex();
  outgoing = xQueueCreate(1, sizeof(Sample));
  if (!dataMutex || !configMutex || !logMutex || !outgoing) { while (true) delay(1000); }
  WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(false);
  delay(1500); logLine("VIDEO8_READY sensor_ms=5000 telemetry_ms=10000 credentials=RAM-only");
  bool ok = xTaskCreate(sensorTask, "DHT20", 4096, nullptr, 1, nullptr) == pdPASS;
  ok &= xTaskCreate(wifiTask, "WiFi", 4096, nullptr, 2, nullptr) == pdPASS;
  ok &= xTaskCreate(telemetryTask, "Telemetry", 3072, nullptr, 1, nullptr) == pdPASS;
  ok &= xTaskCreate(mqttTask, "MQTT", 6144, nullptr, 1, nullptr) == pdPASS;
  ok &= xTaskCreate(serialTask, "SerialConfig", 4096, nullptr, 1, nullptr) == pdPASS;
  if (!ok) { logLine("INIT_ERROR task creation"); while (true) delay(1000); }
}
void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
