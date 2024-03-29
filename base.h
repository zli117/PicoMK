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
  virtual void SetTag(uint8_t tag) { tag_ = tag; }
  virtual uint8_t GetTag() { return tag_; }

  // Called when the config has been updated.
  virtual void OnUpdateConfig(const Config* config){};

  // Override this if the device needs to behave differently when in and out of
  // config mode. One example would be USB output devices, where during config
  // mode it doesn't report any key code or mouse movement to the host.
  virtual void SetConfigMode(bool is_config_mode){};

  // Called during initialization to create the default config. Note that it'll
  // be called even when there's a valid config json file in the filesystem. The
  // default config specifies the structure of the config sub-tree to later
  // parse the json file.
  virtual std::pair<std::string, std::shared_ptr<Config>>
  CreateDefaultConfig() {
    return std::make_pair<std::string, std::shared_ptr<Config>>("", NULL);
  }

 protected:
  uint8_t tag_;
};

class GenericOutputDevice : virtual public GenericDevice {
 public:
  virtual void SetSlow(bool slow) { slow_ = slow; }
  virtual bool IsSlow() { return slow_; }

  // OutputTick is called from a different task than the rest methods.
  virtual void OutputTick() = 0;

  virtual void StartOfInputTick() = 0;
  virtual void FinalizeInputTickOutput() = 0;

 protected:
  bool slow_;
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
  virtual void Pan(int8_t horizontal, int8_t vertical) = 0;
};

class Suspendable {
 public:
  virtual void SuspendEvent(bool is_suspend) = 0;
};

class ScreenOutputDevice : virtual public GenericOutputDevice,
                           virtual public Suspendable {
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
  virtual bool IsConfigMode() const = 0;

  // These functions allow you to draw directly from an input device. However,
  // it's advised aginst so because one input device doesn't have a way of
  // knowing what other input devices are drawing. It's better to create device
  // mixins like the examples in display_mixins.h. Mixins are also more
  // reusable.

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
  virtual void Clear() = 0;
};

template <typename T>
class ScreenMixinBase : virtual public ScreenOutputDevice {
 private:
  // Just to make sure subclass implements the Register method.
  static Status Unused(uint8_t key, bool slow, const std::shared_ptr<T> ptr) {
    return T::Register(key, slow, ptr);
  };
};

