#ifndef IBP_LIB_H_
#define IBP_LIB_H_

#ifdef IBP_KERNEL_MODULE
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

typedef enum {
  IBP_KEYCODE = 0,
  IBP_CONSUMER,
  IBP_MOUSE,
  IBP_ACTIVE_LAYERS,
  IBP_TOTAL,
} FieldType;

#define IBPKeyCodesMAX 8
#define IBPActiveLayersMAX 8

typedef struct {
  uint8_t modifier_bitmask;  // Same as the USB HID.
  uint8_t num_keycodes : 3;
  uint8_t keycodes[IBPKeyCodesMAX];
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
  int8_t x;
  int8_t y;
  int8_t vertical;
  int8_t horizontal;
} IBPMouse;

typedef struct {
  uint8_t num_activated_layers : 3;
  uint8_t active_layers[IBPActiveLayersMAX];
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