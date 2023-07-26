#include "ibp_lib.h"

#define SEGMENT_HEADER_BYTES 2

static uint8_t CalculateParity(uint8_t byte) {
  return 0;  // Dummy
}

static uint8_t CalculateCRC8(uint8_t* data, uint8_t length) {
  return 0;  // Dummy
}

static bool IsBigEndian() {
  uint16_t i = 0x0100;
  uint8_t* ii = &i;
  return ii[0];
}

static uint8_t CreateSegmentHeader(uint8_t data_size, FieldType field_type) {
  uint8_t header = (data_size << 4) | (field_type << 1);
  return header | (CalculateParity(header) & 0x01);
}

static int8_t SerializeKeycodes(const IBPSegment* keycodes_seg, uint8_t* buf,
                                uint8_t buf_size) {
  const uint8_t data_size =
      sizeof(keycodes_seg->field_data.keycodes.modifier_bitmask) +
      keycodes_seg->field_data.keycodes.num_keycodes;
  if (buf_size < data_size + SEGMENT_HEADER_BYTES) {
    return -1;
  }
  buf[0] = CreateSegmentHeader(data_size, IBP_KEYCODE);
  buf[2] = keycodes_seg->field_data.keycodes.modifier_bitmask;
  for (int i = 0; i < keycodes_seg->field_data.keycodes.num_keycodes; ++i) {
    buf[i + 3] = keycodes_seg->field_data.keycodes.keycodes[i];
  }
  buf[1] = CalculateCRC8(buf[2], data_size);
  return data_size + SEGMENT_HEADER_BYTES;
}

static int8_t SerializeConsumerKeycodes(const IBPSegment* consumer_seg,
                                        uint8_t* buf, uint8_t buf_size) {
  const uint8_t data_size = sizeof(uint16_t);
  if (buf_size < data_size + SEGMENT_HEADER_BYTES) {
    return -1;
  }
  buf[0] = CreateSegmentHeader(data_size, IBP_KEYCODE);
  uint16_t* ptr = buf[2];
  *ptr = consumer_seg->field_data.consumer_keycode.consumer_keycode;
  if (IsBigEndian()) {
    uint8_t tmp = buf[2];
    buf[2] = buf[3];
    buf[3] = tmp;
  }
  buf[1] = CalculateCRC8(buf[2], data_size);
  return data_size + SEGMENT_HEADER_BYTES;
}

static int8_t SerializeMouse(const IBPSegment* mouse_seg, uint8_t* buf,
                             uint8_t buf_size) {
  const uint8_t data_size = sizeof(uint8_t) * 5;
  if (buf_size < data_size + SEGMENT_HEADER_BYTES) {
    return -1;
  }
  buf[0] = CreateSegmentHeader(data_size, IBP_KEYCODE);
  buf[2] = mouse_seg->field_data.mouse.button_bitmask;
  buf[3] = (uint8_t)mouse_seg->field_data.mouse.x;
  buf[4] = (uint8_t)mouse_seg->field_data.mouse.y;
  buf[5] = (uint8_t)mouse_seg->field_data.mouse.vertical;
  buf[6] = (uint8_t)mouse_seg->field_data.mouse.horizontal;
  buf[1] = CalculateCRC8(buf[2], data_size);
  return data_size + SEGMENT_HEADER_BYTES;
}

static int8_t SerializeLayers(const IBPSegment* layer_seg, uint8_t* buf,
                              uint8_t buf_size) {
  const uint8_t data_size = layer_seg->field_data.layers.num_activated_layers;
  if (buf_size < data_size + SEGMENT_HEADER_BYTES) {
    return -1;
  }
  buf[0] = CreateSegmentHeader(data_size, IBP_KEYCODE);
  for (int i = 0; i < data_size; ++i) {
    buf[2 + i] = layer_seg->field_data.layers.active_layers[i];
  }
  buf[1] = CalculateCRC8(buf[2], data_size);
  return data_size + SEGMENT_HEADER_BYTES;
}

int8_t SerializeSegments(const IBPSegment* segments, uint8_t num_segments,
                         uint8_t* output, uint8_t buffer_size) {
  uint8_t total_bytes = 1;
  for (uint8_t i = 0; i < num_segments; ++i) {
    switch (segments[i].field_type) {
      case IBP_KEYCODE: {
        int8_t byte_count = SerializeKeycodes(
            &segments[i], &output[total_bytes], buffer_size - total_bytes);
        if (byte_count < 0) {
          return -1;
        }
        total_bytes += byte_count;
        break;
      }
      case IBP_CONSUMER: {
        int8_t byte_count = SerializeConsumerKeycodes(
            &segments[i], &output[total_bytes], buffer_size - total_bytes);
        if (byte_count < 0) {
          return -1;
        }
        total_bytes += byte_count;
        break;
      }
      case IBP_MOUSE: {
        int8_t byte_count = SerializeMouse(&segments[i], &output[total_bytes],
                                           buffer_size - total_bytes);
        if (byte_count < 0) {
          return -1;
        }
        total_bytes += byte_count;
        break;
      }
      case IBP_ACTIVE_LAYERS: {
        int8_t byte_count = SerializeLayers(&segments[i], &output[total_bytes],
                                            buffer_size - total_bytes);
        if (byte_count < 0) {
          return -1;
        }
        total_bytes += byte_count;
        break;
      }

      default:
        break;
    }
  }

  // Pad to multiple of 4
  const uint8_t num_padding = (4 - total_bytes % 4) % 4;
  if (total_bytes + num_padding > buffer_size) {
    return -1;
  }
  for (int i = 0; i < num_padding; ++i) {
    output[total_bytes + i] = 0;
  }
  total_bytes += num_padding;
  output[0] = (total_bytes << 1) | CalculateParity(total_bytes);
  return total_bytes;
}

