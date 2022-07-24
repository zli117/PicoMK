#include "ws2812.h"

#include <vector>

#include "FreeRTOS.h"
#include "configuration.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "semphr.h"
#include "utils.h"
#include "ws2812.pio.h"

WS2812::WS2812(uint8_t pin, uint8_t num_pixels, float max_brightness, PIO pio,
               uint8_t state_machine)
    : pin_(pin),
      pio_(pio),
      sm_(state_machine),
      max_brightness_(max_brightness > 1 ? 1 : max_brightness),
      brightness_(0.25 < max_brightness ? 0.25 : max_brightness),
      mode_(ROTATE),
      redraw_(false),
      enabled_(true),
      buffer_changed_(false),
      active_buffer_(0),
      tick_divider_(0),
      counter_(0),
      double_buffer_(2, std::vector<uint32_t>(num_pixels)),
      rotate_idx_(0) {
  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);

  // Initialize PIO
  const uint32_t offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm_, offset, pin, 800000, false);
}

void WS2812::OutputTick() {
  const uint64_t start_time = time_us_64();

  Mode mode;
  bool enabled;
  float brightness;
  {
    LockSemaphore lock(semaphore_);
    if (!enabled_ && !redraw_) {
      return;
    }
    if (!redraw_) {
      return;
    }
    if (counter_++ < tick_divider_) {
      return;
    }
    counter_ = 0;
    mode = mode_;
    brightness = brightness_;
    redraw_ = false;
    enabled = enabled_;
  }

  if (!enabled) {
    for (size_t i = 0; i < NumPixels(); ++i) {
      PutPixel(0);
    }
    return;
  }

  switch (mode) {
    case SET_PIXEL: {
      std::vector<uint32_t> copy;
      copy.reserve(NumPixels());
      {
        LockSemaphore lock(semaphore_);
        std::copy(double_buffer_[active_buffer_].begin(),
                  double_buffer_[active_buffer_].end(), copy.begin());
      }
      for (const uint32_t pixel : copy) {
        PutPixel(RescaleByBrightness(brightness, pixel));
      }
    }
    case RANDOM:
      RandomAnimation(brightness);
      break;
    case ROTATE:
      RotateAnimation(brightness);
      break;
  }

  const uint64_t end_time = time_us_64();
  LOG_DEBUG("LED write time: %d us", end_time - start_time);
}

void WS2812::StartOfInputTick() {
  LockSemaphore lock(semaphore_);
  if (mode_ != SET_PIXEL && enabled_) {
    redraw_ = true;
  }
}

void WS2812::FinalizeInputTickOutput() {
  LockSemaphore lock(semaphore_);
  if (mode_ == SET_PIXEL && buffer_changed_ && enabled_) {
    active_buffer_ = (active_buffer_ + 1) % 2;
    redraw_ = true;
  }
}

void WS2812::IncreaseBrightness() {
  LockSemaphore lock(semaphore_);
  if (!enabled_) {
    return;
  }
  brightness_ += 0.05;
  if (brightness_ > max_brightness_) {
    brightness_ = max_brightness_;
  }
  redraw_ = true;
}

void WS2812::DecreaseBrightness() {
  LockSemaphore lock(semaphore_);
  if (!enabled_) {
    return;
  }
  brightness_ -= 0.05;
  if (brightness_ < 0) {
    brightness_ = 0;
  }
  redraw_ = true;
}

void WS2812::IncreaseAnimationSpeed() {
  LockSemaphore lock(semaphore_);
  if (!enabled_) {
    return;
  }
  if (tick_divider_ != 1) {
    tick_divider_ -= 1;
  }
  redraw_ = true;
}

void WS2812::DecreaseAnimationSpeed() {
  LockSemaphore lock(semaphore_);
  if (!enabled_) {
    return;
  }
  if (tick_divider_ != 0xff) {
    tick_divider_ += 1;
  }
  redraw_ = true;
}

void WS2812::SetFixedColor(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
  {
    LockSemaphore lock(semaphore_);
    if (!enabled_) {
      return;
    }
  }
  if (mode_ != SET_PIXEL) {
    return;
  }
  const uint32_t color = CombineColors(r, g, b);
  auto& buffer = double_buffer_[(active_buffer_ + 1) % 2];
  std::fill(buffer.begin(), buffer.end(), color);
  buffer_changed_ = true;
}

