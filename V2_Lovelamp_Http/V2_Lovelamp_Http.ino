/*  
 * ***************************新的芯片需要更改信息******************
 * 1 MQTT分配的账户和密码，唯一。 需手动添加在MQTT服务器配置文件，重启服务器。  love_00001_x-love_00100_x留着测试
 * 2 MQTT自己和对方的的名字，用于生成唯一通信话题。
 * 共四个修改
 * 
 * ******************程序流程***********************
1 上电断电自动连接WIFI
2 长按进入网页配网  192.168.4.1  WIFI名= WIFI密码 LOVELAMP_（ESP芯片唯一ID）
3 自动连MQTT
4 进入交互模式
  订阅自己的接受话题
  订阅对方的发送话题
5 单方面短按                灯亮，我在旁边等你。           1同时对两盏灯颜色切换模式  红 绿 栏 白 黄 紫
  单方面在某种颜色下长按    灯单色呼吸，我手正在灯上等你。 1在该颜色下同时两盏灯呼吸动态发光  亮-暗-亮  
                                                           2中途停止则停留在当前颜色和亮度下
                                                              2-1 重新长按触摸接着上次停下的动态呼吸
                                                              2-2 重新短按切换到下个颜色
  双方共同长按              七彩渐变，我们的手同时在灯上。 1各种颜色动态变化
                                                           

说明：
 
 网页服务器，只能配置 AP模式，速度才最快。  
 STA和AP共存模式不行，网页刷不出来。

*/

//---------------------- 修改 -------------------------------------------

// 准入MQTT服务器的账号密码
#define MQTT_SSID        "love_00001_x"  //修改账号
#define MQTT_PASSWORD    "love_00001_x"  //修改密码
// 自己的MQTT连接ID名称     love+_唯一自定义序列号_+x或y                 MQTT_MYNAME用于生成唯一通信话题    MQTT_MYNAME+SN(出场芯片唯一ID)用于生成唯一MQTT  ID
#define MQTT_MYNAME     "love_00001_x"        //修改自己的名称
#define MQTT_YOURNAME   "love_00001_y"        //修改对方的名称
// MQTT服务器
#define MQTT_SERVER "www.dongvdong.top"
#define PORT 1883
//---------------------------------------------------------------------------------------------
/*
话题格式：
接收   lovelamp/+MQTT_MYNAME+"/r"     lovelamp/love_00001_x/r
发布   lovelamp/+MQTT_MYNAME+"/s"     lovelamp/love_00001_x/s
*/

// 自己的话题 
//#define MQTT_MYNAME    "love_testx"        //实际全称 MQTT_MYNAME+SN(esn8266出场唯一设备ID)  必须唯一
String mqtt_mytopic_s=String()+"lovelamp/"+String(MQTT_MYNAME)+"/s";
String mqtt_mytopic_r=String()+"lovelamp/"+String(MQTT_MYNAME)+"/r";
#define MQTT_MYTOPICS mqtt_mytopic_s.c_str()
#define MQTT_MYTOPICR mqtt_mytopic_r.c_str()
 
// 对方的话题 
//#define MQTT_YOURNAME    "love_testy"        //实际全称 MQTT_MYNAME+SN(esn8266出场唯一设备ID)  必须唯一
String mqtt_yourtopic_s=String()+"lovelamp/"+String(MQTT_YOURNAME)+"/s";
String mqtt_yourtopic_r=String()+"lovelamp/"+String(MQTT_YOURNAME)+"/r";
#define MQTT_YOURTOPICS mqtt_yourtopic_s.c_str()
#define MQTT_YOURTOPICR mqtt_yourtopic_r.c_str()


//-----------------------------A-1 库引用开始-----------------//
#include <string.h>
#include <math.h>  // 数学工具库
#include <EEPROM.h>// eeprom  存储库

// WIFI库
#include <ESP8266WiFi.h>
//-------客户端---------
#include <ESP8266HTTPClient.h>
//---------服务器---------
#include <ESP8266WebServer.h>  
#include <FS.h> 

//--------MQTT
#include <PubSubClient.h>//MQTT库
WiFiClient espClient;
PubSubClient client(espClient);



#include <Adafruit_NeoPixel.h>  //WSB彩灯库


//ESP获取自身ID
#ifdef ESP8266
extern "C" {
#include "user_interface.h"   //含有system_get_chip_id（）的库
}
#endif

//-----------------------------A-1 库引用结束-----------------//

//------是否开启打印-----------------
#define Use_Serial Serial
#define smartLED D4 



//-----------------------------A-2 变量声明开始-----------------//
int PIN_Led = D4;
int PIN_Led_light = 0; 
bool PIN_Led_State=0;

int workmode=0;
int led_sudu=80;
/************************ 按键中断7**********************************/
//按键中断
int PIN_Led_Key= D2 ;// 不要上拉电阻


/******普通彩色LED********/
#define BUTTON_PIN   D2
#define PIXEL_PIN    D1 
#define PIXEL_COUNT  8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool oldState = HIGH;
int showType = 0;
 
 int light;//亮度变化
 int pwm;// 动态开始标志位
 
const size_t t_bright=1,t_color=2,t_frequency=3,t_msg_longtouch=4,t_you_hangs_on=5;// 模式
int red = 0,green = 0,blue = 0;// 亮度值
int type = 2;//当前模式 1亮度 2颜色 3呼吸 4开关

int my_hands_on=0; //  本灯的手是否在灯上                  
int you_hands_on=0; //  对双方的灯的手是否在灯上  长短消息  0  没消息 1 短按 2 长按
int old_red = 1,old_green = 1,old_blue = 1;// 当前状态亮度值

bool one_pwm_go=0;
bool two_pwm_go=0;
int led_state=0;//  0 灭  1 单色静止 2 单色动态 3多彩动态
 
int sendtime=0;
int sendtime2=0;


ESP8266WebServer server ( 80 );  
//储存SN号
String SN;

/*----------------WIFI账号和密码--------------*/
 
#define DEFAULT_STASSID "lovelamp"
#define DEFAULT_STAPSW  "lovelamp" 

//下图说明了Arduino环境中使用的Flash布局：

//|--------------|-------|---------------|--|--|--|--|--|
//^              ^       ^               ^     ^
//Sketch    OTA update   File system   EEPROM  WiFi config (SDK)


