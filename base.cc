#include "base.h"

void GenericInputDevice::AddKeyboardOutput(KeyboardOutputDevice* device) {
  keyboard_output_.push_back(device);
}
void GenericInputDevice::AddMouseOutput(MouseOutputDevice* device) {
  mouse_output_.push_back(device);
}
void GenericInputDevice::AddScreenOutput(ScreenOutputDevice* device) {
  screen_output_.push_back(device);
}
void GenericInputDevice::AddLEDOutput(LEDOutputDevice* device) {
  led_output_.push_back(device);
}

void GenericInputDevice::AddConfigModifier(ConfigModifier* device) {
  config_modifier_.push_back(device);
}

DeviceRegistry* DeviceRegistry::GetRegistry() {
  static DeviceRegistry registry;
  return &registry;
}

template <typename F>
static Status RegisterCreatorFunction(
    uint8_t key, bool weak, F func,
    std::map<uint8_t, std::pair<bool, F>>* map) {
  auto it = map->find(key);
  if (it != map->end()) {
    if (!it->first) {
      // Can't override strongly registered function
      return ERROR;
    }
    if (it->first && weak) {
      // Weak can't override weak
      return ERROR;
    }
  }
  map->insert({key, {weak, func}});
  return OK;
}

Status DeviceRegistry::RegisterInputDevice(uint8_t key, bool weak,
                                           GenericInputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func,
                                 &GetRegistry()->input_creators_);
}

Status DeviceRegistry::RegisterKeyboardOutputDevice(
    uint8_t key, bool weak, KeyboardOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &GetRegistry()->keyboard_creators_);
}

Status DeviceRegistry::RegisterMouseOutputDevice(
    uint8_t key, bool weak, MouseOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &GetRegistry()->mouse_creators_);
}

Status DeviceRegistry::RegisterScreenOutputDevice(
    uint8_t key, bool weak, ScreenOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &GetRegistry()->screen_output_creators_);
}

Status DeviceRegistry::RegisterLEDOutputDevice(uint8_t key, bool weak,
                                               LEDOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &GetRegistry()->led_output_creators_);
}

Status DeviceRegistry::RegisterConfigModifier(uint8_t key, bool weak,
                                              ConfigModifierCreator func) {
  return RegisterCreatorFunction(key, weak, func, &GetRegistry()->config_modifier_creators_);
}

// std::map<uint8_t, std::pair<bool, GenericInputDeviceCreator>>
//     DeviceRegistry::input_creators_;
// std::map<uint8_t, std::pair<bool, KeyboardOutputDeviceCreator>>
//     DeviceRegistry::keyboard_creators_;
// std::map<uint8_t, std::pair<bool, MouseOutputDeviceCreator>>
//     DeviceRegistry::mouse_creators_;
// std::map<uint8_t, std::pair<bool, ScreenOutputDeviceCreator>>
//     DeviceRegistry::screen_output_creators_;
// std::map<uint8_t, std::pair<bool, LEDOutputDeviceCreator>>
//     DeviceRegistry::led_output_creators_;
// std::map<uint8_t, std::pair<bool, ConfigModifierCreator>>
//     DeviceRegistry::config_modifier_creators_;

std::vector<std::shared_ptr<GenericInputDevice>>
DeviceRegistry::GetAllInputDevices(Configuration* config) {
  std::vector<std::shared_ptr<GenericInputDevice>> output;
  for (const auto [key, value] : GetRegistry()->input_creators_) {
    output.push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->config_modifier_creators_) {
    output.push_back(value.second(config));
  }
  return output;
}

std::vector<std::shared_ptr<GenericOutputDevice>>
DeviceRegistry::GetAllOutputDevices(Configuration* config) {
  std::vector<std::shared_ptr<GenericOutputDevice>> output;
  for (const auto [key, value] : GetRegistry()->keyboard_creators_) {
    output.push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->mouse_creators_) {
    output.push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->screen_output_creators_) {
    output.push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->led_output_creators_) {
    output.push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->config_modifier_creators_) {
    output.push_back(value.second(config));
  }
  return output;
}
