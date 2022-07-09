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
  void SetJoystickXProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);
  void SetJoystickYProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);

 private:
  Configuration() = default;
  
  std::vector<std::pair<uint16_t, uint8_t>> joystick_x_profile_;
  std::vector<std::pair<uint16_t, uint8_t>> joystick_y_profile_;

  static Configuration* singleton_;
};

#endif /* CONFIGURATION_H_ */