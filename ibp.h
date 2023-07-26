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

class IBPDeviceBase : public virtual KeyboardOutputDevice,
                      public virtual MouseOutputDevice,
                      public virtual GenericInputDevice,
                      public virtual IBPDriverBase {
 public:
  IBPDeviceBase();

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

  void InputLoopStart() override {}
  void InputTick() override;

 protected:
  std::string GetOutPacket();

  // Fully packet with the transaction header.
  void SetInPacket(const std::string& packet);

 private:
  bool is_config_mode_;
  bool has_update_[IBP_TOTAL];
  IBPSegment segments_[IBP_TOTAL];
  std::string inbound_packet_;
  std::string outbound_packet_;

  // Protects both the inbound and outbound packets. One lock is enough because
  // there are usually only two tasks involved: the input task and the low level
  // protocol task.
  SemaphoreHandle_t packet_semaphore_;
};

#endif /* IBP_H_ */