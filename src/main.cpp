#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DHT.h>
#include <RunningAverage.h>
#include <ArduinoJson.h>
#include "pms.h"
#include "packets/pms5003_packet.h"
#include "secrets.h"
#include "debug.h"
#include <MHZ19_uart.h>

// NOTE: These messages tend to be longer than PubSubClient likes so you need to modify
// MQTT_MAX_PACKET_SIZE in PubSubClient.h
const PROGMEM char* humidity_config_message = "{\"name\": \"Esp8266 Temperature\", \"unit_of_measurement\": \"°C\", \"state_topic\":\"homeassistant/sensor/esp8266/humidity/state\"}";
const PROGMEM char* temperature_config_message = "{\"name\": \"Esp8266 Humidity\", \"unit_of_measurement\": \"%\", \"state_topic\":\"homeassistant/sensor/esp8266/temperature/state\"}";
const PROGMEM char* co2_config_message = "{\"name\": \"Esp8266 CO2\", \"unit_of_measurement\": \"ppm\", \"state_topic\":\"homeassistant/sensor/esp8266/co2/state\"}";

// Home-assistant auto-discovery <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
const PROGMEM char* humidity_topic = "homeassistant/sensor/esp8266/humidity/state";
const PROGMEM char* temperature_topic = "homeassistant/sensor/esp8266/temperature/state";
const PROGMEM char* co2_topic = "homeassistant/sensor/esp8266/co2/state";
const PROGMEM char* particulate_topic = "homeassistant/sensor/esp8266/particulate/state";

const PROGMEM char* humidity_config_topic = "homeassistant/sensor/esp8266/humidity/config";
const PROGMEM char* temperature_config_topic = "homeassistant/sensor/esp8266/temperature/config";
const PROGMEM char* co2_config_topic = "homeassistant/sensor/esp8266/co2/config";

#define PMSRXPIN D5
#define PMSTXPIN D6
SoftwareSerial uart(PMSRXPIN, PMSTXPIN, false, 128);
PMS5003Packet pkt5003;
PMS pms5003(uart, pkt5003);

RunningAverage co2RA(12);
RunningAverage temperatureRA(60);
RunningAverage humidityRA(60);
RunningAverage pm1RA(60);
RunningAverage pm25RA(60);
RunningAverage pm10RA(60);

#define PUBLISH_PERIOD 60000
#define SAMPLE_PERIOD 1000
// Seems like the MHZ19 only updates its value every 5s or so
#define MHZ19_SAMPLE_PERIOD 5000

// UART TX connected to the RX pin on the M19
#define M19TX D2
// UART RX connected to the TX pin on the M19
#define M19RX D4
// SoftwareSerial m19UART(M19RX, M19TX, false, 128);
MHZ19_uart mhz19(M19RX, M19TX);


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
  Serial.println(WIFI_SSID);

  // Set Hostname.
  String hostname(HOSTNAME);
  WiFi.hostname(hostname);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(MQTT_SERVER, 1883);

  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();

  pms5003.begin();

  co2RA.clear();
  temperatureRA.clear();
  humidityRA.clear();
  pm1RA.clear();
  pm25RA.clear();
  pm10RA.clear();

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
  client.publish(co2_config_topic, co2_config_message, true);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DLOG("Attempting MQTT connection...\n");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
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
long lastMHZ19SampleTime = 0;
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
      humidityRA.addValue(h);
      temperatureRA.addValue(t);
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

  if (now - lastMHZ19SampleTime > MHZ19_SAMPLE_PERIOD) {
    lastMHZ19SampleTime = now;
    int co2PPM = mhz19.getPPM();
    DLOG("Got CO2 PPM from MHZ19 %d\n", co2PPM);
    if (co2PPM > 0) {
      co2RA.addValue(co2PPM);
    }
  }

  if (client.connected()) {
    if (now - lastPublishTime > PUBLISH_PERIOD) {
      lastPublishTime = now;

      client.publish(temperature_topic, String(humidityRA.getAverage()).c_str(), false);
      client.publish(humidity_topic, String(temperatureRA.getAverage()).c_str(), false);
      client.publish(co2_topic, String(co2RA.getAverage()).c_str(), false);

      if (pm1RA.getCount() > 0) {
        pms5003.generateReport(data, buffer, pm1RA.getAverage(), pm25RA.getAverage(), pm10RA.getAverage());
        String stream;
        root.printTo(stream);
        DLOG("%s\n", stream.c_str());
        DLOG("Length of report %d\n", stream.length());
        bool published = client.publish(particulate_topic, stream.c_str(), true);
        DLOG("Published %d\n", published);
      }

      DLOG("Published averages of %d temp, %d hum, %d pm1, %d pm2.5, %d pm10 readings\n", temperatureRA.getCount(), humidityRA.getCount(), pm1RA.getCount(), pm25RA.getCount(), pm10RA.getCount());

      humidityRA.clear();
      temperatureRA.clear();
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
