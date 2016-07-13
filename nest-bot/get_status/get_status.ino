
// nest irbot by chaeplin@gmail.com

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/nest_setting.h"

//#define NEST_API_TOKEN "c.xxxxxxxx"
//#define NEST_DEVICE_ID "xxxxxx_xxxxxxx-xxxxxx"

#define NEST_API_URL "developer-api.nest.com"
#define NEST_API_PORT 443
#define NEST_API_FINGERPRINT "88 A0 F6 B1 7D 50 4E D7 8B 14 11 CC 9E 7F 4D 4F 08 6B 58 3C"
#define NEST_FIREBASE_FINGERPRINT "AF F8 71 3D D8 D2 67 45 A2 DF 61 95 72 99 70 45 E9 4F 25 16"

#define RETRIVE_INTERVAL 60000 // in msec

WiFiClientSecure sslclient;

bool connected = false;
unsigned long lastMills;

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

  String host;
  String port;
  String path;

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
      host = sslclient.readStringUntil(':');
      port = sslclient.readStringUntil('/');
      path = sslclient.readStringUntil('\n');
      redirFlag = true;
      break;
    }
  }
  sslclient.stop();

  if (!redirFlag)
  {
    return false;
  }

  // redirect

  String humidity;
  String hvac_mode;
  String target_temperature_c;
  String ambient_temperature_c;
  String is_online;
  String hvac_state;

  uri_to_post = "/";
  uri_to_post += path;

  content_header = "GET " + String(uri_to_post) + " HTTP/1.1\r\n";
  content_header += "User-Agent: esp8266_nest_bot_by_chaeplin_V_0.1\r\n";
  content_header += "Host: " + host + "\r\n";
  content_header += "Accept: application/json\r\n\r\n";

  Serial.print("connecting to ");
  Serial.println(host);
  if (!sslclient.connect(host.c_str(), port.toInt()))
  {
    Serial.println("connection failed");
    return false;
  }

  if (sslclient.verify(NEST_FIREBASE_FINGERPRINT, host.c_str()))
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
      humidity = line;
    }

    if (line.startsWith("hvac_mode:"))
    {
      line.replace("hvac_mode:", "");
      hvac_mode = line;
    }

    if (line.startsWith("target_temperature_c:"))
    {
      line.replace("target_temperature_c:", "");
      target_temperature_c = line;
    }

    if (line.startsWith("ambient_temperature_c:"))
    {
      line.replace("ambient_temperature_c:", "");
      ambient_temperature_c = line;
    }

    if (line.startsWith("is_online:"))
    {
      line.replace("is_online:", "");
      is_online = line;
    }

    if (line.startsWith("hvac_state:"))
    {
      line.replace("hvac_state:", "");
      hvac_state = line;
      break;
    }

  }
  sslclient.stop();
  Serial.println("------------------------------");
  Serial.printf("humidity: %s\n", humidity.c_str());
  Serial.printf("hvac_mode: %s\n", hvac_mode.c_str());
  Serial.printf("target_temperature_c: %s\n", target_temperature_c.c_str());
  Serial.printf("ambient_temperature_c: %s\n", ambient_temperature_c.c_str());
  Serial.printf("is_online: %s\n", is_online.c_str());
  Serial.printf("hvac_state: %s\n", hvac_state.c_str());
  Serial.println("------------------------------");

  return true;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.flush();
  Serial.setDebugOutput(false);

  // connect to wifi.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  lastMills = millis();
}


void loop()
{
  if ((millis() - lastMills) > RETRIVE_INTERVAL)
  {
    if (connectToNEST())
    {
      lastMills = millis();
    }
  }
}
