#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRutils.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiMulti.h>
#include <Wire.h>
#include <IRsend.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <RTClib.h>

//#include <LiquidCrystal_I2C.h>
const uint16_t kIrLed = 17;  // The ESP GPIO pin to use that controls the IR LED.
IRac ac(kIrLed);  // Create a A/C object using GPIO to sending messages with.
RTC_DS1307 rtc;
const uint16_t kIrFanLed = 32;
IRsend fanSend(kIrFanLed);

#define DHTPIN 25     // GPIO pin connected to the DHT sensor 
#define DHTTYPE DHT11     // DHT 11

DHT dht(DHTPIN, DHTTYPE);
//LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 columns and 2 rows

const char* ssid = "Tintra";
const char* password = "27112002";

const char* serverURL="http://localhost:8000/emergency?deviceId=";
const char* serverURL1="http://localhost:8000/fanId=";
WebServer WebServer(80);

String user_id;
String device_id;

const char* mqtt_server = "b128048a90a847289dcd30d3ff68e5fc.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "user1"; //User
const char* mqtt_password = "User123@"; //Password

//--------------------------------------------------
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

//------------Connect to MQTT Broker-----------------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID =  "ESPClient-";
    clientID += String(random(0xffff),HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("esp32/client");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//-----Call back Method for Receiving MQTT massage---------
void callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for(int i=0; i<length;i++) incommingMessage += (char)payload[i];
  Serial.println("Massage arived ["+String(topic)+"]"+incommingMessage);
}
//-----Method for Publishing MQTT Messages---------
void publishMessage(const char* topic, String payload, boolean retained){
  if(client.publish(topic,payload.c_str(),true))
    Serial.println("Message published ["+String(topic)+"]: "+payload);
}

void SetDefaultProfile()
{
  ac.next.protocol = decode_type_t::DAIKIN;  // Set a protocol to use.
  ac.next.model = 1;  // Some A/Cs have different models. Try just the first.
  ac.next.mode = stdAc::opmode_t::kCool;  // Run in cool mode initially.
  ac.next.celsius = true;  // Use Celsius for temp units. False = Fahrenheit
  ac.next.degrees = 25;  // 25 degrees.
  ac.next.fanspeed = stdAc::fanspeed_t::kMedium;  // Start the fan at medium.
  ac.next.swingv = stdAc::swingv_t::kOff;  // Don't swing the fan up or down.
  ac.next.swingh = stdAc::swingh_t::kOff;  // Don't swing the fan left or right.
  ac.next.light = false;  // Turn off any LED/Lights/Display that we can.
  ac.next.beep = false;  // Turn off any beep from the A/C if we can.
  ac.next.econo = false;  // Turn off any economy modes if we can.
  ac.next.filter = false;  // Turn off any Ion/Mold/Health filters if we can.
  ac.next.turbo = false;  // Don't use any turbo/powerful/etc modes.
  ac.next.quiet = false;  // Don't use any quiet/silent/etc modes.
  ac.next.sleep = -1;  // Don't set any sleep time or modes.
  ac.next.clean = false;  // Turn off any Cleaning options if we can.
  ac.next.clock = -1;  // Don't set any current time if we can avoid it.
  ac.next.power = false;  // Initially start with the unit off.
}
void HandleSetRequest() {
  if (WebServer.hasArg("plain")) {
    // Parse JSON data
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, WebServer.arg("plain"));
    const char* rq_device_id=jsonDocument["deviceId"];
    const char* rq_user_id=jsonDocument["userId"];
    device_id=String(rq_device_id);
    user_id=String(rq_user_id);
    if (error) {
      WebServer.send(400, "application/json", "Error parsing JSON data");
    } else {
      Serial.println(device_id);
      Serial.println(user_id);
    }
  }  WebServer.send(200, "application/json", "message:SET OK");
}

bool CompareString(const char* str1,const char*str2){
  return strcmp(str1, str2) == 0;
}

bool CompareString(String str1,const char*str2){
  return str1.equals(str2);
}

