#include <Adafruit_MLX90640.h>

Adafruit_MLX90640 mlx;

const int IR_IMAGE_ROWS = 24;
const int IR_IMAGE_COLS = 32;
const int frame_size = IR_IMAGE_COLS * IR_IMAGE_ROWS;

float frame[frame_size];  // buffer for full frame of temperatures

// uncomment *one* of the below
//#define PRINT_TEMPERATURES
#define PRINT_ASCIIART

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(100);

  Serial.println("Adafruit MLX90640 Simple Test");
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 not found!");
    while (1) delay(10);
  }
  Serial.println("Found Adafruit MLX90640");

  Serial.print("Serial number: ");
  Serial.print(mlx.serialNumber[0], HEX);
  Serial.print(mlx.serialNumber[1], HEX);
  Serial.println(mlx.serialNumber[2], HEX);

  //mlx.setMode(MLX90640_INTERLEAVED);
  mlx.setMode(MLX90640_CHESS);
  Serial.print("Current mode: ");
  if (mlx.getMode() == MLX90640_CHESS) {
    Serial.println("Chess");
  } else {
    Serial.println("Interleave");
  }

  mlx.setResolution(MLX90640_ADC_18BIT);
  Serial.print("Current resolution: ");
  mlx90640_resolution_t res = mlx.getResolution();
  switch (res) {
    case MLX90640_ADC_16BIT: Serial.println("16 bit"); break;
    case MLX90640_ADC_17BIT: Serial.println("17 bit"); break;
    case MLX90640_ADC_18BIT: Serial.println("18 bit"); break;
    case MLX90640_ADC_19BIT: Serial.println("19 bit"); break;
  }

  mlx.setRefreshRate(MLX90640_2_HZ);
  Serial.print("Current frame rate: ");
  mlx90640_refreshrate_t rate = mlx.getRefreshRate();
  switch (rate) {
    case MLX90640_0_5_HZ: Serial.println("0.5 Hz"); break;
    case MLX90640_1_HZ: Serial.println("1 Hz"); break;
    case MLX90640_2_HZ: Serial.println("2 Hz"); break;
    case MLX90640_4_HZ: Serial.println("4 Hz"); break;
    case MLX90640_8_HZ: Serial.println("8 Hz"); break;
    case MLX90640_16_HZ: Serial.println("16 Hz"); break;
    case MLX90640_32_HZ: Serial.println("32 Hz"); break;
    case MLX90640_64_HZ: Serial.println("64 Hz"); break;
  }
}

void show_mlx_data(Adafruit_MLX90640 &mlx, float *frame) {
  Serial.println("===================================");
  Serial.print("MLX Ambient temperature = ");
  Serial.print(mlx.getTa(false));  // false = no new frame capture
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

void loop() {

  delay(1000);
  if (mlx.getFrame(frame) != 0) {
    Serial.println("Failed");
    return;
  }

  show_median((float *)&frame);

  // show_mlx_data(mlx, (float *)&frame);
}