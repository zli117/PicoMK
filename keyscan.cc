#include "keyscan.h"

#include <hardware/gpio.h>
#include <layout.h>
#include <cstdint>

namespace keyboard {

void keyscan_init() {
  for (size_t i = 0; i < GetNumInGPIOs(); ++i) {
    const uint8_t pin = GetInGPIO(i);
    gpio_init(pin);
    gpio_set_dir(pin, false);
  }
  for (size_t i = 0; i < GetNumOutGPIOs(); ++i) {
    const uint8_t pin = GetOutGPIO(i);
    gpio_init(pin);
    gpio_set_dir(pin, true);
  }
}

}  // namespace keyboard

extern "C" void keyscan_task() {
  using namespace keyboard;
}