// 用于存上次的WIFI和密码
#define MAGIC_NUMBER 0xAA
struct config_type
{
  char stassid[50];
  char stapsw[50];
  uint8_t magic;
};
config_type config_wifi;


 
  
 
int sendbegin=0;         // 0  没发送  1 发送短消息  2 发送长消息
char sendmsg[100];// 发送话题-原始
String sendstr; // 接收消息-转换
int recbegin=0;         // 0  没接收  1 接收短消息  2 接收长消息
char recmsg[50];// 接收消息-原始
String recstr; // 接收消息-转换


//--------------HTTP请求------------------
struct http_request {  
  String  Referer;
  char host[20];
  int httpPort=80;
  String host_ur ;
  
  String usr_name;//账号
  String usr_pwd;//密码
 
  String postDate;

  };

String http_html;
//-----------------------------A-2 变量声明结束-----------------//


//-----------------------------A-3 函数声明开始-----------------//
void get_espid();
void LED_Int();

void  LEDRGB_Int_WSB() ;
uint32_t Wheel(byte WheelPos);//Input a value 0 to 255 to get a color value.
void rainbow(uint8_t wait) ;//彩虹
void colorWipe(uint32_t c, uint8_t wait);//七彩灯基本函数 指定颜色

void WSBled();//七彩灯按键选择
void startShow(int i) ;//七彩灯按键选择
  void json_rainbow_pub(String pwm);
  void json_rgb_pub(String R,String G,String B);
void do_if_rainbow(uint8_t wait);
void do_if();
void print_doif();
void int_do_if();

void Button_Int();
void highInterrupt();
void lowInterrupt();
void waitKey();

void wifi_Init();
void saveConfig();
void loadConfig();

void SET_AP();
int hdulogin(struct http_request ruqest) ;
void http_wifi_xidian();
String getContentType(String filename);
void handleNotFound();
void handleMain();
void handlePin();
void handleWifi();
void http_jiexi(http_request ruqest);
void handleWifi_wangye();
void handleTest();



void smartConfig();//500闪烁
void Mqtt_message();
void mqtt_reconnect() ;

bool parseData( String  str);// 接收WIFI消息，总动作控制
String  JsontoString(String zifuchuan,String fengefu);// 从Json格式的String中，截取对应键值
int StringtoHex(String temps);//String 转 16进制对应的10进制数
int StringtoInt(String temps);//String 转 10进制对应的10进制数
bool RGB_LED(String str);//WSBled彩灯控制
//---------------------------A-3 函数声明结束------------------------//


//-----------------------------A-3 函数-----------------//
//3-1管脚初始化
void get_espid(){
   SN = (String )system_get_chip_id();
  Use_Serial.println(SN);
  
  }
void LED_Int(){
  
   pinMode(D4, OUTPUT); 
  
  }
 // WSB led
void  LEDRGB_Int_WSB() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
 
 
}
/*************************** 3-2 按键中断函数执行*****************************/
int t1=0,t2=0,t3=0;//  长短按键计时器
// 高电平触发
void highInterrupt(){ 
  // delay(50);
  int reading=digitalRead(PIN_Led_Key);
if(reading==HIGH){
  
     my_hands_on=1;//手在灯上
     t1=millis(); //  开始计时
     t2=0;// 中断内计算时间差
     t3=0;  //主函数计算时间差值
     
     sendtime=0;
     sendtime2=0;
 
       attachInterrupt(PIN_Led_Key, lowInterrupt,FALLING);
}
  
   }
void lowInterrupt(){
  
   my_hands_on=0; // 手离开灯
     sendtime=0;
     sendtime2=0;
       
     t2=millis()-t1;// 计时结束
      
      // 开启发送消息标志位
     if(t2<2000){  // 短按动作 发射短消息
       sendbegin=1;
       
      }
      else{
     //  sendbegin=2;     // 长安动作 发射长安消息  
         sendbegin=0;
        }
             
     attachInterrupt(PIN_Led_Key, highInterrupt,RISING);
  
  }

/*************************** 3-4 WSB七彩LED函数执行*****************************/
/*--------------------- 3-4-1七彩灯按键选择---------------*/
void WSBled(){
  bool newState = digitalRead(BUTTON_PIN);
  
  if (newState == LOW && oldState == HIGH) {
  
    delay(20);
  
    newState = digitalRead(BUTTON_PIN);
    if (newState == LOW) {
      showType++;
      if (showType > 7)
        showType=0;
      startShow(showType);
    }
  }
 
  // Set the last button state to the old state.
  oldState = newState;
}
void json_rgb_pub(String R,String G,String B){
 
   String msg_s= "{\"ledmode\":2,\"cr\":"+R+",\"cg\":"+G+",\"cb\":"+B+"}";
   
    String msg_rain= "{\"ledmode\":3,\"cc\":0}";
    
   char msg_sc[50];
   char msg_rainc[50];
   if(client.connected()) {
     
         strcpy(  msg_rainc, msg_rain.c_str());
          client.publish(MQTT_MYTOPICS,msg_rainc); // 强制关闭彩虹动态
           
            strcpy( msg_sc,msg_s.c_str());
          client.publish(MQTT_MYTOPICS, msg_sc); 
    }    
   
   
  }
   
  void json_rainbow_pub(String pwm){
 
     String msg_s= "{\"ledmode\":3,\"cc\":"+pwm+"}";
      char msg_c[50];
   strcpy( msg_c,msg_s.c_str());
   if(client.connected()) {
          client.publish(MQTT_MYTOPICS, msg_c); 
    }    
  }

  /*--------------------- 3-4-2七彩灯按键选择---------------*/
void startShow(int i) {
  switch(i){
    case 0:
      json_rgb_pub("0","0","0");
    colorWipe(strip.Color(0, 0, 0), 1);    // Black/off
    old_red = 0;old_green = 0;old_blue = 0;
    
         led_state=0;
       //回掉函数直接修改颜色
         one_pwm_go=0;
         two_pwm_go=0;
         recbegin=0;
          
            break;
    case 1:
       json_rgb_pub("ff","0","0");
         colorWipe(strip.Color(255, 0, 0), 1);  // Red
       old_red = 1;old_green = 0;old_blue = 0;
            break;
    case 2:
      json_rgb_pub("0","ff","0");
    colorWipe(strip.Color(0, 255, 0), 1);  // Green
    old_red = 1;old_green = 1;old_blue = 0;
            break;
    case 3:
      json_rgb_pub("0","0","ff");
    colorWipe(strip.Color(0, 0, 255),1);  // Blue
    old_red = 1;old_green = 0;old_blue = 1;
            break;
    case 4: //theaterChase(strip.Color(127, 127, 127), 50); // White
       json_rgb_pub("ff","ff","ff");
            colorWipe(strip.Color(255,255, 255), 1); 
            old_red = 1;old_green = 1;old_blue = 1;
            break;
    case 5:// theaterChase(strip.Color(127,   0,   0), 50); // Red
        json_rgb_pub("ff","ff","0");
            colorWipe(strip.Color(255,   255,   0), 1);
              old_red = 1;old_green = 1;old_blue = 0;
            break;
    case 6: //theaterChase(strip.Color(  0,   0, 127), 50); // Blue
        json_rgb_pub("ff","0","ff");
           colorWipe(strip.Color( 255,   0, 255), 1);
            old_red = 1;old_green = 0;old_blue = 1;
            break;
    case 7:
         json_rgb_pub("0","ff","ff");
    colorWipe(strip.Color( 0,   255, 255), 1);
     old_red = 1;old_green = 1;old_blue = 1;
            break;
    
  }
}
/*--------------------- 3-4-3七彩灯基本函数 指定颜色---------------*/
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
   delay(wait);
 
  }
}
 
