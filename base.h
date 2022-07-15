#ifndef BASE_H_
#define BASE_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration.h"
#include "layout.h"
#include "utils.h"

class GenericDevice {
 public:
  virtual void OnUpdateConfig(const Config* config){};
  virtual void SetConfigMode(bool is_config_mode){};
  virtual std::pair<std::string, std::shared_ptr<Config>>
  CreateDefaultConfig() {
    return std::make_pair<std::string, std::shared_ptr<Config>>("", NULL);
  }
};

class GenericOutputDevice : virtual public GenericDevice {
 public:
  // OutputTick is called from a different task than the rest methods.
  virtual void OutputTick() = 0;

  virtual void StartOfInputTick() = 0;
  virtual void FinalizeInputTickOutput() = 0;
};

class KeyboardOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void SendKeycode(uint8_t keycode) = 0;
  virtual void SendKeycode(const std::vector<uint8_t>& keycode) = 0;
  virtual void SendConsumerKeycode(uint16_t keycode) = 0;
  virtual void ChangeActiveLayers(const std::vector<bool>& layers) = 0;
};

class MouseOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void MouseKeycode(uint8_t keycode) = 0;
  virtual void MouseMovement(int8_t x, int8_t y) = 0;
  virtual void Pan(int8_t x, int8_t y) = 0;
};

class ScreenOutputDevice : virtual public GenericOutputDevice {
 public:
  enum Mode { ADD = 0, SUBTRACT, INVERT };
  enum Font { F5X8 = 0, F8X8, F12X16, F16X32 };

  class CustomFont {
   public:
    // Follows the format that the first two bytes are per character size
    // (width x height)
    virtual const uint8_t* GetFont() const = 0;
  };

  virtual size_t GetNumRows() const = 0;
  virtual size_t GetNumCols() const = 0;
  virtual void SetPixel(size_t row, size_t col, Mode mode) = 0;
  virtual void DrawLine(size_t start_row, size_t start_col, size_t end_row,
                        size_t end_col, Mode mode) = 0;
  virtual void DrawRect(size_t start_row, size_t start_col, size_t end_row,
                        size_t end_col, bool fill, Mode mode) = 0;
  virtual void DrawText(size_t row, size_t col, const std::string& text,
                        Font font, Mode mode) = 0;
  virtual void DrawText(size_t row, size_t col, const std::string& text,
                        const CustomFont& font, Mode mode) = 0;
  virtual void DrawBuffer(const std::vector<uint8_t>& buffer, size_t start_row,
                          size_t start_col, size_t end_row, size_t end_col) = 0;
};

class LEDOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void NextAnimation() = 0;
  virtual void PreviousAnimation() = 0;
  virtual void SetAnimationSpeed() = 0;
  virtual size_t NumPixels() const = 0;
  virtual void SetFixedColor(uint8_t r, uint8_t g, uint8_t b) = 0;
};

class ConfigModifier;

class GenericInputDevice : virtual public GenericDevice {
 public:
  // Everything is called from the same task, including methods in base class.

  virtual void InputLoopStart() = 0;
  virtual void InputTick() = 0;
  virtual void AddKeyboardOutput(KeyboardOutputDevice* device);
  virtual void AddMouseOutput(MouseOutputDevice* device);
  virtual void AddScreenOutput(ScreenOutputDevice* device);
  virtual void AddLEDOutput(LEDOutputDevice* device);
  virtual void AddConfigModifier(ConfigModifier* device);

 protected:
  std::vector<KeyboardOutputDevice*> keyboard_output_;
  std::vector<MouseOutputDevice*> mouse_output_;
  std::vector<ScreenOutputDevice*> screen_output_;
  std::vector<LEDOutputDevice*> led_output_;
  std::vector<ConfigModifier*> config_modifier_;
};

class ConfigModifier : public GenericOutputDevice, public GenericInputDevice {
 public:
  virtual void Up() = 0;
  virtual void Down() = 0;
  virtual void Select() = 0;

  // At most one config modifier allowed
  void AddConfigModifier(ConfigModifier* device) override final {}

  // No config for config modifier
  std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig()
      override final {
    return GenericDevice::CreateDefaultConfig();
  }
};

using GenericInputDeviceCreator =
    std::function<std::shared_ptr<GenericInputDevice>()>;
using KeyboardOutputDeviceCreator =
    std::function<std::shared_ptr<KeyboardOutputDevice>()>;
using MouseOutputDeviceCreator =
    std::function<std::shared_ptr<MouseOutputDevice>()>;
using ScreenOutputDeviceCreator =
    std::function<std::shared_ptr<ScreenOutputDevice>()>;
using LEDOutputDeviceCreator =
    std::function<std::shared_ptr<LEDOutputDevice>()>;
using ConfigModifierCreator =
    std::function<std::shared_ptr<ConfigModifier>(ConfigObject* global_config)>;

class DeviceRegistry {
 public:
  static Status RegisterInputDevice(uint8_t key,
                                    GenericInputDeviceCreator func);
  static Status RegisterKeyboardOutputDevice(uint8_t key, bool slow,
                                             KeyboardOutputDeviceCreator func);
  static Status RegisterMouseOutputDevice(uint8_t key, bool slow,
                                          MouseOutputDeviceCreator func);
  static Status RegisterScreenOutputDevice(uint8_t key, bool slow,
                                           ScreenOutputDeviceCreator func);
  static Status RegisterLEDOutputDevice(uint8_t key, bool slow,
                                        LEDOutputDeviceCreator func);
  static Status RegisterConfigModifier(ConfigModifierCreator func);

  static Status GetAllDevices(
      std::vector<std::shared_ptr<GenericInputDevice>>* input_devices,
      std::vector<std::shared_ptr<GenericOutputDevice>>* output_devices,
      std::vector<std::shared_ptr<GenericOutputDevice>>* slow_output_devices);

  static void UpdateConfig();

 private:
  DeviceRegistry() : initialized_(false) {}

  void AddConfig(GenericDevice* device);

  void InitializeAllDevices();

  void UpdateConfigImpl();

  static DeviceRegistry* GetRegistry();

  std::map<uint8_t, std::pair<bool, GenericInputDeviceCreator>> input_creators_;
  std::map<uint8_t, std::pair<bool, KeyboardOutputDeviceCreator>>
      keyboard_creators_;
  std::map<uint8_t, std::pair<bool, MouseOutputDeviceCreator>> mouse_creators_;
  std::map<uint8_t, std::pair<bool, ScreenOutputDeviceCreator>>
      screen_output_creators_;
  std::map<uint8_t, std::pair<bool, LEDOutputDeviceCreator>>
      led_output_creators_;
  std::optional<ConfigModifierCreator> config_modifier_creator_;

  bool initialized_;
  std::vector<std::shared_ptr<GenericInputDevice>> input_devices_;
  std::vector<std::shared_ptr<GenericOutputDevice>> output_devices_;
  std::vector<std::shared_ptr<GenericOutputDevice>> slow_output_devices_;

  ConfigObject global_config_;
  std::map<GenericDevice*, std::pair<std::string, Config*>> device_to_config_;
};

#endif /* BASE_H_ */
