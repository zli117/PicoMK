#ifndef USB_H_
#define USB_H_

#include <stdint.h>

#include "utils.h"

enum InterfaceID {
  ITF_KEYBOARD = 0,
  ITF_MOUSE,
  ITF_TOTAL,
};

bool IsBootProtocol(uint8_t interface);

status USBInit();

status StartUSBTask();

#endif /* USB_H_ */