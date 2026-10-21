// Week 5: AP+STA provisioning, HTTP polling, physical LEDs and MQTT after semaphore.
// Wi-Fi and device credentials are supplied at runtime and are never printed/stored.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT20.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <stdarg.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <esp_wifi.h>
#include "freertos/semphr.h"

constexpr uint8_t LED1=48,NEO_PIN=45,BOOT=0;
Adafruit_NeoPixel pixel(1,NEO_PIN,NEO_GRB+NEO_KHZ800);
constexpr uint32_t CONNECT_TIMEOUT=30000;
const char *AP_NAME="YoloUNO-Week5";
const char *AP_PASS="yolouno-lab";
WebServer server(80);
WiFiClient transport;
PubSubClient mqtt(transport);
DHT20 dht;
SemaphoreHandle_t logLock,dataLock,networkReady,configLock;
float temperature=NAN,humidity=NAN;
bool validSensor=false,led1=false,led2=false;
std::atomic<bool> connecting{false};
bool wasConnected=false;
uint32_t connectStart=0,sampleSeq=0;
std::atomic<uint32_t> networkGeneration{0};
String requestedSsid,requestedPass,broker="app.coreiot.io",token;
bool wifiRequested=false,routesReady=false;
uint32_t wifiRequestedAt=0;
uint16_t brokerPort=1883;
bool scanRequested=false,scanRunning=false;
std::atomic<bool> scanDoneEvent{false};
uint32_t scanStarted=0;
String scanResult="{\"state\":\"idle\",\"networks\":[]}";

