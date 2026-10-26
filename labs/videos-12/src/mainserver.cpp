#include "mainserver.h"
#include <WiFi.h>
#include <WebServer.h>
namespace {
WebServer server(80);
String requestedSsid, requestedPassword;
bool requested=false, connecting=false, connectedBefore=false;
uint32_t requestedAt=0, connectAt=0;
const char *AP_NAME="YoloUNO-Video12", *AP_PASSWORD="yolouno-lab";
const char HOME[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>YoloUNO — Video 12</title><style>body{font:17px system-ui;background:#eef3f8;color:#153047;max-width:760px;margin:30px auto;padding:0 20px}section{background:white;border-radius:12px;padding:22px;margin:18px 0}button,a{font:inherit}button{padding:12px 18px;cursor:pointer;border-radius:8px;border:1px solid #bdccdc;background:#edf5ff}b{font-size:24px}p{line-height:1.5}.muted{color:#52677c}</style><h1>YoloUNO — Video 12</h1><p class="muted">Máy chủ web trên bo ESP32-S3</p><section><h2>Cảm biến DHT20</h2><p>Nhiệt độ: <b id="temp">...</b> °C</p><p>Độ ẩm: <b id="hum">...</b> %</p><p id="sample" class="muted"></p></section><section><h2>Điều khiển đèn</h2><p><button onclick="toggle(1)">LED đơn: <span id="led1">...</span></button> <button onclick="toggle(2)">NeoPixel: <span id="led2">...</span></button></p><p id="pending" class="muted"></p></section><section><h2>Kết nối Wi-Fi</h2><p id="network">...</p><a href="/settings">Cài đặt Wi-Fi</a></section><script>
async function refresh(){try{const s=await(await fetch('/sensors')).json(),l=await(await fetch('/leds')).json(),n=await(await fetch('/status')).json();document.getElementById('temp').textContent=s.valid?s.temperature.toFixed(2):'Chưa có dữ liệu';document.getElementById('hum').textContent=s.valid?s.humidity.toFixed(2):'Chưa có dữ liệu';document.getElementById('sample').textContent='Mẫu số '+s.sequence;for(const i of [1,2])document.getElementById('led'+i).textContent=l['applied'+i]?'BẬT':'TẮT';document.getElementById('pending').textContent=(l.led1!==l.applied1||l.led2!==l.applied2)?'Đang áp dụng lệnh...':'Đã áp dụng trạng thái đèn';document.getElementById('network').textContent=n.state+' · AP '+n.ap_ip+(n.sta_ip?' · IP mạng nhà '+n.sta_ip:'');}catch(e){document.getElementById('network').textContent='Đang chờ kết nối với bo...'}}
async function toggle(i){await fetch('/toggle?led='+i,{method:'POST'});await refresh()}refresh();setInterval(refresh,2000);
</script></html>)HTML";
const char SETTINGS[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Cài đặt Wi-Fi — Video 12</title><style>body{font:18px system-ui;max-width:640px;margin:40px auto;padding:20px;color:#153047;background:#eef3f8}section{padding:24px;background:white;border-radius:12px}input,button{display:block;font:inherit;padding:12px;width:100%;box-sizing:border-box;margin:14px 0}button{cursor:pointer}pre{white-space:pre-wrap}</style><h1>Cài đặt Wi-Fi</h1><section><p>Kết nối mạng Wi-Fi 2,4 GHz. Thông tin chỉ giữ trong RAM, mất khi khởi động lại.</p><form><label>Tên mạng<input name="ssid" required autocomplete="off"></label><label>Mật khẩu<input name="password" type="password" autocomplete="off"></label><button>Kết nối</button></form><pre id="message"></pre><pre id="status"></pre><a href="/">Về trang chính</a></section><script>document.querySelector('form').onsubmit=async e=>{e.preventDefault();const r=await fetch('/connect',{method:'POST',body:new URLSearchParams(new FormData(e.target))});document.getElementById('message').textContent=await r.text();e.target.elements.password.value=''};setInterval(async()=>{try{const s=await(await fetch('/status')).json();if(s.state==='STA_CONNECTED')document.getElementById('message').textContent='Đã kết nối Wi-Fi thành công.';document.getElementById('status').textContent=s.state+'\nAP: '+s.ap_ip+'\nIP mạng nhà: '+(s.sta_ip||'chưa có')}catch(e){}},1500)</script></html>)HTML";
String statusJson() {
 bool up=WiFi.status()==WL_CONNECTED;
 return String("{\"state\":\"")+(connecting?"CONNECTING":up?"STA_CONNECTED":"AP_ONLY")+"\",\"ap_ip\":\""+WiFi.softAPIP().toString()+"\",\"sta_ip\":\""+(up?WiFi.localIP().toString():"")+"\",\"ap_clients\":"+String(WiFi.softAPgetStationNum())+"}";
}
void apOnly(const char *reason) {
 WiFi.disconnect(false,false);WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME,AP_PASSWORD);
 connecting=false;connectedBefore=false;server.client().stop();server.begin();
 logLine("AP_ONLY reason=%s ip=%s clients=%u",reason,WiFi.softAPIP().toString().c_str(),WiFi.softAPgetStationNum());
}
void requestConnect(const String &ssid,const String &password) {
 requestedSsid=ssid;requestedPassword=password;requested=true;requestedAt=millis();
 logLine("WIFI_CONFIG received, credentials hidden");
}
void routes() {
 server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",HOME);});
 server.on("/settings",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",SETTINGS);});
 server.on("/status",HTTP_GET,[]{server.send(200,"application/json",statusJson());});
 server.on("/sensors",HTTP_GET,[]{auto s=snapshot();String payload=String("{\"valid\":")+(s.valid?"true":"false")+",\"sequence\":"+String(s.sequence)+",\"temperature\":"+(s.valid?String(s.temperature,2):"null")+",\"humidity\":"+(s.valid?String(s.humidity,2):"null")+"}";server.send(200,"application/json",payload);});
 server.on("/leds",HTTP_GET,[]{auto s=snapshot();String payload=String("{\"led1\":")+(s.ledOn?"true":"false")+",\"led2\":"+(s.neoOn?"true":"false")+",\"applied1\":"+(s.ledApplied?"true":"false")+",\"applied2\":"+(s.neoApplied?"true":"false")+"}";server.send(200,"application/json",payload);});
 server.on("/toggle",HTTP_POST,[]{String id=server.arg("led");if(id!="1"&&id!="2"){server.send(400,"text/plain","LED must be 1 or 2");return;}xSemaphoreTake(stateMutex,portMAX_DELAY);sharedState.webControl=true;sharedState.automatic=false;if(id=="1")sharedState.ledOn=!sharedState.ledOn;else sharedState.neoOn=!sharedState.neoOn;bool on=id=="1"?sharedState.ledOn:sharedState.neoOn;xSemaphoreGive(stateMutex);logLine("WEB_TOGGLE led=%s requested=%u",id.c_str(),on);server.send(200,"application/json","{\"accepted\":true}");});
 server.on("/connect",HTTP_POST,[]{if(!server.hasArg("ssid")||server.arg("ssid").isEmpty()||server.arg("ssid").length()>32||server.arg("password").length()>63){server.send(400,"text/plain; charset=utf-8","Tên mạng hoặc mật khẩu không hợp lệ");return;}requestConnect(server.arg("ssid"),server.arg("password"));server.send(202,"text/plain; charset=utf-8","Đang kết nối. Có thể kiểm tra địa chỉ IP mới qua USB.");});
 server.on("/favicon.ico",HTTP_GET,[]{server.send(204);});
 server.onNotFound([]{server.send(404,"text/plain","Not found");});
}
}
void serverTask(void *) {
 pinMode(0,INPUT_PULLUP);WiFi.persistent(false);
 WiFi.onEvent([](arduino_event_id_t event){
  if(event==ARDUINO_EVENT_WIFI_AP_STACONNECTED)logLine("AP_CLIENT_CONNECTED");
  else if(event==ARDUINO_EVENT_WIFI_AP_STADISCONNECTED)logLine("AP_CLIENT_DISCONNECTED");
  else if(event==ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)logLine("AP_CLIENT_IP_ASSIGNED");
 });
 routes();apOnly("boot");
 String line;uint32_t buttonAt=0;bool down=false;
 for(;;) {
  server.handleClient();
  while(Serial.available()) {char c=Serial.read();if(c=='\n'){if(line.startsWith("wifi ")){int split=line.indexOf('|',5);if(split>5)requestConnect(line.substring(5,split),line.substring(split+1));}else if(line=="ap")apOnly("serial-test");else if(line=="status")logLine("NETWORK %s",statusJson().c_str());line="";}else if(c!='\r'){if(line.length()<120)line+=c;else line="";}}
  if(requested&&millis()-requestedAt>=300){requested=false;WiFi.disconnect(false,false);WiFi.mode(WIFI_AP_STA);WiFi.softAP(AP_NAME,AP_PASSWORD);WiFi.begin(requestedSsid.c_str(),requestedPassword.c_str());requestedPassword="";server.client().stop();server.begin();connecting=true;connectedBefore=false;connectAt=millis();logLine("CONNECTING timeout_ms=30000 AP_remains_active=1");}
  if(connecting&&WiFi.status()==WL_CONNECTED){connecting=false;connectedBefore=true;server.client().stop();server.begin();logLine("STA_CONNECTED ip=%s",WiFi.localIP().toString().c_str());}
  else if(connecting&&millis()-connectAt>30000)apOnly("connection-timeout");
  else if(connectedBefore&&WiFi.status()!=WL_CONNECTED)apOnly("link-lost");
  if(digitalRead(0)==LOW){if(!down){down=true;buttonAt=millis();}if(millis()-buttonAt>=80&&(connecting||connectedBefore))apOnly("BOOT-button");}else down=false;
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
