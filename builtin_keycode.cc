#include <vector>

#include "hardware/watchdog.h"
#include "keyscan.h"
#include "layout.h"
#include "pico/bootrom.h"
#include "runner.h"

class MouseButtonHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    (void)sink_idx;
    (void)source_idx;
    key_scan_->SetMouseButtonState(kc.keycode, is_pressed);
  }

  std::string GetName() const override { return "Mouse key handler"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_L, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_R, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_M, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_BACK, true, MouseButtonHandler);
REGISTER_CUSTOM_KEYCODE_HANDLER(MSE_FORWARD, true, MouseButtonHandler);

class LayerButtonHandler : public CustomKeycodeHandler {
 public:
  LayerButtonHandler()
      : switch_states_(GetNumSinkGPIOs() * GetNumSourceGPIOs()) {}

  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    const bool toggle = kc.custom_info & 0x40;
    const uint8_t layer = kc.custom_info & 0x3f;
    if (switch_states_[sink_idx * GetNumSourceGPIOs() + source_idx] !=
        is_pressed) {
      if (is_pressed) {
        if (toggle) {
          key_scan_->ToggleLayerStatus(layer);
        } else {
          key_scan_->SetLayerStatus(layer, true);
        }
      } else if (!toggle) {
        key_scan_->SetLayerStatus(layer, false);
      }
      switch_states_[sink_idx * GetNumSourceGPIOs() + source_idx] = is_pressed;
    }
  }

  std::string GetName() const override { return "Layer switch key handler"; }

 private:
  std::vector<bool> switch_states_;
};

REGISTER_CUSTOM_KEYCODE_HANDLER(LAYER_SWITCH, true, LayerButtonHandler);

class EnterConfigHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    if (is_pressed) {
      runner::SetConfigMode(true);
    }
  }

  std::string GetName() const override { return "Enter config mode"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(ENTER_CONFIG, true, EnterConfigHandler);

class ConfigSelHandler : public CustomKeycodeHandler {
 public:
  ConfigSelHandler() : currently_pressed_(false) {}

  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    if (is_pressed && !currently_pressed_) {
      key_scan_->ConfigSelect();
    }
    currently_pressed_ = is_pressed;
  }

  std::string GetName() const override { return "Config sel handler"; }

 private:
  bool currently_pressed_;
};

REGISTER_CUSTOM_KEYCODE_HANDLER(CONFIG_SEL, true, ConfigSelHandler);

class BootselHandler : public CustomKeycodeHandler {
 public:
  BootselHandler() {}

  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    if (is_pressed) {
      reset_usb_boot(0, 0);
    }
  }

  std::string GetName() const override { return "Enter bootsel handler"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(BOOTSEL, true, BootselHandler);

class RebootHandler : public CustomKeycodeHandler {
 public:
  RebootHandler() {}

  void ProcessKeyState(Keycode kc, bool is_pressed, size_t sink_idx,
                       size_t source_idx) override {
    if (is_pressed) {
      watchdog_reboot(0, 0, 0);
    }
  }

  std::string GetName() const override { return "Reboot"; }
};

REGISTER_CUSTOM_KEYCODE_HANDLER(REBOOT, true, RebootHandler);
