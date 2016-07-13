// nest irbot by chaeplin@gmail.com

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <lgWhisen.h>
#include <IRremoteESP8266.h>
#include <WiFiUdp.h>

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/nest_setting.h"
#include "/usr/local/src/slack_setting.h"

//#define WIFI_SSID "xxxxxxx"
//#define WIFI_PASSWORD "xxxxxxxx"
//#define NEST_API_TOKEN "c.xxxxxxxx"
//#define NEST_DEVICE_ID "xxxxxx_xxxxxxx-xxxxxx"

#define NEST_API_URL "developer-api.nest.com"
#define NEST_API_PORT 443
#define NEST_API_FINGERPRINT "88 A0 F6 B1 7D 50 4E D7 8B 14 11 CC 9E 7F 4D 4F 08 6B 58 3C"
#define NEST_FIREBASE_FINGERPRINT "AF F8 71 3D D8 D2 67 45 A2 DF 61 95 72 99 70 45 E9 4F 25 16"

/*
  For REST and REST Streaming calls, each access token has a limited number of calls.
  Data rate limits apply to read/write calls via REST, and on read calls via REST Streaming.
  To avoid errors, we recommend you limit requests to one call per minute, maximum.
*/
#define RETRIVE_INTERVAL 60000 // in msec

// SLACK
//#define SLACK_IN_WEBHOOK "xxxx/xxxx/xxxxx"
#define SLACK_API_FINGERPRINT "AB F0 5B A9 1A E0 AE 5F CE 32 2E 7C 66 67 49 EC DD 6D 6A 38"
#define SLACK_API_URL "hooks.slack.com"

// AC
#define IR_RX_PIN 14
#define IR_TX_PIN 4
#define AC_CONF_TYPE 0
#define AC_CONF_HEATING 0

typedef struct
{
  String humidity;
  String target_temperature_c;
  String ambient_temperature_c;
  String is_online;
  String hvac_mode;
  String hvac_state;
} nest_hvac_data;

nest_hvac_data nest_hvac_prev, nest_hvac_cur;

struct
{
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  bool haveData;
} ir_data;

IPAddress mqtt_server = MQTT_SERVER;

WiFiClientSecure sslclient;
lgWhisen lgWhisen;
WiFiUDP udp;

bool connected = false;
unsigned long lastMills;
unsigned long cmdCount = 0;
String msg_to_slack;

// Timelib
unsigned int localPort = 12390;
const int timeZone = 9;
time_t prevDisplay = 0;

// NEST
String firebase_host;
String firebase_port;
String firebase_path;

