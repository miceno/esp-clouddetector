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

* `I2C_SCL`: set to the i2c SCL pin (clock). For ESP8266, set it to `D1` or `5`.
* `I2C_SDA`: set to the i2c SDA pin (data). For ESP8266, set it to `D2` or `4`.
* `DHTPIN`: set to the pin for the DHT22 sensor.
* `LDRPIN`: set to the pin for the LDR sensor.
* `BAUD_RATE`: set to the serial baud rate speed.
* `VERSION`: set to the version string to show.

# Architecture

The code reads continuously the data from DHT22 (temperature and humidity sensor: `dht`), MLX90640 (thermal camera: `mlx`) and LDR (light-dependent resistor: `ldr`), and
stores it internally on RAM. When there is a request using a command, available data is used to calculate or return values.

Each pixel in the IR camera data frame must be adjusted according to the
device calibration. Calibration is defined by two parameters: slope and shift.

## Available sensors

The name of sensors is:
* temperature and humidity: `dht`.
* thermal camera: `mlx`.
* daylight: `ldr`.

## Remote Commands

There is a serial interface to interact with the controller. Make sure both sides of the serial communication use the same baudrate 
and serial configuration. Here it is a list of available commands:

`READ`: Read summarized sensor data, like this:
```
c:30.36,t:27.50,h:59.60,n:0
```
Cloud data is calculated as the median of the data on every frame, on every request using the latest thermal camera frame data. 

Temperature and humidity are not calculated on every request, but 
use the latest sampled data available.

It shows current value of sensors:
* `c`: cloud as a float of the median of the temperature of the IR camera.
* `t`: temperature as a float in Celsius degrees.
* `h`: humidity as a float in percentage from 0 to 100.
* `n`: night detected status. It is 0 during the day and 1 during the night.

`IR`: Return infrared data from the thermal camera as an ASCII art, like this:
```
x*oxooooooooooooooooooooxxxxxxxx
xxxxoooooooooooooooooooxxxxxxxxx
xxoooooooooooooooooooooxxxxxxxxx
xxooooooooooooooooooooooooxxxxxx
xooooooooooooooooooooooooooxxxxx
oxooooooooooooooooooooooooxxxxxx
oooooooooooooooooooooooooooxxxxx
oxooooooooooooooooooooooooooxxxx
oooooooooooooooooooooooooooxxxxx
ooooooooooooooooooooooooooooxxxx
ooooooooooooooooooooooooooooxxxx
ooooooooooooooooooooooooooooxxxx
ooooooooooooooooooooooooooooxxxx
ooooooooooooooooooooooooooooxxxx
oooooooooooooooooooooooooooooxxx
oooooooooooooooooooooooooooooxxx
oooooooooooooooooooooooooooxxxxx
ooooooooooooooooooooooooooooxxxx
oooooooooooooooooooooooooooxxxxx
ooooooooooooooooooooooooooxoxxxx
xxoooooooooooooooooooooooooxxxxx
xxxxooooooooooooooooooooooxxxxxx
xxoxoxoooooooooooooooooooxxxxxxx
xxxxooooooooooooooooooxoxxxxxxxx
```

`IRX`: Return IR data as a base64 stream, one line, like this:
```
irx:4096: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
```

`IRT`: Test IR binary encoding and decoding

`PING`: Echo current version and configuration, like this:
```
cloud-0.3.0-wemos,status=[mlx=on,dht=on,ldr=on],data=c:30.28,t:27.30,h:59.80,n:0
```
It shows current version and the status of the sensor sampling. It also show current values of 
sensors as the `READ` command returns.

`START`: Start data collection. With no params, it starts collection of measure from all sensors. 
An additional param with the name of the sensor will start only that sensor data collection.

`STOP`: Stop data collection. Same as for the `START` command, but for stopping data collection. From this 
command, subsequent request to `READ` will return the last collected data before stopping.

`HELP`: Show a list of  available commands.

## ASCII Art

Here it is a table showing the mapping of temperature values to ASCII: the higher the temperature, the higher the chars:

| temperature	 |	character |
|--------------|   -------- | 
| t < 20       |    ' ' |
| t < 23       |    '.' |
| t < 25       |    '_' |
| t < 27       |    '-' |
| t < 29       |    '+' |
| t < 31       |    'o' |
| t < 33       |    'x' |
| t < 35       |    '*' |
| t < 37       |    'O' |
| t > 37       |     '&' |