void logLine(const char *fmt,...) {
 char buf[240];va_list a;va_start(a,fmt);vsnprintf(buf,sizeof(buf),fmt,a);va_end(a);
 xSemaphoreTake(logLock,portMAX_DELAY);Serial.printf("[%8lu ms] %s\n",(unsigned long)millis(),buf);xSemaphoreGive(logLock);
}
void sensorSnapshot(float &t,float &h,bool &ok,uint32_t &seq) {
 xSemaphoreTake(dataLock,portMAX_DELAY);t=temperature;h=humidity;ok=validSensor;seq=sampleSeq;xSemaphoreGive(dataLock);
}
void setLed(unsigned n,bool on,const char *source) {
 xSemaphoreTake(dataLock,portMAX_DELAY);
 if(n==1){led1=on;digitalWrite(LED1,on?HIGH:LOW);}else{led2=on;pixel.setPixelColor(0,on?pixel.Color(0,32,0):0);pixel.show();}
 xSemaphoreGive(dataLock);logLine("LED%u %s source=%s",n,on?"ON":"OFF",source);
}
String ledJson() {
 xSemaphoreTake(dataLock,portMAX_DELAY);bool a=led1,b=led2;xSemaphoreGive(dataLock);
 return String("{\"led1\":\"")+(a?"ON":"OFF")+"\",\"led2\":\""+(b?"ON":"OFF")+"\"}";
}
String netJson() {
 bool up=WiFi.status()==WL_CONNECTED;
 return String("{\"state\":\"")+(connecting?"CONNECTING":up?"STA_CONNECTED":"AP_ONLY")+"\",\"ap_ip\":\""+WiFi.softAPIP().toString()+"\",\"sta_ip\":\""+(up?WiFi.localIP().toString():"")+"\",\"wifi_status\":"+String((int)WiFi.status())+"}";
}
const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>YoloUNO - Tuần 5</title><style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px;background:#f1f5f9;color:#172b4d}section{background:white;border-radius:12px;padding:24px;margin:20px 0}button,input{font:inherit;padding:12px;margin:6px 0}button{cursor:pointer}pre{white-space:pre-wrap;font-size:14px}</style><h1>YoloUNO - Tuần 5</h1><section><h2>Cảm biến DHT20</h2><p>Nhiệt độ: <b id="temp">...</b> °C</p><p>Độ ẩm: <b id="hum">...</b> %</p><p id="sample"></p></section><section><h2>Điều khiển LED</h2><button onclick="toggle(1)">LED1 GPIO48: <b id="led1">...</b></button><button onclick="toggle(2)">LED2 NeoPixel GPIO45: <b id="led2">...</b></button><p>Hai đèn đều có sẵn trên bo: LED đơn GPIO48 và NeoPixel GPIO45.</p></section><section><h2>Kết nối mạng</h2><pre id="network"></pre><a href="/wifi">Danh sách Wi-Fi</a><p><a href="/settings">Cài đặt Wi-Fi và CoreIOT</a></p></section><script>
async function refresh(){try{let s=await(await fetch('/sensors')).json();document.getElementById('temp').textContent=s.valid?s.temp:'Chưa có dữ liệu';document.getElementById('hum').textContent=s.valid?s.hum:'Chưa có dữ liệu';document.getElementById('sample').textContent='Mẫu số '+s.sequence;let n=await(await fetch('/status')).json();document.getElementById('network').textContent=JSON.stringify(n,null,2);let l=await(await fetch('/leds')).json();for(let k of ['led1','led2'])document.getElementById(k).textContent=l[k];}catch(e){document.getElementById('network').textContent='Đang chờ kết nối...'}}
async function toggle(i){await fetch('/toggle?led='+i,{method:'POST'});await refresh()}refresh();setInterval(refresh,3000);
</script></html>)HTML";
const char SETTINGS[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Cài đặt YoloUNO</title><style>body{font:18px system-ui;max-width:650px;margin:40px auto;padding:20px}input,button{display:block;font:inherit;padding:12px;margin:12px 0;box-sizing:border-box;width:100%}pre{white-space:pre-wrap}</style><h1>Cài đặt YoloUNO</h1><h2>Wi-Fi 2,4 GHz</h2><p><a href="/wifi">Quét và chọn mạng Wi-Fi</a></p><form id="wifi"><input name="ssid" placeholder="Tên Wi-Fi" required><input name="pass" type="password" placeholder="Mật khẩu"><button>Kết nối</button></form><pre id="status"></pre><h2>CoreIOT</h2><p>Chỉ nhập mã truy cập của thiết bị thuộc tài khoản của bạn. Thông tin chỉ giữ trong RAM, mất khi khởi động lại.</p><form id="mqtt"><input name="host" value="app.coreiot.io" required><input name="port" type="number" value="1883" required><input name="token" type="password" placeholder="Mã truy cập thiết bị" required><button>Lưu cấu hình CoreIOT</button></form><pre id="msg"></pre><a href="/">Về trang chính</a><script>for(const id of ['wifi','mqtt'])document.getElementById(id).onsubmit=async e=>{e.preventDefault();let r=await fetch(id==='wifi'?'/connect':'/mqtt',{method:'POST',body:new URLSearchParams(new FormData(e.target))});document.getElementById('msg').textContent=await r.text();e.target.querySelectorAll('input[type=password]').forEach(x=>x.value='')};setInterval(async()=>{try{document.getElementById('status').textContent=JSON.stringify(await(await fetch('/status')).json(),null,2)}catch(e){}},1500)</script></html>)HTML";
const char WIFI_PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Danh sách Wi-Fi - YoloUNO</title><style>body{font:17px system-ui;max-width:760px;margin:32px auto;padding:0 20px;background:#f1f5f9;color:#172b4d}section{background:white;border-radius:12px;padding:22px;margin:18px 0}button,input{font:inherit;padding:11px;margin:5px 0;box-sizing:border-box}button{cursor:pointer}button:disabled{cursor:wait;opacity:.6}.network{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid #dbe2ea}.network strong{overflow-wrap:anywhere}.meta{color:#526174;font-size:14px;margin-top:4px}input{width:100%}#message{white-space:pre-wrap}a{color:#174f86}</style><h1>Danh sách Wi-Fi</h1><p>Chọn mạng 2,4 GHz để kết nối YoloUNO.</p><section><button id="scan">Quét lại</button><p id="scanState" role="status">Chưa quét.</p><div id="list"></div><p class="meta">Mạng ẩn không hiện tên. Có thể nhập tên mạng tại <a href="/settings">trang cài đặt</a>.</p></section><section id="selection" hidden><h2>Kết nối mạng đã chọn</h2><form id="connect"><label for="ssid">Tên Wi-Fi</label><input id="ssid" name="ssid" readonly required><label id="passLabel" for="pass">Mật khẩu</label><input id="pass" name="pass" type="password" autocomplete="off"><button id="connectButton">Kết nối</button></form></section><section><h2>Trạng thái kết nối</h2><p id="message" role="status">Đang đọc trạng thái...</p></section><p><a href="/">Về trang chính</a> · <a href="/settings">Cài đặt CoreIOT</a></p><script>
const scanButton=document.getElementById('scan'),list=document.getElementById('list'),scanState=document.getElementById('scanState'),connectButton=document.getElementById('connectButton');let polling=null,connectingNow=false,scanningNow=false;
function showNetworks(data){list.replaceChildren();for(const n of data.networks){let row=document.createElement('div');row.className='network';let info=document.createElement('div'),name=document.createElement('strong'),meta=document.createElement('div'),choose=document.createElement('button');name.textContent=n.ssid;meta.className='meta';meta.textContent=n.rssi+' dBm · Kênh '+n.channel+' · '+n.security;choose.textContent='Chọn';choose.disabled=!n.connectable;choose.addEventListener('click',()=>{document.getElementById('ssid').value=n.ssid;document.getElementById('pass').value='';document.getElementById('pass').hidden=!n.secured;document.getElementById('passLabel').hidden=!n.secured;document.getElementById('pass').required=n.secured;document.getElementById('selection').hidden=false;document.getElementById(n.secured?'pass':'ssid').focus()});info.append(name,meta);row.append(info,choose);list.append(row)}scanState.textContent=data.networks.length?'Tìm thấy '+data.networks.length+' mạng. Chọn một mạng bên dưới.':'Không tìm thấy mạng có tên. Có thể quét lại hoặc nhập tên tại trang cài đặt.'}
function stopScan(){clearInterval(polling);polling=null;scanningNow=false;scanButton.disabled=connectingNow}
async function readScan(){try{let r=await fetch('/wifi/scan'),d=await r.json();if(d.state==='done'){stopScan();showNetworks(d)}else if(d.state==='failed'){stopScan();scanState.textContent='Quét thất bại. Hãy thử lại.'}}catch(e){stopScan();scanState.textContent='Không đọc được danh sách. Hãy thử lại.'}}
async function startScan(){scanButton.disabled=true;scanningNow=true;scanState.textContent='Đang quét các mạng Wi-Fi...';try{let r=await fetch('/wifi/scan',{method:'POST'});if(!r.ok)throw new Error(await r.text());polling=setInterval(readScan,800);await readScan()}catch(e){stopScan();scanState.textContent=e.message}}
async function status(){try{let n=await(await fetch('/status')).json();connectingNow=n.state==='CONNECTING';connectButton.disabled=connectingNow;scanButton.disabled=connectingNow||scanningNow;document.getElementById('message').textContent=n.state==='STA_CONNECTED'?'Đã kết nối Wi-Fi. IP STA: '+n.sta_ip+'\nAP cấu hình: '+n.ap_ip:n.state==='CONNECTING'?'Đang kết nối. AP cấu hình vẫn hoạt động.':'Bo đang ở chế độ AP. Chọn mạng để kết nối. Nếu vừa kết nối thất bại, hãy kiểm tra lại mật khẩu.'}catch(e){document.getElementById('message').textContent='Đang chờ kết nối tới bo. Nếu địa chỉ IP thay đổi, truy cập AP tại 192.168.4.1.'}}
document.getElementById('connect').onsubmit=async e=>{e.preventDefault();connectButton.disabled=true;document.getElementById('message').textContent='Đang gửi yêu cầu kết nối...';try{let r=await fetch('/connect',{method:'POST',body:new URLSearchParams(new FormData(e.target))});document.getElementById('pass').value='';if(!r.ok)throw new Error(await r.text());connectingNow=true;await status()}catch(e){connectButton.disabled=false;document.getElementById('message').textContent='Không gửi được yêu cầu: '+e.message}};
scanButton.onclick=startScan;status();setInterval(status,1500);startScan();
</script></html>)HTML";

