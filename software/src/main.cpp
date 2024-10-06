#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Arduino.h"

#define FAN_PIN D0
#define PWM_CHANNEL 0
#define RESOLUTION 10  // in bits
#define RANGE 1023
#define FREQUENCY 20000

#define DEVICE_MANUFACTURER "Barebaric"
#define DEVICE_FW_VERSION "1.1"
#define DEVICE_MODEL "ESP32-C3"
#define DEVICE_ID (char*)String((uint64_t)ESP.getEfuseMac(), HEX).c_str()
#define DEVICE_NAME "AirCleaner"
#define DEVICE_OBJECTID "aircleaner"
#define WIFI_HOSTNAME (String(DEVICE_NAME) + "-" + String(DEVICE_ID))
#define DEVICE_CLASS "fan"
#define TOPIC_PFX "homeassistant/fan/" + String(DEVICE_ID)
#define TOPIC_CONF (TOPIC_PFX + "/config")
#define TOPIC_ON_STATE (TOPIC_PFX + "/state")
#define TOPIC_ON_SET (TOPIC_PFX + "/state/set")
#define TOPIC_SPEED_STATE (TOPIC_PFX + "/speed")
#define TOPIC_SPEED_SET (TOPIC_PFX + "/speed/set")
#define SPEED_RANGE_MIN 500
#define SPEED_RANGE_MAX RANGE

// Update these with values suitable for your network.
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;

float speed = 0.5;
int off = false;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.setHostname(WIFI_HOSTNAME.c_str());
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void publishOnState() {
  const char* state = off ? "OFF" : "ON";
  Serial.print("Sending state: ");
  Serial.println(state);
  client.publish(TOPIC_ON_STATE.c_str(), state, false);
}

void publishSpeedState() {
  String speed_state = String((uint16_t)(SPEED_RANGE_MAX*speed));
  Serial.print("Current speed: ");
  Serial.println(String(speed));
  Serial.print("Sending speed: ");
  Serial.println(speed_state);
  client.publish(TOPIC_SPEED_STATE.c_str(), speed_state.c_str(), false);
}

void publishState() {
  publishOnState();
  publishSpeedState();
}

void onSwitchRequest(byte* payload) {
  String target = String((char*)payload);
  Serial.println(target);

  // Switch on or off.
  off = (target == "OFF");
  publishState();
}

void onSpeedRequest(byte* payload) {
  int target = String((char*)payload).toInt();
  Serial.println(String(target));

  off = false;
  speed = (float)max(SPEED_RANGE_MIN, min(target, SPEED_RANGE_MAX))/SPEED_RANGE_MAX; 
  Serial.print("New speed: ");
  Serial.println(String(speed));
  publishState();
}

void publishConfig() {
  JsonDocument jsonDoc;

  //jsonDoc["name"] = DEVICE_LABEL;
  jsonDoc["uniq_id"] = DEVICE_ID;
  jsonDoc["obj_id"] = DEVICE_OBJECTID;
  jsonDoc["dev_cla"] = DEVICE_CLASS;
  jsonDoc["stat_t"] = TOPIC_ON_STATE;
  jsonDoc["cmd_t"] = TOPIC_ON_SET;
  jsonDoc["pct_stat_t"] = TOPIC_SPEED_STATE;
  jsonDoc["pct_cmd_t"] = TOPIC_SPEED_SET;
  jsonDoc["spd_rng_min"] = String(SPEED_RANGE_MIN);
  jsonDoc["spd_rng_max"] = String(SPEED_RANGE_MAX);
  jsonDoc["opt"] = false;  // Optimistic
  //jsonDoc["retain"] = false;

  JsonObject dev = jsonDoc["dev"].to<JsonObject>();
  JsonArray ids = dev["ids"].to<JsonArray>();
  ids.add(DEVICE_ID);
  dev["name"] = DEVICE_NAME;
  dev["mdl"] = DEVICE_MODEL;
  dev["sw"] = DEVICE_FW_VERSION;
  dev["mf"] = DEVICE_MANUFACTURER;
  dev["sn"] = DEVICE_ID;

  serializeJsonPretty(jsonDoc, Serial);
  Serial.println("");
  char message[512];
  serializeJson(jsonDoc, message, sizeof(message));

  Serial.println("Publishing config:");
  Serial.println(TOPIC_CONF);
  Serial.println(message);
  client.publish(TOPIC_CONF.c_str(), message, sizeof(message));
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String strTopic = String((char*)topic);
  Serial.println(strTopic);

  if(strTopic == TOPIC_ON_STATE) {
  }
  else if (strTopic == TOPIC_ON_SET) {
    onSwitchRequest(payload);
  }
  else if(strTopic == TOPIC_SPEED_STATE) {
  }
  else if(strTopic == TOPIC_SPEED_SET) {
    onSpeedRequest(payload);
  }
  else if(strTopic == TOPIC_CONF) {
    publishState();
  }
  else {
    Serial.println("Unknown topic");
  }
}
 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(DEVICE_ID, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      publishConfig();
      client.subscribe((TOPIC_PFX + String("/#")).c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
 
void setup()
{
  Serial.begin(115200);

  setup_wifi(); 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(1024);

  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);

  // initialize LED digital pin as an output.
  pinMode(FAN_PIN, OUTPUT);
  ledcSetup(PWM_CHANNEL, FREQUENCY, RESOLUTION);
  ledcAttachPin(FAN_PIN, PWM_CHANNEL);
}
 
void loop()
{
  if (!client.connected()) {
    reconnect();
    publishState();
  }
  client.loop();

  if (off)
    ledcWrite(PWM_CHANNEL, 0);
  else
    ledcWrite(PWM_CHANNEL, RANGE*speed);
}