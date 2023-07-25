#ifndef IBP_BASE_H_
#define IBP_BASE_H_

#include "stdint.h"

typedef enum {
  KEYBOARD = 0,
  CONSUMER,
  MOUSE,
  ACTIVE_LAYERS,
  INVALID,
} FieldType;

// Do not use bitfields as their binary format is compiler dependent.

typedef struct __attribute__((packed)) {
  //        7                                                0
  // +------+------+------+------+------+------+------+------+
  // |    number of segments     |      reserved      |parity|
  // +------+------+------+------+------+------+------+------+
  uint8_t data;
} PacketHeader;

typedef struct __attribute__((packed)) {
  //        7                                                0
  // +------+------+------+------+------+------+------+------+
  // |   number of data bytes    |     field type     |parity|
  // +------+------+------+------+------+------+------+------+
  uint8_t header;
  uint8_t data_crc8;
} SegmentHeader;

typedef struct __attribute__((packed)) {
  uint8_t modifier_bitmask;  // Same as the USB HID.
  uint8_t keycodes[6];  // 6 keycodes max. Can be less. Infer the actual size
                        // from the segment header.
} KeyCodes;

typedef struct __attribute__((packed)) {
  // Represent uint16_t in little endian.
  uint8_t consumer_keycode[2];
} Consumer;

typedef struct __attribute__((packed)) {
  // For buttons.
  //        7                                                0
  // +------+------+------+------+------+------+------+------+
  // |      Reserved      |FRWARD| BACK |MSE_M |MSE_R |MSE_L |
  // +------+------+------+------+------+------+------+------+
  uint8_t buttons;
  uint8_t x;
  uint8_t y;
  uint8_t vertical;
  uint8_t horizontal;
} Mouse;

uint8_t CalculateParity(uint8_t byte);
uint8_t CalculateCRC8(uint8_t* data, uint8_t length);

int8_t GetNumberofSegments(PacketHeader packet_header);
int8_t GetBytesInSegment(SegmentHeader segment_header);
FieldType GetFieldType(SegmentHeader segment_header);

#endif /* IBP_BASE_H_ */