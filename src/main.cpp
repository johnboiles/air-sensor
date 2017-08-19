#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <MQ135.h>
#include <DHT.h>
#include <RunningAverage.h>
#include <ArduinoJson.h>
#include "pms.h"
#include "packets/pms5003_packet.h"
#include "secrets.h"

#if LOGGING
#include <RemoteDebug.h>
#define DLOG(msg, ...) if(Debug.isActive(Debug.DEBUG)){Debug.printf(msg, ##__VA_ARGS__);}
RemoteDebug Debug;
#else
#define DLOG(msg, ...)
#endif

#define ANALOGPIN A0
#define HOSTNAME "ESP8266-OTA-"

#define mqtt_version MQTT_VERSION_3_1_1
#define mqtt_server "10.0.0.2"
#define mqtt_user "homeassistant"

// NOTE: These messages tend to be longer than PubSubClient likes so you need to modify
// MQTT_MAX_PACKET_SIZE in PubSubClient.h
const PROGMEM char* humidity_config_message = "{\"name\": \"Esp8266 Temperature\", \"device_class\": \"sensor\", \"unit_of_measurement\": \"°C\"}";
const PROGMEM char* temperature_config_message = "{\"name\": \"Esp8266 Humidity\", \"device_class\": \"sensor\", \"unit_of_measurement\": \"%rh\"}";
const PROGMEM char* rzero_config_message = "{\"name\": \"Esp8266 RZero\", \"device_class\": \"sensor\", \"unit_of_measurement\": \"z\"}";
const PROGMEM char* co2_config_message = "{\"name\": \"Esp8266 CO2\", \"device_class\": \"sensor\", \"unit_of_measurement\": \"ppm\"}";

// Home-assistant auto-discovery <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
const PROGMEM char* humidity_topic = "homeassistant/sensor/esp8266/humidity/state";
const PROGMEM char* temperature_topic = "homeassistant/sensor/esp8266/temperature/state";
const PROGMEM char* rzero_topic = "homeassistant/sensor/esp8266/rzero/state";
const PROGMEM char* co2_topic = "homeassistant/sensor/esp8266/co2/state";
const PROGMEM char* particulate_topic = "homeassistant/sensor/esp8266/particulate/state";

const PROGMEM char* humidity_config_topic = "homeassistant/sensor/esp8266/humidity/config";
const PROGMEM char* temperature_config_topic = "homeassistant/sensor/esp8266/temperature/config";
const PROGMEM char* rzero_config_topic = "homeassistant/sensor/esp8266/rzero/config";
const PROGMEM char* co2_config_topic = "homeassistant/sensor/esp8266/co2/config";

MQ135 gasSensor = MQ135(ANALOGPIN);

#define PMSRXPIN D5
#define PMSTXPIN D6
SoftwareSerial uart(PMSRXPIN, PMSTXPIN);
PMS5003Packet pkt5003;
PMS pms5003(uart, pkt5003);

RunningAverage rzeroRA(60);
RunningAverage co2RA(60);
RunningAverage temperatureRA(60);
RunningAverage humidityRA(60);
RunningAverage pm1RA(60);
RunningAverage pm25RA(60);
RunningAverage pm10RA(60);

#define PUBLISH_PERIOD 60000
#define SAMPLE_PERIOD 2000

// DHT - D1/GPIO5
#define DHTPIN 5
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup() {
  dht.begin();

  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  WiFi.begin(ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);

  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();

  pms5003.begin();

  rzeroRA.clear();
  co2RA.clear();
  temperatureRA.clear();
  humidityRA.clear();

  #if LOGGING
  // Initiaze the telnet server - HOST_NAME is the used in MDNS.begin
  Debug.begin((const char *)hostname.c_str());
  // Enable the reset command
  Debug.setResetCmdEnabled(true);
  #endif
}

bool has_sent_discover_config = false;

void send_discover_config() {
  client.publish(temperature_config_topic, temperature_config_message, true);
  client.publish(humidity_config_topic, humidity_config_message, true);
  client.publish(rzero_config_topic, rzero_config_message, true);
  client.publish(co2_config_topic, co2_config_message, true);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DLOG("Attempting MQTT connection...\n");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      DLOG("MQTT connected\n");
      if (!has_sent_discover_config) {
        send_discover_config();
      }
    } else {
      DLOG("MQTT failed rc=%d try again in 5 seconds\n", client.state());
    }
  }
}

long lastSampleTime = 0;
long lastPublishTime = 0;

void loop() {
  ArduinoOTA.handle();
  yield();

  DynamicJsonBuffer buffer;
  JsonObject& root = buffer.createObject();
  root["version"] = 1;
  JsonArray &data = buffer.createArray();
  root["data"] = data;

  long now = millis();
  if (now - lastSampleTime > SAMPLE_PERIOD) {
    lastSampleTime = now;

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      DLOG("ERROR: Failed to read from DHT sensor!\n");
    } else {
      float rzero = gasSensor.getCorrectedRZero(t, h);
      float c02ppm = gasSensor.getCorrectedPPM(t, h);

      humidityRA.addValue(h);
      temperatureRA.addValue(t);
      rzeroRA.addValue(rzero);
      co2RA.addValue(c02ppm);
    }
    if (pms5003.read()) {
      DLOG("Got pms5003 packet %s %s %s\n", String(pkt5003.pm1()).c_str(), String(pkt5003.pm25()).c_str(), String(pkt5003.pm10()).c_str());
      pm1RA.addValue(pkt5003.pm1());
      pm25RA.addValue(pkt5003.pm25());
      pm10RA.addValue(pkt5003.pm10());
    } else {
      DLOG("Failed to get pms5003 packet\n");
    }
  }
  if (client.connected()) {
    if (now - lastPublishTime > PUBLISH_PERIOD) {
      lastPublishTime = now;

      client.publish(temperature_topic, String(humidityRA.getAverage()).c_str(), true);
      client.publish(humidity_topic, String(temperatureRA.getAverage()).c_str(), true);
      client.publish(rzero_topic, String(rzeroRA.getAverage()).c_str(), true);
      client.publish(co2_topic, String(co2RA.getAverage()).c_str(), true);

      if (pm1RA.getCount() > 0) {
        pms5003.generateReport(data, buffer, pm1RA.getAverage(), pm25RA.getAverage(), pm10RA.getAverage());
        String stream;
        root.printTo(stream);
        DLOG("%s\n", stream.c_str());
        DLOG("Length of report %d\n", stream.length());
        bool published = client.publish(particulate_topic, stream.c_str(), true);
        DLOG("Published %d\n", published);
      }

      DLOG("Published averages of %d temp, %d hum, %d rzero, %d co2, %d pm1, %d pm2.5, %d pm10 readings\n", temperatureRA.getCount(), humidityRA.getCount(), rzeroRA.getCount(), co2RA.getCount(), pm1RA.getCount(), pm25RA.getCount(), pm10RA.getCount());

      humidityRA.clear();
      temperatureRA.clear();
      rzeroRA.clear();
      co2RA.clear();
      pm1RA.clear();
      pm25RA.clear();
      pm10RA.clear();
    }
  } else {
    reconnect();
    // Wait 5 seconds before retrying
    delay(5000);
  }
  client.loop();
  Debug.handle();
}
