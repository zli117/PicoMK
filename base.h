#ifndef BASE_H_
#define BASE_H_

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "configuration.h"
#include "layout.h"
#include "utils.h"

class GenericDevice {
 public:
  virtual void Tick() = 0;
  virtual void OnUpdateConfig() = 0;
  virtual void SetConfigMode(bool is_config_mode) = 0;
};

class GenericOutputDevice : virtual public GenericDevice {
 public:
  virtual void StartOfInputTick() = 0;
  virtual void FinalizeInputTickOutput() = 0;
};

class  KeyboardOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void SendKeycode(uint8_t keycode) = 0;
  virtual void SendKeycode(const std::vector<uint8_t>& keycode) = 0;
};

class  MouseOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void MouseKeycode(uint8_t keycode) = 0;
  virtual void MouseMovement(int8_t x, int8_t y) = 0;
  virtual void Pan(int8_t x, int8_t y) = 0;
};

class ScreenOutputDevice : public GenericOutputDevice {
 public:
  enum Alignment { LEFT, CENTER, RIGHT };

  virtual void SelectRow(uint8_t row) = 0;
  virtual void DrawText(const std::string& string, Alignment align) = 0;
  virtual void Draw8Bits(uint8_t bits) = 0;
  virtual void Draw16Bits(uint8_t bits) = 0;
  virtual void SetRowHighlight(bool highlight) = 0;
};

class LEDOutputDevice : public GenericOutputDevice {
 public:
  virtual void NextAnimation() = 0;
  virtual void PreviousAnimation() = 0;
  virtual void SetAnimationSpeed() = 0;
  virtual size_t NumPixels() const = 0;
  virtual void SetFixedColor(uint8_t r, uint8_t g, uint8_t b) = 0;
};

class GenericInputDevice : virtual public GenericDevice {
 public:
  virtual void AddKeyboardOutput( KeyboardOutputDevice* device);
  virtual void AddMouseOutput( MouseOutputDevice* device);
  virtual void AddScreenOutput(ScreenOutputDevice* device);
  virtual void AddLEDOutput(LEDOutputDevice* device);

 protected:
  std::vector< KeyboardOutputDevice*> usb_keyboard_output_;
  std::vector< MouseOutputDevice*> usb_mouse_output_;
  std::vector<ScreenOutputDevice*> screen_output_;
  std::vector<LEDOutputDevice*> led_output_;
};

class ConfigModifier : public GenericOutputDevice, public GenericInputDevice {
 public:
  virtual void Up() = 0;
  virtual void Down() = 0;
  virtual void Select() = 0;
};

using GenericInputDeviceCreator =
    std::function<std::shared_ptr<GenericInputDevice>(const Configuration*)>;
using  KeyboardOutputDeviceCreator =
    std::function<std::shared_ptr< KeyboardOutputDevice>(
        const Configuration*)>;
using  MouseOutputDeviceCreator =
    std::function<std::shared_ptr< MouseOutputDevice>(const Configuration*)>;
using ScreenOutputDeviceCreator =
    std::function<std::shared_ptr<ScreenOutputDevice>(const Configuration*)>;
using LEDOutputDeviceCreator =
    std::function<std::shared_ptr<LEDOutputDevice>(const Configuration*)>;
using ConfigModifierCreator =
    std::function<std::shared_ptr<ConfigModifier>(Configuration*)>;

class DeviceRegistry {
 public:
  static Status RegisterInputDevice(uint8_t key, bool weak,
                                    GenericInputDeviceCreator func);
  static Status RegisterKeyboardOutputDevice(
      uint8_t key, bool weak,  KeyboardOutputDeviceCreator func);
  static Status RegisterMouseOutputDevice(uint8_t key, bool weak,
                                              MouseOutputDeviceCreator func);
  static Status RegisterScreenOutputDevice(uint8_t key, bool weak,
                                           ScreenOutputDeviceCreator func);
  static Status RegisterLEDOutputDevice(uint8_t key, bool weak,
                                        LEDOutputDeviceCreator func);
  static Status RegisterConfigModifier(uint8_t key, bool weak,
                                       ConfigModifierCreator func);

  static std::vector<std::shared_ptr<GenericInputDevice>> GetAllInputDevices(
      Configuration* config);
  static std::vector<std::shared_ptr<GenericOutputDevice>> GetAllOutputDevices(
      Configuration* config);

 private:
  static std::map<uint8_t, std::pair<bool, GenericInputDeviceCreator>>
      input_creators_;
  static std::map<uint8_t, std::pair<bool,  KeyboardOutputDeviceCreator>>
      usb_keyboard_creators_;
  static std::map<uint8_t, std::pair<bool,  MouseOutputDeviceCreator>>
      usb_mouse_creators_;
  static std::map<uint8_t, std::pair<bool, ScreenOutputDeviceCreator>>
      screen_output_creators_;
  static std::map<uint8_t, std::pair<bool, LEDOutputDeviceCreator>>
      led_output_creators_;
  static std::map<uint8_t, std::pair<bool, ConfigModifierCreator>>
      config_modifier_creators_;
};

#endif /* BASE_H_ */
