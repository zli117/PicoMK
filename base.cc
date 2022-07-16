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

Status DeviceRegistry::RegisterConfigModifier(ConfigModifierCreator func) {
  if (GetRegistry()->config_modifier_creator_.has_value()) {
    return ERROR;
  }
  GetRegistry()->config_modifier_creator_ = func;
  return OK;
}

template <typename T>
void Dedup(std::vector<std::shared_ptr<T>>* devices) {
  if (devices->empty()) {
    return;
  }
  std::sort(devices->begin(), devices->end(),
            [](std::shared_ptr<T> a, std::shared_ptr<T> b) {
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

void DeviceRegistry::AddConfig(GenericDevice* device) {
  if (device_to_config_.find(device) == device_to_config_.end()) {
    auto [name, config] = device->CreateDefaultConfig();
    if (config == NULL) {
      return;
    }
    device_to_config_[device] = {name, config.get()};
    (*global_config_.GetMembers())[name] = std::move(config);
  }
}

void DeviceRegistry::InitializeAllDevices() {
  if (!initialized_) {
    input_devices_.clear();
    output_devices_.clear();
    slow_output_devices_.clear();

    for (const auto [key, value] : input_creators_) {
      auto device = value.second();
      input_devices_.push_back(device);
      AddConfig(device.get());
    }

    for (const auto [key, value] : GetRegistry()->keyboard_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      if (slow) {
        slow_output_devices_.push_back(output_device);
      } else {
        output_devices_.push_back(output_device);
      }
      AddConfig(output_device.get());
      for (auto input_device : input_devices_) {
        input_device->AddKeyboardOutput(output_device.get());
      }
    }

    for (const auto [key, value] : GetRegistry()->mouse_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      if (slow) {
        slow_output_devices_.push_back(output_device);
      } else {
        output_devices_.push_back(output_device);
      }
      AddConfig(output_device.get());
      for (auto input_device : input_devices_) {
        input_device->AddMouseOutput(output_device.get());
      }
    }
    for (const auto [key, value] : GetRegistry()->led_output_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      if (slow) {
        slow_output_devices_.push_back(output_device);
      } else {
        output_devices_.push_back(output_device);
      }
      AddConfig(output_device.get());
      for (auto input_device : input_devices_) {
        input_device->AddLEDOutput(output_device.get());
      }
    }
    for (const auto [key, value] : GetRegistry()->screen_output_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      if (slow) {
        slow_output_devices_.push_back(output_device);
      } else {
        output_devices_.push_back(output_device);
      }
      AddConfig(output_device.get());
      for (auto input_device : input_devices_) {
        input_device->AddScreenOutput(output_device.get());
      }
    }

    Dedup(&input_devices_);
    Dedup(&output_devices_);
    Dedup(&slow_output_devices_);

    if (config_modifier_creator_.has_value()) {
      auto device = config_modifier_creator_.value()(&global_config_);
      input_devices_.push_back(device);
      output_devices_.push_back(device);
      for (auto input_device : input_devices_) {
        input_device->AddConfigModifier(device.get());
      }
    }
  }

  UpdateConfigImpl();

  initialized_ = true;
}

Status DeviceRegistry::GetAllDevices(
    std::vector<std::shared_ptr<GenericInputDevice>>* input_devices,
    std::vector<std::shared_ptr<GenericOutputDevice>>* output_devices,
    std::vector<std::shared_ptr<GenericOutputDevice>>* slow_output_devices) {
  auto& registry = *GetRegistry();
  if (!registry.initialized_) {
    registry.InitializeAllDevices();
  }

  input_devices->clear();
  output_devices->clear();
  slow_output_devices->clear();

  for (const auto device : registry.input_devices_) {
    input_devices->push_back(device);
  }
  for (const auto device : registry.output_devices_) {
    output_devices->push_back(device);
  }
  for (const auto device : registry.slow_output_devices_) {
    slow_output_devices->push_back(device);
  }

  return OK;
}

void DeviceRegistry::UpdateConfigImpl() {
  for (auto& [device, config] : device_to_config_) {
    device->OnUpdateConfig(config.second);
  }
}

void DeviceRegistry::UpdateConfig() { GetRegistry()->UpdateConfigImpl(); }
