#ifndef ROTARY_ENCODER_H_
#define ROTARY_ENCODER_H_

#include "FreeRTOS.h"
#include "base.h"
#include "semphr.h"

class RotaryEncoder : public GenericInputDevice {
 public:
  RotaryEncoder();
  RotaryEncoder(uint8_t pin_a, uint8_t pin_b, uint8_t resolution);
  
  void InputLoopStart() override {}
  void InputTick() override;
  void OnUpdateConfig() override {}
  void SetConfigMode(bool is_config_mode) override;

 protected:
  virtual void HandleMovement(bool dir);

  const uint8_t pin_a_; 
  const uint8_t pin_b_; 
  const uint8_t resolution_;
  bool a_state_;
  uint8_t pulse_count_;
  bool dir_;
  bool is_config_;

  // SemaphoreHandle_t semaphore_;
};

#endif /* ROTARY_ENCODER_H_ */