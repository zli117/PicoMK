// Derived from the WS2812 example in pico-example

#ifndef WH2812_H_
#define WH2812_H_

#include <vector>

#include "FreeRTOS.h"
#include "base.h"
#include "configuration.h"
#include "hardware/pio.h"
#include "semphr.h"
#include "utils.h"

class WS2812 : public LEDOutputDevice {
 public:
  enum Mode { SET_PIXEL = 0, BREATH, ROTATE, TOTAL };

  WS2812(uint8_t pin, uint8_t num_pixels, float max_brightness, PIO pio,
         uint8_t state_machine);

  void OutputTick() override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  void IncreaseBrightness() override;
  void DecreaseBrightness() override;
  void IncreaseAnimationSpeed() override;
  void DecreaseAnimationSpeed() override;
  size_t NumPixels() const override { return double_buffer_[0].size(); }
  void SetFixedColor(uint8_t w, uint8_t r, uint8_t g, uint8_t b) override;
  void SetPixel(size_t idx, uint8_t w, uint8_t r, uint8_t g,
                uint8_t b) override;

  void OnUpdateConfig(const Config* config) override;
  void SetConfigMode(bool is_config_mode) override;
  std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig()
      override;

  void SetLedStatus(LEDIndicators indicators) override {}

  void SuspendEvent(bool is_suspend) override;

 protected:
  uint32_t RescaleByBrightness(float brightness, uint32_t pixel);
  uint32_t CombineColors(uint8_t r, uint8_t g, uint8_t b);
  void SeparateColors(uint32_t pixel, uint8_t* r, uint8_t* g, uint8_t* b);
  void PutPixel(uint32_t pixel);

  void BreathAnimation(float brightness);
  void RotateAnimation(float brightness);

  const uint8_t pin_;
  const PIO pio_;
  const uint8_t sm_;
  const float max_brightness_;
  float brightness_;
  Mode mode_;
  bool redraw_;
  bool enabled_;
  bool buffer_changed_;
  uint8_t active_buffer_;
  uint8_t tick_divider_;
  uint8_t counter_;
  std::vector<std::vector<uint32_t>> double_buffer_;
  std::vector<uint32_t> random_buffer_;
  uint8_t rotate_idx_;
  float breath_scalar_;
  float breath_scalar_delta_;
  bool suspend_;

  SemaphoreHandle_t semaphore_;
};

Status RegisterWS2812(uint8_t tag, uint8_t pin, uint8_t num_pixels,
                      float max_brightness = 0.5, PIO pio = pio0,
                      uint8_t state_machine = 0);

#endif /* WH2812_H_ */
