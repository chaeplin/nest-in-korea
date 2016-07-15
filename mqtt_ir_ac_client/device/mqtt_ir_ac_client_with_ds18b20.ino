#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <lgWhisen.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DallasTemperature.h>

#define IR_TX_PIN 4
#define AC_CONF_TYPE 1    // 0: tower, 1: wall
#define AC_CONF_HEATING 0   // 0: cooling, 1: heating
#define AC_DEFAULT_TEMP 27  // temp 18 ~ 30
#define AC_DEFAULT_FLOW 1   // fan speed 0: low, 1: mid, 2: high

#define ONE_WIRE_BUS 12
#define TEMPERATURE_PRECISION 12
#define REPORT_INTERVAL 5000 // in msec

#include "/usr/local/src/rpi2_setting.h"

/*
  #define WIFI_SSID "xxxxxxx"
  #define WIFI_PASSWORD "xxxxxxx"

  #define MQTT_FINGERPRINT "xx xx xx xx xx xx"
  #define MQTT_SERVER_CN "xxxxxxx"
  #define MQTT_SERVER { 192, 168, 10, 10 }
  #define MQTT_USER "xxxxx"
  #define MQTT_PASS "xxxxxxxx"
*/

char* subscribe_topic = "cool/LIVING_ROOM/set";
char* reporting_topic = "status/cool/LIVING_ROOM";

volatile struct
{
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  uint8_t prev_mode;
  uint8_t prev_temp;
  uint8_t prev_flow;  
  float tempCinside;
  bool haveData;
} ir_data;

IPAddress mqtt_server = MQTT_SERVER;

String clientName;
long lastReconnectAttempt = 0;

unsigned int localPort = 12390;
const int timeZone = 9;
time_t prevDisplay = 0;

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length);

//
WiFiClientSecure sslclient;
PubSubClient client(mqtt_server, 8883, callback, sslclient);
lgWhisen lgWhisen;
WiFiUDP udp;

// ds18b20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;
bool bDalasison;
bool bDalasstarted;
unsigned long startMills;


bool ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payloadtosend, bool retain = false)
{
  unsigned int msg_length = payloadtosend.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payloadtosend.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, retain))
  {
    free(p);
    client.loop();

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" published");

    return true;

  } else {
    free(p);
    client.loop();

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" publish failed");

    return false;
  }
}

void ICACHE_RAM_ATTR sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root    = jsonBuffer.createObject();
  root["ac_mode"]     = ir_data.ac_mode;
  root["ac_temp"]     = ir_data.ac_temp;
  root["ac_flow"]     = ir_data.ac_flow;
  root["tempCinside"] = ir_data.tempCinside;
  String json;
  root.printTo(json);

  sendmqttMsg(reporting_topic, json, false);
}

void ICACHE_RAM_ATTR parseMqttMsg(String receivedpayload, String receivedtopic)
{
  char json[] = "{\"ac_mode\":0,\"ac_temp\":27,\"ac_flow\":1}";

  receivedpayload.toCharArray(json, 150);
  StaticJsonBuffer<150> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success())
  {
    return;
  }

  if (receivedtopic == subscribe_topic)
  {
    if (root.containsKey("ac_cmd"))
    {
      ir_data.ac_mode = root["ac_cmd"];
    }

    if (root.containsKey("ac_temp"))
    {
      ir_data.ac_temp = root["ac_temp"];
    }

    if (root.containsKey("ac_flow"))
    {
      ir_data.ac_flow = root["ac_flow"];
    }

    if (ir_data.ac_mode != ir_data.prev_mode)
    {
      ir_data.prev_mode = ir_data.ac_mode;
      ir_data.haveData  = true;
    }

    if (ir_data.ac_temp != ir_data.prev_temp || ir_data.ac_flow != ir_data.prev_flow)
    {
      ir_data.prev_temp = ir_data.ac_temp;
      ir_data.prev_flow = ir_data.ac_flow;

      if (ir_data.ac_mode == 1)
      {
        ir_data.haveData  = true;
      }
      else
      {
        sendCheck();
      }
    }
  }
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (unsigned int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }

  Serial.print("[MQTT] intopic : ");
  Serial.print(receivedtopic);
  Serial.print(" payload: ");
  Serial.println(receivedpayload);

  parseMqttMsg(receivedpayload, receivedtopic);
}

