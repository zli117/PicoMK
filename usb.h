#ifndef USB_H_
#define USB_H_

#include <stdint.h>

#include "utils.h"
#include "config.h"

enum InterfaceID {
  ITF_KEYBOARD = 0,
  ITF_MOUSE,

#if CONFIG_DEBUG_ENABLE_USB_SERIAL
  ITF_CDC_CTRL,
  ITF_CDC_DATA,
#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

  ITF_TOTAL,
};

bool IsBootProtocol(uint8_t interface);

status USBInit();

status StartUSBTask();

#endif /* USB_H_ */