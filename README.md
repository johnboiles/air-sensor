# air-sensor

Monitor temperature, humidity, particulate count, and CO2 with an ESP12 and some cheap-ish sensors.

Uses:
* [Wemos D1 Mini ESP12](https://www.amazon.com/Makerfocus-NodeMcu-Development-ESP8266-ESP-12F/dp/B01N3P763C)
* [PMS5003 High Precision Laser Dust Sensor](https://www.ebay.com/itm/PMS5003-High-Precision-Laser-Dust-Sensor-Module-PM1-0-PM2-5-PM10-Built-in-Fan-N-/263421941788?hash=item3d552bd81c)
* [MHZ-19 Intelligent Infrared CO2 Module](http://www.winsen-sensor.com/products/ndir-co2-sensor/mh-z19.html)
* [DHT22 Temperature & Humidity Sensor](https://www.adafruit.com/product/385)

## Compiling the code

First off you'll need to create a `src/secrets.h`. This file is `.gitignore`'d so you don't put your passwords on Github.

    cp src/secrets.example.h src/secrets.h

Then edit your `src/secrets.h` file to reflect your wifi ssid/password and MQTT configuration.

## Programming the board

If you don't have it already, you probably need a driver for the CH340 USB->Serial chip. [This janky looking website](http://www.wch.cn/download/CH341SER_MAC_ZIP.html) is the chip manufacturer's official download page.

The easiest way to build and upload the code is with the [PlatformIO IDE](http://platformio.org/platformio-ide). I like the PlatformIO IDE extension for VSCode the best.

The first time you program your board you'll want to do it over USB. After that, programming can be done over wifi. To program over USB, remove the `upload_port` line in the `platformio.ini` file. PlatformIO can typically autodetect the right serial port.

After that, replace the `upload_port` line in the `platformio.ini` file, and you should be able to program the board over wifi.

## Debugging

Included in the firmware is a telnet debugging interface. To connect run `telnet air-sensor.local` (replace `air-sensor` with the `HOSTNAME` you set in `secrets.h`. With that you can log messages from code with the `DLOG` macro and also send commands back that the code can act on (see the `debugCallback` function).
