#ifndef KEYSCAN_H_
#define KEYSCAN_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "FreeRTOS.h"
#include "base.h"
#include "layout.h"
#include "semphr.h"
#include "utils.h"

// Each registration creates at most one instance of the handler. If
// CAN_OVERRIDE is true, then another registration with CAN_OVERRIDE equals
// false will override it.
#define REGISTER_CUSTOM_KEYCODE_HANDLER(KEYCODE, CAN_OVERRIDE, CLS)  \
  status register_##KEYCODE = KeyScan::RegisterCustomKeycodeHandler( \
      (KEYCODE), (CAN_OVERRIDE),                                     \
      []() -> CustomKeycodeHandler* { return new CLS(); });

class KeyScan;

class CustomKeycodeHandler {
 public:
  // Key state is always called regardless whether it's a state transition.
  virtual void ProcessKeyState(Keycode kc, bool is_pressed, size_t key_idx) {}

  // Key events are only called when the key goes from pressed to released or
  // released to pressed, after debouncing.
  virtual void ProcessKeyEvent(Keycode kc, bool is_pressed, size_t key_idx) {}

  // Getting a reference to the KeyScan instance
  virtual void SetKeyScan(KeyScan* keyscan) { key_scan_ = keyscan; }

  // Get the name of this custom key
  virtual std::string GetName() const = 0;

 protected:
  KeyScan* key_scan_;
};

class KeyScan : public GenericInputDevice {
 public:
  using CustomKeycodeHandlerCreator = std::function<CustomKeycodeHandler*()>;

  KeyScan();

  void InputLoopStart() override;
  void InputTick() override;
  void SetConfigMode(bool is_config_mode) override;

  static status RegisterCustomKeycodeHandler(
      uint8_t keycode, bool overridable, CustomKeycodeHandlerCreator creator);

  Status SetLayerStatus(uint8_t layer, bool active);
  Status ToggleLayerStatus(uint8_t layer);
  std::vector<uint8_t> GetActiveLayers();

  void SetMouseButtonState(uint8_t mouse_key, bool is_pressed);
  void ConfigUp();
  void ConfigDown();
  void ConfigSelect();

 protected:
  struct DebounceTimer {
    uint8_t tick_count : 7;
    uint8_t pressed : 1;

    DebounceTimer() : tick_count(0), pressed(0) {}
  };

  class HandlerRegistry {
   public:
    static status RegisterHandler(uint8_t keycode, bool overridable,
                                  CustomKeycodeHandlerCreator creator);
    static CustomKeycodeHandler* RegisteredHandlerFactory(uint8_t keycode,
                                                          KeyScan* outer);

   private:
    static HandlerRegistry* GetRegistry();

    std::map<uint8_t, std::pair<bool, CustomKeycodeHandlerCreator>>
        custom_handlers_;
    std::map<uint8_t, CustomKeycodeHandler*> handler_singletons_;
  };

  virtual void SinkGPIODelay();

  virtual void NotifyOutput(const std::vector<uint8_t>& pressed_keycode);
  virtual void LayerChanged();

  std::vector<DebounceTimer> debounce_timer_;
  std::vector<bool> active_layers_;
  // SemaphoreHandle_t semaphore_;
  bool is_config_mode_;
};

Status RegisterKeyscan(uint8_t tag);

#endif /* KEYSCAN_H_ */