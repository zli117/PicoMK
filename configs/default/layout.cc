// layout_helper.h defines the helper macros as well as short alias to each
// keycode. Please include it at the top of the layout.cc file.
#include "layout_helper.h"

// Alias for the key matrix GPIO pins. This is for better readability and is
// optional.

#define C0 0
#define C1 1
#define C2 2
#define C3 3
#define C4 4
#define C5 5
#define C6 6
#define C7 7
#define C8 8
#define C9 9
#define C10 10
#define C11 11
#define C12 13
#define C13 12
#define R0 14
#define R1 15
#define R2 18
#define R3 17
#define R4 16

// These two are also optional

#define CONFIG_NUM_PHY_ROWS 6
#define CONFIG_NUM_PHY_COLS 15

// This is a special layer that rotary encoder might have different behaviors.
// Also optional.

#define ALT_LY 4

// For a layout.cc file, the followings are required: kGPIOMatrix, and
// kKeyCodes. They need to have exactly the same shape and type. They also need
// to be constexpr. For array types you don't need to specify the size for all
// the dimenions as long as compiler is happy. See docs/layout_cc.md for an
// example key matrix setup.

// clang-format off

// Keyboard switch physical GPIO connection setup. This is a map from the
// physical layout of the keys to their switch matrix. The reason for having 
// this mapping is that often times the physical layout of the switches does not
// match up with their wiring matrix. For each switch, it specifies the 
// direction of scanning.
static constexpr GPIO kGPIOMatrix[CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  {G(C0, R0),  G(C1, R0),  G(C2, R0),  G(C3, R0),  G(C4, R0),  G(C5, R0),  G(C6, R0),  G(C7, R0),  G(C8, R0),  G(C9, R0),  G(C10, R0),  G(C11, R0),  G(C12, R0),  G(C13, R0),  G(C13, R1)},
  {G(C0, R1),  G(C1, R1),  G(C2, R1),  G(C3, R1),  G(C4, R1),  G(C5, R1),  G(C6, R1),  G(C7, R1),  G(C8, R1),  G(C9, R1),  G(C10, R1),  G(C11, R1),  G(C12, R1),  G(C13, R2),  G(C13, R3)},
  {G(C0, R2),  G(C1, R2),  G(C2, R2),  G(C3, R2),  G(C4, R2),  G(C5, R2),  G(C6, R2),  G(C7, R2),  G(C8, R2),  G(C9, R2),  G(C10, R2),  G(C11, R2),  G(C12, R2),  G(C13, R4)},
  {G(C0, R3),  G(C1, R3),  G(C2, R3),  G(C3, R3),  G(C4, R3),  G(C5, R3),  G(C6, R3),  G(C7, R3),  G(C8, R3),  G(C9, R3),  G(C10, R3),  G(C11, R3),  G(C12, R3)},
  {G(C0, R4),  G(C1, R4),  G(C2, R4),  G(C5, R4),  G(C7, R4),  G(C8, R4),  G(C9, R4),  G(C10, R4), G(C11, R4), G(C12, R4)},
  {G(C3, R4),  G(C4, R4),  G(C6, R4)}
};

