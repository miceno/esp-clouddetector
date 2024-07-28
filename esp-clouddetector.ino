#include <Arduino.h>
#include <Adafruit_MLX90640.h>

// Import it before including SerialCommand.h
#define SERIALCOMMAND_MAXCOMMANDLENGTH 16
// #define SERIALCOMMAND_DEBUG 1

#include <SerialCommand.h>
#include "DHT.h"
#include "arduino_base64.hpp"
#include "microlzw.h"
#include "I2Cbus.h"
/*
Author: Orestes Sanchez <miceno.atreides@gmail.com>
*/

typedef void (*cmd_callback_t)();

typedef struct {
  const char *name;
  cmd_callback_t callback;
  const char *description;
} command_entry_t;

/*!
 *  @brief  Class that stores state and functions for DHT
 */
class LightSensor {
public:
  LightSensor(uint8_t pin);
  void begin(uint8_t pin);
  void begin(void);
  bool read_status(void);
private:
  uint8_t _pin = UINT8_MAX;
};

LightSensor::LightSensor(uint8_t pin) {
  _pin = pin;
}

void LightSensor::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT);
}

void LightSensor::begin(void) {
  begin(_pin);
}

bool LightSensor::read_status(void) {
  // Read data as a boolean:
  // true  : if the sensor value is > 0,
  // false : if sensorvalue <= 0.
  int value = digitalRead(_pin);
  // Serial.print("ldr_status=");
  // Serial.println(value);
  return (value == LOW);
}


#define I2C_SCL PIN_WIRE_SCL
#define I2C_SDA PIN_WIRE_SDA

#define LDRPIN D0  // Digital pin connected to the LDR (light-dependent resistors)

#define DHTPIN D5      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321
#define BAUD_RATE 115200

Adafruit_MLX90640 *mlx = new Adafruit_MLX90640();
SerialCommand sCmd;  // The demo SerialCommand object

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

LightSensor ldr(LDRPIN);

// MLX90640 camera is 32 x 24
const int IR_IMAGE_COLS = 32;
const int IR_IMAGE_ROWS = 24;
const int frame_size = IR_IMAGE_COLS * IR_IMAGE_ROWS;

// Compression dict size
const int LZW_DICT_SIZE = 1024;

const char *VERSION = "cloud-0.3.0-wemos";

// Internal variables for the status of the detector.
float *frame = (float *)malloc(frame_size * sizeof(float));  // buffer for full frame of temperatures
float temp = 0.0;                                            // current temperature from DHT22 sensor
float humidity = 0.0;                                        // current humidity from DHT22 sensor
boolean is_night = false;                                    // represents if it is day or night according to the sensor

// start_SENSOR variables control if the controller is updating its internal
// state using data from the sensor.
boolean start_dht = true;
boolean start_mlx = true;
boolean start_ldr = true;

int i2cbusstatus;

// uncomment *one* of the below
//#define PRINT_TEMPERATURES
#define PRINT_ASCIIART

/*
  MLX 90640 code
*/
Adafruit_MLX90640 *setup_mlx(mlx90640_mode_t p_mode = MLX90640_CHESS,
                             mlx90640_resolution_t p_resolution = MLX90640_ADC_18BIT,
                             mlx90640_refreshrate_t p_refresh_rate = MLX90640_2_HZ) {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mlx->begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println(F("MLX90640 not found!"));
    return NULL;
  }
  Serial.println(F("Found Adafruit MLX90640"));

  mlx->setMode(p_mode);
  mlx->setResolution(p_resolution);
  mlx->setRefreshRate(p_refresh_rate);

  return mlx;
}

command_entry_t COMMANDS[] = {
  { "READ", read_data, "Read summarized sensor data" },
  { "IR", send_ir_image, "Return IR data as an ASCII art" },
  { "IRX", send_irx_image, "Return IR data as a base64 stream" },
  // { "IRB", send_irb_image, "Return IR data as a base64 lzw-compressed stream" },
  // { "IRBT", send_irbt_image, "Test IR data as a base64 lzw-compressed stream" },
  { "IRT", send_irt_image, "Test IR base64 binary encoding and decoding" },
  { "PING", show_ping, "Echo current version" },
  { "START", start_data_collection, "Start data collection" },
  { "STOP", stop_data_collection, "Stop data collection" },
  { "HELP", show_help, "Show available commands" }
};

size_t MAX_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

