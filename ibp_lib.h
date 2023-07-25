#ifndef IBP_LIB_H_
#define IBP_LIB_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  IBP_KEYCODE = 0,
  IBP_CONSUMER,
  IBP_MOUSE,
  IBP_ACTIVE_LAYERS,
  IBP_INVALID,
} FieldType;

typedef struct {
  uint8_t modifier_bitmask;  // Same as the USB HID.
  uint8_t num_keycodes : 3;
  uint8_t keycodes[8];  // 8 keycodes max.
} IBPKeyCodes;

typedef struct {
  uint16_t consumer_keycode;
} IBPConsumer;

typedef struct {
  // For buttons.
  //        7                                                0
  // +------+------+------+------+------+------+------+------+
  // |      Reserved      |FRWARD| BACK |MSE_M |MSE_R |MSE_L |
  // +------+------+------+------+------+------+------+------+
  uint8_t button_bitmask;
  uint8_t x;
  uint8_t y;
  uint8_t vertical;
  uint8_t horizontal;
} IBPMouse;

typedef struct {
  // Represents little endian uint32_t
  uint8_t num_activated_layers : 3;
  uint8_t active_layers[8];
  // uint8_t layers[4];  // At most 32 layers.
} IBPLayers;

typedef union {
  IBPKeyCodes keycodes;
  IBPConsumer consumer_keycode;
  IBPMouse mouse;
  IBPLayers layers;
} FieldData;

typedef struct {
  //        7                                                0
  // +------+------+------+------+------+------+------+------+
  // |   number of data bytes    |     field type     |parity|
  // +------+------+------+------+------+------+------+------+
  FieldType field_type : 3;
  FieldData field_data;
} IBPSegment;

// // For the actual transaction through low level protocols such as SPI, I2C,
// the
// // total number of bytes transmitted is always padded to a multiple of 4. The
// // total number of bytes includes the transaction header and padding.
// typedef struct {
//   //        7                                                0
//   // +------+------+------+------+------+------+------+------+
//   // |    total number of bytes incl. this header     |parity|
//   // +------+------+------+------+------+------+------+------+
//   uint8_t total_bytes : 7;
// } IBPTransactionHeader;

int8_t SerializeSegments(const IBPSegment* segments, uint8_t num_segments,
                         uint8_t* output, uint8_t buffer_size);
int8_t DeSerializeSegment(const uint8_t* input, uint8_t input_buffer_size,
                          IBPSegment* segment);
int8_t GetTransactionTotalSize(uint8_t transaction_first_byte);

#endif /* IBP_LIB_H_ */