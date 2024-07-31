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
Cloud data is calculated as the median of the data on every frame, on every request using the latest thermal camera frame data. Thermal data is calibrated.

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
irx:4096:pmIPQl4NDUI ... CrhUEQoa8BELWogRC
```

`CAL A B`: Set IR calibration data, using float values.

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
