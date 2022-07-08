#ifndef PERIPHERAL_H_
#define PERIPHERAL_H_

#include "layout.h"
#include "utils.h"

status PeripheralInit();

status StartPeripheralTask();

// Peripheral API. Threadsafe and always available regardless of the config.
// However, they might be nop if components are disabled.

enum JoystickMode {
  MOUSE = 0,
  XY_SCROLL,
};

status SetMouseButtonState(BuiltInCustomKeyCode mouse_key, bool is_pressed);
status SetJoystickMode(JoystickMode mode);

#endif /* PERIPHERAL_H_ */