#include "keyscan.h"

#include <stdio.h>

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/gpio.h"
#include "layout.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include "tusb.h"
#include "utils.h"

class MouseButtonHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed) override {
    key_scan_->SetMouseButtonState(kc.keycode, is_pressed);
  }

  std::string GetName() const override { return "Mouse key handler"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_L, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_R, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_M, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_BACK, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_FORWARD, true, MouseButtonHandler);

class LayerButtonHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed) override {
    const bool toggle = kc.custom_info & 0x40;
    const uint8_t layer = kc.custom_info & 0x3f;
    if (is_pressed) {
      if (toggle) {
        key_scan_->ToggleLayerStatus(layer);
      } else {
        key_scan_->SetLayerStatus(layer, true);
      }
    } else {
      if (!toggle) {
        key_scan_->SetLayerStatus(layer, false);
      }
    }
  }

  std::string GetName() const override { return "Layer switch key handler"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(LAYER_SWITCH, true, LayerButtonHandler);

std::shared_ptr<KeyScan> KeyScan::Create(const Configuration* config) {
  static std::shared_ptr<KeyScan> singleton = NULL;
  if (singleton == NULL) {
    singleton = std::shared_ptr<KeyScan>(new KeyScan());
  }
  return singleton;
}

void KeyScan::SetMouseButtonState(uint8_t mouse_key, bool is_pressed) {
  for (auto output : mouse_output_) {
    if (is_pressed) {
      output->MouseKeycode(mouse_key);
    }
  }
}

void KeyScan::Tick() {
  bool is_config_mode;
  {
    LockSemaphore lock(semaphore_);
    is_config_mode = is_config_mode_;
  }
  const std::vector<uint8_t> active_layers = GetActiveLayers();

  // Set all sink GPIOs to HIGH
  for (size_t sink = 0; sink < GetNumSinkGPIOs(); ++sink) {
    gpio_put(GetSinkGPIO(sink), true);
  }

  std::vector<uint8_t> pressed_keycode;

  for (size_t sink = 0; sink < GetNumSinkGPIOs(); ++sink) {
    // Set one sink pin to LOW
    gpio_put(GetSinkGPIO(sink), false);
    SinkGPIODelay();

    for (size_t source = 0; source < GetNumSourceGPIOs(); ++source) {
      DebounceTimer& d_timer =
          debounce_timer_[sink * GetNumSourceGPIOs() + source];

      // gpio_get returns true when no button is pressed (because of pull up)
      // and  false when a button is pressed.
      const bool pressed = !gpio_get(GetSourceGPIO(source));
      if (pressed != d_timer.pressed) {
        d_timer.tick_count += CONFIG_SCAN_TICKS;
        if (d_timer.tick_count >= CONFIG_DEBOUNCE_TICKS) {
          d_timer.pressed = !d_timer.pressed;
          d_timer.tick_count = 0;
        }
      }

      Keycode kc = {0};
      for (uint8_t l : active_layers) {
        Keycode layer_kc = GetKeycodeAtLayer(l, sink, source);
        if (layer_kc.is_custom || layer_kc.keycode != HID_KEY_NONE) {
          kc = layer_kc;
          break;
        }
      }

      if (kc.is_custom) {
        auto* handler =
            HandlerRegistry::RegisteredHandlerFactory(kc.keycode, this);
        if (handler != NULL) {
          handler->ProcessKeyState(kc, d_timer.pressed);
        } else {
          LOG_WARNING("Custom Keycode (%d) missing handler", kc.keycode);
        }
      } else if (d_timer.pressed) {
        pressed_keycode.push_back(kc.keycode);
      }
    }

    // Set the sink back to high
    gpio_put(GetSinkGPIO(sink), true);
  }

  NotifyOutput(pressed_keycode);
}

void KeyScan::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  is_config_mode_ = is_config_mode;
}

status KeyScan::RegisterCustomKeycodeHandler(
    uint8_t keycode, bool overridable, CustomKeycodeHandlerCreator creator) {
  return HandlerRegistry::RegisterHandler(keycode, overridable, creator);
}

KeyScan::KeyScan() : is_config_mode_(false) {
  for (size_t i = 0; i < GetNumSinkGPIOs(); ++i) {
    const uint8_t pin = GetSinkGPIO(i);
    gpio_init(pin);
    gpio_set_dir(pin, true);
  }
  for (size_t i = 0; i < GetNumSourceGPIOs(); ++i) {
    const uint8_t pin = GetSourceGPIO(i);
    gpio_init(pin);

    // Set the source GPIO to be INPUT with pullup
    gpio_set_dir(pin, false);
    gpio_pull_up(pin);
  }

  debounce_timer_.resize(GetNumSinkGPIOs() * GetNumSourceGPIOs());

  active_layers_.resize(GetKeyboardNumLayers());
  active_layers_[0] = true;

  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);
}

Status KeyScan::SetLayerStatus(uint8_t layer, bool active) {
  LockSemaphore lock(semaphore_);
  if (layer > active_layers_.size()) {
    return ERROR;
  }
  if (layer == 0) {
    return OK;
  }
  active_layers_[layer] = active;
  return OK;
}

Status KeyScan::ToggleLayerStatus(uint8_t layer) {
  LockSemaphore lock(semaphore_);
  if (layer > active_layers_.size()) {
    return ERROR;
  }
  if (layer == 0) {
    return OK;
  }
  active_layers_[layer] = !active_layers_[layer];
  return OK;
}

std::vector<uint8_t> KeyScan::GetActiveLayers() {
  LockSemaphore lock(semaphore_);
  std::vector<uint8_t> output;
  for (int16_t i = active_layers_.size() - 1; i >= 0; --i) {
    if (active_layers_[i]) {
      output.push_back(i);
    }
  }
  return output;
}

void KeyScan::SinkGPIODelay() { sleep_us(CONFIG_GPIO_SINK_DELAY_US); }

KeyScan::HandlerRegistry* KeyScan::HandlerRegistry::GetRegistry() {
  static HandlerRegistry instance;
  return &instance;
}

status KeyScan::HandlerRegistry::RegisterHandler(
    uint8_t keycode, bool overridable, CustomKeycodeHandlerCreator creator) {
  HandlerRegistry* instance = GetRegistry();
  auto it = instance->custom_handlers_.find(keycode);
  if (it != instance->custom_handlers_.end()) {
    // Not overridable
    if (!it->second.first) {
      return ERROR;
    }
  }
  instance->custom_handlers_.insert(
      {keycode, std::make_pair(overridable, creator)});
  return OK;
}

CustomKeycodeHandler* KeyScan::HandlerRegistry::RegisteredHandlerFactory(
    uint8_t keycode, KeyScan* outer) {
  HandlerRegistry* instance = GetRegistry();
  auto it = instance->handler_singletons_.find(keycode);
  if (it != instance->handler_singletons_.end()) {
    return it->second;
  } else {
    auto creator_it = instance->custom_handlers_.find(keycode);
    if (creator_it == instance->custom_handlers_.end()) {
      return NULL;
    }
    auto* handler = creator_it->second.second();
    handler->SetOuterClass(outer);
    instance->handler_singletons_[keycode] = handler;
    return handler;
  }
}

void KeyScan::NotifyOutput(const std::vector<uint8_t>& pressed_keycode) {
  for (auto* output : keyboard_output_) {
    output->SendKeycode(pressed_keycode);
  }
}

static Status registered =
    DeviceRegistry::RegisterInputDevice(1, true, KeyScan::Create);