const char *authName(wifi_auth_mode_t auth) {
 switch(auth){case WIFI_AUTH_OPEN:return "Mở";case WIFI_AUTH_WEP:return "WEP";case WIFI_AUTH_WPA_PSK:return "WPA";case WIFI_AUTH_WPA2_PSK:return "WPA2";case WIFI_AUTH_WPA_WPA2_PSK:return "WPA/WPA2";case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2 doanh nghiệp";case WIFI_AUTH_WPA3_PSK:return "WPA3";case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/WPA3";default:return "Bảo mật khác";}
}
void finishScan(int count) {
 DynamicJsonDocument doc(12288);doc["state"]="done";JsonArray nets=doc.createNestedArray("networks");
 std::vector<int> order;for(int i=0;i<count;++i)order.push_back(i);
 std::sort(order.begin(),order.end(),[](int a,int b){return WiFi.RSSI(a)>WiFi.RSSI(b);});
 std::vector<String> seen;
 for(int i:order){String name=WiFi.SSID(i);if(name.isEmpty()||name==AP_NAME)continue;
  if(std::find(seen.begin(),seen.end(),name)!=seen.end())continue;
  seen.push_back(name);if(nets.size()>=30)break;
  wifi_auth_mode_t auth=WiFi.encryptionType(i);JsonObject n=nets.createNestedObject();
  n["ssid"]=name;n["rssi"]=WiFi.RSSI(i);n["channel"]=WiFi.channel(i);n["security"]=authName(auth);n["secured"]=auth!=WIFI_AUTH_OPEN;n["connectable"]=auth!=WIFI_AUTH_WPA2_ENTERPRISE;
 }
 scanResult="";serializeJson(doc,scanResult);WiFi.scanDelete();scanRunning=false;
 logLine("WIFI_SCAN complete raw_count=%d visible_unique=%u",count,(unsigned)nets.size());
}
void processScan() {
 if(scanRequested){scanRequested=false;scanDoneEvent=false;WiFi.mode(WIFI_AP_STA);scanStarted=millis();scanRunning=true;
  int code=WiFi.scanNetworks(true,true,false,300);logLine("WIFI_SCAN started async code=%d",code);
  if(code>=0)finishScan(code);else if(code!=WIFI_SCAN_RUNNING){scanRunning=false;scanResult="{\"state\":\"failed\",\"networks\":[]}";}
 }
 if(scanRunning){
  // Read completion only after the driver event: repeated scanComplete() calls
  // otherwise trigger the older Arduino core's short internal scan timeout.
  if(scanDoneEvent.exchange(false)){int n=WiFi.scanComplete();if(n>=0)finishScan(n);}
  if(scanRunning&&millis()-scanStarted>20000){esp_wifi_scan_stop();WiFi.scanDelete();scanRunning=false;scanResult="{\"state\":\"failed\",\"networks\":[]}";logLine("WIFI_SCAN failed: 20s deadline");}
 }
}