/*--------------------- 3-4-4七彩灯基本函数 彩虹---------------*/
void rainbow(uint8_t wait) {
  uint16_t i, j;
 
  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      client.loop();//MUC接收数据的主循环函数。 
      if(pwm==1){
      strip.setPixelColor(i, Wheel((i+j) & 255));
      }
      else{
        return ;
        }
    }
   strip.show();
   delay(wait);
  }
}
/*--------------------- 3-4-4七彩灯基本函数 基础---------------*/
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}



void int_do_if(){
              led_state=0;
               //回掉函数直接修改颜色
              one_pwm_go=0;
              recbegin=0;
              sendbegin=0;
}
void print_doif(){
   
    Serial.print("-sendbegin-");      Serial.print(sendbegin);     Serial.print("-you_hands_on-");      Serial.print(you_hands_on);  Serial.print("-my_hands_on-");     Serial.print(my_hands_on);     
    Serial.print("-one_wm_go-");      Serial.print(one_pwm_go);    Serial.print("-two_pwwwm_go-");      Serial.println(two_pwm_go); 
  }

  
void do_if(){
 
if(sendbegin==0){// 1 没有按键
 
    if(you_hands_on==1){// 2-1 对方手在
 
    if(my_hands_on==1){
       one_pwm_go=0;  // 开启独闪模式
       two_pwm_go=1;  // 关闭共闪模式
    sendtime2++;
    if(sendtime2<=1){
    if(client.connected()){ client.publish(MQTT_MYTOPICS," {\"ledmode\":5,\"my_hands_on\":1}");   }  
                   }
      }
      else{      
       
      one_pwm_go=1;  // 开启独闪模式
      two_pwm_go=0;  // 关闭共闪模式
       sendtime2++;
    if(sendtime2<=1){
    if(client.connected()){ client.publish(MQTT_MYTOPICS," {\"ledmode\":5,\"my_hands_on\":0}");   }  
                   }
        }
       
    }
     else{
       one_pwm_go=0;  // 开启独闪模式
       two_pwm_go=0;  // 关闭共闪模式
          }
 //sendbegin=0;
 }
   
else if(sendbegin==1){//2 短按键
   
    if(you_hands_on==1){// 2-1 对方手在
       
       one_pwm_go=1;  // 开启独闪模式小尾巴
       two_pwm_go=0;  // 关闭共闪模式
        
      }
      else {//2-2 对方手不在
         
       one_pwm_go=0;  // 开启独闪模式
       two_pwm_go=0;  // 关闭共闪模式
         
       //----------- 按键循环改变自己和对方的灯------------
      showType++;         // 按键计数++
      if (showType > 7)   // 其中颜色状态
      { showType=0;}      //从头开始
     // 消息内容-具体颜色值
      startShow(showType); // 中断按键检测 切换灯的颜色 
     //------------------------------------------------------
    
        }
  sendbegin=0;
  }
else if(sendbegin==2){//3长按键
 
  if(my_hands_on==1){
    sendtime2++;
    if(sendtime2<=1){
    if(client.connected()){ client.publish(MQTT_MYTOPICS," {\"ledmode\":5,\"my_hands_on\":1}");   }  
                   }
   
    if(you_hands_on==1){// 2-1 对方手在
       
       one_pwm_go=0;  // 关闭独闪模式
       two_pwm_go=1; // 开启共同闪模式
 
        
      }
      else {//2-2 对方手不在
 
       one_pwm_go=1;  // 开启独闪模式
       two_pwm_go=0;  // 关闭共闪模式
 
        
 
      }
   }
   else{
     
    sendbegin=0;
    }
      
 //  sendbegin==0
  }
 
 
 
       
}

int pwm_up_down=1;
void do_if_rainbow(uint8_t wait) {
  uint16_t i, j;
 
  for(j=0; j<256; j++) {
        
    for(i=0; i<strip.numPixels(); i++) {
       
      client.loop();//MUC接收数据的主循环函数。 
      do_if();
     if(two_pwm_go==0&&one_pwm_go==0){return;}
 
     
    
     if(one_pwm_go==1){
 
      if(pwm_up_down==1)
           { strip.setPixelColor(i, strip.Color(j*old_red, j*old_green, j*old_blue));
              if(j==255&& i==strip.numPixels()-1){pwm_up_down=0; }
            
           }
           else{
            strip.setPixelColor(i, strip.Color(old_red*(255-j), old_green*(255-j), old_blue*(255-j)));
 
              if(j==255&& i==strip.numPixels()-1){pwm_up_down=1; }
            }
             
            }
             
      else{   
               if(two_pwm_go==1){ strip.setPixelColor(i, Wheel((i+j) & 255));  }
                   else { break; }
       }
   strip.show();
   delay(wait);
  }
 }
}


  /*************************** 2-2 按键LED函数初始化（）*****************************/ 
void Button_Int(){
 pinMode(PIN_Led_Key, INPUT);
 attachInterrupt(PIN_Led_Key, highInterrupt, RISING);
  }



