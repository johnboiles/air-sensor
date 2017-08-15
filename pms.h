#ifndef _PMS_H
#define _PMS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#include "pms_packet_interface.h"

class PMS {
public:
  PMS(SoftwareSerial &, PMSPacketInterface &);
  bool probe();
  void begin();
  bool read();
  bool readUntilSuccessful(int);
  void sleep();
  void wake_up(bool force=false);
  bool report(JsonArray &, DynamicJsonBuffer &);
  void generateReport(JsonArray &data, DynamicJsonBuffer &buffer, float pm1, float pm25, float pm10);

private:
  bool detected;
  SoftwareSerial &uart;
  PMSPacketInterface &packet;
};

#endif
