#include "joystick.h"

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/adc.h"
#include "semphr.h"
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

JoystickInputDeivce::JoystickInputDeivce(const Configuration* config,
                                         uint8_t x_adc_pin, uint8_t y_adc_pin,
                                         size_t buffer_size, bool flip_x_dir,
                                         bool flip_y_dir)
    : config_(config),
      x_(x_adc_pin, buffer_size, flip_x_dir),
      y_(y_adc_pin, buffer_size, flip_y_dir),
      divider_(1),
      counter_(0),
      is_config_mode_(false) {
  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);
  OnUpdateConfig();
}

void JoystickInputDeivce::Tick() {
  // We still sample at the normal frequency, but only report when counter
  // reaches divider.
  const int8_t x_val = TranslateReading(profile_x_, x_.GetValue());
  const int8_t y_val = TranslateReading(profile_x_, y_.GetValue());
  x_.SetMappedValue(x_val);
  y_.SetMappedValue(y_val);

  if (counter_++ < divider_) {
    return;
  }
  counter_ = 0;

  bool is_profile;
  {
    LockSemaphore lock(semaphore_);
    is_profile = is_config_mode_;
  }
  if (is_profile) {
    for (auto* config_modifier : config_modifier_) {
      if (y_val > 0) {
        config_modifier->Up();
      } else if (y_val < 0) {
        config_modifier->Down();
      }
    }
  } else {
    for (auto* mouse_output : mouse_output_) {
      mouse_output->MouseMovement(x_val, y_val);
    }
  }
}

void JoystickInputDeivce::OnUpdateConfig() {
  profile_x_.clear();
  profile_y_.clear();
  profile_x_.push_back(std::make_pair(0, 0));
  profile_y_.push_back(std::make_pair(0, 0));
  profile_x_.insert(profile_x_.end(), config_->GetJoystickXProfile()->begin(),
                    config_->GetJoystickXProfile()->end());
  profile_y_.insert(profile_y_.end(), config_->GetJoystickYProfile()->begin(),
                    config_->GetJoystickYProfile()->end());
  divider_ = config_->GetJoystickScanDivider();
  x_.SetCalibrationSamples(config_->GetJoystickCalibrationSamples());
  x_.SetCalibrationThreshold(config_->GetJoystickCalibrationThreshold());
  y_.SetCalibrationSamples(config_->GetJoystickCalibrationSamples());
  y_.SetCalibrationThreshold(config_->GetJoystickCalibrationThreshold());
}

void JoystickInputDeivce::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  is_config_mode_ = is_config_mode;
}

int8_t JoystickInputDeivce::TranslateReading(
    const std::vector<std::pair<uint16_t, uint8_t>>& profile, int16_t reading) {
  const uint16_t abs = reading < 0 ? -reading : reading;
  const int8_t sign = reading < 0 ? -1 : 1;

  // Use dumb for loop instead of std::lower_bound to save binary size. Assuming
  // profile is already sorted.
  for (size_t i = 1; i < profile.size(); ++i) {
    if (abs < profile[i].first && abs >= profile[i - 1].first) {
      return profile[i - 1].second * sign;
    }
  }
  return profile.back().second * sign;
}

#if CONFIG_ENABLE_JOYSTICK

static Status registered = DeviceRegistry::RegisterInputDevice(
    2, true, [](const Configuration* config) {
      return std::make_shared<JoystickInputDeivce>(
          config, CONFIG_JOYSTICK_GPIO_X, CONFIG_JOYSTICK_GPIO_Y,
          CONFIG_JOYSTICK_SMOOTH_BUFFER_LEN, CONFIG_JOYSTICK_FLIP_X_DIR,
          CONFIG_JOYSTICK_FLIP_Y_DIR);
    });

#endif /* CONFIG_ENABLE_JOYSTICK */