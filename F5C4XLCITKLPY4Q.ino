#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <MQ135.h>

#define ANALOGPIN A0
#define HOSTNAME "ESP8266-OTA-"

#define mqtt_server "10.0.0.2"
#define mqtt_user "homeassistant"
#define mqtt_password ""
#define config_message "{\"name\": \"MQ135 RZero\", \"device_class\": \"sensor\", \"unit_of_measurement\": \"z\"}"

// Home-assistant auto-discovery <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
#define humidity_topic "sensor/humidity"
#define temperature_topic "sensor/temperature"
#define rzero_topic "homeassistant/sensor/mq135/rzero/state"
#define rzero_config_topic "homeassistant/sensor/mq135/rzero/config"

char ssid[] = ""; // Insert your SSID here
char password[] = ""; // Insert your SSID password here
MQ135 gasSensor = MQ135(ANALOGPIN);

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
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
      client.publish(rzero_config_topic, config_message, true);
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
      if (now - lastMsg > 2000) {
        lastMsg = now;

        // put your main code here, to run repeatedly:
        float rzero = gasSensor.getRZero(); //this to get the rzero value, uncomment this to get ppm value
        Serial.printf("Rzero %s\n", String(rzero).c_str()); // this to display the rzero value continuously, uncomment this to get ppm value
        // float ppm = gasSensor.getPPM(); // this to get ppm value, uncomment this to get rzero value
        // Serial.println(ppm); // this to display the ppm value continuously, uncomment this to get rzero value

        client.publish(rzero_topic, String(rzero).c_str(), true);
      }
    } else {
      reconnect();
      // Wait 5 seconds before retrying
      delay(5000);
    }
    client.loop();

    delay(500);
}
