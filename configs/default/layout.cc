#include "layout_helper.h"

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

#define CONFIG_NUM_PHY_ROWS 6
#define CONFIG_NUM_PHY_COLS 15

static constexpr uint8_t kRowGPIO[] = {R0, R1, R2, R3, R4};
static constexpr uint8_t kColGPIO[] = {C0, C1, C2, C3,  C4,  C5,  C6,
                                       C7, C8, C9, C10, C11, C12, C13};
static constexpr bool kDiodeColToRow = true;

// clang-format off

// Keyboard switch physical GPIO connection setup.
static constexpr GPIO kGPIOMatrix[CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  {G(R0, C0),  G(R0, C1),  G(R0, C2),  G(R0, C3),  G(R0, C4),  G(R0, C5),  G(R0, C6),  G(R0, C7),  G(R0, C8),  G(R0, C9),  G(R0, C10),  G(R0, C11),  G(R0, C12),  G(R0, C13),  G(R1, C13)},
  {G(R1, C0),  G(R1, C1),  G(R1, C2),  G(R1, C3),  G(R1, C4),  G(R1, C5),  G(R1, C6),  G(R1, C7),  G(R1, C8),  G(R1, C9),  G(R1, C10),  G(R1, C11),  G(R1, C12),  G(R2, C13),  G(R3, C13)},
  {G(R2, C0),  G(R2, C1),  G(R2, C2),  G(R2, C3),  G(R2, C4),  G(R2, C5),  G(R2, C6),  G(R2, C7),  G(R2, C8),  G(R2, C9),  G(R2, C10),  G(R2, C11),  G(R2, C12),  G(R4, C13)},
  {G(R3, C0),  G(R3, C1),  G(R3, C2),  G(R3, C3),  G(R3, C4),  G(R3, C5),  G(R3, C6),  G(R3, C7),  G(R3, C8),  G(R3, C9),  G(R3, C10),  G(R3, C11),  G(R3, C12)},
  {G(R4, C0),  G(R4, C1),  G(R4, C2),  G(R4, C5),  G(R4, C7),  G(R4, C8),  G(R4, C9),  G(R4, C10), G(R4, C11), G(R4, C12)},
  {G(R4, C3),  G(R4, C4),  G(R4, C6)}
};

static constexpr Keycode kKeyCodes[][CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  [0]={
    {K(K_MUTE),  K(K_GRAVE),  K(K_1),     K(K_2),     K(K_3),     K(K_4),     K(K_5),     K(K_6),     K(K_7),     K(K_8),     K(K_9),     K(K_0),     K(K_MINUS), K(K_EQUAL), K(K_BACKS)},
    {K(K_ESC),   K(K_TAB),    K(K_Q),     K(K_W),     K(K_E),     K(K_R),     K(K_T),     K(K_Y),     K(K_U),     K(K_I),     K(K_O),     K(K_P),     K(K_BRKTL), K(K_BRKTR), K(K_BKSL)},
    {K(K_DEL),   K(K_CTR_L),  K(K_A),     K(K_S),     K(K_D),     K(K_F),     K(K_G),     K(K_H),     K(K_J),     K(K_K),     K(K_L),     K(K_SEMIC), K(K_APST),  K(K_ENTER)},
    {K(K_INS),   K(K_SFT_L),  K(K_Z),     K(K_X),     K(K_C),     K(K_V),     K(K_B),     K(K_N),     K(K_M),     K(K_COMMA), K(K_PERID), K(K_SLASH), K(K_SFT_R)},
    {______,     K(K_GUI_L),  K(K_ALT_L), K(K_SPACE), MO(1),      MO(2),      K(K_ARR_L), K(K_ARR_D), K(K_ARR_U), K(K_ARR_R)},
    {CK(MSE_L),  CK(MSE_M),   CK(MSE_R)}
  },
  [1]={
    {K(K_MUTE),  K(K_GRAVE),  K(K_F1),    K(K_F2),    K(K_F3),    K(K_F4),    K(K_F5),    K(K_F6),    K(K_F7),    K(K_F8),    K(K_F9),    K(K_F10),   K(K_F11),   K(K_F12),   K(K_BACKS)},
    {K(K_ESC),   K(K_TAB),    K(K_Q),     K(K_W),     K(K_E),     K(K_R),     K(K_T),     K(K_Y),     K(K_U),     K(K_I),     K(K_O),     K(K_P),     K(K_BRKTL), K(K_BRKTR), K(K_BKSL)},
    {K(K_DEL),   K(K_CTR_L),  K(K_A),     K(K_S),     K(K_D),     K(K_F),     K(K_G),     K(K_H),     K(K_J),     K(K_K),     K(K_L),     K(K_SEMIC), K(K_APST),  K(K_ENTER)},
    {K(K_INS),   K(K_SFT_L),  K(K_Z),     K(K_X),     K(K_C),     K(K_V),     K(K_B),     K(K_N),     K(K_M),     K(K_COMMA), K(K_PERID), K(K_SLASH), K(K_SFT_R)},
    {CONFIG,     TG(2),       K(K_ALT_L), K(K_SPACE), ______,     ______,     K(K_ARR_L), K(K_ARR_D), K(K_ARR_U), K(K_ARR_R)},
    {CK(MSE_L),  CK(MSE_M),   CK(MSE_R)}
  },
  [2]={
    {CK(CONFIG_SEL)},
    {CK(BOOTSEL)},
  }
};