bool SetFan(const char* FM)
{
  if(CompareString(FM,"LOW"))
  {
    ac.next.fanspeed=stdAc::fanspeed_t::kLow;
    return true;
  }
  else if(CompareString(FM,"MEDIUM"))
  {
    ac.next.fanspeed=stdAc::fanspeed_t::kMedium;
    return true;
  }
  else if(CompareString(FM,"HIGH"))
  {
    ac.next.fanspeed=stdAc::fanspeed_t::kHigh;
    return true;
  }
  return false;
}

bool SetBrand(const char* B)
{
  if(CompareString(B,"SHARP"))
  {
    ac.next.protocol=decode_type_t::SHARP_AC;
    return true;
  }
  if(CompareString(B,"TOSHIBA"))
  {
    ac.next.protocol=decode_type_t::TOSHIBA_AC;
    return true;
  }
  if(CompareString(B,"SAMSUNG"))
  {
    ac.next.protocol=decode_type_t::SAMSUNG_AC;
    return true;
  }
  if(CompareString(B,"LG"))
  {
    ac.next.protocol=decode_type_t::LG;
    return true;
  }
  if(CompareString(B,"MITSUBISHI"))
  {
    ac.next.protocol=decode_type_t::MITSUBISHI_AC;
    return true;
  }
  if(CompareString(B,"PANASONIC"))
  {
    ac.next.protocol=decode_type_t::PANASONIC_AC;
    return true;
  }
  if(CompareString(B,"DAIKIN"))
  {
    ac.next.protocol=decode_type_t::DAIKIN;
    return true;
  }

  return false;
}

bool SetStatus(const char* status)
{
  if(CompareString(status,"ON"))
  {
    ac.next.power=true;
    return true;
  }
  else if(CompareString(status,"OFF"))
  {
    ac.next.power=false;
    return true;
  }
  else return false;
}

void HandleSendSignalRequest()
{
  Serial.println(device_id);
  Serial.println(user_id);
  if (WebServer.hasArg("plain")) {
    // Parse JSON data
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, WebServer.arg("plain"));
    const char* rq_device_id=jsonDocument["deviceId"];
    const char* rq_user_id=jsonDocument["userId"];
    const char* rq_fan=jsonDocument["fan"];
    const char* rq_brand=jsonDocument["brand"];
    const char*rq_status=jsonDocument["status"];
    int rq_temp=jsonDocument["temp"];
    Serial.println(rq_device_id);
    Serial.println(rq_user_id);
    Serial.println(rq_fan);
    Serial.println(rq_temp);
    Serial.println(rq_brand);
    if (error) {
      WebServer.send(400, "application/json", "Error parsing JSON data");
    } else {
      if(CompareString(device_id,rq_device_id)  && CompareString(user_id,rq_user_id) )
      {
        if(!SetFan(rq_fan))
        {
          WebServer.send(400,"application/json","message: Wrong Request");
          return;
        }
        if(!SetBrand(jsonDocument["brand"]))
        {
          WebServer.send(400,"application/json","message: Wrong Request");
          return;
        }
        if(!SetStatus(rq_status))
        {
          WebServer.send(400,"application/json","message: Wrong Request");
          return;
        }
        ac.next.degrees=int(jsonDocument["temp"]);
        Serial.println("Protocol " + String(ac.next.protocol) + " / " +
                      typeToString(ac.next.protocol) + " is supported.");
        
        ac.sendAc();   //Have the IRac class create and send a message.
        Serial.println("Sent!");
        delay(500);
        ac.sendAc();   //Have the IRac class create and send a message.
        Serial.println("Sent!");
        delay(500);
        ac.sendAc();   //Have the IRac class create and send a message.
        Serial.println("Sent!");
        delay(500);
      }
      else 
      {
        WebServer.send(400,"application/json","message : Wrong!");
        return;
      }
    }
  }  WebServer.send(200, "application/json", "message : SET OK");
}

void HandleStatsRequest() {
  // Đọc temperature and humidity từ DHT
  float tempC = dht.readTemperature();
  float humidity = dht.readHumidity();
  // JSON response
  String jsonResponse = "{\"device_id\":\"" + device_id + "\",\"temp\":" + String(tempC) + ",\"humidity\":" + String(humidity) + "}"; 
  WebServer.send(200, "application/json", jsonResponse);
}
//////////////////////////// FOR FAN /////////////////////////////////////////////////

String fan_id;

