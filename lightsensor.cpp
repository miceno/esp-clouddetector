#include "lightsensor.h"

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
