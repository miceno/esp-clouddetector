#include <Adafruit_MLX90640.h>
#include <SerialCommand.h>
#include "DHT.h"
#include "arduino_base64.hpp"

/*
Author: Orestes Sanchez <miceno.atreides@gmail.com>
*/

#define DHTPIN D6      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321

Adafruit_MLX90640 *mlx = new Adafruit_MLX90640();
SerialCommand sCmd;  // The demo SerialCommand object

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

// MLX90640 camera is 32 x 24
const int IR_IMAGE_COLS = 32;
const int IR_IMAGE_ROWS = 24;
const int frame_size = IR_IMAGE_COLS * IR_IMAGE_ROWS;

const char *VERSION = "cloud-0.2.0";

// Internal variables for the status of the detector.
float frame[frame_size];  // buffer for full frame of temperatures
float temp = 0.0;         // current temperature from DHT22 sensor
float humidity = 0.0;     // current humidity from DHT22 sensor
boolean is_day = false;   // represents if it is day or night according to the sensor

// start_SENSOR variables control if the controller is updating its internal
// state using data from the sensor.
boolean start_dht = true;
boolean start_mlx = true;
boolean start_day = true;

// uncomment *one* of the below
//#define PRINT_TEMPERATURES
#define PRINT_ASCIIART

/*
  MLX 90640 code
*/
Adafruit_MLX90640 *setupMlx(mlx90640_mode_t p_mode = MLX90640_CHESS,
                            mlx90640_resolution_t p_resolution = MLX90640_ADC_18BIT,
                            mlx90640_refreshrate_t p_refresh_rate = MLX90640_2_HZ) {
  if (!mlx->begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 not found!");
    return NULL;
  }
  Serial.println("Found Adafruit MLX90640");

  mlx->setMode(p_mode);
  mlx->setResolution(p_resolution);
  mlx->setRefreshRate(p_refresh_rate);

  return mlx;
}

void show_mlx_status() {
  Serial.print(",mlx_serial=");
  Serial.print(mlx->serialNumber[0], HEX);
  Serial.print(mlx->serialNumber[1], HEX);
  Serial.print(mlx->serialNumber[2], HEX);

  Serial.print(",mlx_mode=");
  if (mlx->getMode() == MLX90640_CHESS) {
    Serial.print("chess");
  } else {
    Serial.print("interleave");
  }

  Serial.print(",mlx_resolution=");
  mlx90640_resolution_t res = mlx->getResolution();
  switch (res) {
    case MLX90640_ADC_16BIT: Serial.print("16bit"); break;
    case MLX90640_ADC_17BIT: Serial.print("17bit"); break;
    case MLX90640_ADC_18BIT: Serial.print("18bit"); break;
    case MLX90640_ADC_19BIT: Serial.print("19bit"); break;
  }
  Serial.print(",mlx_frame_rate=");
  mlx90640_refreshrate_t rate = mlx->getRefreshRate();
  switch (rate) {
    case MLX90640_0_5_HZ: Serial.print("0.5Hz"); break;
    case MLX90640_1_HZ: Serial.print("1Hz"); break;
    case MLX90640_2_HZ: Serial.print("2Hz"); break;
    case MLX90640_4_HZ: Serial.print("4Hz"); break;
    case MLX90640_8_HZ: Serial.print("8Hz"); break;
    case MLX90640_16_HZ: Serial.print("16Hz"); break;
    case MLX90640_32_HZ: Serial.print("32Hz"); break;
    case MLX90640_64_HZ: Serial.print("64Hz"); break;
  }
  Serial.println();
}

void show_mlx_data() {
  Serial.println("===================================");
  Serial.print("MLX Ambient temperature = ");
  Serial.print(mlx->getTa(false));  // false = no new frame capture
  Serial.println(" degC");
  Serial.println();
  Serial.println();
  show_frame(frame);
}

// Function for calculating median
double Median(float *v, int n) {
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
      Serial.print(", ");
#endif
#ifdef PRINT_ASCIIART
      char c = '&';
      if (t < 20) c = ' ';
      else if (t < 23) c = '.';
      else if (t < 25) c = '-';
      else if (t < 27) c = '*';
      else if (t < 29) c = '+';
      else if (t < 31) c = 'x';
      else if (t < 33) c = '%';
      else if (t < 35) c = '#';
      else if (t < 37) c = 'X';
      Serial.print(c);
#endif
    }
    Serial.println();
  }
}

void show_median(float *frame) {
  float median = Median(frame, frame_size);

  Serial.print("Median frame temperature = ");
  Serial.print(median);
  Serial.println(" degC");
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
  char *output = encode_base64(data, size);
  char compressed[size];
  mlzw_compress((char*)output, compressed, &comp_size, dict_size);

  Serial.println(output);
  free(output);
}

