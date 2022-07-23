#include "temperature.h"

#include <cstdio>
#include <memory>

#include "hardware/adc.h"
#include "hardware/timer.h"

constexpr uint8_t kADC = 4;
constexpr uint8_t kBufferSize = 100;

TemperatureInputDeivce::TemperatureInputDeivce()
    : is_fahrenheit_(true),
      buffer_(kBufferSize),
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
}

void TemperatureInputDeivce::InputTick() {
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
  auto config =
      CONFIG_OBJECT(CONFIG_OBJECT_ELEM("fahrenheit", CONFIG_INT(1, 0, 1)));
  return {"temperature", config};
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
}

void TemperatureInputDeivce::WriteTemp(int32_t temp) {
  std::unique_ptr<char[]> buffer(new char[16]);
  for (auto screen : *screen_output_) {
    size_t len = std::snprintf(buffer.get(), 16, "Temp: %4d", temp);

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
