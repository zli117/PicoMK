#include "base.h"

#include <algorithm>

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
    uint8_t key, bool slow, F func,
    std::map<uint8_t, std::pair<bool, F>>* map) {
  auto it = map->find(key);
  if (it != map->end()) {
    // Already registered
    return ERROR;
  }
  map->insert({key, {slow, func}});
  return OK;
}

Status DeviceRegistry::RegisterInputDevice(uint8_t key,
                                           GenericInputDeviceCreator func) {
  return RegisterCreatorFunction(key, false, func,
                                 &GetRegistry()->input_creators_);
}

Status DeviceRegistry::RegisterKeyboardOutputDevice(
    uint8_t key, bool slow, KeyboardOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, slow, func,
                                 &GetRegistry()->keyboard_creators_);
}

Status DeviceRegistry::RegisterMouseOutputDevice(
    uint8_t key, bool slow, MouseOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, slow, func,
                                 &GetRegistry()->mouse_creators_);
}

Status DeviceRegistry::RegisterScreenOutputDevice(
    uint8_t key, bool slow, ScreenOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, slow, func,
                                 &GetRegistry()->screen_output_creators_);
}

Status DeviceRegistry::RegisterLEDOutputDevice(uint8_t key, bool slow,
                                               LEDOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, slow, func,
                                 &GetRegistry()->led_output_creators_);
}

Status DeviceRegistry::RegisterConfigModifier(uint8_t key, bool slow,
                                              ConfigModifierCreator func) {
  return RegisterCreatorFunction(key, slow, func,
                                 &GetRegistry()->config_modifier_creators_);
}

template<typename T>
void Dedup(std::vector<std::shared_ptr<T>>* devices) {
  if (devices->empty()) {
    return;
  }
  std::sort(devices->begin(), devices->end(),
            [](std::shared_ptr<T> a,
               std::shared_ptr<T> b) {
              return ((uint32_t)a.get()) < ((uint32_t)b.get());
            });
  std::vector<std::shared_ptr<T>> no_dup;
  no_dup.push_back((*devices)[0]);
  for (size_t i = 1; i < devices->size(); ++i) {
    if ((*devices)[i].get() != (*devices)[i - 1].get()) {
      no_dup.push_back((*devices)[i]);
    }
  }
  std::swap(*devices, no_dup);
}

Status DeviceRegistry::GetAllDevices(
    Configuration* config,
    std::vector<std::shared_ptr<GenericInputDevice>>* input_devices,
    std::vector<std::shared_ptr<GenericOutputDevice>>* output_devices,
    std::vector<std::shared_ptr<GenericOutputDevice>>* slow_output_devices) {
  input_devices->clear();
  output_devices->clear();
  slow_output_devices->clear();
  for (const auto [key, value] : GetRegistry()->input_creators_) {
    input_devices->push_back(value.second(config));
  }
  for (const auto [key, value] : GetRegistry()->config_modifier_creators_) {
    input_devices->push_back(value.second(config));
  }

  for (const auto [key, value] : GetRegistry()->keyboard_creators_) {
    bool slow = value.first;
    auto output_device = value.second(config);
    if (slow) {
      slow_output_devices->push_back(output_device);
    } else {
      output_devices->push_back(output_device);
    }
    for (auto input_device : *input_devices) {
      input_device->AddKeyboardOutput(output_device.get());
    }
  }
  for (const auto [key, value] : GetRegistry()->mouse_creators_) {
    bool slow = value.first;
    auto output_device = value.second(config);
    if (slow) {
      slow_output_devices->push_back(output_device);
    } else {
      output_devices->push_back(output_device);
    }
    for (auto input_device : *input_devices) {
      input_device->AddMouseOutput(output_device.get());
    }
  }
  for (const auto [key, value] : GetRegistry()->led_output_creators_) {
    bool slow = value.first;
    auto output_device = value.second(config);
    if (slow) {
      slow_output_devices->push_back(output_device);
    } else {
      output_devices->push_back(output_device);
    }
    for (auto input_device : *input_devices) {
      input_device->AddLEDOutput(output_device.get());
    }
  }
  for (const auto [key, value] : GetRegistry()->config_modifier_creators_) {
    bool slow = value.first;
    auto output_device = value.second(config);
    if (slow) {
      slow_output_devices->push_back(output_device);
    } else {
      output_devices->push_back(output_device);
    }
    for (auto input_device : *input_devices) {
      input_device->AddConfigModifier(output_device.get());
    }
  }
  for (const auto [key, value] : GetRegistry()->screen_output_creators_) {
    bool slow = value.first;
    auto output_device = value.second(config);
    if (slow) {
      slow_output_devices->push_back(output_device);
    } else {
      output_devices->push_back(output_device);
    }
    for (auto input_device : *input_devices) {
      input_device->AddScreenOutput(output_device.get());
    }
  }

  Dedup(input_devices);
  Dedup(output_devices);
  Dedup(slow_output_devices);
  return OK;
}