static constexpr Keycode kKeyCodes[][CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  [0]={
    {K(K_MUTE),  K(K_GRAVE),  K(K_1),     K(K_2),     K(K_3),     K(K_4),     K(K_5),     K(K_6),     K(K_7),     K(K_8),     K(K_9),     K(K_0),     K(K_MINUS), K(K_EQUAL), K(K_BACKS)},
    {K(K_ESC),   K(K_TAB),    K(K_Q),     K(K_W),     K(K_E),     K(K_R),     K(K_T),     K(K_Y),     K(K_U),     K(K_I),     K(K_O),     K(K_P),     K(K_BRKTL), K(K_BRKTR), K(K_BKSL)},
    {K(K_DEL),   K(K_CTR_L),  K(K_A),     K(K_S),     K(K_D),     K(K_F),     K(K_G),     K(K_H),     K(K_J),     K(K_K),     K(K_L),     K(K_SEMIC), K(K_APST),  K(K_ENTER)},
    {K(K_INS),   K(K_SFT_L),  K(K_Z),     K(K_X),     K(K_C),     K(K_V),     K(K_B),     K(K_N),     K(K_M),     K(K_COMMA), K(K_PERID), K(K_SLASH), K(K_SFT_R)},
    {MO(ALT_LY), K(K_GUI_L),  K(K_ALT_L), K(K_SPACE), MO(1),      MO(2),      K(K_ARR_L), K(K_ARR_D), K(K_ARR_U), K(K_ARR_R)},
    {CK(MSE_L),  CK(MSE_M),   CK(MSE_R)}
  },
  [1]={
    {K(K_MUTE),  K(K_GRAVE),  K(K_F1),    K(K_F2),    K(K_F3),    K(K_F4),    K(K_F5),    K(K_F6),    K(K_F7),    K(K_F8),    K(K_F9),    K(K_F10),   K(K_F11),   K(K_F12),   K(K_BACKS)},
    {K(K_ESC),   K(K_TAB),    K(K_Q),     K(K_W),     K(K_E),     K(K_R),     K(K_T),     K(K_Y),     K(K_U),     K(K_I),     K(K_O),     K(K_P),     K(K_BRKTL), K(K_BRKTR), K(K_BKSL)},
    {K(K_DEL),   K(K_CAPS),   K(K_A),     K(K_S),     K(K_D),     K(K_F),     K(K_G),     K(K_H),     K(K_J),     K(K_K),     K(K_L),     K(K_SEMIC), K(K_APST),  K(K_ENTER)},
    {MO(3),      K(K_SFT_L),  K(K_Z),     K(K_X),     K(K_C),     K(K_V),     K(K_B),     K(K_N),     K(K_M),     K(K_COMMA), K(K_PERID), K(K_SLASH), K(K_SFT_R)},
    {CONFIG,     TG(2),       K(K_ALT_L), K(K_SPACE), ______,     ______,     K(K_ARR_L), K(K_ARR_D), K(K_ARR_U), K(K_ARR_R)},
    {CK(MSE_L),  CK(MSE_M),   CK(MSE_R)}
  },
  [2]={
    {CK(CONFIG_SEL)},
    {______},
    {CK(REBOOT)},
  },
  [3]={
    {______},
    {CK(BOOTSEL)},
  },
  [ALT_LY]={},
};

// clang-format on

// Compile time validation and conversion for the key matrix. Must include this.
#include "layout_internal.inc"

// Create the screen display. The content displayed on the screen is created
// with the help of mixins. See display_mixins.h for the builtin mixins. Each
// mixin displays a specific functionality at a specific region.

class Screen : public virtual SSD1306Display,
               public virtual ActiveLayersDisplayMixin<> {
 public:
  Screen() : SSD1306Display(i2c0, 20, 21, 0x3c, SSD1306Display::R_64, true) {}

  static Status RegisterScreen(uint8_t key) {
    std::shared_ptr<Screen> instance = std::make_shared<Screen>();
    if (ActiveLayersDisplayMixin<>::Register(key, /*slow=*/true, instance) !=
            OK ||
        SSD1306Display::Register(key, /*slow=*/true, instance) != OK) {
      return ERROR;
    }
    return OK;
  }
};

// Register all the devices

// Each device is registered with a unique tag.
enum {
  JOYSTICK = 0,
  KEYSCAN,
  ENCODER,
  SSD1306,
  USB_KEYBOARD,
  USB_MOUSE,
  USB_INPUT,
  TEMPERATURE,
  LED,
};

// The return values have to be retained and static for the registration code to
// execute at initialization time. See each device's documentation for what
// values are needed for the registration function.

static Status register1 = RegisterConfigModifier(SSD1306);
static Status register2 =
    RegisterJoystick(JOYSTICK, 28, 27, 5, false, false, true, ALT_LY);
static Status register3 = RegisterKeyscan(KEYSCAN);
static Status register4 = RegisterEncoder(ENCODER, 19, 22, 2);
static Status register5 = Screen::RegisterScreen(SSD1306);
static Status register6 = RegisterUSBKeyboardOutput(USB_KEYBOARD);
static Status register7 = RegisterUSBMouseOutput(USB_MOUSE);
static Status register8 = RegisterUSBInput(USB_INPUT);
static Status register9 = RegisterTemperatureInput(TEMPERATURE);
static Status register10 = RegisterWS2812(LED, 26, 17);