void smartConfig()
{
  led_sudu=30;
  Serial.println("Start smartConfig module");
  pinMode(smartLED, OUTPUT);
  digitalWrite(smartLED, 0);
   
  WiFi.mode(WIFI_STA);
  Serial.println("\r\nWait for Smartconfig");
  WiFi.stopSmartConfig();
  WiFi.beginSmartConfig();

        unsigned long preTick = millis();
      int num = 0;
  while (1)
 {  
       
    if (millis() - preTick < 10 ) continue;//等待10ms
    preTick = millis();
    num++;
    if (num % led_sudu == 0)  //50*10=500ms=0.5s 反转一次
    {
        PIN_Led_State=!PIN_Led_State;
    Serial.print(".");             
    digitalWrite(smartLED, PIN_Led_State);
        colorWipe(strip.Color(PIN_Led_State*254, PIN_Led_State*254, PIN_Led_State*0), 0); 
          }
    if (WiFi.smartConfigDone())
    {  Serial.println(" ");
      Serial.println("SmartConfig Success");      
      strcpy(config_wifi.stassid, WiFi.SSID().c_str());
      strcpy(config_wifi.stapsw, WiFi.psk().c_str());      
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
      saveConfig();
      break;
    }
   
  }
}          

//等待按键动作  
/*************************6-1 上电出始等待 *************************************/
void waitKey()
{
  pinMode (PIN_Led, OUTPUT);
  pinMode (PIN_Led_Key , INPUT);
  digitalWrite(PIN_Led, 0);
  Serial.println(" press key 2s: smartconfig mode! \r\n press key 0s: connect last time wifi!");
      
  char keyCnt = 0;
  unsigned long preTick = millis();
  unsigned long preTick2 = millis();
  int num = 0;
  while (1)
  { 
    ESP.wdtFeed();
   
    if (millis() - preTick < 10 ) continue;//等待10ms
    preTick = millis();
    num++;
    if (num % led_sudu == 0 )  //50*10=500ms=0.5s 反转一次
    {
      PIN_Led_State = !PIN_Led_State;
      digitalWrite(PIN_Led, PIN_Led_State);
        colorWipe(strip.Color(PIN_Led_State*254, PIN_Led_State*254, PIN_Led_State*254), 1); 
      Serial.print(".");
    }
     
     if(  workmode!=1 && digitalRead(PIN_Led_Key) == 1){
      // 按键触摸大于2S  进入自动配网模式
    if ( keyCnt >= 200 ) // 200*10=2000ms=2s  大于2s反转
    { //按2S 进入一键配置
       workmode=1;
      Serial.println("\r\n Short Press key");

            //  smartConfig();  workmode=0;

             SET_AP();       // 建立WIFI
             SPIFFS.begin(); // ESP8266自身文件系统 启用 此方法装入SPIFFS文件系统。必须在使用任何其他FS API之前调用它。如果文件系统已成功装入，则返回true，否则返回false。
             Server_int();  // HTTP服务器开启
             led_sudu=30;
    }
     }

   
      // 不按按键，自动连接上传WIFI
    if (millis() - preTick2 > 5000 && digitalRead(PIN_Led_Key) == 0) {   // 大于5S还灭有触摸，直接进入配网
       Serial.println("\r\n 10s timeout!");
            if(  workmode==0){ wifi_Init();}          
            return; 
            }
                                  
    
    if (digitalRead(PIN_Led_Key) == 1){ keyCnt++;}
    else{keyCnt = 0;}
    
  }

   Serial.println("\r\n wait key over!");  
  digitalWrite(PIN_Led, 0);
  digitalWrite(PIN_Led_Key, 0);
  
}


//3-2WIFI初始化
void wifi_Init(){
  led_sudu=80;
 // WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP_STA);
  workmode=0; 
  Use_Serial.println("Connecting to ");
  Use_Serial.println(config_wifi.stassid);
  WiFi.begin(config_wifi.stassid, config_wifi.stapsw);
   server.handleClient();   
  unsigned long preTick = millis();    
 int num;

  while (WiFi.status() != WL_CONNECTED) {
  
   if (millis() - preTick < 10 ) continue;//等待10ms
  preTick = millis();
   num++;
       
   server.handleClient();   
  
  ESP.wdtFeed();
  
 // delay(pwm_wifi_connect);

  if(num% led_sudu==0){
 
  PIN_Led_State = !PIN_Led_State;
  digitalWrite(PIN_Led, PIN_Led_State);
  colorWipe(strip.Color(PIN_Led_State*254, PIN_Led_State*254, PIN_Led_State*254), 1); 
  Use_Serial.print(".");
  num=0;
  }

  
 
  }
  WiFi.mode(WIFI_STA);
  Use_Serial.println("--------------WIFI CONNECT!-------------  ");
  Use_Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
  Use_Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
  Use_Serial.println("----------------------------------------  ");
//  workmode=0; 
 
  }



/*
 *3-/2 保存参数到EEPROM
*/
void saveConfig()
{
  Serial.println("Save config!");
  Serial.print("stassid:");
  Serial.println(config_wifi.stassid);
  Serial.print("stapsw:");
  Serial.println(config_wifi.stapsw);
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&config_wifi);
  for (int i = 0; i < sizeof(config_wifi); i++)
  {
    EEPROM.write(i, *(p + i));
  }
  EEPROM.commit();
}
/*
 * 从EEPROM加载参数
*/
void loadConfig()
{
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&config_wifi);
  for (int i = 0; i < sizeof(config_wifi); i++)
  {
    *(p + i) = EEPROM.read(i);
  }
  EEPROM.commit();
  //出厂自带
  if (config_wifi.magic != MAGIC_NUMBER)
  {
    strcpy(config_wifi.stassid, DEFAULT_STASSID);
    strcpy(config_wifi.stapsw, DEFAULT_STAPSW);
    config_wifi.magic = MAGIC_NUMBER;
    saveConfig();
    Serial.println("Restore config!");
  }
  Serial.println(" ");
  Serial.println("-----Read config-----");
  Serial.print("stassid:");
  Serial.println(config_wifi.stassid);
  Serial.print("stapsw:");
  Serial.println(config_wifi.stapsw);
  Serial.println("-------------------");

}
 
  
//3-3ESP8266建立无线热点
void SET_AP(){
  WiFi.mode(WIFI_AP);
 // WiFi.mode(WIFI_AP_STA);
  // 设置内网
  IPAddress softLocal(192,168,4,1);   // 1 设置内网WIFI IP地址
  IPAddress softGateway(192,168,4,1);
  IPAddress softSubnet(255,255,255,0);
  WiFi.softAPConfig(softLocal, softGateway, softSubnet);
   
  String apName = ("Lovelamp_"+(String)ESP.getChipId());  // 2 设置WIFI名称
  const char *softAPName = apName.c_str();
   
  WiFi.softAP(softAPName, "admin");      // 3创建wifi  名称 +密码 admin

  Use_Serial.print("softAPName: ");  // 5输出WIFI 名称
  Use_Serial.println(apName);

  IPAddress myIP = WiFi.softAPIP();  // 4输出创建的WIFI IP地址
  Use_Serial.print("AP IP address: ");     
  Use_Serial.println(myIP);
  
  }