static bool DeSerializeKeycodes(const uint8_t* buf, uint8_t buf_size,
                                IBPSegment* keycodes_seg) {
  if (buf_size > IBPKeyCodesMAX + 1) {
    return false;
  }
  keycodes_seg->field_data.keycodes.modifier_bitmask = buf[0];
  keycodes_seg->field_data.keycodes.num_keycodes = buf_size - 1;
  for (int i = 1; i < buf_size; ++i) {
    keycodes_seg->field_data.keycodes.keycodes[i - 1] = buf[i];
  }
  return true;
}

static bool DeSerializeConsumerKeycodes(const uint8_t* buf, uint8_t buf_size,
                                        IBPSegment* consumer_seg) {
  if (buf_size != 2) {
    return false;
  }
  uint8_t buf_copy[2];
  if (IsBigEndian()) {
    buf_copy[1] = buf[0];
    buf_copy[0] = buf[1];
  } else {
    buf_copy[0] = buf[0];
    buf_copy[1] = buf[1];
  }
  consumer_seg->field_data.consumer_keycode.consumer_keycode =
      *((uint16_t*)buf_copy);
  return true;
}

static bool DeSerializeMouse(const uint8_t* buf, uint8_t buf_size,
                             IBPSegment* mouse_seg) {
  if (buf_size != 5) {
    return false;
  }
  mouse_seg->field_data.mouse.button_bitmask = buf[0];
  mouse_seg->field_data.mouse.x = (int8_t)buf[1];
  mouse_seg->field_data.mouse.y = (int8_t)buf[2];
  mouse_seg->field_data.mouse.vertical = (int8_t)buf[3];
  mouse_seg->field_data.mouse.horizontal = (int8_t)buf[4];
  return true;
}

static bool DeSerializeLayer(const uint8_t* buf, uint8_t buf_size,
                             IBPSegment* layer_seg) {
  if (buf_size > IBPActiveLayersMAX) {
    return false;
  }
  layer_seg->field_data.layers.num_activated_layers = buf_size;
  for (int i = 0; i < buf_size; ++i) {
    layer_seg->field_data.layers.active_layers[i] = buf[i];
  }
  return true;
}

int8_t DeSerializeSegment(const uint8_t* input, uint8_t input_buffer_size,
                          IBPSegment* segment) {
  if (input_buffer_size == 0 || input[0] == 0 ||
      CalculateParity(input[0]) != 0) {
    return -1;
  }
  const uint8_t num_data_bytes = (input[0] >> 4) & 0xf0;
  segment->field_type = (input[0] & 0x0e) >> 1;
  if (num_data_bytes + SEGMENT_HEADER_BYTES > input_buffer_size ||
      CalculateCRC8(&input[2], num_data_bytes) != input[1]) {
    return -1;
  }
  switch (segment->field_type) {
    case IBP_KEYCODE: {
      if (DeSerializeKeycodes(&input[2], num_data_bytes, segment)) {
        return num_data_bytes + SEGMENT_HEADER_BYTES;
      }
      break;
    }
    case IBP_CONSUMER: {
      if (DeSerializeConsumerKeycodes(&input[2], num_data_bytes, segment)) {
        return num_data_bytes + SEGMENT_HEADER_BYTES;
      }
      break;
    }
    case IBP_MOUSE: {
      if (DeSerializeMouse(&input[2], num_data_bytes, segment)) {
        return num_data_bytes + SEGMENT_HEADER_BYTES;
      }
      break;
    }
    case IBP_ACTIVE_LAYERS: {
      if (DeSerializeLayer(&input[2], num_data_bytes, segment)) {
        return num_data_bytes + SEGMENT_HEADER_BYTES;
      }
      break;
    }
    default:
      break;
  }
  return -1;
}

int8_t GetTransactionTotalSize(uint8_t transaction_first_byte) {
  const uint8_t total_bytes = transaction_first_byte >> 1;
  if (CalculateParity(transaction_first_byte) != 0 ||
      transaction_first_byte % 4 != 0) {
    return -1;
  }
  return total_bytes;
}
