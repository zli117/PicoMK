#ifndef DISPLAY_MIXINS_H_
#define DISPLAY_MIXINS_H_

#include <memory>

#include "FreeRTOS.h"
#include "base.h"

template <size_t row_offset = 1>
class ActiveLayersDisplayMixin
    : virtual public ScreenMixinBase<ActiveLayersDisplayMixin<row_offset>>,
      virtual public KeyboardOutputDevice {
 public:
  void SendKeycode(uint8_t) override {}
  void SendKeycode(const std::vector<uint8_t>&) override {}
  void SendConsumerKeycode(uint16_t keycode) override {}
  void ChangeActiveLayers(const std::vector<bool>& layers) override {
    if (this->IsConfigMode()) {
      return;
    }
    this->DrawText(1 + row_offset, 0, "Active Layers", ScreenOutputDevice::F8X8,
                   ScreenOutputDevice::ADD);
    this->DrawRect(9 + row_offset, 0, 17, this->GetNumCols() - 1,
                   /*fill=*/false, ScreenOutputDevice::ADD);
    for (size_t i = 0; i < layers.size() && i < 16; ++i) {
      this->DrawRect(
          11, i * 7 + 2, 15, i * 7 + 7, /*fill=*/true,
          layers[i] ? ScreenOutputDevice::ADD : ScreenOutputDevice::SUBTRACT);
    }
  }

  static Status Register(
      uint8_t key, bool slow,
      const std::shared_ptr<ActiveLayersDisplayMixin<row_offset>> ptr) {
    return DeviceRegistry::RegisterKeyboardOutputDevice(key, slow,
                                                        [=]() { return ptr; });
  }
};

template <size_t row_offset = 19>
class StatusLEDDisplayMixin
    : virtual public ScreenMixinBase<StatusLEDDisplayMixin<row_offset>>,
      virtual public LEDOutputDevice {
 public:
  StatusLEDDisplayMixin() : first_run_(true) {}

  void IncreaseBrightness() override {}
  void DecreaseBrightness() override {}
  void IncreaseAnimationSpeed() override {}
  void DecreaseAnimationSpeed() override {}
  size_t NumPixels() const override { return 0; }
  void SetFixedColor(uint8_t w, uint8_t r, uint8_t g, uint8_t b) override {}
  void SetPixel(size_t idx, uint8_t w, uint8_t r, uint8_t g,
                uint8_t b) override {}

  void SetLedStatus(LEDIndicators indicators) override {
    if (this->IsConfigMode()) {
      return;
    }
    if (indicators == current_indicators_ && !first_run_) {
      return;
    }
    first_run_ = false;

    this->DrawRect(row_offset, 0, this->GetNumRows() - 1,
                   this->GetNumCols() - 1, /*fill=*/false,
                   ScreenOutputDevice::ADD);
    uint8_t offset = row_offset + 2;
    DrawLEDIndicator("NumLock", offset, indicators.num_lock);
    if (this->GetNumRows() == 64) {
      DrawLEDIndicator("CapsLock", offset += 8, indicators.caps_lock);
      DrawLEDIndicator("ScrollLock", offset += 8, indicators.scroll_lock);
      DrawLEDIndicator("Compose", offset += 8, indicators.compose);
      DrawLEDIndicator("Kana", offset += 8, indicators.kana);
    }
    current_indicators_ = indicators;
  }

  static Status Register(
      uint8_t key, bool slow,
      const std::shared_ptr<StatusLEDDisplayMixin<row_offset>> ptr) {
    return DeviceRegistry::RegisterLEDOutputDevice(key, slow,
                                                   [=]() { return ptr; });
  }

 protected:
  void DrawLEDIndicator(const std::string& name, uint8_t row, bool on) {
    this->DrawText(row, 2, name, ScreenOutputDevice::F8X8,
                   ScreenOutputDevice::ADD);
    if (!on) {
      // Clean up the rectangle
      this->DrawRect(row + 1, this->GetNumCols() - 9, row + 6,
                     this->GetNumCols() - 4,
                     /*fill=*/true, ScreenOutputDevice::SUBTRACT);
    }
    this->DrawRect(row + 1, this->GetNumCols() - 9, row + 6,
                   this->GetNumCols() - 4, on, ScreenOutputDevice::ADD);
  }

  LEDIndicators current_indicators_;
  bool first_run_;
};

#endif /* DISPLAY_MIXINS_H_ */