void HandleSetFanRequest() {
  if (WebServer.hasArg("plain")) {
    // Parse JSON data
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, WebServer.arg("plain"));
    const char* rq_fan_id=jsonDocument["fanId"];
    const char* rq_user_id=jsonDocument["userId"];
    fan_id=String(rq_fan_id);
    user_id=String(rq_user_id);
    if (error) {
      WebServer.send(400, "application/json", "Error parsing JSON data");
    } else {
      Serial.println(fan_id);
      Serial.println(user_id);
    }
  }  WebServer.send(200, "application/json", "message:SET OK");
}

bool SetFanSpeed(const char* fanSpeed)
{
  if(CompareString(fanSpeed, "INCREASE"))
  {
    fanSend.sendNEC(0xFFE01F);
    Serial.println("Sent fanspeed!!");
    return true;
  }
  else if(CompareString(fanSpeed,"DECREASE"))
  {
    fanSend.sendNEC(0xFF906F);
    Serial.println("Sent fanspeed!!");
    return true;
  }
  else if(CompareString(fanSpeed,"NONE"))
  {
    return false;
  }
  else return false;
}

bool SetFanStatus(const char* status)
{
  if(CompareString(status,"ON"))
  {

    return true;
  }
  else if(CompareString(status,"OFF"))
  {

    return true;
  }
  else return false;
}

void HandleSendFanSignalRequest()
{
  Serial.println(fan_id);
  Serial.println(user_id);
  if (WebServer.hasArg("plain")) {
    // Parse JSON data
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, WebServer.arg("plain"));
    const char* rq_fan_id=jsonDocument["fanId"];
    const char* rq_user_id=jsonDocument["userId"];
    const char* rq_fan_speed=jsonDocument["fanSpeed"];
    const char* rq_swing=jsonDocument["swing"];
    const char* rq_light=jsonDocument["light"];
    const char*rq_status=jsonDocument["status"];
    // int rq_temp=jsonDocument["temp"];
    Serial.println(rq_fan_id);
    // Serial.println(fan_id);
    Serial.println(rq_user_id);
    // Serial.println(user_id);
    Serial.println(rq_fan_speed);
    Serial.println(rq_swing);
    Serial.println(rq_light);
    Serial.println(rq_status);
    if (error) {
      WebServer.send(400, "application/json", "Error parsing JSON data");
    } else {

      if(CompareString(fan_id,rq_fan_id)  && CompareString(user_id,rq_user_id) )
      {
        // if(!SetFanSpeed(rq_fan))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        // if(!SetBrand(jsonDocument["brand"]))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        // if(!SetFanSpeed(rq_fan_speed))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        if(!SetFanStatus(rq_status))
        {
          WebServer.send(400,"application/json","message: Wrong Request");
          return;
        }
        fanSend.sendNEC(0xFF629D);
        Serial.println("Sent!!");
        delay(2000);
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // delay(2000);
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // ac.next.degrees=int(jsonDocument["temp"]);
        // Serial.println("Protocol " + String(ac.next.protocol) + " / " +
        //               typeToString(ac.next.protocol) + " is supported.");
        
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
      }
      else 
      {
        WebServer.send(400,"application/json","message : Wrong!");
        return;
      }
    }
  }  WebServer.send(200, "application/json", "message : SET OK");
}

