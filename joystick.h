#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <stdint.h>

#include <vector>

#include "config.h"
#include "utils.h"

class CenteringPotentialMeterDriver {
 public:
  CenteringPotentialMeterDriver(uint8_t adc_pin, size_t smooth_buffer_size,
                                bool flip_dir);

  // Translate the raw ADC readings to USB offsets, from -127 to 127
  int8_t GetValue();

  // Get samples and other stuffs
  void Tick();

 protected:
  uint8_t adc_;
  uint16_t origin_;
  std::vector<uint16_t> buffer_;
  uint32_t buffer_idx_;
  int32_t sum_;
  bool flip_dir_;
};

#endif /* JOYSTICK_H_ */