void show_mlx_status() {
  Serial.print(F(",mlx_serial="));
  Serial.print(mlx->serialNumber[0], HEX);
  Serial.print(mlx->serialNumber[1], HEX);
  Serial.print(mlx->serialNumber[2], HEX);

  Serial.print(F(",mlx_mode="));
  if (mlx->getMode() == MLX90640_CHESS) {
    Serial.print(F("chess"));
  } else {
    Serial.print(F("interleave"));
  }

  Serial.print(F(",mlx_resolution="));
  mlx90640_resolution_t res = mlx->getResolution();
  switch (res) {
    case MLX90640_ADC_16BIT: Serial.print(F("16bit")); break;
    case MLX90640_ADC_17BIT: Serial.print(F("17bit")); break;
    case MLX90640_ADC_18BIT: Serial.print(F("18bit")); break;
    case MLX90640_ADC_19BIT: Serial.print(F("19bit")); break;
  }
  Serial.print(F(",mlx_frame_rate="));
  mlx90640_refreshrate_t rate = mlx->getRefreshRate();
  switch (rate) {
    case MLX90640_0_5_HZ: Serial.print(F("0.5Hz")); break;
    case MLX90640_1_HZ: Serial.print(F("1Hz")); break;
    case MLX90640_2_HZ: Serial.print(F("2Hz")); break;
    case MLX90640_4_HZ: Serial.print(F("4Hz")); break;
    case MLX90640_8_HZ: Serial.print(F("8Hz")); break;
    case MLX90640_16_HZ: Serial.print(F("16Hz")); break;
    case MLX90640_32_HZ: Serial.print(F("32Hz")); break;
    case MLX90640_64_HZ: Serial.print(F("64Hz")); break;
  }
  Serial.println();
}

void show_mlx_data() {
  Serial.println(F("==================================="));
  Serial.print(F("MLX Ambient temperature = "));
  Serial.print(mlx->getTa(false));  // false = no new frame capture
  Serial.println(F(" degC"));
  Serial.println();
  Serial.println();
  show_frame(frame);
}

// Function for calculating median
double calculate_median(float *v, int n) {
  // Sort the vector
  qsort(v, n, sizeof(float), compare_floats);

  // Check if the number of elements is odd
  if (n % 2 != 0)
    return (double)v[n / 2];

  // If the number of elements is even, return the average
  // of the two middle elements
  return (double)(v[(n - 1) / 2] + v[n / 2]) / 2.0;
}


