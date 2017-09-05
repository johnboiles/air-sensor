#include "secrets.h"

#define MQ135PIN A0
#define HOSTNAME "air-sensor"

#define mqtt_server "10.0.0.2"
#define mqtt_user "homeassistant"

#define PMSRXPIN D5
#define PMSTXPIN D6

#define PUBLISH_PERIOD 60000
#define SAMPLE_PERIOD 1000
#define SAMPLES_PER_PUBLISH (PUBLISH_PERIOD / SAMPLE_PERIOD)
// Seems like the MHZ19 only updates its value every 5s or so
#define MHZ19_SAMPLE_PERIOD 5000
#define MHZ19_SAMPLES_PER_PUBLISH (PUBLISH_PERIOD / MHZ19_SAMPLE_PERIOD)

// UART TX connected to the RX pin on the M19
#define M19TX D2
// UART RX connected to the TX pin on the M19
#define M19RX D3
// DHT - D1/GPIO5
#define DHTPIN 5
#define DHTTYPE DHT22
