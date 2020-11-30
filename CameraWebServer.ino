#include "esp_camera.h"
#include <WiFi.h>
#include <EEPROM.h>
//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiMulti.h>

const char* AP_SSID  = "MyCAM_ESP32"; //热点名称
const char* AP_PASS  = "12345678";  //密码
#define ROOT_HTML  "<!DOCTYPE html><html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><head><title>请连接到家里的路由器</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><style type=\"text/css\">.input{display: block; margin-top: 10px;}.input span{width: 100px; float: left; float: left; height: 36px; line-height: 36px;}.input input{height: 30px;width: 200px;}.btn{width: 120px; height: 35px; background-color: #000000; border:0px; color:#ffffff; margin-top:15px; margin-left:100px;}</style><body><form method=\"GET\" action=\"connect\"><label class=\"input\"><h1>请连接到家里的路由器</h1><span>WiFi SSID：</span><input type=\"text\" name=\"ssid\"></label><label class=\"input\"><span>WiFi密码：</span><input type=\"text\"  name=\"pass\"></label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"连接\"></form></body></html>"
WebServer server(80);
WiFiMulti wifiMulti;

uint8_t resr_count_down = 250;//重启倒计时s
TimerHandle_t xTimer_rest;
void restCallback(TimerHandle_t xTimer );

struct config_type
{
  int status;// 0: WIFI not set, 1: WIFI setted, 2: want to use AP to reset wifi setting, -1: not initial.
  char ssid[32];
  char psw[64];
  char lastRouterIP[16];
};

config_type wfconfig;

void startCameraServer();

void saveConfig()
{
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&wfconfig);
  for (int i = 0; i < sizeof(wfconfig); i++)
  {
    EEPROM.write(i, *(p + i));
  }
  EEPROM.commit();
}