//3-4ESP建立网页服务器
void Server_int(){
  
   server.on ("/", handleMain); // 绑定‘/’地址到handleMain方法处理 ----  返回主页面 一键配网页面 
  server.on ("/pin", HTTP_GET, handlePin); // 绑定‘/pin’地址到handlePin方法处理  ---- 开关灯请求 
   server.on ("/wifi", HTTP_GET, handleWifi); // 绑定‘/wifi’地址到handlePWIFI方法处理  --- 重新配网请求
    server.on ("/wifi_wangye", HTTP_GET, handleWifi_wangye);
    server.on ("/test", HTTP_GET, handleTest);
    server.onNotFound ( handleNotFound ); // NotFound处理
   server.begin(); 
   
   Use_Serial.println ( "HTTP server started" );
  
  }
//3-5-1 网页服务器主页
void handleMain() {  

  /* 返回信息给浏览器（状态码，Content-type， 内容） 

   * 这里是访问当前设备ip直接返回一个String 

   */  

  Serial.print("handleMain");  

//打开一个文件。path应该是以斜线开头的绝对路径（例如/dir/filename.txt）。mode是一个指定访问模式的字符串。
//它可以是“r”，“w”，“a”，“r +”，“w +”，“a +”之一。这些模式的含义与fopenC功能相同
  File file = SPIFFS.open("/index.html", "r");  

  size_t sent = server.streamFile(file, "text/html");  

  file.close();  

  return;  

}  
//3-5-2 网页控制引脚
/* 引脚更改处理 

 * 访问地址为htp://192.162.xxx.xxx/pin?a=XXX的时候根据a的值来进行对应的处理 

 */  

void handlePin() {  

  if(server.hasArg("a")) { // 请求中是否包含有a的参数  

    String action = server.arg("a"); // 获得a参数的值  

    if(action == "on") { // a=on  

      digitalWrite(2, LOW); // 点亮8266上的蓝色led，led是低电平驱动，需要拉低才能亮  

      server.send ( 200, "text/html", "测试灯点亮！"); return; // 返回数据  

    } else if(action == "off") { // a=off  

      digitalWrite(2, HIGH); // 熄灭板载led  

      server.send ( 200, "text/html", "测试灯熄灭！"); return;  

    }  

    server.send ( 200, "text/html", "未知操作！"); return;  

  }  

  server.send ( 200, "text/html", "action no found");  

}  

//3-5-3 网页修改普通家庭WIFI连接账号密码
/* WIFI更改处理 

 * 访问地址为htp://192.162.xxx.xxx/wifi?config=on&name=Testwifi&pwd=123456
  根据wifi进入 WIFI数据处理函数
  根据config的值来进行 on
  根据name的值来进行  wifi名字传输
  根据pwd的值来进行   wifi密码传输


 */  


void handleWifi(){
  
  
   if(server.hasArg("config")) { // 请求中是否包含有a的参数  

    String config = server.arg("config"); // 获得a参数的值  
        String wifiname;
        String wifipwd;

     
        
    if(config == "on") { // a=on  
          if(server.hasArg("name")) { // 请求中是否包含有a的参数  
        wifiname = server.arg("name"); // 获得a参数的值

          }
          
    if(server.hasArg("pwd")) { // 请求中是否包含有a的参数  
         wifipwd = server.arg("pwd"); // 获得a参数的值    
           }
                  
          String backtxt= String()+" WIFI:\""+wifiname+"\"配置成功! \n\r 已进入慢闪模式！\n\r请重启设备等待其自行连接！" ;// 用于串口和网页返回信息
          
          Use_Serial.println ( backtxt); // 串口打印给电脑
          
         // server.send ( 200, "text/html", backtxt); // 网页返回给手机提示
           // wifi连接开始

         wifiname.toCharArray(config_wifi.stassid, 50);    // 从网页得到的 WIFI名
         wifipwd.toCharArray(config_wifi.stapsw, 50);  //从网页得到的 WIFI密码               
         
         saveConfig();
         server.send ( 200, "text/html", backtxt); // 网页返回给手机提示  
         //ESP.reset();
         wifi_Init();
 
          //   server.send ( 200, "text/html", "connect scucces!"); // 网页返回给手机提示   
     
       
          return;          
           

    } else if(config == "off") { // a=off  
                server.send ( 200, "text/html", "config  is off!");
        return;

    }  

    server.send ( 200, "text/html", "unknown action"); return;  

  }  

  server.send ( 200, "text/html", "action no found");  
  
  }

//3-5-5-1 网页认证上网
void handleWifi_wangye(){
  if(server.hasArg("config")) { // 请求中是否包含有a的参数  
// 1 解析数据是否正常
    String config = server.arg("config"); // 获得a参数的值  
        String wifi_wname,wifi_wpwd,wifi_wip,wifi_postdata;
          
  if(config == "on") { // a=on  
        if(server.hasArg("wifi_wname")) { // 请求中是否包含有a的参数  
        wifi_wname = server.arg("wifi_wname"); // 获得a参数的值
           if(wifi_wname.length()==0){            
             server.send ( 200, "text/html", "please input WIFI名称!"); // 网页返回给手机提示
            return;} 
          }
        if(server.hasArg("wifi_wpwd")) { // 请求中是否包含有a的参数  
        wifi_wpwd = server.arg("wifi_wpwd"); // 获得a参数的值
               
          }
     if(server.hasArg("wifi_wip")) { // 请求中是否包含有a的参数  
         wifi_wip = server.arg("wifi_wip"); // 获得a参数的值    
          if(wifi_wip.length()==0){            
             server.send ( 200, "text/html", "please input 登陆网址!"); // 网页返回给手机提示
            return;} 

           }
        if(server.hasArg("wifi_postdata")) { // 请求中是否包含有a的参数  

            String message=server.arg(3);
            for (uint8_t i=4; i<server.args(); i++){
               message += "&" +server.argName(i) + "=" + server.arg(i) ;
             }
  
         wifi_postdata = message; // 获得a参数的值 
            if(wifi_postdata.length()==0){            
             server.send ( 200, "text/html", "please input 联网信息!"); // 网页返回给手机提示
            return;}    
           }
       
//--------------------------------- wifi连接-------------------------------------------------
         wifi_wname.toCharArray(config_wifi.stassid, 50);    // 从网页得到的 WIFI名
         wifi_wpwd.toCharArray(config_wifi.stapsw, 50);  //从网页得到的 WIFI密码               
         
         saveConfig();
      
         // wifi连接开始
         wifi_Init();
 
  //--------------------------------- 认证上网-------------------------------------------------      


  
// 2 发送认证
                
// ruqest.Referer="http://10.255.44.33/srun_portal_pc.php";
  http_request ruqest_http;
  ruqest_http.httpPort = 80; 
  ruqest_http.Referer=wifi_wip;// 登录网址
  ruqest_http.postDate=wifi_postdata;
   
   Use_Serial.println (wifi_wip);
   Use_Serial.println (wifi_postdata);
  

    
  http_jiexi(ruqest_http);
          
           return; 
  } else if(config == "off") { // a=off  
                server.send ( 200, "text/html", "config  is off!");
        return;
  }  
    server.send ( 200, "text/html", "unknown action"); return;  
  }  
      server.send ( 200, "text/html", "action no found");  

  }

