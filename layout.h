#ifndef LAYOUT_H_
#define LAYOUT_H_

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>

struct Keycode {
  uint8_t keycode;
  bool is_custom : 1;
  uint8_t custom_info : 7;
};

struct GPIO {
  uint8_t source;  // in
  uint8_t sink;    // out
};

size_t GetKeyboardNumLayers();
size_t GetTotalNumGPIOs();
uint8_t GetGPIOPin(size_t gpio_idx);

size_t GetTotalScans();
uint8_t GetSourceGPIO(size_t scan_idx);
uint8_t GetSinkGPIO(size_t scan_idx);
bool IsSourceChange(size_t scan_idx);
Keycode GetKeycodeAtLayer(uint8_t layer, size_t scan_idx);

enum BuiltInCustomKeyCode {
  // Do not change the order of the mouse buttons, nor adding new items in
  // between mouse buttons.
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