void requestWifi(const String &name,const String &password) {
 xSemaphoreTake(configLock,portMAX_DELAY);requestedSsid=name;requestedPass=password;wifiRequestedAt=millis();wifiRequested=true;xSemaphoreGive(configLock);
}
void requestMqtt(const String &host,uint16_t port,const String &access) {
 xSemaphoreTake(configLock,portMAX_DELAY);broker=host;brokerPort=port;token=access;xSemaphoreGive(configLock);
 logLine("MQTT config received; access token hidden");
}
void startAP(const char *reason) {
 if(scanRunning){esp_wifi_scan_stop();WiFi.scanDelete();scanRunning=false;scanRequested=false;scanResult="{\"state\":\"idle\",\"networks\":[]}";}
 WiFi.disconnect(false,false);WiFi.mode(WIFI_AP);
 bool ok=WiFi.softAP(AP_NAME,AP_PASS);
 connecting=false;wasConnected=false;++networkGeneration;
 if(routesReady){server.client().stop();server.begin();}
 while(xSemaphoreTake(networkReady,0)==pdTRUE){}
 logLine("AP_ONLY reason=%s ap_ok=%u ip=%s",reason,ok,WiFi.softAPIP().toString().c_str());
}
void callback(char *topic,byte *payload,unsigned length) {
 StaticJsonDocument<384> d;
 if(deserializeJson(d,payload,length)){logLine("RPC invalid JSON");return;}
 String path(topic),prefix="v1/devices/me/rpc/request/";
 if(!path.startsWith(prefix))return;
 String method=d["method"]|"";
 if(method=="getLedSwitchValue") {
  xSemaphoreTake(dataLock,portMAX_DELAY);bool current=led1;xSemaphoreGive(dataLock);
  String responseTopic=String("v1/devices/me/rpc/response/")+path.substring(prefix.length());
  mqtt.publish(responseTopic.c_str(),current?"true":"false");
  logLine("RPC getLedSwitchValue response=%s",current?"true":"false");return;
 }
 bool supported=method=="setLedSwitchValue"||method=="setLed1";
 if(supported && d["params"].is<bool>()) {
  bool on=d["params"].as<bool>();setLed(1,on,"CoreIOT_RPC");
  String response=String("{\"success\":true,\"ledState\":")+(on?"true":"false")+"}";
  mqtt.publish((String("v1/devices/me/rpc/response/")+path.substring(prefix.length())).c_str(),response.c_str());
 } else mqtt.publish((String("v1/devices/me/rpc/response/")+path.substring(prefix.length())).c_str(),"{\"success\":false,\"error\":\"Use setLedSwitchValue with boolean params\"}");
}
void setupRoutes() {
 server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PAGE);});
 server.on("/wifi",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",WIFI_PAGE);});
 server.on("/wifi/scan",HTTP_POST,[]{
  if(connecting){server.send(409,"text/plain; charset=utf-8","Đang kết nối Wi-Fi. Hãy chờ kết quả trước khi quét.");return;}
  if(!scanRunning&&!scanRequested){WiFi.scanDelete();scanRequested=true;scanResult="{\"state\":\"scanning\",\"networks\":[]}";}
  server.send(202,"application/json","{\"state\":\"scanning\"}");
 });
 server.on("/wifi/scan",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",scanResult);});
 server.on("/settings",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",SETTINGS);});
 server.on("/sensors",HTTP_GET,[]{
  float t,h;bool ok;uint32_t seq;sensorSnapshot(t,h,ok,seq);
  String j=String("{\"valid\":")+(ok?"true":"false")+",\"temp\":"+(ok?String(t,2):"null")+",\"hum\":"+(ok?String(h,2):"null")+",\"sequence\":"+String(seq)+"}";
  server.send(200,"application/json",j);
 });
 server.on("/leds",HTTP_GET,[]{server.send(200,"application/json",ledJson());});
 server.on("/status",HTTP_GET,[]{server.send(200,"application/json",netJson());});
 server.on("/toggle",HTTP_POST,[]{int n=server.arg("led").toInt();if(n!=1&&n!=2){server.send(400,"text/plain","LED must be 1 or 2");return;}xSemaphoreTake(dataLock,portMAX_DELAY);bool old=n==1?led1:led2;xSemaphoreGive(dataLock);setLed(n,!old,"HTTP");server.send(200,"application/json",ledJson());});
 server.on("/connect",HTTP_POST,[]{if(scanRunning||scanRequested){server.send(409,"text/plain; charset=utf-8","Đang quét Wi-Fi. Hãy chờ danh sách hoàn tất.");return;}if(!server.hasArg("ssid")||server.arg("ssid").isEmpty()){server.send(400,"text/plain","SSID required");return;}requestWifi(server.arg("ssid"),server.arg("pass"));server.send(202,"text/plain; charset=utf-8","Đang kết nối. AP vẫn hoạt động, trạng thái sẽ cập nhật bên dưới.");});
 server.on("/mqtt",HTTP_POST,[]{if(!server.hasArg("host")||server.arg("host").isEmpty()||!server.hasArg("token")||server.arg("token").isEmpty()||server.arg("port").toInt()<1||server.arg("port").toInt()>65535){server.send(400,"text/plain","Host, port 1..65535 and token required");return;}requestMqtt(server.arg("host"),server.arg("port").toInt(),server.arg("token"));server.send(200,"text/plain; charset=utf-8","Đã lưu trong RAM. Mã truy cập không hiển thị trong nhật ký.");});
 server.onNotFound([]{server.send(404,"text/plain","Not found");});server.begin();routesReady=true;
}
void sensorTask(void *) {
 bool ready=dht.begin(11,12);vTaskDelay(pdMS_TO_TICKS(1500));
 for(;;) {
  if(!ready)ready=dht.begin(11,12);
  int status=ready?dht.read():DHT20_ERROR_CONNECT;
  if(status==DHT20_OK) {
   float t=dht.getTemperature(),h=dht.getHumidity();
   if(isfinite(t)&&isfinite(h)) {
    xSemaphoreTake(dataLock,portMAX_DELAY);temperature=t;humidity=h;validSensor=true;++sampleSeq;xSemaphoreGive(dataLock);
    logLine("SENSOR temp=%.2f C hum=%.2f %%",t,h);
   }
  } else {xSemaphoreTake(dataLock,portMAX_DELAY);validSensor=false;xSemaphoreGive(dataLock);logLine("DHT20 error=%d",status);}
  vTaskDelay(pdMS_TO_TICKS(5000));
 }
}
void serverTask(void *) {
 startAP("boot");setupRoutes();uint32_t buttonStart=0;bool down=false;int lastStatus=-99;
 for(;;) {
  server.handleClient();
  processScan();
  String name,pass;bool requested=false;
  xSemaphoreTake(configLock,portMAX_DELAY);
  if(wifiRequested && millis()-wifiRequestedAt>=300){name=requestedSsid;pass=requestedPass;requestedPass="";wifiRequested=false;requested=true;}
  xSemaphoreGive(configLock);
  if(requested) {
   WiFi.disconnect(false,false);WiFi.mode(WIFI_AP_STA);WiFi.softAP(AP_NAME,AP_PASS);
   WiFi.begin(name.c_str(),pass.c_str());pass="";server.client().stop();server.begin();connectStart=millis();connecting=true;wasConnected=false;lastStatus=-99;
   ++networkGeneration;while(xSemaphoreTake(networkReady,0)==pdTRUE){}
   logLine("CONNECTING ssid=%s timeout=30s AP remains active",name.c_str());
  }
  int st=(int)WiFi.status();
  if(connecting&&st!=lastStatus){logLine("WIFI status=%d",st);lastStatus=st;}
  if(connecting&&st==WL_CONNECTED) {
   connecting=false;wasConnected=true;server.client().stop();server.begin();
   logLine("STA_CONNECTED ip=%s rssi=%d AP=%s",WiFi.localIP().toString().c_str(),WiFi.RSSI(),WiFi.softAPIP().toString().c_str());
   xSemaphoreGive(networkReady);logLine("SEMAPHORE give network-ready (Wi-Fi connected; external Internet not yet verified)");
  } else if(connecting&&millis()-connectStart>CONNECT_TIMEOUT)startAP("connection-timeout");
  else if(wasConnected&&st!=WL_CONNECTED)startAP("STA-link-lost");
  if(digitalRead(BOOT)==LOW) {if(!down){down=true;buttonStart=millis();}if(down&&millis()-buttonStart>=80&&(connecting||wasConnected))startAP("BOOT-button");}
  else down=false;
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
void internetTask(void *) {
 mqtt.setBufferSize(512);mqtt.setSocketTimeout(2);mqtt.setCallback(callback);
 uint32_t lastPublish=0,lastAttempt=0,generation=0;String activeBroker,activeToken;uint16_t activePort=0;bool ready=false,announced=false;
 for(;;) {
  if(xSemaphoreTake(networkReady,pdMS_TO_TICKS(50))==pdTRUE){ready=true;generation=networkGeneration;logLine("SEMAPHORE take network-ready; MQTT task awakened");}
  if(ready&&(WiFi.status()!=WL_CONNECTED||generation!=networkGeneration)){mqtt.disconnect();ready=false;announced=false;logLine("MQTT paused: network not ready");}
  if(!ready)continue;
  String host,access;uint16_t port;
  xSemaphoreTake(configLock,portMAX_DELAY);host=broker;access=token;port=brokerPort;xSemaphoreGive(configLock);
  if(access.isEmpty()){if(!announced){logLine("MQTT WAIT: configure own CoreIOT token via /settings (not configured, not sent)");announced=true;}continue;}
  if(host!=activeBroker||access!=activeToken||port!=activePort){mqtt.disconnect();activeBroker=host;activeToken=access;activePort=port;mqtt.setServer(activeBroker.c_str(),activePort);lastAttempt=millis()-5000;}
  if(!mqtt.connected()&&millis()-lastAttempt>=5000) {
   lastAttempt=millis();String clientId="YoloUNO-week5-"+WiFi.macAddress();
   if(mqtt.connect(clientId.c_str(),activeToken.c_str(),"")) {
    bool sub=mqtt.subscribe("v1/devices/me/rpc/request/+");
    logLine("MQTT CONNECTED broker=%s port=%u RPC_subscribed=%u",activeBroker.c_str(),activePort,sub);lastPublish=millis()-10000;
   }else logLine("MQTT connect failed state=%d",mqtt.state());
  }
  if(mqtt.connected()) {
   mqtt.loop();
   if(millis()-lastPublish>=10000){lastPublish=millis();float t,h;bool ok;uint32_t seq;sensorSnapshot(t,h,ok,seq);
    if(ok){String payload=String("{\"temperature\":")+String(t,2)+",\"humidity\":"+String(h,2)+"}";bool accepted=mqtt.publish("v1/devices/me/telemetry",payload.c_str());logLine("MQTT TX local_write_ok=%u sample=%lu payload=%s (check CoreIOT for server receipt)",accepted,(unsigned long)seq,payload.c_str());}
   }
  }
 }
}
void serialTask(void *) {
 String line;
 for(;;){while(Serial.available()){char c=Serial.read();if(c=='\n'){
  StaticJsonDocument<512> d;
  if(!deserializeJson(d,line)) {
   String cmd=d["cmd"]|"";
   if(cmd=="wifi")requestWifi(d["ssid"]|"",d["pass"]|"");
   else if(cmd=="mqtt")requestMqtt(d["host"]|"app.coreiot.io",d["port"]|1883,d["token"]|"");
   else if(cmd=="status")logLine("STATUS %s",netJson().c_str());
   else logLine("Unknown serial command");
  }else logLine("Invalid serial JSON (payload hidden)");line="";
 }else if(c!='\r'){if(line.length()<512)line+=c;else line="";}}
 vTaskDelay(pdMS_TO_TICKS(10));}
}
void setup() {
 Serial.begin(115200);pinMode(BOOT,INPUT_PULLUP);pinMode(LED1,OUTPUT);pixel.begin();pixel.clear();pixel.show();
 logLock=xSemaphoreCreateMutex();dataLock=xSemaphoreCreateMutex();configLock=xSemaphoreCreateMutex();networkReady=xSemaphoreCreateBinary();
 if(!logLock||!dataLock||!configLock||!networkReady){while(true)delay(1000);}
 WiFi.persistent(false);
 WiFi.onEvent([](arduino_event_id_t event){if(event==ARDUINO_EVENT_WIFI_SCAN_DONE)scanDoneEvent=true;});
 delay(1500);
 logLine("WEEK5 ready; credentials RAM-only. LED1 GPIO48; LED2 NeoPixel GPIO45. LCD not installed.");
 xTaskCreate(sensorTask,"DHT20",4096,NULL,1,NULL);
 xTaskCreate(serverTask,"HTTP",8192,NULL,2,NULL);
 xTaskCreate(internetTask,"CoreIOT",6144,NULL,1,NULL);
 xTaskCreate(serialTask,"SerialConfig",4096,NULL,1,NULL);
}
void loop(){vTaskDelay(pdMS_TO_TICKS(1000));}
