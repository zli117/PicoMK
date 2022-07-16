#include "base.h"

#include <algorithm>

void GenericInputDevice::SetKeyboardOutputs(
    const std::vector<std::shared_ptr<KeyboardOutputDevice>>* devices) {
  keyboard_output_ = devices;
}
void GenericInputDevice::SetMouseOutputs(
    const std::vector<std::shared_ptr<MouseOutputDevice>>* device) {
  mouse_output_ = device;
}
void GenericInputDevice::SetScreenOutputs(
    const std::vector<std::shared_ptr<ScreenOutputDevice>>* device) {
  screen_output_ = device;
}
void GenericInputDevice::SetLEDOutputs(
    const std::vector<std::shared_ptr<LEDOutputDevice>>* device) {
  led_output_ = device;
}
void GenericInputDevice::SetConfigModifier(
    std::shared_ptr<ConfigModifier> config_modifier) {
  config_modifier_ = config_modifier;
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
    keyboard_devices_.clear();
    mouse_devices_.clear();
    screen_devices_.clear();
    led_devices_.clear();
    config_modifier_ = NULL;

    for (const auto [key, value] : GetRegistry()->keyboard_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      keyboard_devices_.push_back(output_device);
      output_device->SetTag(key);
      output_device->SetSlow(slow);
      AddConfig(output_device.get());
    }
    for (const auto [key, value] : GetRegistry()->mouse_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      mouse_devices_.push_back(output_device);
      output_device->SetTag(key);
      output_device->SetSlow(slow);
      AddConfig(output_device.get());
    }
    for (const auto [key, value] : GetRegistry()->screen_output_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      screen_devices_.push_back(output_device);
      output_device->SetTag(key);
      output_device->SetSlow(slow);
      AddConfig(output_device.get());
    }
    for (const auto [key, value] : GetRegistry()->led_output_creators_) {
      bool slow = value.first;
      auto output_device = value.second();
      led_devices_.push_back(output_device);
      output_device->SetTag(key);
      output_device->SetSlow(slow);
      AddConfig(output_device.get());
    }

    Dedup(&keyboard_devices_);
    Dedup(&mouse_devices_);
    Dedup(&screen_devices_);
    Dedup(&led_devices_);

    if (config_modifier_creator_.has_value()) {
      config_modifier_ = config_modifier_creator_.value()(&global_config_);
      config_modifier_->SetTag(0xff);
      config_modifier_->SetSlow(false);
      config_modifier_->SetKeyboardOutputs(&keyboard_devices_);
      config_modifier_->SetMouseOutputs(&mouse_devices_);
      config_modifier_->SetScreenOutputs(&screen_devices_);
      config_modifier_->SetLEDOutputs(&led_devices_);
      input_devices_.push_back(config_modifier_);
    }

    for (const auto [key, value] : input_creators_) {
      auto device = value.second();
      input_devices_.push_back(device);
      device->SetTag(key);
      device->SetKeyboardOutputs(&keyboard_devices_);
      device->SetMouseOutputs(&mouse_devices_);
      device->SetScreenOutputs(&screen_devices_);
      device->SetLEDOutputs(&led_devices_);
      device->SetConfigModifier(config_modifier_);
      AddConfig(device.get());
    }

    Dedup(&input_devices_);
  }

  UpdateConfigImpl();

  initialized_ = true;
}

std::vector<std::shared_ptr<GenericInputDevice>>
DeviceRegistry::GetInputDevices() {
  auto& registry = *GetRegistry();
  if (!registry.initialized_) {
    registry.InitializeAllDevices();
  }
  return registry.input_devices_;
}

std::vector<std::shared_ptr<GenericOutputDevice>>
DeviceRegistry::GetOutputDevices(bool is_slow) {
  auto& registry = *GetRegistry();
  if (!registry.initialized_) {
    registry.InitializeAllDevices();
  }
  std::vector<std::shared_ptr<GenericOutputDevice>> outputs;
  for (auto device : registry.keyboard_devices_) {
    if (device->IsSlow() == is_slow) {
      outputs.push_back(device);
    }
  }
  for (auto device : registry.mouse_devices_) {
    if (device->IsSlow() == is_slow) {
      outputs.push_back(device);
    }
  }
  for (auto device : registry.screen_devices_) {
    if (device->IsSlow() == is_slow) {
      outputs.push_back(device);
    }
  }
  for (auto device : registry.led_devices_) {
    if (device->IsSlow() == is_slow) {
      outputs.push_back(device);
    }
  }
  if (!is_slow && registry.config_modifier_ != NULL) {
    outputs.push_back(registry.config_modifier_);
  }
  Dedup(&outputs);
  return outputs;
}

void DeviceRegistry::UpdateConfigImpl() {
  for (auto& [device, config] : device_to_config_) {
    device->OnUpdateConfig(config.second);
  }
}

void DeviceRegistry::UpdateConfig() { GetRegistry()->UpdateConfigImpl(); }