//3-5-5-2 网页解析IP消息 
void http_jiexi(http_request ruqest){
  
    int datStart ; int datEnd;

    // 1 解析ip
   //   http://10.255.44.33/ 
    char h[]="http://";  char l[]="/";
    datStart  =  ruqest.Referer.indexOf(h)+ strlen(h);
    datEnd= ruqest.Referer.indexOf(l,datStart);
    String host=ruqest.Referer.substring(datStart, datEnd);
    host.toCharArray( ruqest.host , 20); 
    
    Use_Serial.println ( ruqest.host); // 串口打印给电脑

    // 2 解析请求的网页文件-登录页面
   //   srun_portal_pc.php?ac_id&";
    char s[] ="?"; 
    datStart  = ruqest.Referer.indexOf(l,datStart)+strlen(l);
   // datEnd = ruqest.Referer.indexOf(s,datStart)-strlen(s)+1;
   if(ruqest.Referer.indexOf(s,datStart)==-1)
    {datEnd = ruqest.Referer.length();}
    else{    
       datEnd = ruqest.Referer.indexOf(s,datStart)-1;
      }
    ruqest.host_ur  = String(ruqest.Referer.substring(datStart, datEnd));
    Use_Serial.println ( ruqest.host_ur); // 串口打印给电脑

 
    Use_Serial.println( ruqest.postDate);
     
   if (hdulogin(ruqest) == 0) {   
      Use_Serial.println("WEB Login Success!");
    }
    else {  
      Use_Serial.println("WEB Login Fail!");
    }    
          return;  
  
  }

//3-5-5-3 网页认证发送HTTP请求
/*---------------------------------------------------------------*/
int hdulogin(struct http_request ruqest) {
  WiFiClient client;

  if (!client.connect(ruqest.host, ruqest.httpPort)) {
    Use_Serial.println("connection failed");
    return 1;
  }
  delay(10);
 
  if (ruqest.postDate.length() && ruqest.postDate != "0") {
    String data = (String)ruqest.postDate;
    int length = data.length();

/*
gzip：GNU压缩格式

compress：UNIX系统的标准压缩格式

deflate：是一种同时使用了LZ77和哈弗曼编码的无损压缩格式

identity：不进行压缩
*/
    String postRequest =
                         (String)("POST ") + "/"+ruqest.host_ur+" HTTP/1.1\r\n" +
                         "Host: " +ruqest.host + "\r\n" +
                         "Connection: Keep Alive\r\n" +
                         "Content-Length: " + length + "\r\n" +
                         "Accept:text/html,application/xhtml+xml,application/xml;*/*\r\n" +
                         "Origin: http://"+ruqest.host+"\r\n" +
                          "Upgrade-Insecure-Requests: 1"+"\r\n" +
                         "Content-Type: application/x-www-form-urlencoded;" + "\r\n" +
                         "User-Agent: zyzandESP8266\r\n" +
                        //  "Accept-Encoding: gzip, deflate"+"\r\n" +
                          "Accept-Encoding: identity"+"\r\n" +
                          "Accept-Language: zh-CN,zh;q=0.9"+"\r\n" +                                             
                         "\r\n" +
                         data + "\r\n";
//  String postDate = String("")+"action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&url=&username=+"+usr_name+"&password="+usr_pwd;
    client.print(postRequest);
    delay(600);
    //处理返回信息
    String line = client.readStringUntil('\n');
    Use_Serial.println(line);
    while (client.available() > 0) {
    Use_Serial.println(client.readStringUntil('\n'));
    line += client.readStringUntil('\n');
    
   // line += client.readStringUntil('\n');
    }

  //  http_html=line;
    
    client.stop();
    
    if (line.indexOf("时间") != -1 || line.indexOf("登陆") != -1) { //认证成功
      return 0;
    
    }
    else {
      return 2;
    }

  }
  client.stop();
  return 2;
}

//3-5-6 网页测试
void handleTest(){ 
     server.send(200, "text/html", http_html);
  }

//3-5-。。。 网页没有对应请求如何处理
void handleNotFound() {  

  String path = server.uri();  

  Serial.print("load url:");  

  Serial.println(path);  

  String contentType = getContentType(path);  

  String pathWithGz = path + ".gz";  

  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  

    if(SPIFFS.exists(pathWithGz))  

      path += ".gz";  

    File file = SPIFFS.open(path, "r");  

    size_t sent = server.streamFile(file, contentType);  

    file.close(); 

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message); 

    return;  

  }  

  String message = "File Not Found\n\n";  

  message += "URI: ";  

  message += server.uri();  

  message += "\nMethod: ";  

  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";  

  message += "\nArguments: ";  

  message += server.args();  

  message += "\n";  

  for ( uint8_t i = 0; i < server.args(); i++ ) {  

    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";  

  }  

  server.send ( 404, "text/plain", message );  

}  
// 解析请求的文件
/** 

 * 根据文件后缀获取html协议的返回内容类型 

 */  

String getContentType(String filename){  

  if(server.hasArg("download")) return "application/octet-stream";  

  else if(filename.endsWith(".htm")) return "text/html";  

  else if(filename.endsWith(".html")) return "text/html";  

  else if(filename.endsWith(".css")) return "text/css";  

  else if(filename.endsWith(".js")) return "application/javascript";  

  else if(filename.endsWith(".png")) return "image/png";  

  else if(filename.endsWith(".gif")) return "image/gif";  

  else if(filename.endsWith(".jpg")) return "image/jpeg";  

  else if(filename.endsWith(".ico")) return "image/x-icon";  

  else if(filename.endsWith(".xml")) return "text/xml";  

  else if(filename.endsWith(".pdf")) return "application/x-pdf";  

  else if(filename.endsWith(".zip")) return "application/x-zip";  

  else if(filename.endsWith(".gz")) return "application/x-gzip";  

  return "text/plain";  

}  




