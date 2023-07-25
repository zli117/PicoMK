#ifndef IBP_H_
#define IBP_H_

#include <string>
#include <string_view>

#include "FreeRTOS.h"
#include "base.h"
#include "semphr.h"

extern "C" {
#include "ibp_lib.h"
}

class IBPProtocolOutputDevice : public virtual KeyboardOutputDevice,
                                public virtual MouseOutputDevice,
                                public virtual IBPDriverBase {
 public:
  IBPProtocolOutputDevice();

  // No need to run anything on the output task as it's likely subclass will
  // have its own task for communication.
  void OutputTick() override {}

  // Input task

  void SendKeycode(uint8_t keycode) override;
  void SendKeycode(const std::vector<uint8_t>& keycode) override;
  void SendConsumerKeycode(uint16_t keycode) override;
  void ChangeActiveLayers(const std::vector<bool>& layers) override;

  void MouseKeycode(uint8_t keycode) override;
  void MouseMovement(int8_t x, int8_t y) override;
  void Pan(int8_t x, int8_t y) override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  void SetConfigMode(bool is_config_mode) { is_config_mode_ = is_config_mode; }

 protected:
  bool is_config_mode_;
  bool has_keycode_update_;
  bool has_mouse_update_;
  bool has_consumer_key_update_;
  bool has_layer_update_;
  IBPKeyCodes keycodes_;
  IBPConsumer consumer_keycodes_;
  IBPMouse mouse_;
  IBPLayers layers_;
  // SemaphoreHandle_t semaphore_;
  std::string outbound_packet_;

  Status ReceiveBuffer(std::string_view) override final { return OK; }
};

#endif /* IBP_H_ */