// clang-format on

// Compile time validation and conversion for the key matrix
#include "layout_internal.inc"

// Register everything

enum {
  JOYSTICK = 0,
  KEYSCAN,
  ENCODER,
  SSD1306_1,
  SSD1306_2,
  USB_KEYBOARD,
  USB_MOUSE,
};

static Status registered_config_modifier =
    DeviceRegistry::RegisterConfigModifier([](ConfigObject* global_config) {
      return std::shared_ptr<ConfigModifiersImpl>(
          new ConfigModifiersImpl(global_config, SSD1306_2));
    });

static Status registered_joystick =
    DeviceRegistry::RegisterInputDevice(JOYSTICK, []() {
      return std::make_shared<JoystickInputDeivce>(
          /*x_adc_pin=*/28, /*y_adc_pin=*/27,
          /*buffer_size=*/3, /*flip_x_dir=*/false,
          /*flip_y_dir=*/false, CONFIG_SCAN_TICKS);
    });

static Status registered_keyscan = DeviceRegistry::RegisterInputDevice(
    KEYSCAN, []() { return std::shared_ptr<KeyScan>(new KeyScan()); });

static Status registered_encoder =
    DeviceRegistry::RegisterInputDevice(ENCODER, []() {
      return std::make_shared<RotaryEncoder>(/*pin_a=*/19, /*pin_b=*/22,
                                             /*resolution=*/2);
    });

static std::shared_ptr<SSD1306Display> GetSSD1306Display() {
  static std::shared_ptr<SSD1306Display> singleton;
  if (singleton == NULL) {
    singleton = std::make_shared<SSD1306Display>(
        i2c0, /*sda_pin=*/20, /*scl_pin=*/21, /*i2c_addr=*/0x3C,
        SSD1306Display::R_64, /*flip=*/true);
  }
  return singleton;
}
static Status registered_ssd1306_1 =
    DeviceRegistry::RegisterKeyboardOutputDevice(SSD1306_1, true,
                                                 GetSSD1306Display);
static Status registered_ssd1306_2 = DeviceRegistry::RegisterScreenOutputDevice(
    SSD1306_2, true, GetSSD1306Display);

static Status registered_usb_keyboard_out =
    DeviceRegistry::RegisterKeyboardOutputDevice(
        USB_KEYBOARD, false, []() -> std::shared_ptr<USBKeyboardOutput> {
          return USBKeyboardOutput::GetUSBKeyboardOutput();
        });
static Status registered_usb_mouse_out =
    DeviceRegistry::RegisterMouseOutputDevice(
        USB_MOUSE, false, []() -> std::shared_ptr<USBMouseOutput> {
          return USBMouseOutput::GetUSBMouseOutput();
        });
