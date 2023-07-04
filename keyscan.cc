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
  std::vector<uint8_t> pressed_keycode;

  for (size_t i = 0; i < GetTotalScans(); ++i) {
    const uint8_t source_pin = GetSourceGPIO(i);
    if (IsSourceChange(i)) {
      // Only source is the output. Others are all input with pull down.
      for (size_t j = 0; j < GetTotalNumGPIOs(); ++j) {
        const uint8_t sink_pin = GetGPIOPin(j);
        if (sink_pin == source_pin) {
          continue;
        }
        gpio_set_dir(sink_pin, false);
        gpio_pull_down(sink_pin);
      }
      gpio_set_dir(source_pin, true);
      gpio_put(source_pin, true);
      SinkGPIODelay();
    }

    DebounceTimer& d_timer = debounce_timer_[i];

    const bool pressed = gpio_get(GetSinkGPIO(i));

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
      Keycode layer_kc = GetKeycodeAtLayer(l, i);
      if (layer_kc.is_custom || layer_kc.keycode != HID_KEY_NONE) {
        kc = layer_kc;
        break;
      }
    }

    if (kc.is_custom) {
      auto* handler =
          HandlerRegistry::RegisteredHandlerFactory(kc.keycode, this);
      if (handler != NULL) {
        handler->ProcessKeyState(kc, d_timer.pressed, i);
        if (key_event) {
          handler->ProcessKeyEvent(kc, d_timer.pressed, i);
        }
      } else {
        LOG_WARNING("Custom Keycode (%d) missing handler", kc.keycode);
      }
    } else if (d_timer.pressed) {
      pressed_keycode.push_back(kc.keycode);
    }
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
  for (size_t i = 0; i < GetTotalNumGPIOs(); ++i) {
    gpio_init(GetGPIOPin(i));
  }

  debounce_timer_.resize(GetTotalScans());

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
    handler->SetKeyScan(outer);
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
