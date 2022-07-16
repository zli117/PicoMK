#include "joystick.h"

#include <algorithm>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
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

void CenteringPotentialMeterDriver::SetMappedValue(int16_t mapped) {
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

JoystickInputDeivce::JoystickInputDeivce(uint8_t x_adc_pin, uint8_t y_adc_pin,
                                         size_t buffer_size, bool flip_x_dir,
                                         bool flip_y_dir,
                                         uint8_t scan_num_ticks)
    : x_(x_adc_pin, buffer_size, flip_x_dir),
      y_(y_adc_pin, buffer_size, flip_y_dir),
      report_n_scan_(1),
      counter_(0),
      x_move_(0),
      y_move_(0),
      is_config_mode_(false),
      scan_num_ticks_(scan_num_ticks) {
  profile_x_.push_back({0, 0});
  profile_y_.push_back({0, 0});
}

void JoystickInputDeivce::InputTick() {
  // We still sample at the normal frequency, but only update member speed
  // variable when counter expires.
  const int16_t x_speed = GetSpeed(profile_x_, x_.GetValue());
  const int16_t y_speed = GetSpeed(profile_x_, y_.GetValue());
  x_.SetMappedValue(x_speed);
  y_.SetMappedValue(y_speed);

  if (counter_ == 0) {
    const int16_t tick_hz = configTICK_RATE_HZ;
    x_move_ = x_speed * report_n_scan_ * scan_num_ticks_ / tick_hz;
    y_move_ = y_speed * report_n_scan_ * scan_num_ticks_ / tick_hz;
  }

  const int8_t x_report_speed = x_move_ / (scan_num_ticks_ - counter_);
  const int8_t y_report_speed = y_move_ / (scan_num_ticks_ - counter_);
  x_move_ -= x_report_speed;
  y_move_ -= y_report_speed;

  counter_ = (counter_ + 1) % report_n_scan_;

  if (is_config_mode_) {
    if (config_modifier_ != NULL) {
      if (y_report_speed > 0) {
        config_modifier_->Up();
      } else if (y_report_speed < 0) {
        config_modifier_->Down();
      }
    }
  } else {
    for (auto mouse_output : *mouse_output_) {
      mouse_output->MouseMovement(x_report_speed, y_report_speed);
    }
  }
}

std::pair<std::string, std::shared_ptr<Config>>
JoystickInputDeivce::CreateDefaultConfig() {
  auto config = CONFIG_OBJECT(
      CONFIG_OBJECT_ELEM("report_n_scan", CONFIG_INT(5, 1, 100)),
      CONFIG_OBJECT_ELEM("calib_samples", CONFIG_INT(1000, 0, INT32_MAX)),
      CONFIG_OBJECT_ELEM("calib_threshold", CONFIG_INT(400, 0, INT32_MAX)),
      CONFIG_OBJECT_ELEM(
          "x_profile",
          CONFIG_LIST(
              CONFIG_PAIR(CONFIG_INT(20, 0, 2048), CONFIG_INT(40, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(700, 0, 2048), CONFIG_INT(80, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(1000, 0, 2048), CONFIG_INT(120, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(1500, 0, 2048), CONFIG_INT(180, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(2000, 0, 2048),
                          CONFIG_INT(300, 40, 1000)))),
      CONFIG_OBJECT_ELEM(
          "y_profile",
          CONFIG_LIST(
              CONFIG_PAIR(CONFIG_INT(20, 0, 2048), CONFIG_INT(40, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(700, 0, 2048), CONFIG_INT(80, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(1000, 0, 2048), CONFIG_INT(120, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(1500, 0, 2048), CONFIG_INT(180, 40, 1000)),
              CONFIG_PAIR(CONFIG_INT(2000, 0, 2048),
                          CONFIG_INT(300, 40, 1000)))));

  return {"joystick", config};
}

Status JoystickInputDeivce::ParseProfileConfig(
    const ConfigList& list,
    std::vector<std::pair<uint16_t, uint16_t>>* output) {
  output->clear();
  const auto& l = *list.GetList();
  output->push_back({0, 0});
  for (const auto& v : l) {
    if (v->GetType() != Config::LIST) {
      return ERROR;
    }
    const auto& pair = *(((ConfigList*)v.get())->GetList());
    if (pair.size() != 2) {
      return ERROR;
    }
    if (pair[0]->GetType() != Config::INTEGER ||
        pair[1]->GetType() != Config::INTEGER) {
      return ERROR;
    }
    output->push_back(std::make_pair<uint16_t, uint16_t>(
        ((ConfigInt*)(pair[0].get()))->GetValue(),
        ((ConfigInt*)(pair[1].get()))->GetValue()));
  }

  std::sort(output->begin(), output->end(), [](auto a, auto b) {
    return a.first == b.first ? a.second < b.second : a.first < b.first;
  });
  return OK;
}

void JoystickInputDeivce::OnUpdateConfig(const Config* config) {
  if (config->GetType() != Config::OBJECT) {
    LOG_ERROR("Root config has to be an object.");
    return;
  }
  const auto& root_map = *((ConfigObject*)config)->GetMembers();

  // Get speed divider

  auto it = root_map.find("report_n_scan");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `report_n_scan` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`report_n_scan` invalid type");
    return;
  }
  report_n_scan_ = ((ConfigInt*)it->second.get())->GetValue();
  counter_ = 0;

  // Get calibration samples

  it = root_map.find("calib_samples");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `calib_samples` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`calib_samples` invalid type");
    return;
  }
  const int32_t calib_samples = ((ConfigInt*)it->second.get())->GetValue();
  x_.SetCalibrationSamples(calib_samples);
  y_.SetCalibrationSamples(calib_samples);

  // Get calibration threshold

  it = root_map.find("calib_threshold");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `calib_threshold` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`calib_threshold` invalid type");
    return;
  }
  const int32_t calib_threshold = ((ConfigInt*)it->second.get())->GetValue();
  x_.SetCalibrationThreshold(calib_samples);
  y_.SetCalibrationThreshold(calib_samples);

  // Get x_profile

  it = root_map.find("x_profile");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `x_profile` in config");
    return;
  }
  if (it->second->GetType() != Config::LIST) {
    LOG_ERROR("`x_profile` invalid type");
    return;
  }
  if (ParseProfileConfig(*((ConfigList*)it->second.get()), &profile_x_) != OK) {
    LOG_ERROR("Failed to parse `x_profile`");
    return;
  }

  // Get y_profile

  it = root_map.find("y_profile");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `y_profile` in config");
    return;
  }
  if (it->second->GetType() != Config::LIST) {
    LOG_ERROR("`y_profile` invalid type");
    return;
  }
  if (ParseProfileConfig(*((ConfigList*)it->second.get()), &profile_x_) != OK) {
    LOG_ERROR("Failed to parse `y_profile`");
    return;
  }
}

void JoystickInputDeivce::SetConfigMode(bool is_config_mode) {
  is_config_mode_ = is_config_mode;
}

int16_t JoystickInputDeivce::GetSpeed(
    const std::vector<std::pair<uint16_t, uint16_t>>& profile,
    int16_t reading) {
  const uint16_t abs = reading < 0 ? -reading : reading;
  const int8_t sign = reading < 0 ? -1 : 1;

  // Use dumb for loop instead of std::lower_bound to save binary size. Assuming
  // profile is already sorted.
  int16_t speed = profile.back().second * sign;
  for (size_t i = 1; i < profile.size(); ++i) {
    if (abs < profile[i].first && abs >= profile[i - 1].first) {
      speed = profile[i - 1].second * sign;
      break;
    }
  }

  return speed;
}

#if CONFIG_ENABLE_JOYSTICK

static Status registered = DeviceRegistry::RegisterInputDevice(2, []() {
  return std::make_shared<JoystickInputDeivce>(
      CONFIG_JOYSTICK_GPIO_X, CONFIG_JOYSTICK_GPIO_Y,
      CONFIG_JOYSTICK_SMOOTH_BUFFER_LEN, CONFIG_JOYSTICK_FLIP_X_DIR,
      CONFIG_JOYSTICK_FLIP_Y_DIR, CONFIG_SCAN_TICKS);
});

#endif /* CONFIG_ENABLE_JOYSTICK */