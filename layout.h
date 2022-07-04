#ifndef LAYOUT_H_
#define LAYOUT_H_

#include <array>
#include <cstdint>
#include <stdexcept>

namespace keyboard {

struct Keycode {
  uint8_t keycode;
  bool is_custom;
};

struct GPIO {
  uint8_t in;
  uint8_t out;
};

size_t GetKeyboardNumLayers();
size_t GetNumOutGPIOs();
size_t GetNumInGPIOs();

uint8_t GetOutGPIO(size_t idx);
uint8_t GetInGPIO(size_t idx);
Keycode GetKeycode(size_t in_gpio_idx, size_t out_gpio_idx);

enum CustomKeyCode {
  ENCODER = 0,  // Rotary encoder button
  JY,           // Joystick button
  MSE_L,
  MSE_R,
  FN1,
  FN2
};

}  // namespace keyboard

#endif /* LAYOUT_H_ */