/************************* 4-2 服务器重连配置 *************************************/
 
void mqtt_reconnect() {//等待，直到连接上服务器


  if (!client.connected()){

        unsigned long preTick = millis();    
       int num;
 while (!client.connected()) {//如果没有连接上

       if (millis() - preTick < 10 ) continue;//等待10ms
        preTick = millis();
         num++;      

       if(num%100==0){
        PIN_Led_State = !PIN_Led_State;
        digitalWrite(PIN_Led, PIN_Led_State);
        Use_Serial.print(".");
        num=0;
        }
        
        server.handleClient(); 

    if (client.connect(((String)MQTT_MYNAME+SN).c_str(),MQTT_SSID,MQTT_PASSWORD)) {//接入时的用户名，尽量取一个很不常用的用户名
     
             client.subscribe(MQTT_MYTOPICR);//接收外来的数据时的intopic 
             client.subscribe(MQTT_YOURTOPICS);//接收外来的数据时的intopic 
       client.publish(MQTT_MYTOPICS,"hello world ");
             client.publish(MQTT_YOURTOPICR,"hello world ");
             Use_Serial.println("Connect succes mqtt!");//重新连接
            Use_Serial.println(client.state());//重新连接
             break;

    } else {
            server.handleClient(); 
      Use_Serial.println("connect failed!");//连接失败
      Use_Serial.println(client.state());//重新连接
      Use_Serial.println(" try connect again in 2 seconds");//延时2秒后重新连接
      delay(2000);
    }

     

  }

  }
}



 
/* 3 接收数据处理， 服务器回掉函数*/
void callback(char* topic, byte* payload, unsigned int length) {//用于接收数据
   //-----------------1数据解析-----------------------------//
  Use_Serial.print("Message arrived [");
  Use_Serial.print(topic);
  Use_Serial.print("] ");
   char  recmsg1[length+1];
   for (int i = length-1; i >=0; i--) {   
  recmsg1[i]=(char)payload[i];
  }  
   recmsg1[length]='\0';
   Serial.println(recmsg1); 
     
 //-------------2数据转换 char[]-String --------------------//
   
  String str(recmsg1); // char 转换String
  recstr=str;
  str="";
   
 //------------- 3开启动作执行标志位    --------------------//
  
/*------ W-ALL 数据总解析 动作执行 callback  执行一次---*/
   parseData(recstr);
 
}
 
/*************************** 4 服务器函数配置*****************************/
 
void Mqtt_message(){
 
    Use_Serial.println("----------Mqtt--------");

    Use_Serial.print("Name:");
    Use_Serial.println((String)MQTT_MYNAME);
     Use_Serial.println("pub_topic:");
     Use_Serial.println(MQTT_MYTOPICS);     
     Use_Serial.println("rec_topic:");
    Use_Serial.println(MQTT_MYTOPICR);  
        Use_Serial.println(MQTT_YOURTOPICS);
   
  }

/*-------------------------1 总解析数据控制----------------------------------*/
 /*
  *功能： 接收WIFI消息，总动作控制
  *输入： callback得到数据  String recstr; 
   输出： bool
 */
bool parseData( String  str) {
   
    // 2 WSB七彩灯控制             
   RGB_LED( str);
  
 
}

/*************************** 数据解析 ***********************************/
/*-------------------------1 WSBled彩灯控制----------------------------------*/
/**
     * @Desc 解析json
     * 有三种
     * 1.亮度控制页面(0-255)
     * {
     *     "t": 1,
     *     "cl": 102
     * }
     * 2.颜色控制页面 0-255
     * {
     *     "t": 2,
     *     "cr": 154,
     *     "cg": 147,
     *     "cb": 255
     * }
     * 3.呼吸灯控制页面(0关闭  1开启 )
     * {
     *    "t": 3,
     *    "cc": 1
     * }
   
     **/
//放在开头定义了
// int light;//亮度变化
 //int pwm;// 动态开始标志位
 
bool RGB_LED(String str){
   
  int type = StringtoHex(JsontoString(str,"ledmode"));//分割调用
  if(type==0)return false;
  switch(type){ 
    
       case t_bright://亮度
             light = StringtoInt(JsontoString(str,"cl"));//分割调用              
                    colorWipe(strip.Color(light,light,light), 0);// 点亮WSB彩灯  
                    Serial.println("Mode1: light!");                 
                  //  client.publish(rec_topic,"Mode1: light!");                              
                   break;  
                           
        case t_color:// 颜色
               
         red = StringtoHex(JsontoString(str,"cr"));//分割调用
         green = StringtoHex(JsontoString(str,"cg"));//分割调用
         blue =  StringtoHex(JsontoString(str,"cb"));//分割调用
 
                   if(red>0){old_red = 1;}else{old_red = 0;}
                   if(green>0){old_green = 1;}else{old_green = 0;}
                   if(blue>0){old_blue = 1;}else{old_blue = 0;}
                    
                 int_do_if();// 清空按键
//                json_rgb_pub(String.valueOf(red),String.valueOf(green),String.valueOf(blue));// 改变配对的灯
                     
                  colorWipe(strip.Color(red,green,blue), 0);// 点亮WSB彩灯
                  Serial.println("Mode2: colorRGB!");
//                  client.publish(rec_topic,"Mode2: colorRGB!");  
                  break;
      case t_frequency://彩虹动态
                 pwm = StringtoHex(JsontoString(str,"cc"));//分割调用
        while(pwm==1) {
                //    client.publish(rec_topic,"Mode3: rainbow!");       
                    rainbow(30) ;        
                    Serial.println("Mode3: rainbow!"); 
                                    
                    }    
                      break;
 
           case t_msg_longtouch:
                       recbegin = StringtoHex(JsontoString(str,"ledmsg_long"));//分割调用
                       led_state = StringtoHex(JsontoString(str,"change_Bled_sta"));//分割调用
                 break;
                  
           case t_you_hangs_on:
                      you_hands_on = StringtoHex(JsontoString(str,"my_hands_on"));//分割调用
                      
                 break;
            default:          
                break;   
              }
               return true;
  }



 
 
 /*------------------------- 解析基本函数----------------------------------*/
 