void HandleSendFanSpeedSignalRequest()
{
  Serial.println(fan_id);
  Serial.println(user_id);
  if (WebServer.hasArg("plain")) {
    // Parse JSON data
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, WebServer.arg("plain"));
    const char* rq_fan_id=jsonDocument["fanId"];
    const char* rq_user_id=jsonDocument["userId"];
    const char* rq_fan_speed=jsonDocument["fanSpeed"];
    // const char* rq_swing=jsonDocument["swing"];
    // const char* rq_light=jsonDocument["light"];
    // const char*rq_status=jsonDocument["status"];
    // int rq_temp=jsonDocument["temp"];
    Serial.println(rq_fan_id);
    // Serial.println(fan_id);
    Serial.println(rq_user_id);
    // Serial.println(user_id);
    Serial.println(rq_fan_speed);
    // Serial.println(rq_swing);
    // Serial.println(rq_light);
    // Serial.println(rq_status);
    if (error) {
      WebServer.send(400, "application/json", "Error parsing JSON data");
    } else {

      if(CompareString(fan_id,rq_fan_id)  && CompareString(user_id,rq_user_id) )
      {
        // if(!SetFanSpeed(rq_fan))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        // if(!SetBrand(jsonDocument["brand"]))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        // if(!SetFanSpeed(rq_fan_speed))
        // {
        //   WebServer.send(400,"application/json","message: Wrong Request");
        //   return;
        // }
        if(!SetFanSpeed(rq_fan_speed))
        {
          WebServer.send(400,"application/json","message: Wrong Request");
          return;
        }

        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // fanSend.sendNEC(0xFF629D);
        // Serial.println("Sent!!");
        // ac.next.degrees=int(jsonDocument["temp"]);
        // Serial.println("Protocol " + String(ac.next.protocol) + " / " +
        //               typeToString(ac.next.protocol) + " is supported.");
        
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
        // ac.sendAc();   //Have the IRac class create and send a message.
        // Serial.println("Sent!");
        // delay(500);
      }
      else 
      {
        WebServer.send(400,"application/json","message : Wrong!");
        return;
      }
    }
  }  WebServer.send(200, "application/json", "message : SET OK");
}

void HandleFanStatsRequest() {
  // Đọc temperature and humidity từ DHT
  float tempC = dht.readTemperature();
  float humidity = dht.readHumidity();
  // JSON response
  String jsonResponse = "{\"fan_id\":\"" + fan_id + "\",\"temp\":" + String(tempC) + ",\"humidity\":" + String(humidity) + "}"; 
  WebServer.send(200, "application/json", jsonResponse);
}
////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  fanSend.begin();
  Serial.begin(9600);
  Serial.print("hello");
  delay(200);
  //Thiết lập trạng thái mặc định của AC
  SetDefaultProfile();
  // Kết nối đến mạng WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Khởi tạo DHT11 và LCD
  dht.begin();
  // lcd.init();
  // lcd.backlight();
  // lcd.setCursor(0, 0);
  // lcd.print("Temp:       C");
  // lcd.setCursor(0, 1);
  // lcd.print("Humidity:   %");
  // Bắt đầu máy chủ trên cổng 80

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  WebServer.on("/api/device/set",HTTP_POST,HandleSetRequest);
  WebServer.on("/api/device/sendsignal",HTTP_POST,HandleSendSignalRequest);
  WebServer.on("/api/device/stats", HTTP_GET, HandleStatsRequest);

  WebServer.on("/api/fan/set", HTTP_POST, HandleSetFanRequest);
  WebServer.on("/api/fan/sendsignal",HTTP_POST,HandleSendFanSignalRequest);
  WebServer.on("/api/fan/sendspeedsignal",HTTP_POST,HandleSendFanSpeedSignalRequest);
  WebServer.on("/api/fan/stats", HTTP_GET, HandleFanStatsRequest);

  Serial.println("Hello");
  WebServer.begin();
}

void Emergency()
{
  float tempC = dht.readTemperature();
  if(tempC>60)
  {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverURL + device_id);  // Specify the URL of the server

      int httpResponseCode = http.GET();  // Send the GET request

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String response = http.getString();  // Get the response payload
        Serial.println("Response: " + response);
      } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    }
    delay(5000);
  }
}

char buffer[20];

void loop() {
  WebServer.handleClient();
  
  // Đọc temperature và humidity từ DHT
  float tempC = dht.readTemperature();
  float humidity = dht.readHumidity();
  // Emergency();
  if (!client.connected()) {
    Serial.println('.');
    reconnect();

  }
  delay(5000);
  client.loop();
  ///////////////////////////////////////////////////////////////////////////
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    DateTime now = rtc.now();
    // sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    unsigned long epochTime = now.unixtime();
    String timeString = String(buffer); // Chuyển đổi buffer thành String
    DynamicJsonDocument doc(1024);
    doc["h1"]=h;
    doc["te1"]=t;
    doc["sensor_id"]="sensor1";
    doc["t1"] = String(epochTime);

    char mqtt_message[128];
    serializeJson(doc,mqtt_message);
    publishMessage("esp32/dht22", mqtt_message, true);


    // check if any reads failed
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT22 sensor!");
    }
}