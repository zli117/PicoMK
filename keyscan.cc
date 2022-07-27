#include "keyscan.h"

#include <stdio.h>

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "FreeRTOS.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "layout.h"
#include "pico/stdlib.h"
#include "runner.h"
#include "semphr.h"
#include "tusb.h"
#include "utils.h"

void KeyScan::SetMouseButtonState(uint8_t mouse_key, bool is_pressed) {
  for (auto output : *mouse_output_) {
    if (is_pressed) {
      output->MouseKeycode(mouse_key);
    }
  }
}

void KeyScan::ConfigUp() { config_modifier_->Up(); }

void KeyScan::ConfigDown() { config_modifier_->Down(); }

void KeyScan::ConfigSelect() { config_modifier_->Select(); }

void KeyScan::InputLoopStart() { LayerChanged(); }

void KeyScan::InputTick() {
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
      // and false when a button is pressed.
      const bool pressed = !gpio_get(GetSourceGPIO(source));
      bool key_event = false;
      if (pressed != d_timer.pressed) {
        d_timer.tick_count += CONFIG_SCAN_TICKS;
        if (d_timer.tick_count >= CONFIG_DEBOUNCE_TICKS) {
          d_timer.pressed = !d_timer.pressed;
          d_timer.tick_count = 0;
          key_event = true;
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
          handler->ProcessKeyState(kc, d_timer.pressed, sink, source);
          if (key_event) {
            handler->ProcessKeyEvent(kc, d_timer.pressed, sink, source);
          }
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
}

Status KeyScan::SetLayerStatus(uint8_t layer, bool active) {
  if (layer > active_layers_.size()) {
    return ERROR;
  }
  if (layer == 0) {
    return OK;
  }
  if (active_layers_[layer] == active) {
    return OK;
  }
  active_layers_[layer] = active;
  LayerChanged();
  return OK;
}

Status KeyScan::ToggleLayerStatus(uint8_t layer) {
  return SetLayerStatus(layer, !active_layers_[layer]);
}

std::vector<uint8_t> KeyScan::GetActiveLayers() {
  std::vector<uint8_t> output;
  for (int16_t i = active_layers_.size() - 1; i >= 0; --i) {
    if (active_layers_[i]) {
      output.push_back(i);
    }
  }
  return output;
}

void KeyScan::SinkGPIODelay() { busy_wait_us_32(CONFIG_GPIO_SINK_DELAY_US); }

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
  for (auto output : *keyboard_output_) {
    output->SendKeycode(pressed_keycode);
  }
}

void KeyScan::LayerChanged() {
  for (auto output : *keyboard_output_) {
    output->ChangeActiveLayers(active_layers_);
  }
}

Status RegisterKeyscan(uint8_t tag) {
  return DeviceRegistry::RegisterInputDevice(
      tag, []() { return std::shared_ptr<KeyScan>(new KeyScan()); });
}
