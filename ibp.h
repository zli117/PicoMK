#ifndef IBP_H_
#define IBP_H_

#include "base.h"
#include "ibp_base.h"

class IBPProtocolDevice : public virtual KeyboardOutputDevice,
                          public virtual MouseOutputDevice,
                          public virtual GenericInputDevice,
                          public virtual IBPDriverBase {
 public:
  virtual void SendKeycode(uint8_t keycode) = 0;
  virtual void SendKeycode(const std::vector<uint8_t>& keycode) = 0;
  virtual void SendConsumerKeycode(uint16_t keycode) = 0;
  virtual void ChangeActiveLayers(const std::vector<bool>& layers) = 0;

  virtual void MouseKeycode(uint8_t keycode) = 0;
  virtual void MouseMovement(int8_t x, int8_t y) = 0;
  virtual void Pan(int8_t x, int8_t y) = 0;

  virtual void InputLoopStart() = 0;
  virtual void InputTick() = 0;

  virtual Status IBPInitialize() = 0;
};

#endif /* IBP_H_ */