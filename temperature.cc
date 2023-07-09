#include "temperature.h"

#include <cstdio>
#include <memory>

#include "hardware/adc.h"
#include "hardware/timer.h"

constexpr uint8_t kADC = 4;
constexpr uint8_t kBufferSize = 100;

TemperatureInputDeivce::TemperatureInputDeivce()
    : is_fahrenheit_(true),
      is_config_(false),
      enabled_(true),
      buffer_(kBufferSize),
      sample_every_ticks_(1),
      counter_(0),
      buffer_idx_(0),
      sum_(0),
      prev_temp_(0) {
  adc_init();
  adc_set_temp_sensor_enabled(true);
}

void TemperatureInputDeivce::InputLoopStart() {
  adc_select_input(kADC);

  sum_ = 0;

  // Fill the buffer
  for (size_t i = 0; i < buffer_.size(); ++i) {
    const uint16_t reading = adc_read();
    buffer_[i] = reading;
    sum_ += reading;
    busy_wait_us(2);
  }
  buffer_idx_ = 0;
  prev_temp_ = 0;
}

void TemperatureInputDeivce::InputTick() {
  if (!enabled_) {
    return;
  }

  counter_ = (counter_ + 1) % sample_every_ticks_;
  if (counter_ != 0) {
    return;
  }

  adc_select_input(kADC);
  const uint16_t adc_raw = adc_read();

  sum_ -= buffer_[buffer_idx_];
  sum_ += adc_raw;
  buffer_[buffer_idx_] = adc_raw;
  buffer_idx_ = (buffer_idx_ + 1) % buffer_.size();

  int32_t temp = ConvertTemperature();
  if (temp != prev_temp_) {
    prev_temp_ = temp;
    WriteTemp(temp);
  }
}

std::pair<std::string, std::shared_ptr<Config>>
TemperatureInputDeivce::CreateDefaultConfig() {
  auto config = CONFIG_OBJECT(
      CONFIG_OBJECT_ELEM("fahrenheit", CONFIG_INT(1, 0, 1)),
      CONFIG_OBJECT_ELEM("enabled", CONFIG_INT(1, 0, 1)),
      CONFIG_OBJECT_ELEM("sample_n_ticks", CONFIG_INT(100, 0, 1000)));
  return {"Temperature", config};
}

void TemperatureInputDeivce::OnUpdateConfig(const Config* config) {
  if (config->GetType() != Config::OBJECT) {
    LOG_ERROR("Root config has to be an object.");
    return;
  }
  const auto& root_map = *((ConfigObject*)config)->GetMembers();
  auto it = root_map.find("fahrenheit");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `fahrenheit` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`fahrenheit` invalid type");
    return;
  }
  is_fahrenheit_ = ((ConfigInt*)it->second.get())->GetValue() == 1;

  it = root_map.find("enabled");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `enabled` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`enabled` invalid type");
    return;
  }
  enabled_ = ((ConfigInt*)it->second.get())->GetValue() == 1;

  it = root_map.find("sample_n_ticks");
  if (it == root_map.end()) {
    LOG_ERROR("Can't find `sample_n_ticks` in config");
    return;
  }
  if (it->second->GetType() != Config::INTEGER) {
    LOG_ERROR("`sample_n_ticks` invalid type");
    return;
  }
  sample_every_ticks_ = ((ConfigInt*)it->second.get())->GetValue();
  counter_ = 0;
}

void TemperatureInputDeivce::SetConfigMode(bool is_config_mode) {
  is_config_ = is_config_mode;
}

void TemperatureInputDeivce::WriteTemp(int32_t temp) {
  // Note: This function writes directly to the screen output because it's
  // allowed by the framework. However, it's generally a bad idea, since the
  // code here has no way of konwing what's being painted on the screen by other
  // devices. A better way is to create a mixin. See display_mixins.h for
  // examples.
  if (is_config_) {
    return;
  }

  for (auto screen : *screen_output_) {
    std::unique_ptr<char[]> buffer(new char[screen->GetNumCols() / 8]);
    const size_t padding = screen->GetNumCols() / 8 - 7;
    size_t len = std::snprintf(buffer.get(), 16, "Temp:%*d%s", padding, temp,
                               is_fahrenheit_ ? "F" : "C");

    // Clear the row first
    screen->DrawRect(screen->GetNumRows() - 8, 0, screen->GetNumRows(),
                     screen->GetNumCols(), true, ScreenOutputDevice::SUBTRACT);
    screen->DrawText(screen->GetNumRows() - 8, 0,
                     std::string(buffer.get(), len), ScreenOutputDevice::F8X8,
                     ScreenOutputDevice::ADD);
  }
}

int32_t TemperatureInputDeivce::ConvertTemperature() {
  constexpr float kConversionFactor = 3.3f / (1 << 12);

  float adc = (float)sum_ * kConversionFactor / buffer_.size();
  float temp = 27.0f - (adc - 0.706f) / 0.001721f;

  if (is_fahrenheit_) {
    return (int32_t)(temp * 9 / 5 + 32 + 0.5);
  } else {
    return (int32_t)(temp + 0.5);
  }
}

Status RegisterTemperatureInput(uint8_t tag) {
  return DeviceRegistry::RegisterInputDevice(
      tag, []() { return std::make_shared<TemperatureInputDeivce>(); });
}