bool postToSLACK(const char* msg)
{
  String uri_to_post = "/services/";
  uri_to_post += SLACK_IN_WEBHOOK;

  String msg_body = "{\"text\": \"";
  msg_body += msg;
  msg_body += "\"}";

  String msg_to_post = "payload=";
  msg_to_post += String(URLEncode(msg_body.c_str()));

  String content_header = "POST " + String(uri_to_post) + " HTTP/1.1\r\n";
  content_header += "User-Agent: esp8266_nest_bot_by_chaeplin_V_0.1\r\n";
  content_header += "Content-Type: application/x-www-form-urlencoded\r\n";
  content_header += "Connection: close\r\n";
  content_header += "Host: " + String(SLACK_API_URL) + "\r\n";
  content_header += "Content-Length: " + String(msg_to_post.length()) + "\r\n\r\n";

  Serial.print("connecting to ");
  Serial.println(SLACK_API_URL);

  Serial.print("uri_to_post: ");
  Serial.println(uri_to_post);

  Serial.print("msg_to_post: ");
  Serial.println(msg_to_post);

  Serial.println(ESP.getFreeHeap());

  Serial.print("connecting to ");
  Serial.println(SLACK_API_URL);
  if (!sslclient.connect(SLACK_API_URL, 443))
  {
    Serial.println("connection failed");
    return false;
  }

  if (sslclient.verify(SLACK_API_FINGERPRINT, SLACK_API_URL))
  {
    Serial.println("certificate matches");
  }
  else
  {
    Serial.println("certificate doesn't match");
    return false;
  }

  sslclient.print(content_header);
  sslclient.print(msg_to_post);

  Serial.println("request sent");

  int _returnCode = 0;
  while (sslclient.connected())
  {
    String line = sslclient.readStringUntil('\n');
    if (line.startsWith("HTTP/1."))
    {
      _returnCode = line.substring(9, line.indexOf(' ', 9)).toInt();
      break;
    }
  }

  sslclient.stop();

  Serial.print("Slack return code: ");
  Serial.println(_returnCode);

  if (_returnCode >= 200 && _returnCode < 400)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool connectToFIREBASE()
{
  // redirect
  String uri_to_post = "/";
  uri_to_post += firebase_path;

  String content_header = "GET " + String(uri_to_post) + " HTTP/1.1\r\n";
  content_header += "User-Agent: esp8266_nest_bot_by_chaeplin_V_0.1\r\n";
  content_header += "Host: " + firebase_host + "\r\n";
  content_header += "Accept: application/json\r\n\r\n";

  Serial.print("connecting to ");
  Serial.println(firebase_host);
  if (!sslclient.connect(firebase_host.c_str(), firebase_port.toInt()))
  {
    Serial.println("connection failed");
    return false;
  }

  if (sslclient.verify(NEST_FIREBASE_FINGERPRINT, firebase_host.c_str()))
  {
    Serial.println("certificate matches");
  }
  else
  {
    Serial.println("certificate doesn't match");
    return false;
  }

  sslclient.print(content_header);
  Serial.println("request sent");

  sslclient.readStringUntil('{');
  while (sslclient.connected())
  {
    String line = sslclient.readStringUntil(',');

    line.replace("\"", "");
    line.replace("\n", "");
    line.replace("{", "");
    line.replace("}", "");

    if (line.startsWith("humidity:"))
    {
      line.replace("humidity:", "");
      nest_hvac_cur.humidity = line;
    }

    if (line.startsWith("hvac_mode:"))
    {
      line.replace("hvac_mode:", "");
      nest_hvac_cur.hvac_mode = line;
    }

    if (line.startsWith("target_temperature_c:"))
    {
      line.replace("target_temperature_c:", "");
      nest_hvac_cur.target_temperature_c = line;
    }

    if (line.startsWith("ambient_temperature_c:"))
    {
      line.replace("ambient_temperature_c:", "");
      nest_hvac_cur.ambient_temperature_c = line;
    }

    if (line.startsWith("is_online:"))
    {
      line.replace("is_online:", "");
      nest_hvac_cur.is_online = line;
    }

    if (line.startsWith("hvac_state:"))
    {
      line.replace("hvac_state:", "");
      nest_hvac_cur.hvac_state = line;
      break;
    }

  }
  sslclient.stop();

  Serial.println("------------------------------");
  Serial.printf("humidity: %s\n", nest_hvac_cur.humidity.c_str());
  Serial.printf("hvac_mode: %s\n", nest_hvac_cur.hvac_mode.c_str());
  Serial.printf("target_temperature_c: %s\n", nest_hvac_cur.target_temperature_c.c_str());
  Serial.printf("ambient_temperature_c: %s\n", nest_hvac_cur.ambient_temperature_c.c_str());
  Serial.printf("is_online: %s\n", nest_hvac_cur.is_online.c_str());
  Serial.printf("hvac_state: %s\n", nest_hvac_cur.hvac_state.c_str());
  Serial.println("------------------------------");

  if (nest_hvac_cur.is_online == "true" && (nest_hvac_cur.hvac_mode == "cool" || nest_hvac_cur.hvac_mode == "heat-cool"))
  {
    if (
      nest_hvac_cur.hvac_state           != nest_hvac_prev.hvac_state  ||
      nest_hvac_cur.target_temperature_c != nest_hvac_prev.target_temperature_c
    )
    {
      if (nest_hvac_cur.hvac_state == "cooling")
      {
        ir_data.ac_mode = 1;
      }
      else
      {
        ir_data.ac_mode = 0;
      }

      int current_temp = atoi(nest_hvac_cur.ambient_temperature_c.c_str());
      int target_temp = atoi(nest_hvac_cur.target_temperature_c.c_str());

      if (target_temp >= 18 && target_temp <= 30)
      {
        if ((current_temp - target_temp) > 3)
        {
          ir_data.ac_flow = 1;
        }
        else
        {
          ir_data.ac_flow = 0;
        }

        ir_data.ac_temp  = target_temp;
      }
      ir_data.haveData = true;
    }
  }

  nest_hvac_prev.humidity                 = nest_hvac_cur.humidity;
  nest_hvac_prev.target_temperature_c     = nest_hvac_cur.target_temperature_c;
  nest_hvac_prev.ambient_temperature_c    = nest_hvac_cur.ambient_temperature_c;
  nest_hvac_prev.is_online                = nest_hvac_cur.is_online;
  nest_hvac_prev.hvac_mode                = nest_hvac_cur.hvac_mode;
  nest_hvac_prev.hvac_state               = nest_hvac_cur.hvac_state;

  cmdCount++;
  return true;
}

bool connectToNEST()
{
  String uri_to_post = "/devices/thermostats/";
  uri_to_post += NEST_DEVICE_ID;
  uri_to_post += "?auth=";
  uri_to_post += NEST_API_TOKEN;

  String content_header = "GET " + String(uri_to_post) + " HTTP/1.1\r\n";
  content_header += "User-Agent: esp8266_nest_bot_by_chaeplin_V_0.1\r\n";
  content_header += "Host: " + String(NEST_API_URL) + "\r\n";
  content_header += "Accept: application/json\r\n\r\n";

  Serial.print("connecting to ");
  Serial.println(NEST_API_URL);
  if (!sslclient.connect(NEST_API_URL, NEST_API_PORT))
  {
    Serial.println("connection failed");
    return false;
  }

  if (sslclient.verify(NEST_API_FINGERPRINT, NEST_API_URL))
  {
    Serial.println("certificate matches");
  }
  else
  {
    Serial.println("certificate doesn't match");
    return false;
  }

  sslclient.print(content_header);
  Serial.println("request sent");

  bool redirFlag = false;

  while (sslclient.connected()) {
    String line = sslclient.readStringUntil('\n');
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }

    if (sslclient.find("Location: https://"))
    {
      Serial.println("Found re-direction URL!");
      firebase_host = sslclient.readStringUntil(':');
      firebase_port = sslclient.readStringUntil('/');
      firebase_path = sslclient.readStringUntil('\n');
      redirFlag = true;
      break;
    }
  }
  sslclient.stop();

  if (!redirFlag)
  {
    return false;
  }
  else
  {
    return true;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.flush();

  // ac
  ir_data.ac_mode     = 0;
  ir_data.ac_temp     = 27;
  ir_data.ac_flow     = 0;

  lgWhisen.setActype(AC_CONF_TYPE);
  lgWhisen.setHeating(AC_CONF_HEATING);
  lgWhisen.setTemp(ir_data.ac_temp);    // 18 ~ 30
  lgWhisen.setFlow(ir_data.ac_flow);    // 0 : low, 1 : mid, 2 : high, if setActype == 1, 3 : change
  lgWhisen.setIrpin(IR_TX_PIN);         // ir tx pin

  nest_hvac_prev.target_temperature_c     = "27";
  nest_hvac_prev.hvac_state               = "off";

  // connect to wifi.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.hostname("esp-slack-nest-irbot");
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // ntp update
  udp.begin(localPort);
  if (timeStatus() == timeNotSet)
  {
    Serial.println("get ntp time");
    setSyncProvider(getNtpTime);
    delay(500);
  }

  lastMills = millis();
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
  }

  if (cmdCount == 0)
  {
    if (!postToSLACK("connected"))
    {
      postToSLACK("connected");
    }
    cmdCount++;
  }

  if ((millis() - lastMills) > RETRIVE_INTERVAL)
  {
    if (connectToNEST())
    {
      if (connectToFIREBASE())
      {
        lastMills = millis();
      }
    }
  }

  if (ir_data.haveData)
  {
    Serial.println(ESP.getFreeHeap());

    msg_to_slack = "ac_mode : ";
    if (ir_data.ac_mode == 0)
    {
      msg_to_slack += ":black_square_for_stop:";
    }
    else
    {
      msg_to_slack += ":arrows_counterclockwise:";
    }
    msg_to_slack += "\nac_temp : ";
    msg_to_slack += ir_data.ac_temp;
    msg_to_slack += "\nac_flow : ";
    msg_to_slack += ir_data.ac_flow;
    msg_to_slack += "\ncur temp : ";
    msg_to_slack += nest_hvac_cur.ambient_temperature_c;
    if (!postToSLACK(msg_to_slack.c_str()))
    {
      postToSLACK(msg_to_slack.c_str());
    }
  }

  if (ir_data.haveData)
  {
    lgWhisen.setTemp(ir_data.ac_temp);
    lgWhisen.setFlow(ir_data.ac_flow);

    switch (ir_data.ac_mode)
    {
      // ac power down
      case 0:
        Serial.println("IR -----> AC Power Down");
        lgWhisen.power_down();
        delay(5);
        break;

      // ac on
      case 1:
        Serial.println("IR -----> AC Power On");
        lgWhisen.activate();
        delay(5);
        break;

      default:
        break;
    }
    ir_data.haveData = false;
  }
}

//----------------
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

String URLEncode(const char* msg)
{
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0')
  {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9')
         || *msg  == '-' || *msg == '_' || *msg == '.' || *msg == '~' ) {
      encodedMsg += *msg;
    }
    else
    {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

//end
