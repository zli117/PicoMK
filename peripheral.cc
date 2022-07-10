#include "peripheral.h"

#include "FreeRTOS.h"
#include "config.h"
#include "joystick.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "tusb.h"
#include "usb.h"

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
    LOG_DEBUG("Mouse movement: %d, %d", x_val, y_val);
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
