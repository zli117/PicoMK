#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include <linux/input.h>

#include "../ibp_lib.h"

int CreateKeyboardDevice(int (*open)(void), void (*close)(void));
int DestroyKeyboardDevice(void);
int OnNewKeycodes(IBPKeyCodes* keys);
int OnNewConsumerKeycodes(IBPConsumer* keys);

#endif /*KEYBOARD_H_*/
