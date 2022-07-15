#include "rotary_encoder.h"

#include "hardware/gpio.h"
#include "tusb.h"
#include "utils.h"

RotaryEncoder::RotaryEncoder() : RotaryEncoder(0, 0, 1) {}
RotaryEncoder::RotaryEncoder(uint8_t pin_a, uint8_t pin_b, uint8_t resolution)
    : pin_a_(pin_a),
      pin_b_(pin_b),
      resolution_(resolution),
      a_state_(false),
      pulse_count_(0),
      dir_(false),
      is_config_(false) {
  gpio_init(pin_a_);
  gpio_init(pin_b_);
  gpio_set_dir(pin_a_, GPIO_IN);
  gpio_set_dir(pin_b_, GPIO_IN);
  gpio_disable_pulls(pin_a_);
  gpio_disable_pulls(pin_b_);
  a_state_ = gpio_get(pin_a_);
}

void RotaryEncoder::InputTick() {
  const bool a_state = gpio_get(pin_a_);
  if (a_state != a_state_) {
    const bool b_state = gpio_get(pin_b_);
    const bool dir = a_state == b_state;
    if (dir == dir_) {
      if (++pulse_count_ >= resolution_) {
        HandleMovement(dir);
        pulse_count_ = 0;
      }
    } else {
      dir_ = dir;
      pulse_count_ = 1;
    }
    a_state_ = a_state;
  }
}

void RotaryEncoder::SetConfigMode(bool is_config_mode) {
  is_config_ = is_config_mode;
}

void RotaryEncoder::HandleMovement(bool dir) {
  if (is_config_) {
    for (auto* config_out : config_modifier_) {
      if (dir) {
        config_out->Up();
      } else {
        config_out->Down();
      }
    }
  } else {
    for (auto* keyboard_out : keyboard_output_) {
      keyboard_out->SendConsumerKeycode(
          dir ? HID_USAGE_CONSUMER_VOLUME_INCREMENT
              : HID_USAGE_CONSUMER_VOLUME_DECREMENT);
    }
  }
}

static Status registered = DeviceRegistry::RegisterInputDevice(
    3, []() { return std::make_shared<RotaryEncoder>(19, 22, 2); });
