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
  uint8_t row; // in
  uint8_t col; // out
};

size_t GetKeyboardNumLayers();
size_t GetNumSinkGPIOs();
size_t GetNumSourceGPIOs();

uint8_t GetSinkGPIO(size_t idx);
uint8_t GetSourceGPIO(size_t idx);
Keycode GetKeycode(size_t layer, size_t sink_gpio_idx, size_t source_gpio_idx);

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