bool verifytls()
{
  Serial.print("[MQTT] tls connecting to ");
  Serial.println(mqtt_server);
  if (!sslclient.connect(mqtt_server, 8883))
  {
    Serial.println("[MQTT] tls connection failed");
    return false;
  }

  if (sslclient.verify(MQTT_FINGERPRINT, MQTT_SERVER_CN))
  {
    Serial.println("[MQTT] tls certificate matches");
    sslclient.stop();
    return true;
  }
  else
  {
    Serial.println("[MQTT] tls certificate doesn't match");
    sslclient.stop();
    return false;
  }
}

boolean reconnect()
{
  if (!client.connected())
  {
    if (verifytls())
    {
      if (client.connect((char*) clientName.c_str(), MQTT_USER, MQTT_PASS))
      {
        client.subscribe(subscribe_topic);
        client.loop();
        Serial.println("[MQTT] mqtt connected");
      }
      else
      {
        Serial.print("[MQTT] mqtt failed, rc=");
        Serial.println(client.state());
      }
    }
  }
  return client.connected();
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println();
    Serial.print("[WIFI] Connecting to ");
    Serial.println(WIFI_SSID);

    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        Serial.println();
        Serial.println("[WIFI] Could not connect to WIFI, restarting...");
        Serial.flush();
        ESP.restart();
        delay(200);
      }
    }

    Serial.println();
    Serial.print("[WIFI] connected");
    Serial.print(" --> IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting....... ");
  
  lastReconnectAttempt = 0;
  bDalasstarted        = false;
  bDalasison           = false;

  ir_data.ac_mode     = 0;
  ir_data.ac_temp     = AC_DEFAULT_TEMP;
  ir_data.ac_flow     = AC_DEFAULT_FLOW;

  ir_data.prev_mode   = 0;
  ir_data.prev_temp   = AC_DEFAULT_TEMP;
  ir_data.prev_flow   = AC_DEFAULT_FLOW;
  
  ir_data.haveData    = false;

  lgWhisen.setActype(AC_CONF_TYPE);
  lgWhisen.setHeating(AC_CONF_HEATING);
  lgWhisen.setTemp(ir_data.ac_temp);
  lgWhisen.setFlow(ir_data.ac_flow);
  lgWhisen.setIrpin(IR_TX_PIN);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // ntp update
  udp.begin(localPort);
  if (timeStatus() == timeNotSet)
  {
    Serial.println("[NTP] get ntp time");
    setSyncProvider(getNtpTime);
    delay(500);
  }

  // start with ac power down
  lgWhisen.power_down();

  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) {
    Serial.println("Unable to find address for Device 0");
  }
  else
  {
    sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
    sensors.requestTemperatures();
    ir_data.tempCinside = sensors.getTempC(insideThermometer);
    sensors.setWaitForConversion(false);
  
    if ( ir_data.tempCinside < -30 ) {
      Serial.println("Failed to read from DS18B20 sensor!");
    }
    else
    {
      bDalasison = true;
    }
  }
  reconnect();
  startMills = millis();
}

void loop()
{
  if (now() != prevDisplay)
  {
    prevDisplay = now();
    if (timeStatus() == timeSet)
    {
      digitalClockDisplay();
    }
    else
    {
      Serial.println("[TIME] time not set");
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      long now = millis();
      if (now - lastReconnectAttempt > 5000)
      {
        lastReconnectAttempt = now;
        if (reconnect())
        {
          lastReconnectAttempt = 0;
        }
      }
    }
    else
    {
      // ac change, ir tx
      if (ir_data.haveData)
      {
        lgWhisen.setTemp(ir_data.ac_temp);
        lgWhisen.setFlow(ir_data.ac_flow);

        switch (ir_data.ac_mode)
        {
          // ac power down
          case 0:
            Serial.println("[IR] -----> AC Power Down");
            lgWhisen.power_down();
            break;

          // ac on
          case 1:
            Serial.println("[IR] -----> AC Power On");
            lgWhisen.activate();
            break;

          default:
            break;
        }
        sendCheck();
        ir_data.haveData = false;
      }

      if (bDalasstarted && bDalasison) {
        if (millis() > (startMills + (750 / (1 << (12 - TEMPERATURE_PRECISION))))) {
          ir_data.tempCinside = sensors.getTempC(insideThermometer);
          bDalasstarted = false;
        }
      }

      if (((millis() - startMills) > REPORT_INTERVAL) && bDalasison) {
        sendCheck();
        sensors.requestTemperatures();
        bDalasstarted = true;
        startMills = millis();
      }

      client.loop();
    }
  }
  else
  {
    wifi_connect();
  }
}


String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(mqtt_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress & address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void digitalClockDisplay()
{
  Serial.print("[TIME] ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.println(year());
}

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// end of file
