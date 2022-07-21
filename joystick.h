#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <stdint.h>

#include <vector>

#include "FreeRTOS.h"
#include "base.h"
#include "config.h"
#include "configuration.h"
#include "semphr.h"
#include "utils.h"

class CenteringPotentialMeterDriver {
 public:
  CenteringPotentialMeterDriver(uint8_t adc_pin, size_t smooth_buffer_size,
                                bool flip_dir);

  // Raw ADC reading
  int16_t GetValue();

  void SetMappedValue(int16_t mapped);

  void SetCalibrationSamples(uint32_t calibration_samples);
  void SetCalibrationThreshold(uint32_t calibration_threshold);

 protected:
  uint8_t adc_;
  uint16_t origin_;
  std::vector<uint16_t> buffer_;
  uint32_t buffer_idx_;
  int32_t sum_;
  bool flip_dir_;

  uint16_t last_value_;
  uint32_t calibration_samples_;
  uint32_t calibration_threshold_;
  uint32_t calibration_sum_;
  uint32_t calibration_zero_count_;
  uint32_t calibration_count_;
};

class JoystickInputDeivce : virtual public GenericInputDevice {
 public:
  JoystickInputDeivce(uint8_t x_adc_pin, uint8_t y_adc_pin, size_t buffer_size,
                      bool flip_x_dir, bool flip_y_dir, uint8_t scan_num_ticks);

  void InputLoopStart() override {}
  void InputTick() override;
  void SetConfigMode(bool is_config_mode) override;
  std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig()
      override;
  void OnUpdateConfig(const Config* config) override;

 protected:
  int16_t GetSpeed(const std::vector<std::pair<uint16_t, uint16_t>>& profile,
                   int16_t reading);

  Status ParseProfileConfig(const ConfigList& list,
                            std::vector<std::pair<uint16_t, uint16_t>>* output);

  CenteringPotentialMeterDriver x_;
  CenteringPotentialMeterDriver y_;
  std::vector<std::pair<uint16_t, uint16_t>> profile_x_;
  std::vector<std::pair<uint16_t, uint16_t>> profile_y_;
  int16_t report_n_scan_;
  uint8_t counter_;
  int16_t x_move_;
  int16_t y_move_;
  bool enable_joystick_;
  bool is_config_mode_;
  const int16_t scan_num_ticks_;

  // SemaphoreHandle_t semaphore_;
};

Status RegisterJoystick(uint8_t tag, uint8_t x_adc_pin, uint8_t y_adc_pin,
                        size_t buffer_size, bool flip_x_dir, bool flip_y_dir);

#endif /* JOYSTICK_H_ */
