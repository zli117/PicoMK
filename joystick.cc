#include "joystick.h"

#include "config.h"
#include "hardware/adc.h"
#include "utils.h"

constexpr uint8_t kADCStartPinNum = 26;

CenteringPotentialMeterDriver::CenteringPotentialMeterDriver(
    uint8_t adc_pin, size_t smooth_buffer_size, bool flip_dir)
    : adc_(adc_pin - kADCStartPinNum),
      origin_(0),
      buffer_(smooth_buffer_size),
      buffer_idx_(0),
      sum_(0),
      flip_dir_(flip_dir) {
  adc_init();
  adc_gpio_init(adc_pin);

  // Fill the buffer
  adc_select_input(adc_);
  for (size_t i = 0; i < smooth_buffer_size; ++i) {
    const uint16_t reading = adc_read();
    buffer_.push_back(reading);
    sum_ += reading;
  }
  origin_ = sum_ / smooth_buffer_size;
}

int8_t CenteringPotentialMeterDriver::GetValue() {
  int32_t diff = origin_ - sum_ / buffer_.size();

  if (diff > 0) {
    return diff * 20 / origin_ * (flip_dir_ ? -1 : 1);
  } else {
    return diff * 20 / ((1 << 12) - 1 - origin_) * (flip_dir_ ? -1 : 1);
  }
}

void CenteringPotentialMeterDriver::Tick() {
  adc_select_input(adc_);
  const uint16_t adc_raw = adc_read();

  sum_ -= buffer_[buffer_idx_];
  sum_ += adc_raw;
  buffer_[buffer_idx_] = adc_raw;
  buffer_idx_ = (buffer_idx_ + 1) % buffer_.size();
}
