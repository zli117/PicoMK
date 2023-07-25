#include "ibp.h"

#include "utils.h"

extern "C" {
#include "ibp_lib.h"
}

IBPProtocolOutputDevice::IBPProtocolOutputDevice()
    : is_config_mode_(false),
      has_keycode_update_(false),
      has_mouse_update_(false),
      has_consumer_key_update_(false),
      has_layer_update_(false) {
  // semaphore_ = xSemaphoreCreateBinary();
  // xSemaphoreGive(semaphore_);
}

void IBPProtocolOutputDevice::SendKeycode(uint8_t keycode) {
}

void IBPProtocolOutputDevice::SendKeycode(const std::vector<uint8_t>& keycode) {
}
void IBPProtocolOutputDevice::SendConsumerKeycode(uint16_t keycode) {}
void IBPProtocolOutputDevice::ChangeActiveLayers(
    const std::vector<bool>& layers) {}