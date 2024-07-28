#ifndef __miceno_lightsensor_h
#define __miceno_lightsensor_h

#include <Arduino.h>

/*!
 *  @brief  Class that stores state and functions for the LDR (light-dependent resistor)
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


#endif