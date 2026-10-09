#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ====== CHANGE THESE THREE ======
const char* WIFI_SSID     = "Oscar";
const char* WIFI_PASSWORD = "oscar@0806";
const char* MQTT_SERVER   = "pi ip";
// ================================

const int MQTT_PORT = 1883;
const char* MQTT_TOPIC_PUB = "ventra/readings";
const char* MQTT_TOPIC_SUB = "ventra/command";

const int PIN_GAS=36, PIN_FLAME=34, PIN_DHT=4, PIN_RELAY=5, PIN_BUZZ=25;
const int PIN_LED_G=26, PIN_LED_Y=27, PIN_LED_R=14;

const int FLAME_TRIGGER=1500, GAS_WARN=200, GAS_ALARM=500;


#define RELAY_ON  LOW
#define RELAY_OFF HIGH

DHT dht(PIN_DHT, DHT11);
WiFiClient espClient;
PubSubClient mqtt(espClient);
int gasBaseline=0;
unsigned long lastPublish=0;

void setLEDs(bool g,bool y,bool r){
  digitalWrite(PIN_LED_G,g);
  digitalWrite(PIN_LED_Y,y);
  digitalWrite(PIN_LED_R,r);
}

void mqttCallback(char* topic,byte* payload,unsigned int length){
  String m;
  for(unsigned int i=0;i<length;i++) m+=(char)payload[i];
  if(m=="FAN_ON")  digitalWrite(PIN_RELAY,RELAY_ON);
  if(m=="FAN_OFF") digitalWrite(PIN_RELAY,RELAY_OFF);
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== VENTRA starting ===");

  pinMode(PIN_RELAY,OUTPUT); digitalWrite(PIN_RELAY,RELAY_OFF);
  pinMode(PIN_BUZZ,OUTPUT);  digitalWrite(PIN_BUZZ,LOW);
  pinMode(PIN_LED_G,OUTPUT);
  pinMode(PIN_LED_Y,OUTPUT);
  pinMode(PIN_LED_R,OUTPUT);
  setLEDs(false,false,false);
  dht.begin();

  // ===== RELAY SELF-TEST =====
  Serial.println(">> RELAY TEST: ON (HIGH)");
  digitalWrite(PIN_RELAY, RELAY_ON);
  delay(3000);
  Serial.println(">> RELAY TEST: OFF (LOW)");
  digitalWrite(PIN_RELAY, RELAY_OFF);
  delay(3000);

  Serial.println("Warming up gas sensor (30s)...");
  setLEDs(true,true,true);
  delay(30000);

  long sum=0;
  for(int i=0;i<50;i++){ sum+=analogRead(PIN_GAS); delay(20); }
  gasBaseline=sum/50;
  Serial.println("Gas baseline = "+String(gasBaseline));
  setLEDs(true,false,false);

  Serial.print("WiFi trying");
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  int w=0;
  while(WiFi.status()!=WL_CONNECTED && w<10){ delay(500); Serial.print("."); w++; }
  if(WiFi.status()==WL_CONNECTED){
    Serial.println(" connected. IP: "+WiFi.localIP().toString());
  } else {
    Serial.println(" not connected - continuing without WiFi (OK for testing)");
  }

  mqtt.setServer(MQTT_SERVER,MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  Serial.println("=== Setup done, sensors starting ===");
}

void loop(){
  static unsigned long lastMqttTry=0;
  if(WiFi.status()==WL_CONNECTED){
    if(mqtt.connected()){
      mqtt.loop();
    } else if(millis()-lastMqttTry>10000){
      lastMqttTry=millis();
      if(mqtt.connect("ventra-esp32")) {
        mqtt.subscribe(MQTT_TOPIC_SUB);
      }
    }
  }

  int gasRaw=analogRead(PIN_GAS);
  int flameRaw=analogRead(PIN_FLAME);
  float temp=dht.readTemperature();
  float hum=dht.readHumidity();
  if(isnan(temp))temp=0;
  if(isnan(hum))hum=0;

  int gasDelta=gasRaw-gasBaseline;
  bool flame=(flameRaw<FLAME_TRIGGER);

  
  String state="Normal";
  if(flame||(gasDelta>GAS_ALARM&&temp>40)){
    state="Fire";
    digitalWrite(PIN_RELAY,RELAY_OFF);   // fire = fan OFF
    digitalWrite(PIN_BUZZ,HIGH);
    setLEDs(false,false,true);
  }
  else if(gasDelta>GAS_ALARM){
    state="GAS_HIGH";
    digitalWrite(PIN_RELAY,RELAY_ON);    // vent
    digitalWrite(PIN_BUZZ,HIGH);
    setLEDs(false,false,true);
  }
  else if(gasDelta>GAS_WARN){
    state="Gas";
    digitalWrite(PIN_RELAY,RELAY_ON);    // vent
    digitalWrite(PIN_BUZZ,LOW);
    setLEDs(false,true,false);
  }
  else{
    state="Normal";
    digitalWrite(PIN_RELAY,RELAY_OFF);
    digitalWrite(PIN_BUZZ,LOW);
    setLEDs(true,false,false);
  }

  if(millis()-lastPublish>2000){
    lastPublish=millis();
    StaticJsonDocument<256> doc;
    doc["gas"]=gasRaw;
    doc["gas_delta"]=gasDelta;
    doc["flame"]=flame;
    doc["flame_raw"]=flameRaw;
    doc["temp"]=temp;
    doc["hum"]=hum;
    doc["state"]=state;
    char buf[256];
    serializeJson(doc,buf);
    if(mqtt.connected()) mqtt.publish(MQTT_TOPIC_PUB,buf);
    Serial.println(buf);
  }
  delay(100);
}