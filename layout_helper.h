#ifndef LAYOUT_HELPER_H_
#define LAYOUT_HELPER_H_

#include <memory>

#include "class/hid/hid.h"
#include "config.h"
#include "config_modifier.h"
#include "joystick.h"
#include "keyscan.h"
#include "layout.h"
#include "rotary_encoder.h"
#include "ssd1306.h"
#include "temperature.h"
#include "usb.h"
#include "utils.h"
#include "ws2812.h"

#define K(KEYCODE) \
  { .keycode = (KEYCODE), .is_custom = false, .custom_info = 0 }

#define CK(KEYCODE) \
  { .keycode = (KEYCODE), .is_custom = true, .custom_info = 0 }

#define ______ \
  { .keycode = (HID_KEY_NONE), .is_custom = false, .custom_info = 0 }

// Temporarily activate the layer. Deactive once released.
#define MO(LAYER)                                 \
  {                                               \
    .keycode = (LAYER_SWITCH), .is_custom = true, \
    .custom_info = ((LAYER)&0x3f)                 \
  }

// Toggle layer activation.
#define TG(LAYER)                                 \
  {                                               \
    .keycode = (LAYER_SWITCH), .is_custom = true, \
    .custom_info = (((LAYER)&0x3f) | 0x40)        \
  }

#define G(ROW, COL) \
  { .row = (ROW), .col = (COL) }

#define CONFIG CK(ENTER_CONFIG)

// clang-format off

// Alias with shorter names. Each name should be no more than 7 characters long.

