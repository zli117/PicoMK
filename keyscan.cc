#include "keyscan.h"

#include <hardware/gpio.h>
#include <layout.h>
#include <status.h>

status keyscan_init(void) {
  uint8_t is_in[0xff] = {0};
  for (size_t i = 0; i < num_gpio_in_pins; ++i) {
    gpio_init(gpio_in_pins[i]);
    gpio_set_dir(gpio_in_pins[i], false);
    is_in[gpio_in_pins[i]] = 1;
  }
  for (size_t i = 0; i < num_gpio_out_pins; ++i) {
    gpio_init(gpio_out_pins[i]);
    gpio_set_dir(gpio_in_pins[i], true);
  }
  return OK;
}

void keyscan_task(void) {}