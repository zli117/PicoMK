#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "FreeRTOS.h"
#include "base.h"
#include "configuration.h"
#include "layout.h"
#include "semphr.h"
#include "joystick.h" 
#include "utils.h"

// status PeripheralInit();

// status StartPeripheralTask();

// // Peripheral API. Threadsafe and always available regardless of the config.
// // However, they might be nop if components are disabled.

// enum JoystickMode {
//   MOUSE = 0,
//   XY_SCROLL,
// };

// status SetMouseButtonState(BuiltInCustomKeyCode mouse_key, bool is_pressed);
// status SetJoystickMode(JoystickMode mode);

class JoystickInputDeivce : public GenericInputDevice {
 public:
  JoystickInputDeivce(const Configuration* config, uint8_t x_adc_pin,
                      uint8_t y_adc_pin, size_t buffer_size, bool flip_x_dir,
                      bool flip_y_dir);

  void Tick() override;
  void OnUpdateConfig() override;
  void SetConfigMode(bool is_config_mode) override;

 protected:
  int8_t TranslateReading(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile,
      int16_t reading);

  const Configuration* config_;
  CenteringPotentialMeterDriver x_;
  CenteringPotentialMeterDriver y_;
  std::vector<std::pair<uint16_t, uint8_t>> profile_x_;
  std::vector<std::pair<uint16_t, uint8_t>> profile_y_;
  uint8_t divider_;
  uint8_t counter_;
  bool is_config_mode_;

  SemaphoreHandle_t semaphore_;
};

#endif /* PERIPHERAL_H_ */