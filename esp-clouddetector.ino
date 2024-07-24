#include <Adafruit_MLX90640.h>
#include <SerialCommand.h>
#include "DHT.h"
#include "arduino_base64.hpp"
#include "microlzw.h"

/*
Author: Orestes Sanchez <miceno.atreides@gmail.com>
*/

typedef void (*cmd_callback_t)();

typedef struct {
  const char *name;
  cmd_callback_t callback;
  const char *description;
} command_entry_t;


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

// Compression dict size
const int LZW_DICT_SIZE = 1024;

const char *VERSION = "cloud-0.3.0";

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
Adafruit_MLX90640 *setup_mlx(mlx90640_mode_t p_mode = MLX90640_CHESS,
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

command_entry_t COMMANDS[] = {
  {"READ", send_data, "Read summarized sensor data"},
  {"IR", send_ir_image, "Return IR data as a stream of float values"},
  {"IRX", send_irx_image, "Return IR data as a base64 stream"},
  {"IRB", send_irb_image, "Return IR data as a base64 lzw-compressed stream"},
  {"IRT", send_irt_image, "Test IR binary encoding and decoding"},
  {"PING", show_ping, "Echo current version"},
  {"START", start_data_collection, "Start data collection"},
  {"STOP", stop_data_collection, "Stop data collection"},
  { "HELP", show_help, "Show available commands" }
};

size_t MAX_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

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
  float median = calculate_median(frame, frame_size);

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
  char *msg_base64 = encode_base64(data, size);
  Serial.println(msg_base64);
  free(msg_base64);
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


void loop_mlx() {
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

void setup_dht() {
  dht.begin();
}

void loop_dht() {
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

void loop_serial_commands() {
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
  float median = calculate_median(frame, frame_size);
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
  Send raw IR as base64 data over the serial line.
*/
void send_irx_image() {
  Serial.print("irx:");
  send_base64_encode((uint8_t *)frame, sizeof(float) * frame_size);
}

/*
  Send raw IR as binary data over the serial line.
*/
void send_irb_image() {
  Serial.print("irb:");
  char *msg_base64 = compress_irb_image((uint8_t *)frame, sizeof(float) * frame_size);
  Serial.println(msg_base64);
  free(msg_base64);
}

char *compress_irb_image(uint8_t *data, size_t size) {
  uint8_t *compressed = (uint8_t *)malloc(size);
  size_t comp_size = 0;

  // Serial.printf("size=%d\n", size);
  mlzw_compress_binary(data, size, compressed, &comp_size, LZW_DICT_SIZE);
  // Serial.printf("comp_size=%d ", comp_size);
  char *msg_base64 = encode_base64(compressed, comp_size);
  free(compressed);
  Serial.print("ratio=");
  Serial.print(1.0 * comp_size / size * 100.0);
  Serial.print(" ");
  // Serial.printf("base64_size=%d ", strlen(msg_base64));
  return msg_base64;
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
  Serial.print(calculate_median(frame, frame_size));
  Serial.print(",temp=");
  Serial.print(temp);
  Serial.print(",hum=");
  Serial.print(humidity);
  show_mlx_status();
}

/*
  Show available commands
*/
void show_help() {
  show_ping();
  for(int i=0; i<MAX_COMMANDS; i++){
    command_entry_t command = COMMANDS[i];
    Serial.printf("%s: %s\n", command.name, command.description);
  }
  Serial.println();
}

/*
  Serial setup
*/
void setup_serial() {
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
  setup_serial();
  delay(50);
  setup_dht();
  delay(50);
  setup_mlx();
  setup_serial_commands();
}

void loop() {
  delay(50);
  loop_dht();
  loop_mlx();
  loop_serial_commands();
}
