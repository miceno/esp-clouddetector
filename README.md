# esp-clouddetector
A cloud detector written for the ESP8266 ecosystem

# Requirements

For sensors:
* [Adafruit_MLX90640](https://github.com/adafruit/Adafruit_MLX90640) library.
* [DHT22](https://github.com/adafruit/DHT-sensor-library) library. 

For utilities:
* [Serial Commands](https://github.com/shyd/Arduino-SerialCommand). Allows the common pattern of parsing 
and executing command over serial line.
* [Base64 encoding](https://github.com/dojyorin/arduino_base64)
* [I2Cbus](https://github.com/maarten-pennings/I2Cbus/tree/master). A library to reset I2C bus.

# Setup

In order to configure the input and output ports:

* `I2C_SCL`: set to the i2c SCL pin (clock). For ESP8266, set it to `D1` or `5`. For Arduino Nano, set it to `xx`.
* `I2C_SDA`: set to the i2c SDA pin (data). For ESP8266, set it to `D2` or `4`. For Arduino Nano, set it to `xx`.
* `DHTPIN`: set to the pin for the DHT22 sensor.
* `BAUD_RATE`: set to the serial baud rate speed.
* `VERSION`: set to the version string to show.