class LEDOutputDevice : virtual public GenericOutputDevice,
                        virtual public Suspendable {
 public:
  struct LEDIndicators {
    bool num_lock : 1;
    bool caps_lock : 1;
    bool scroll_lock : 1;
    bool compose : 1;
    bool kana : 1;

    LEDIndicators()
        : num_lock(false),
          caps_lock(false),
          scroll_lock(false),
          compose(false),
          kana(false) {}

    bool operator==(const LEDIndicators&) const = default;
  };

  virtual void IncreaseBrightness() = 0;
  virtual void DecreaseBrightness() = 0;
  virtual void IncreaseAnimationSpeed() = 0;
  virtual void DecreaseAnimationSpeed() = 0;
  virtual size_t NumPixels() const = 0;
  virtual void SetFixedColor(uint8_t w, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void SetPixel(size_t idx, uint8_t w, uint8_t r, uint8_t g,
                        uint8_t b) = 0;
  virtual void SetLedStatus(LEDIndicators indicators) = 0;
};

class MiscOutputDevice : virtual public GenericOutputDevice {
 public:
  virtual void Output(const std::string& output_string) = 0;
};

class ConfigModifier;

class GenericInputDevice : virtual public GenericDevice {
 public:
  // Everything is called from the same task, including methods in base class.

  virtual void InputLoopStart() = 0;
  virtual void InputTick() = 0;
  virtual void SetKeyboardOutputs(
      const std::vector<std::shared_ptr<KeyboardOutputDevice>>* devices);
  virtual void SetMouseOutputs(
      const std::vector<std::shared_ptr<MouseOutputDevice>>* device);
  virtual void SetScreenOutputs(
      const std::vector<std::shared_ptr<ScreenOutputDevice>>* device);
  virtual void SetLEDOutputs(
      const std::vector<std::shared_ptr<LEDOutputDevice>>* device);
  virtual void SetMiscOutputs(
      const std::vector<std::shared_ptr<MiscOutputDevice>>* device);
  virtual void SetConfigModifier(
      std::shared_ptr<ConfigModifier> config_modifier);

 protected:
  const std::vector<std::shared_ptr<KeyboardOutputDevice>>* keyboard_output_;
  const std::vector<std::shared_ptr<MouseOutputDevice>>* mouse_output_;
  const std::vector<std::shared_ptr<ScreenOutputDevice>>* screen_output_;
  const std::vector<std::shared_ptr<LEDOutputDevice>>* led_output_;
  const std::vector<std::shared_ptr<MiscOutputDevice>>* misc_output_;
  std::shared_ptr<ConfigModifier> config_modifier_;
};

class ConfigModifier : virtual public GenericOutputDevice,
                       virtual public GenericInputDevice {
 public:
  virtual void Up() = 0;
  virtual void Down() = 0;
  virtual void Select() = 0;

  void SetConfigModifier(
      const std::shared_ptr<ConfigModifier> config_modifier) override final {}

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
using MiscOutputDeviceCreator =
    std::function<std::shared_ptr<MiscOutputDevice>()>;
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
  static Status RegisterMiscOutputDevice(uint8_t key, bool slow,
                                         MiscOutputDeviceCreator func);
  static Status RegisterConfigModifier(ConfigModifierCreator func);

  static std::vector<std::shared_ptr<GenericInputDevice>> GetInputDevices();
  static std::vector<std::shared_ptr<GenericOutputDevice>> GetOutputDevices(
      bool is_slow);

  static void UpdateConfig();
  static void CreateDefaultConfig();
  static void SaveConfig();

 private:
  DeviceRegistry() : initialized_(false) {}

  void InitializeAllDevices();

  void AddConfig(GenericDevice* device);
  void UpdateConfigImpl();
  void CreateDefaultConfigImpl();

  static DeviceRegistry* GetRegistry();

  std::map<uint8_t, std::pair<bool, GenericInputDeviceCreator>> input_creators_;
  std::map<uint8_t, std::pair<bool, KeyboardOutputDeviceCreator>>
      keyboard_creators_;
  std::map<uint8_t, std::pair<bool, MouseOutputDeviceCreator>> mouse_creators_;
  std::map<uint8_t, std::pair<bool, ScreenOutputDeviceCreator>>
      screen_output_creators_;
  std::map<uint8_t, std::pair<bool, LEDOutputDeviceCreator>>
      led_output_creators_;
  std::map<uint8_t, std::pair<bool, MiscOutputDeviceCreator>>
      misc_output_creators_;
  std::optional<ConfigModifierCreator> config_modifier_creator_;

  bool initialized_;
  std::vector<std::shared_ptr<GenericInputDevice>> input_devices_;
  std::vector<std::shared_ptr<KeyboardOutputDevice>> keyboard_devices_;
  std::vector<std::shared_ptr<MouseOutputDevice>> mouse_devices_;
  std::vector<std::shared_ptr<ScreenOutputDevice>> screen_devices_;
  std::vector<std::shared_ptr<LEDOutputDevice>> led_devices_;
  std::vector<std::shared_ptr<MiscOutputDevice>> misc_devices_;
  std::shared_ptr<ConfigModifier> config_modifier_;

  ConfigObject global_config_;
  std::map<GenericDevice*, std::pair<std::string, Config*>> device_to_config_;
};

class IBPDriverBase {
 public:
  virtual Status IBPInitialize() = 0;
};

using IBPDriverCreator = std::function<std::shared_ptr<IBPDriverBase>()>;

// IBP stands for Inter Board Protocol.
class IBPDriverRegistry {
 public:
  static Status RegisterDriver(uint8_t key, IBPDriverCreator func);
  static Status InitializeAll();

 private:
  IBPDriverRegistry() : initialized_(false) {}
  static IBPDriverRegistry* GetRegistry();

  bool initialized_;
  std::map<uint8_t, IBPDriverCreator> driver_creators_;
  std::map<uint8_t, std::shared_ptr<IBPDriverBase>> drivers_;
};

#endif /* BASE_H_ */