void WS2812::SetPixel(size_t idx, uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
  {
    LockSemaphore lock(semaphore_);
    if (!enabled_) {
      return;
    }
  }
  if (mode_ != SET_PIXEL || idx >= NumPixels()) {
    return;
  }
  auto& buffer = double_buffer_[(active_buffer_ + 1) % 2];
  buffer[idx] = CombineColors(r, g, b);
  buffer_changed_ = true;
}

void WS2812::OnUpdateConfig(const Config* config) {
  LockSemaphore lock(semaphore_);
  if (config->GetType() != Config::OBJECT) {
    LOG_ERROR("Root config has to be an object.");
    return;
  }
  const auto& root_map = *((ConfigObject*)config)->GetMembers();
  auto it = root_map.find("brightness");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `brightness` in config");
    return;
  }
  if (it->second->GetType() != Config::FLOAT) {
    LOG_ERROR("`brightness` invalid type");
    return;
  }
  brightness_ = ((ConfigFloat*)it->second.get())->GetValue();

  it = root_map.find("tick_dividier");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `tick_dividier` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`tick_dividier` invalid type");
    return;
  }
  tick_divider_ = ((ConfigInt*)it->second.get())->GetValue();
  counter_ = 0;

  it = root_map.find("enabled");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `enabled` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`enabled` invalid type");
    return;
  }
  enabled_ = ((ConfigInt*)it->second.get())->GetValue();

  it = root_map.find("animation");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `animation` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`animation` invalid type");
    return;
  }
  mode_ = (Mode)(((ConfigInt*)it->second.get())->GetValue());
}

void WS2812::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  redraw_ = true;
}

std::pair<std::string, std::shared_ptr<Config>> WS2812::CreateDefaultConfig() {
  auto config = CONFIG_OBJECT(
      CONFIG_OBJECT_ELEM("brightness",
                         CONFIG_FLOAT(0.25, 0.0, max_brightness_, 0.02)),
      CONFIG_OBJECT_ELEM("tick_dividier", CONFIG_INT(10, 1, 250)),
      CONFIG_OBJECT_ELEM("enabled", CONFIG_INT(1, 0, 1)),
      CONFIG_OBJECT_ELEM("animation", CONFIG_INT(ROTATE, 0, TOTAL - 1)));
  return {"ws2812", config};
}

uint32_t WS2812::RescaleByBrightness(float brightness, uint32_t pixel) {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  SeparateColors(pixel, &r, &g, &b);
  return CombineColors((uint8_t)(r * brightness), (uint8_t)(g * brightness),
                       (uint8_t)(b * brightness));
}

uint32_t WS2812::CombineColors(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void WS2812::SeparateColors(uint32_t pixel, uint8_t* r, uint8_t* g,
                            uint8_t* b) {
  *r = (uint8_t)((pixel >> 8) & 0xff);
  *g = (uint8_t)((pixel >> 16) & 0xff);
  *b = (uint8_t)(pixel & 0xff);
}

void WS2812::PutPixel(uint32_t pixel) {
  pio_sm_put_blocking(pio_, sm_, pixel << 8u);
}

void WS2812::RandomAnimation(float brightness) {
  for (size_t i = 0; i < NumPixels(); ++i) {
    PutPixel(RescaleByBrightness(brightness, rand()));
  }
}

void WS2812::RotateAnimation(float brightness) {
  if (rotate_buffer_.empty()) {
    rotate_buffer_.reserve(NumPixels());
    for (size_t i = 0; i < NumPixels(); ++i) {
      rotate_buffer_.push_back(rand() % 0x00ffffff);
    }
  }
  for (size_t i = 0; i < rotate_buffer_.size(); ++i) {
    PutPixel(RescaleByBrightness(
        brightness, rotate_buffer_[(i + rotate_idx_) % rotate_buffer_.size()]));
  }
  rotate_idx_ = (rotate_idx_ + 1) % rotate_buffer_.size();
}

Status RegisterWS2812(uint8_t tag, uint8_t pin, uint8_t num_pixels,
                      float max_brightness, PIO pio, uint8_t state_machine) {
  DeviceRegistry::RegisterLEDOutputDevice(tag, /*slow=*/false, [=]() {
    return std::make_shared<WS2812>(pin, num_pixels, max_brightness, pio,
                                    state_machine);
  });
  return OK;
}
