#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <MQ135.h>
#include <DHT.h>
#include <RunningAverage.h>

#define ANALOGPIN A0
#define HOSTNAME "ESP8266-OTA-"

#define mqtt_version MQTT_VERSION_3_1_1
#define mqtt_server "10.0.0.2"
#define mqtt_user "homeassistant"
#define mqtt_password ""

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

const PROGMEM char* humidity_config_topic = "homeassistant/sensor/esp8266/humidity/config";
const PROGMEM char* temperature_config_topic = "homeassistant/sensor/esp8266/temperature/config";
const PROGMEM char* rzero_config_topic = "homeassistant/sensor/esp8266/rzero/config";
const PROGMEM char* co2_config_topic = "homeassistant/sensor/esp8266/co2/config";

const PROGMEM char* ssid = ""; // Insert your SSID here
const PROGMEM char* password = ""; // Insert your SSID password here

MQ135 gasSensor = MQ135(ANALOGPIN);

RunningAverage rzeroRA(30);
RunningAverage co2RA(30);
RunningAverage temperatureRA(30);
RunningAverage humidityRA(30);

#define PUBLISH_PERIOD 30000
#define SAMPLE_PERIOD 1000

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

  WiFi.begin(ssid, password);

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

  rzeroRA.clear();
  co2RA.clear();
  temperatureRA.clear();
  humidityRA.clear();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.publish(temperature_config_topic, temperature_config_message, true);
      client.publish(humidity_config_topic, humidity_config_message, true);
      client.publish(rzero_config_topic, rzero_config_message, true);
      client.publish(co2_config_topic, co2_config_message, true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}

long lastSampleTime = 0;
long lastPublishTime = 0;

void loop() {
  ArduinoOTA.handle();
  yield();

  long now = millis();
  if (now - lastSampleTime > SAMPLE_PERIOD) {
    lastSampleTime = now;

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("ERROR: Failed to read from DHT sensor!");
    } else {
      float rzero = gasSensor.getCorrectedRZero(t, h);
      float c02ppm = gasSensor.getCorrectedPPM(t, h);

      humidityRA.addValue(h);
      temperatureRA.addValue(t);
      rzeroRA.addValue(rzero);
      co2RA.addValue(c02ppm);
    }
  }
  if (client.connected()) {
    if (now - lastPublishTime > PUBLISH_PERIOD) {
      lastPublishTime = now;

      client.publish(temperature_topic, String(humidityRA.getAverage()).c_str(), true);
      client.publish(humidity_topic, String(temperatureRA.getAverage()).c_str(), true);
      client.publish(rzero_topic, String(rzeroRA.getAverage()).c_str(), true);
      client.publish(co2_topic, String(co2RA.getAverage()).c_str(), true);

      humidityRA.clear();
      temperatureRA.clear();
      rzeroRA.clear();
      co2RA.clear();
    }
  } else {
    reconnect();
    // Wait 5 seconds before retrying
    delay(5000);
  }
  client.loop();
}
