#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <MQ135.h>
#include <DHT.h>

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

long lastMsg = 0;

void loop() {
    ArduinoOTA.handle();
    yield();

    if (client.connected()) {
      long now = millis();
      if (now - lastMsg > 20000) {
        lastMsg = now;

        // Reading temperature or humidity takes about 250 milliseconds!
        // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
        float h = dht.readHumidity();
        // Read temperature as Celsius (the default)
        float t = dht.readTemperature();
        if (isnan(h) || isnan(t)) {
          Serial.println("ERROR: Failed to read from DHT sensor!");
        } else {
          client.publish(humidity_topic, String(t).c_str(), true);
          client.publish(temperature_topic, String(h).c_str(), true);

          float rzero = gasSensor.getCorrectedRZero(t, h); //this to get the rzero value, uncomment this to get ppm value
          Serial.printf("Rzero %s\n", String(rzero).c_str()); // this to display the rzero value continuously, uncomment this to get ppm value
          client.publish(rzero_topic, String(rzero).c_str(), true);
          float ppm = gasSensor.getCorrectedPPM(t, h); // this to get ppm value, uncomment this to get rzero value
          // Serial.println(ppm); // this to display the ppm value continuously, uncomment this to get rzero value
          client.publish(co2_topic, String(ppm).c_str(), true);

        }


      }
    } else {
      reconnect();
      // Wait 5 seconds before retrying
      delay(5000);
    }
    client.loop();

    delay(500);
}
