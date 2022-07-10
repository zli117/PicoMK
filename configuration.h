#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <stdint.h>

#include <utility>
#include <vector>

class Configuration {
 public:
  static Configuration* GetConfig();

  // Joystick X, Y accelaration profile
  inline const std::vector<std::pair<uint16_t, uint8_t>>* GetJoystickXProfile()
      const {
    return &joystick_x_profile_;
  }
  inline const std::vector<std::pair<uint16_t, uint8_t>>* GetJoystickYProfile()
      const {
    return &joystick_y_profile_;
  }
  inline uint8_t GetJoystickScanDivider() const {
    return joystick_scan_frequency_divider_;
  }
  inline uint32_t GetJoystickCalibrationSamples() const {
    return joystick_calibration_samples_;
  }
  inline uint32_t GetJoystickCalibrationThreshold() const {
    return joystick_calibration_threshold_;
  }
  void SetJoystickXProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);
  void SetJoystickYProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);
  void SetJoystickScanDivider(uint8_t divider) {
    joystick_scan_frequency_divider_ = divider;
  }
  void SetJoystickCalibrationSamples(uint32_t calibration_samples) {
    joystick_calibration_samples_ = calibration_samples;
  }
  void SetJoystickCalibrationThreshold(uint32_t calibration_threshold) {
    joystick_calibration_threshold_ = calibration_threshold;
  }

 private:
  Configuration() = default;

  std::vector<std::pair<uint16_t, uint8_t>> joystick_x_profile_;
  std::vector<std::pair<uint16_t, uint8_t>> joystick_y_profile_;
  uint8_t joystick_scan_frequency_divider_;
  uint32_t joystick_calibration_samples_;
  uint32_t joystick_calibration_threshold_;

  static Configuration* singleton_;
};

#endif /* CONFIGURATION_H_ */