int compare_floats(const void *a, const void *b) {
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

void show_frame(float *frame) {

  for (uint8_t h = 0; h < IR_IMAGE_ROWS; h++) {
    for (uint8_t w = 0; w < IR_IMAGE_COLS; w++) {
      float t = frame[h * IR_IMAGE_COLS + w];
#ifdef PRINT_TEMPERATURES
      Serial.print(t, 1);
      Serial.print(F(", "));
#endif
#ifdef PRINT_ASCIIART
      char c = '&';
      if (t < 20) c = ' ';
      else if (t < 23) c = '.';
      else if (t < 25) c = '_';
      else if (t < 27) c = '-';
      else if (t < 29) c = '+';
      else if (t < 31) c = 'o';
      else if (t < 33) c = 'x';
      else if (t < 35) c = '*';
      else if (t < 37) c = 'O';
      Serial.print(c);
#endif
    }
    Serial.println();
  }
}

void show_median(float *frame) {
  float median = calculate_median(frame, frame_size);

  Serial.print(F("Median frame temperature = "));
  Serial.print(median);
  Serial.println(F(" degC"));
  Serial.println();
}

char *encode_base64(uint8_t *data, int size) {
  auto output_size = base64::encodeLength(size);
  char *output = (char *)malloc(output_size);
  // Serial.printf("input size: %d\n", size);
  base64::encode(data, size, output);
  // Serial.printf("encoded size: %d\n", output_size);
  return output;
}

void send_base64_encode(uint8_t *data, int size) {
  char *msg_base64 = encode_base64(data, size);
  Serial.println(msg_base64);
  free(msg_base64);
}

uint8_t *decode_base64(char *input, size_t *output_size) {
  *output_size = base64::decodeLength(input);
  uint8_t *output = (uint8_t *)malloc(sizeof(uint8_t) * (*output_size));
  Serial.print(F("string input size="));
  Serial.print(strlen(input));
  base64::decode(input, output);
  Serial.print(F(",binary decoded size="));
  Serial.print(*output_size);
  Serial.print(F(","));
  return output;
}

void test_base64_encode(uint8_t *data, int size) {
  char *output = encode_base64(data, size);
  size_t decode_size;
  uint8_t *test = decode_base64(output, &decode_size);
  if (memcmp(test, frame, size) == 0) {
    Serial.println(F("OK: Encoding and decoding"));
  } else {
    Serial.println(F("ERROR: Encoding and decoding"));
  }

  free(output);
  free(test);
}


void loop_mlx() {
  if (start_mlx) {
    if (mlx && mlx->getFrame(frame) == 0) {
      // show_median(frame);
    } else {
      Serial.println(F("Failed"));
      // Clear bus before Wire.begin()
      i2cbusstatus = I2Cbus_clear(I2C_SDA, I2C_SCL);
      Serial.println(I2Cbus_statusstr(i2cbusstatus));

      // Enable I2C
      Wire.begin(SDA, SCL);  // For ESP8266 NodeMCU boards [VDD to 3V3, GND to GND, SDA to D2, SCL to D1]
      delay(500);
      ESP.reset();
      return;
    }
  }
}


/* 
  DHT code
*/

void setup_dht() {
  dht.begin();
}

void loop_dht() {
  if (start_dht) {
    read_data_dht();
  }
}

void read_data_dht() {
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  humidity = h;
  temp = t;
}

/* 
  Photoresistor code
*/

void setup_ldr() {
  ldr.begin();
}

void loop_ldr() {
  if (start_ldr) {
    read_data_ldr();
  }
}

void read_data_ldr() {
  bool ldr_status = ldr.read_status();

  if (isnan(ldr_status)) {
    Serial.println(F("Failed to read from photoresistor sensor!"));
    return;
  }
  is_night = ldr_status;
}

/*
  Serial commands
*/
void setup_serial_commands() {
  // Setup callbacks for SerialCommand commands
  for (int i = 0; i < MAX_COMMANDS; i++) {
    sCmd.addCommand(COMMANDS[i].name, COMMANDS[i].callback);  // Read summarized sensor data
  }

  sCmd.setDefaultHandler(unrecognized);  // Handler for command that isn't matched  (says "NACK")
  show_ping();
}

/*
  Start data collection command
*/
void start_data_collection() {
  char *arg;

  arg = sCmd.next();
  if (arg != NULL) {
    char *sensor_to_start = arg;
    if (strcmp(sensor_to_start, "DHT") == 0) {
      start_dht = true;
    } else if (strcmp(sensor_to_start, "MLX") == 0) {
      start_mlx = true;
    } else if (strcmp(sensor_to_start, "LDR") == 0) {
      start_ldr = true;
    } else {
      return;
    }
    Serial.print(F("STARTING "));
    Serial.print(arg);
    Serial.println(F(" SENSOR"));
  } else {
    Serial.println(F("STARTING ALL SENSORS"));
    start_dht = start_mlx = start_ldr = true;
  }
}

/*
  Stop data collection command
*/
void stop_data_collection() {
  char *arg;

  arg = sCmd.next();
  if (arg != NULL) {
    char *sensor_to_stop = arg;
    if (strcmp(sensor_to_stop, "DHT") == 0) {
      start_dht = false;
    } else if (strcmp(sensor_to_stop, "MLX") == 0) {
      start_mlx = false;
    } else if (strcmp(sensor_to_stop, "LDR") == 0) {
      start_ldr = false;
    } else {
      return;
    }
    Serial.print(F("STOPING "));
    Serial.print(arg);
    Serial.println(F(" SENSOR"));
  } else {
    Serial.println(F("STOPING ALL SENSORS"));
    start_dht = start_mlx = start_ldr = false;
  }
}

void loop_serial_commands() {
  sCmd.readSerial();  // We don't do much, just process serial commands
}

/*
  Default handler for commands. This gets set as the default handler, and gets called when no other command matches.
*/
void unrecognized(const char *command) {
  Serial.print(F("NACK["));
  Serial.print(command);
  Serial.println(F("]"));
}


/*
  Send sensor data over the serial line.
*/
void read_data() {
  Serial.print(F("c:"));
  float median = calculate_median(frame, frame_size);
  Serial.print(median);

  Serial.print(F(",t:"));
  Serial.print(temp);

  Serial.print(F(",h:"));
  Serial.print(humidity);

  Serial.print(F(",n:"));
  Serial.print(is_night);
  Serial.println();
}

/*
  Send raw IR as ASCII data over the serial line.
*/
void send_ir_image() {
  Serial.print(F("ir:"));
  show_frame(frame);
  Serial.println();
}

/*
  Send raw IR as base64 data over the serial line.
*/
void send_irx_image() {
  Serial.print(F("irx:"));
  send_base64_encode((uint8_t *)frame, sizeof(float) * frame_size);
}


/*
  Test IR image encoding and decoding
*/
void send_irt_image() {
  Serial.print(F("irt:"));
  test_base64_encode((uint8_t *)frame, sizeof(float) * frame_size);
}

/*
  Show status data over the serial line.
*/
void show_ping() {
  const char *ENABLED = "on";
  const char *DISABLED = "off";
  Serial.print(VERSION);
  Serial.print(F(",status=[mlx="));
  Serial.print(start_mlx ? ENABLED : DISABLED);
  Serial.print(F(",dht="));
  Serial.print(start_dht ? ENABLED : DISABLED);
  Serial.print(F(",ldr="));
  Serial.print(start_ldr ? ENABLED : DISABLED);
  Serial.print(F("],data="));
  read_data();
  // show_mlx_status();
}

/*
  Show available commands
*/
void show_help() {
  show_ping();
  for (int i = 0; i < MAX_COMMANDS; i++) {
    command_entry_t command = COMMANDS[i];
    Serial.print(command.name);
    Serial.print(" ");
    Serial.println(command.description);
  }
  Serial.println();
}

/*
  Serial setup
*/
void setup_serial() {
  Serial.begin(BAUD_RATE);
  Serial.println();
  while (!Serial) {
    Serial.println(F("Waiting for Serial line"));
    delay(100);
  }
}

void setup() {
  setup_serial();
  delay(50);
  setup_dht();
  delay(50);
  setup_mlx();
  setup_ldr();
  delay(50);
  setup_serial_commands();
}

void loop() {
  delay(50);
  loop_dht();
  loop_mlx();
  loop_serial_commands();
  loop_ldr();
  Serial.flush();
}
