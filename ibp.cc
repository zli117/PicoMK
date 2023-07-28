#include "ibp.h"

#include <string.h>

#include <utility>

#include "layout.h"
#include "tusb.h"
#include "utils.h"

extern "C" {
#include "ibp_lib.h"
}

IBPDeviceBase::IBPDeviceBase() : is_config_mode_(false), has_update_({0}) {
  packet_semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(packet_semaphore_);
}

void IBPDeviceBase::SendKeycode(uint8_t keycode) {
  if (is_config_mode_) {
    return;
  }
  auto& keycodes = segments_[IBP_KEYCODE].field_data.keycodes;
  if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
    keycodes.modifier_bitmask |= (1 << (keycode - HID_KEY_CONTROL_LEFT));
  } else if (keycodes.num_keycodes < IBPKeyCodesMAX) {
    keycodes.keycodes[keycodes.num_keycodes++] = keycode;
  }
  has_update_[IBP_KEYCODE] = true;
}

void IBPDeviceBase::SendKeycode(const std::vector<uint8_t>& keycodes) {
  for (uint8_t keycode : keycodes) {
    SendKeycode(keycode);
  }
}
void IBPDeviceBase::SendConsumerKeycode(uint16_t keycode) {
  if (is_config_mode_) {
    return;
  }
  segments_[IBP_CONSUMER].field_data.consumer_keycode.consumer_keycode =
      keycode;
  has_update_[IBP_CONSUMER] = true;
}

void IBPDeviceBase::ChangeActiveLayers(const std::vector<bool>& layers) {
  if (is_config_mode_) {
    return;
  }
  auto& output_layers = segments_[IBP_ACTIVE_LAYERS].field_data.layers;
  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i]) {
      output_layers.active_layers[output_layers.num_activated_layers++] = i;
    }
    if (output_layers.num_activated_layers >= IBPActiveLayersMAX) {
      break;
    }
  }
  has_update_[IBP_ACTIVE_LAYERS] = true;
}

void IBPDeviceBase::MouseKeycode(uint8_t keycode) {
  if (keycode > MSE_FORWARD || is_config_mode_) {
    return;
  }
  segments_[IBP_MOUSE].field_data.mouse.button_bitmask |= (1 << keycode);
  has_update_[IBP_MOUSE] = true;
}

void IBPDeviceBase::MouseMovement(int8_t x, int8_t y) {
  if (is_config_mode_) {
    return;
  }
  auto& mouse = segments_[IBP_MOUSE].field_data.mouse;
  mouse.x = x;
  mouse.y = y;
  has_update_[IBP_MOUSE] = true;
}

void IBPDeviceBase::Pan(int8_t x, int8_t y) {
  if (is_config_mode_) {
    return;
  }
  auto& mouse = segments_[IBP_MOUSE].field_data.mouse;
  mouse.horizontal = x;
  mouse.vertical = y;
  has_update_[IBP_MOUSE] = true;
}

void IBPDeviceBase::StartOfInputTick() {
  memset(has_update_, false, sizeof(has_update_));
  memset(segments_, 0, sizeof(segments_));
}

constexpr size_t kOutputBufferSize = 128;

void IBPDeviceBase::FinalizeInputTickOutput() {
  IBPSegment segments[IBP_TOTAL];
  size_t num_segments = 0;
  for (size_t i = 0; i < IBP_TOTAL; ++i) {
    if (has_update_[i]) {
      segments[num_segments++] = segments_[i];
    }
  }
  uint8_t buffer[kOutputBufferSize];
  const int num_bytes =
      SerializeSegments(segments, num_segments, buffer, sizeof(buffer));
  if (num_bytes <= 0) {
    LOG_ERROR("Failed to serialize segments");
    return;
  }
  LockSemaphore lock(packet_semaphore_);
  outbound_packet_ = std::string(buffer, buffer + num_bytes);
}

void IBPDeviceBase::InputTick() {
  std::string local_copy;
  {
    LockSemaphore lock(packet_semaphore_);
    std::swap(local_copy, inbound_packet_);
  }
  const size_t total_bytes = local_copy.size();
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(local_copy.c_str());
  size_t offset = 1;
  while (offset < total_bytes) {
    IBPSegment segment;
    int bytes_consumed =
        DeSerializeSegment(bytes + offset, total_bytes - offset, &segment);
    if (bytes_consumed <= 0) {
      break;
    }
    switch (segment.field_type) {
      case IBP_KEYCODE: {
        const auto& ibp_keycodes = segment.field_data.keycodes;
        std::vector<uint8_t> keycodes(
            ibp_keycodes.keycodes,
            ibp_keycodes.keycodes + ibp_keycodes.num_keycodes);
        uint8_t bitmask = ibp_keycodes.modifier_bitmask;
        for (size_t i = 0; i < 8; ++i) {
          if (bitmask & 0x01) {
            keycodes.push_back(HID_KEY_CONTROL_LEFT + i);
          }
          bitmask >>= 1;
        }
        for (auto& keyboard_output : *keyboard_output_) {
          if (keyboard_output.get() == this) {
            continue;
          }
          keyboard_output->SendKeycode(keycodes);
        }
        break;
      }
      case IBP_CONSUMER: {
        const auto& ibp_consumer = segment.field_data.consumer_keycode;
        for (auto& keyboard_output : *keyboard_output_) {
          if (keyboard_output.get() == this) {
            continue;
          }
          keyboard_output->SendConsumerKeycode(ibp_consumer.consumer_keycode);
        }
        break;
      }
      case IBP_MOUSE: {
        const auto& ibp_mouse = segment.field_data.mouse;
        std::vector<uint8_t> mouse_keycodes;
        uint8_t bitmask = ibp_mouse.button_bitmask;
        for (size_t i = MSE_L; i <= MSE_FORWARD; ++i) {
          if (bitmask & 0x01) {
            mouse_keycodes.push_back(i);
          }
          bitmask >>= 1;
        }
        for (auto& mouse_output : *mouse_output_) {
          if (mouse_output.get() == this) {
            continue;
          }
          for (auto k : mouse_keycodes) {
            mouse_output->MouseKeycode(k);
          }
          mouse_output->MouseMovement(ibp_mouse.x, ibp_mouse.y);
          mouse_output->Pan(ibp_mouse.horizontal, ibp_mouse.vertical);
        }
        break;
      }
      // TODO: pass the layers in the future.
      default:
        break;
    }
  }
}

std::string IBPDeviceBase::GetOutPacket() {
  uint8_t buffer[kOutputBufferSize];
  const int num_bytes =
      SerializeSegments(segments_, /*num_segments=*/0, buffer, sizeof(buffer));
  if (num_bytes <= 0) {
    LOG_ERROR("Create empty packet shouldn't fail.");
    return "";
  }
  std::string packet_copy(buffer, buffer + num_bytes);
  {
    LockSemaphore lock(packet_semaphore_);
    std::swap(packet_copy, outbound_packet_);
  }
  return packet_copy;
}

void IBPDeviceBase::SetInPacket(const std::string& packet) {
  LockSemaphore lock(packet_semaphore_);
  inbound_packet_ = packet;
}