void *decode_base64(char *input) {
  auto output_size = base64::decodeLength(input);
  uint8_t *output = (uint8_t *)malloc(output_size);
  // Serial.printf("input size: %d\n", strlen(input));
  base64::decode(input, output);
  // Serial.printf("decoded size: %d\n", output_size);
  return output;
}

void test_base64_encode(uint8_t *data, int size) {
  char *output = encode_base64(data, size);

  void *test = decode_base64(output);
  if (memcmp(test, frame, size) == 0) {
    Serial.println("OK: Encoding and decoding");
  } else {
    Serial.println("ERROR: Encoding and decoding");
  }

  free(output);
  free(test);
}


void loopMlx() {
  if (start_mlx) {
    if (mlx && mlx->getFrame(frame) == 0) {
      // show_median(frame);
    } else {
      Serial.println("Failed");
      return;
    }
  }
}


/* 
  DHT code
*/

void setupDHT() {
  dht.begin();
}

void loopDHT() {
  if (start_dht) {
    read_data();
  }
}

void read_data() {
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
  Serial commands
*/
void setupSerialCommands() {
  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("READ", send_data);               // Read summarized sensor data
  sCmd.addCommand("IR", send_ir_image);             // Return IR data as a stream of float values
  sCmd.addCommand("IRX", send_irx_image);           // Return IR data as a stream of float values
  sCmd.addCommand("IRT", send_irt_image);           // Test IR binary encoding and decoding
  sCmd.addCommand("PING", show_ping);               // Echo current version
  sCmd.addCommand("START", start_data_collection);  // Start data collection
  sCmd.addCommand("STOP", stop_data_collection);    // Stop data collection
  sCmd.setDefaultHandler(unrecognized);             // Handler for command that isn't matched  (says "NACK")
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
    } else if (strcmp(sensor_to_start, "DAY") == 0) {
      start_day = true;
    } else {
      return;
    }
    Serial.printf("STARTING %s SENSOR\n", arg);
  } else {
    Serial.println("STARTING ALL SENSORS");
    start_dht = start_mlx = start_day = true;
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
    } else if (strcmp(sensor_to_stop, "DAY") == 0) {
      start_day = false;
    } else {
      return;
    }
    Serial.printf("STOPING %s SENSOR\n", arg);
  } else {
    Serial.println("STOPING ALL SENSORS");
    start_dht = start_mlx = start_day = false;
  }
}

void loopSerialCommands() {
  sCmd.readSerial();  // We don't do much, just process serial commands
}

/*
  Default handler for commands. This gets set as the default handler, and gets called when no other command matches.
*/
void unrecognized(const char *command) {
  Serial.print("NACK[");
  Serial.print(command);
  Serial.println("]");
}


/*
  Send sensor data over the serial line.
*/
void send_data() {
  Serial.print("cloud:");
  float median = Median(frame, frame_size);
  Serial.print(median);

  Serial.print(",temp:");
  Serial.print(temp);

  Serial.print(",hum:");
  Serial.print(humidity);

  Serial.print(",day:");
  Serial.print(is_day);
  Serial.println();
}

/*
  Send raw IR as ASCII data over the serial line.
*/
void send_ir_image() {
  Serial.print("ir:");
  show_frame(frame);
}

/*
  Send raw IR as binary data over the serial line.
*/
void send_irx_image() {
  Serial.print("irx:");
  send_base64_encode((uint8_t *)frame, sizeof(float) * frame_size);
}

/*
  Test IR image encoding and decoding
*/
void send_irt_image() {
  Serial.print("irt:");
  test_base64_encode((uint8_t *)frame, sizeof(float) * frame_size);
}


/*
  Show status data over the serial line.
*/
void show_ping() {
  Serial.print(VERSION);
  Serial.printf(",dht=%d,mlx=%d,day=%d", start_dht, start_mlx, start_day);
  Serial.print(",cloud=");
  Serial.print(Median(frame, frame_size));
  Serial.print(",temp=");
  Serial.print(temp);
  Serial.print(",hum=");
  Serial.print(humidity);
  show_mlx_status();
  Serial.println();
}

/*
  Serial setup
*/
void setupSerial() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Waiting for Serial line");
  while (!Serial) {
    Serial.print(".");
    delay(10);
  }
  Serial.println();
}

void setup() {
  setupSerial();
  delay(50);
  setupDHT();
  delay(50);
  setupMlx();
  setupSerialCommands();
}

void loop() {
  delay(50);
  loopDHT();
  loopMlx();
  loopSerialCommands();
}