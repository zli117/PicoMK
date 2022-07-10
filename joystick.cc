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
      flip_dir_(flip_dir),
      last_value_(0),
      calibration_samples_(0),
      calibration_threshold_(0),
      calibration_sum_(0),
      calibration_zero_count_(0),
      calibration_count_(0) {
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

int16_t CenteringPotentialMeterDriver::GetValue() {
  adc_select_input(adc_);
  const uint16_t adc_raw = adc_read();
  last_value_ = adc_raw;

  sum_ -= buffer_[buffer_idx_];
  sum_ += adc_raw;
  buffer_[buffer_idx_] = adc_raw;
  buffer_idx_ = (buffer_idx_ + 1) % buffer_.size();
  return origin_ - sum_ / buffer_.size();
}

void CenteringPotentialMeterDriver::SetCalibrationSamples(
    uint32_t calibration_samples) {
  calibration_samples_ = calibration_samples;
  calibration_sum_ = 0;
  calibration_zero_count_ = 0;
  calibration_count_ = 0;
}
void CenteringPotentialMeterDriver::SetCalibrationThreshold(
    uint32_t calibration_threshold) {
  calibration_threshold_ = calibration_threshold;
  calibration_sum_ = 0;
  calibration_zero_count_ = 0;
  calibration_count_ = 0;
}

void CenteringPotentialMeterDriver::SetMappedValue(int8_t mapped) {
  if (calibration_samples_ * calibration_threshold_ == 0) {
    return;
  }
  ++calibration_count_;
  if (mapped == 0) {
    ++calibration_zero_count_;
    calibration_sum_ += last_value_;
  }
  if (calibration_count_ >= calibration_samples_) {
    if (calibration_zero_count_ >= calibration_threshold_) {
      origin_ = calibration_sum_ / calibration_zero_count_;
    }
    calibration_count_ = 0;
    calibration_zero_count_ = 0;
    calibration_sum_ = 0;
  }
}