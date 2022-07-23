#ifndef LAYOUT_H_
#define LAYOUT_H_

#include <array>
#include <cstdint>
#include <stdexcept>

struct Keycode {
  uint8_t keycode;
  bool is_custom : 1;
  uint8_t custom_info : 7;
};

struct GPIO {
  uint8_t row;  // in
  uint8_t col;  // out
};

size_t GetKeyboardNumLayers();
size_t GetNumSinkGPIOs();
size_t GetNumSourceGPIOs();

uint8_t GetSinkGPIO(size_t idx);
uint8_t GetSourceGPIO(size_t idx);
Keycode GetKeycodeAtLayer(uint8_t layer, size_t sink_gpio_idx,
                          size_t source_gpio_idx);

enum BuiltInCustomKeyCode {
  MSE_L = 0,
  MSE_R,
  MSE_M,
  MSE_BACK,
  MSE_FORWARD,
  LAYER_SWITCH,
  ENTER_CONFIG,
  CONFIG_SEL,
  BOOTSEL,
  REBOOT,
  TOTAL_BUILT_IN_KC
};

#endif /* LAYOUT_H_ */