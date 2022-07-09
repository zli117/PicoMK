#include "base.h"

void GenericInputDevice::AddKeyboardOutput(KeyboardOutputDevice* device) {
  usb_keyboard_output_.push_back(device);
}
void GenericInputDevice::AddMouseOutput(MouseOutputDevice* device) {
  usb_mouse_output_.push_back(device);
}
void GenericInputDevice::AddScreenOutput(ScreenOutputDevice* device) {
  screen_output_.push_back(device);
}
void GenericInputDevice::AddLEDOutput(LEDOutputDevice* device) {
  led_output_.push_back(device);
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
  return RegisterCreatorFunction(key, weak, func, &input_creators_);
}

Status DeviceRegistry::RegisterKeyboardOutputDevice(
    uint8_t key, bool weak, KeyboardOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &usb_keyboard_creators_);
}

Status DeviceRegistry::RegisterMouseOutputDevice(
    uint8_t key, bool weak, MouseOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &usb_mouse_creators_);
}

Status DeviceRegistry::RegisterScreenOutputDevice(
    uint8_t key, bool weak, ScreenOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &screen_output_creators_);
}

Status DeviceRegistry::RegisterLEDOutputDevice(uint8_t key, bool weak,
                                               LEDOutputDeviceCreator func) {
  return RegisterCreatorFunction(key, weak, func, &led_output_creators_);
}

Status DeviceRegistry::RegisterConfigModifier(uint8_t key, bool weak,
                                              ConfigModifierCreator func) {
  return RegisterCreatorFunction(key, weak, func, &config_modifier_creators_);
}