/*数说明： 从Json格式的String中，截取对应键值
输入：  String 数据帧 String 键名
输出：  int 类型的 键值
示例;
  String m=  "{\"ledmode\":2,\"cr\":ff,\"cg\":a,\"cb\":1}";
  int a=JsontoString(m,"ledmode");//分割调用
结果： a=2;
*/
String  JsontoString(String zifuchuan,String fengefu)
  {
  fengefu="\""+fengefu+"\"";
  int weizhi_KEY; //找查的位置
  int weizhi_DH;
  String temps;//临时字符串
 weizhi_KEY = zifuchuan.indexOf(fengefu);//找到位置
 temps=zifuchuan.substring( weizhi_KEY, weizhi_KEY+fengefu.length());
 if(temps!=fengefu) return "";// 解析失败 返回0 所以要判断这个字符和输入的是否一样
  
 temps=zifuchuan.substring( weizhi_KEY+fengefu.length(), zifuchuan.length());//打印取第一个字符
 weizhi_DH = weizhi_KEY+fengefu.length()+ temps.indexOf(',');//找到位置
 if( temps.indexOf(',')==-1){
  // weizhi_DH = weizhi_KEY+fengefu.length()+ temps.indexOf('}');//找到位置
 weizhi_DH = zifuchuan.length()-1;
  }
 temps="";
  temps=zifuchuan.substring( weizhi_KEY+fengefu.length()+1,  weizhi_DH);//打印取第一个字符
//  Serial.print(temps);
//  Serial.print("---");
   return temps;   //返回string字符
// return StringtoHex(temps);
  }
 
/*说明 String 转 16进制对应的10进制数
输入：  String
输出：  int
示例：
FF   255
ff   255
1    1
*/
int StringtoHex(String temps)
  {
  int l=0;
  int p=1;
  for (int i = temps.length()-1; i >=0; i--) {
   if(temps[i]=='a'||temps[i]=='b'||temps[i]=='c'||temps[i]=='d'||temps[i]=='e'||temps[i]=='f'){
     l+=((int)(temps[i]-'a')+10)*p;
    p*=16;
   }
   else if(temps[i]=='A'||temps[i]=='B'||temps[i]=='C'||temps[i]=='D'||temps[i]=='E'||temps[i]=='F'){
     l+=((int)(temps[i]-'A')+10)*p;
    p*=16;
   }
   else if(temps[i]=='0'||temps[i]=='1'||temps[i]=='2'||temps[i]=='3'||temps[i]=='4'||temps[i]=='5'||temps[i]=='6'||temps[i]=='7'||temps[i]=='8'||temps[i]=='9')
   {
      l+=(int)(temps[i]-'0')*p;
    p*=16;
     
    }
    
 }
 // Serial.println(l);
  return  l;
     
    }
 
    /*
说明 String 转 10进制对应的10进制数
输入：  String
输出：  int
示例：
1023  1023
1    1
*/
int StringtoInt(String temps)
  {
  int l=0;
  int p=1;
  for (int i = temps.length()-1; i >=0; i--) {
   if(temps[i]=='0'||temps[i]=='1'||temps[i]=='2'||temps[i]=='3'||temps[i]=='4'||temps[i]=='5'||temps[i]=='6'||temps[i]=='7'||temps[i]=='8'||temps[i]=='9')
   {
      l+=(int)(temps[i]-'0')*p;
    p*=10;
     
    }
    
 }
 // Serial.println(l);
  return  l;
     
    }
void mqtt_handle(){
     if( my_hands_on==1)
       {
          //------2-1 长短按键----------
               t3=millis()-t1;// 计时结束 
               if(t3>2000){    // 短按动作 发射短消息
               sendbegin=2;   // 长安动作 发射长安消息  
         }
     if(sendbegin=2){// 以免短按当成长按误触发
             sendtime++;
              if(sendtime<=1){
                if(client.connected()){ client.publish(MQTT_MYTOPICS," {\"ledmode\":5,\"my_hands_on\":1}");   }  
                               }
                      }            
         }   
     else{
           //------ 1-2 告诉B我的手是否在灯上----------
           sendtime++;
           if(sendtime<=1){
           if(client.connected()){ client.publish(MQTT_MYTOPICS," {\"ledmode\":5,\"my_hands_on\":0}");   }
            }
              //------2-2 长短按键---------- 
      }
 
        // 2 循环动作
     do_if_rainbow(2);       
   
     
  
  }

//------------------------------------------- void setup() ------------------------------------------

void setup() {
    Use_Serial.begin(115200); //串口初始化
    get_espid();              //获取自身唯一ID
    LED_Int();                //管脚初始化
    Button_Int();             //按钮初始化     
    loadConfig();             //读取保存信息 WIFI账号和密码
    
    LEDRGB_Int_WSB();
    
   client.setServer(MQTT_SERVER, PORT);
   client.setCallback(callback);
    Mqtt_message();
    
  
  // 1 不按，直接连接WIFI  慢闪模式
   // wifi_Init(); // 3次尝试
  // WiFi.begin(config_wifi.stassid, config_wifi.stapsw);
   
   //2短按2S但小于5S 开启WIFI系统，可进入配网模式 快闪模式
   // WiFi.mode(WIFI_AP);
  //  SET_AP();       // 建立WIFI
 //   SPIFFS.begin(); // ESP8266自身文件系统 启用 此方法装入SPIFFS文件系统。必须在使用任何其他FS API之前调用它。如果文件系统已成功装入，则返回true，否则返回false。
 //   Server_int();  // HTTP服务器开启
   
    waitKey();                //等待按键动作 
     
}


//------------------------------------------- void void loop()  ------------------------------------------
void mqtt_handle();
void loop() 
{

   // 等待网页请求
   server.handleClient();  

   // 等待WIFI连接
   if(WiFi.status() == WL_CONNECTED){  

      mqtt_reconnect();//确保连上服务器，否则一直等待。
   client.loop();//MUC接收数据的主循环函数。 

       mqtt_handle();
       
  
                   }
   else {
              //WIFI断开，重新连接WIFI
               
     if(workmode==0)
       {wifi_Init();}      
     else if(workmode==1){                    
           unsigned long preTick = millis();
           int num = 0;
               while(WiFi.status() != WL_CONNECTED&&workmode==1){
                  server.handleClient(); 
              if (millis() - preTick < 10 ) continue;//等待10ms
              preTick = millis();
              num++;
              if (num % led_sudu == 0 )  //50*10=500ms=0.5s 反转一次
              {
                PIN_Led_State = !PIN_Led_State;
                digitalWrite(PIN_Led, PIN_Led_State);
                  colorWipe(strip.Color(PIN_Led_State*254, PIN_Led_State*254, PIN_Led_State*254), 1); 
                Serial.print(".");
              }
               }
         
          }

       
   }    

}