#define K_NONE       HID_KEY_NONE
#define K_A          HID_KEY_A
#define K_B          HID_KEY_B
#define K_C          HID_KEY_C
#define K_D          HID_KEY_D
#define K_E          HID_KEY_E
#define K_F          HID_KEY_F
#define K_G          HID_KEY_G
#define K_H          HID_KEY_H
#define K_I          HID_KEY_I
#define K_J          HID_KEY_J
#define K_K          HID_KEY_K
#define K_L          HID_KEY_L
#define K_M          HID_KEY_M
#define K_N          HID_KEY_N
#define K_O          HID_KEY_O
#define K_P          HID_KEY_P
#define K_Q          HID_KEY_Q
#define K_R          HID_KEY_R
#define K_S          HID_KEY_S
#define K_T          HID_KEY_T
#define K_U          HID_KEY_U
#define K_V          HID_KEY_V
#define K_W          HID_KEY_W
#define K_X          HID_KEY_X
#define K_Y          HID_KEY_Y
#define K_Z          HID_KEY_Z
#define K_1          HID_KEY_1
#define K_2          HID_KEY_2
#define K_3          HID_KEY_3
#define K_4          HID_KEY_4
#define K_5          HID_KEY_5
#define K_6          HID_KEY_6
#define K_7          HID_KEY_7
#define K_8          HID_KEY_8
#define K_9          HID_KEY_9
#define K_0          HID_KEY_0
#define K_ENTER      HID_KEY_ENTER
#define K_ESC        HID_KEY_ESCAPE
#define K_BACKS      HID_KEY_BACKSPACE
#define K_TAB        HID_KEY_TAB
#define K_SPACE      HID_KEY_SPACE
#define K_MINUS      HID_KEY_MINUS
#define K_EQUAL      HID_KEY_EQUAL
#define K_BRKTL      HID_KEY_BRACKET_LEFT
#define K_BRKTR      HID_KEY_BRACKET_RIGHT
#define K_BKSL       HID_KEY_BACKSLASH
#define K_EU_1       HID_KEY_EUROPE_1
#define K_SEMIC      HID_KEY_SEMICOLON
#define K_APST       HID_KEY_APOSTROPHE
#define K_GRAVE      HID_KEY_GRAVE
#define K_COMMA      HID_KEY_COMMA
#define K_PERID      HID_KEY_PERIOD
#define K_SLASH      HID_KEY_SLASH
#define K_CAPS       HID_KEY_CAPS_LOCK
#define K_F1         HID_KEY_F1
#define K_F2         HID_KEY_F2
#define K_F3         HID_KEY_F3
#define K_F4         HID_KEY_F4
#define K_F5         HID_KEY_F5
#define K_F6         HID_KEY_F6
#define K_F7         HID_KEY_F7
#define K_F8         HID_KEY_F8
#define K_F9         HID_KEY_F9
#define K_F10        HID_KEY_F10
#define K_F11        HID_KEY_F11
#define K_F12        HID_KEY_F12
#define K_PRTSC      HID_KEY_PRINT_SCREEN
#define K_SCRLK      HID_KEY_SCROLL_LOCK
#define K_PAUSE      HID_KEY_PAUSE
#define K_INS        HID_KEY_INSERT
#define K_HOME       HID_KEY_HOME
#define K_PAGEU      HID_KEY_PAGE_UP
#define K_DEL        HID_KEY_DELETE
#define K_END        HID_KEY_END
#define K_PAGED      HID_KEY_PAGE_DOWN
#define K_ARR_R      HID_KEY_ARROW_RIGHT
#define K_ARR_L      HID_KEY_ARROW_LEFT
#define K_ARR_D      HID_KEY_ARROW_DOWN
#define K_ARR_U      HID_KEY_ARROW_UP
#define K_NUM_L      HID_KEY_NUM_LOCK
#define K_EU_2       HID_KEY_EUROPE_2
#define K_APP        HID_KEY_APPLICATION
#define K_POWER      HID_KEY_POWER
#define K_F13        HID_KEY_F13
#define K_F14        HID_KEY_F14
#define K_F15        HID_KEY_F15
#define K_F16        HID_KEY_F16
#define K_F17        HID_KEY_F17
#define K_F18        HID_KEY_F18
#define K_F19        HID_KEY_F19
#define K_F20        HID_KEY_F20
#define K_F21        HID_KEY_F21
#define K_F22        HID_KEY_F22
#define K_F23        HID_KEY_F23
#define K_F24        HID_KEY_F24
#define K_EXE        HID_KEY_EXECUTE
#define K_HELP       HID_KEY_HELP
#define K_MENU       HID_KEY_MENU
#define K_SEL        HID_KEY_SELECT
#define K_STOP       HID_KEY_STOP
#define K_AGAIN      HID_KEY_AGAIN
#define K_UNDO       HID_KEY_UNDO
#define K_CUT        HID_KEY_CUT
#define K_COPY       HID_KEY_COPY
#define K_PASTE      HID_KEY_PASTE
#define K_FIND       HID_KEY_FIND
#define K_MUTE       HID_KEY_MUTE
#define K_VOL_U      HID_KEY_VOLUME_UP
#define K_VOL_D      HID_KEY_VOLUME_DOWN
#define K_CANCL      HID_KEY_CANCEL
#define K_CLEAR      HID_KEY_CLEAR
#define K_PRIOR      HID_KEY_PRIOR
#define K_RE         HID_KEY_RETURN
#define K_SEP        HID_KEY_SEPARATOR
#define K_OUT        HID_KEY_OUT
#define K_OPER       HID_KEY_OPER
#define K_CTR_L      HID_KEY_CONTROL_LEFT
#define K_SFT_L      HID_KEY_SHIFT_LEFT
#define K_ALT_L      HID_KEY_ALT_LEFT
#define K_GUI_L      HID_KEY_GUI_LEFT
#define K_CTR_R      HID_KEY_CONTROL_RIGHT
#define K_SFT_R      HID_KEY_SHIFT_RIGHT
#define K_ALT_R      HID_KEY_ALT_RIGHT
#define K_GUI_R      HID_KEY_GUI_RIGHT

// clang-format on

#endif /* LAYOUT_HELPER_H_ */