void loadConfig()
{
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&wfconfig);
  for (int i = 0; i < sizeof(wfconfig); i++)
  {
    *(p + i) = EEPROM.read(i);
  }
  EEPROM.commit();
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  loadConfig();
  Serial.print(wfconfig.status);
  Serial.println();
  //test
  //wfconfig.status=-1;
  if(wfconfig.status==-1 || wfconfig.status==0)
  {
      WiFi.mode(WIFI_MODE_APSTA);//WIFI_AP:配置为AP模式
      boolean result = WiFi.softAP(AP_SSID, AP_PASS);//开启WIFI热点
      
      if (result)
      {
        //打印相关信息
        IPAddress myIP = WiFi.softAPIP();
        Serial.println("");
        Serial.print("Soft-AP IP address = ");
        Serial.println(myIP);
        Serial.println(String("MAC address = ")  + WiFi.softAPmacAddress().c_str());
        Serial.println("waiting ...");
    
        xTimer_rest = xTimerCreate("xTimer_rest", 1000 / portTICK_PERIOD_MS, pdTRUE, ( void * ) 0, restCallback);
        xTimerStart( xTimer_rest, 0 );  //开启定时器
    
      } else {  //开启热点失败
        Serial.println("WiFiAP Failed");
        delay(3000);
        ESP.restart();  //复位esp32
      }
    
      if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
      }

      
      if(wfconfig.status==-1)
      {
        //首页
        server.on("/", []() {
          server.send(200, "text/html", ROOT_HTML);
        });
      }
      else if(wfconfig.status==0)
      {
        // will show lastRouterIP on AP WebServer
        //Serial.println("listen on webserver /");
        server.on("/", []() {
          //Serial.println("will show lastRouterIP on AP WebServer");
          String routerip=String(wfconfig.lastRouterIP);
          String a = "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><head><script type=\"text/javascript\">function submitBtnClick(){var url = window.location.href + \"restart\";var request = new XMLHttpRequest();request.open(\"GET\", url);request.send(null);var startTime = new Date().getTime() + parseInt(10000, 10);while(new Date().getTime() < startTime){};window.location.href =\"http://"+routerip;
          String b = "\";}</script></head><body><h1>请点击重启按钮，切换到摄像头界面。</h1><h2>下次请访问：http://"+routerip;
          String c = +"。<button id=\"submitBtn\" onclick=\"submitBtnClick()\">重启</button></h2></body></html>";
          server.send(200, "text/html", a+b+c);
          wfconfig.status=1;
          saveConfig();
        });
      }
    
      //连接
      server.on("/connect", []() {
        String connectMsg="<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><head><script type=\"text/javascript\">var startTime = new Date().getTime() + parseInt(20000, 10);while(new Date().getTime() < startTime){};window.location.href =\"http://192.168.4.1";//+String(WiFi.softAPIP());
        server.send(200, "text/html", connectMsg+"\"</script></head><body><h1>正在连接...</h1></body></html>");
    
        //WiFi.softAPdisconnect(true);
        Serial.println(WiFi.softAPIP());
        //获取输入的WIFI账户和密码
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        Serial.println("WiFi is Connecting SSID:" + ssid + "  PASS:" + pass);
        //设置为STA模式并连接WIFI
        //WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        //Serial.println(WiFi.softAPIP());
        resr_count_down = 250;
        xTimerStop(xTimer_rest, 0);
    
        uint8_t Connect_time = 0; //用于连接计时，如果长时间连接不成功，复位设备
    
        while (WiFi.status() != WL_CONNECTED) {  //等待WIFI连接成功
          delay(500);
          Serial.print(".");
          Connect_time ++;
          if (Connect_time > 80) {  //长时间连接不上，复位设备
            Serial.println("Connection timeout, check input is correct or try again later!");
            delay(3000);
            ESP.restart();
          }
        }
        //connected now, and never set wifi setting before.
        Serial.println("WiFi Connected SSID:" + ssid + "  PASS:" + pass);
        IPAddress ip = WiFi.localIP();
        String routerip = String(ip[0])+'.'+String(ip[1])+'.'+String(ip[2])+'.'+String(ip[3]);
        //Serial.println(WiFi.softAPIP());
        //WiFi.disconnect();
        //server.begin();
        //server.send(200, "text/html", "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><body><h1>连接成功！</h1></body></html>");
        wfconfig.status=0;
        strcpy(wfconfig.ssid, ssid.c_str());
        strcpy(wfconfig.psw, pass.c_str());
        strcpy(wfconfig.lastRouterIP, routerip.c_str());
        Serial.println(routerip);
        Serial.println(wfconfig.lastRouterIP);
        saveConfig();
        //loadConfig();
        Serial.println("saveConfig()");
        ESP.restart();
        //String a = "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><head><script type=\"text/javascript\">function submitBtnClick(){var url = window.location.href + \"/restart\";var request = new XMLHttpRequest();request.open(\"POST\", url);request.send(\"F\");var startTime = new Date().getTime() + parseInt(10000, 10);while(new Date().getTime() < startTime){};window.location.href =\"http://"+WiFi.localIP();
        //String b = "\";}</script></head><body><h1>已存储连接密码。</h1><h2>下次请访问：http://"+WiFi.localIP();
        //String c = +"。<button id=\"submitBtn\" οnclick=\"submitBtnClick()\">重启</button></h2></body></html>";
        //server.send(200, "text/html", a + b + c);
      });

      server.on("/restart", []() {
        //server.send(200, "text/html", "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><body><h1>已存储连接密码。</h1><h2>设备正在自动重启。</h2></body></html>");
        //delay(10000);
        ESP.restart();
      });
      server.begin();
  }
  else if(wfconfig.status==1)
  {
    //wifi setting stored before.
    
    Serial.print(wfconfig.ssid);
    Serial.println();
    Serial.print(wfconfig.psw);
    Serial.println();
    Serial.print(wfconfig.lastRouterIP);
    Serial.println();
  
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    //init with high specs to pre-allocate larger buffers
    if(psramFound()){
      config.frame_size = FRAMESIZE_UXGA;
      config.jpeg_quality = 10;
      config.fb_count = 2;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
    }
  
  #if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
  #endif
  
    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      return;
    }
  
    sensor_t * s = esp_camera_sensor_get();
    //drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_QVGA);
  
  #if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  #endif
  
    WiFi.begin(wfconfig.ssid, wfconfig.psw);
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
  
    startCameraServer();
  
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if(wfconfig.status==-1||wfconfig.status==0)
  {
    server.handleClient();
    while (WiFi.status() == WL_CONNECTED) {
      //WIFI已连接
      //Serial.println("WIFI is connected");
    }
  }
  else if(wfconfig.status==1)
  {
    delay(10000);
  }
}

void restCallback(TimerHandle_t xTimer ) {  //长时间不访问WIFI Config 将复位设备
  resr_count_down --;
  Serial.print("resr_count_down: ");
  Serial.println(resr_count_down);
  if (resr_count_down < 1) {
    ESP.restart();
  }
}
