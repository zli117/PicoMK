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

#define ALT_LY 4

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

// Compile time validation and conversion for the key matrix
#include "layout_internal.inc"

// Implement a custom SSD1306 device that can also display status LEDs

class CustomSSD1306 : virtual public SSD1306Display,
                      virtual public LEDOutputDevice {
 public:
  CustomSSD1306(i2c_inst_t* i2c, uint8_t sda_pin, uint8_t scl_pin,
                uint8_t i2c_addr, NumRows num_rows, bool flip)
      : SSD1306Display(i2c, sda_pin, scl_pin, i2c_addr, num_rows, flip) {}

  void IncreaseBrightness() override {}
  void DecreaseBrightness() override {}
  void IncreaseAnimationSpeed() override {}
  void DecreaseAnimationSpeed() override {}
  size_t NumPixels() const override { return 0; }
  void SetFixedColor(uint8_t w, uint8_t r, uint8_t g, uint8_t b) override {}
  void SetPixel(size_t idx, uint8_t w, uint8_t r, uint8_t g,
                uint8_t b) override {}

  void SetLedStatus(LEDIndicators indicators) override {
    if (config_mode_) {
      return;
    }

    // Note: this implementation is still inefficient. It can take a significant
    // amount of time to render everything everytime.
    DrawRect(19, 0, GetNumRows() - 1, GetNumCols() - 1, /*fill=*/false, ADD);
    uint8_t offset = 21;
    DrawLEDIndicator("NumLock", offset, indicators.num_lock);
    if (GetNumRows() == 64) {
      DrawLEDIndicator("CapsLock", offset += 8, indicators.caps_lock);
      DrawLEDIndicator("ScrollLock", offset += 8, indicators.scroll_lock);
      DrawLEDIndicator("Compose", offset += 8, indicators.compose);
      DrawLEDIndicator("Kana", offset += 8, indicators.kana);
    }
  }

 private:
  void DrawLEDIndicator(const std::string& name, uint8_t row, bool on) {
    DrawText(row, 2, name, F8X8, ADD);
    if (!on) {
      // Clean up the rectangle
      DrawRect(row + 1, GetNumCols() - 9, row + 6, GetNumCols() - 4,
               /*fill=*/true, SUBTRACT);
    }
    DrawRect(row + 1, GetNumCols() - 9, row + 6, GetNumCols() - 4, on, ADD);
  }
};

Status RegisterCustomSSD1306(uint8_t tag, i2c_inst_t* i2c, uint8_t sda_pin,
                             uint8_t scl_pin, uint8_t i2c_addr,
                             SSD1306Display::NumRows num_rows, bool flip) {
  std::shared_ptr<CustomSSD1306> instance = std::make_shared<CustomSSD1306>(
      i2c, sda_pin, scl_pin, i2c_addr, num_rows, flip);
  if (DeviceRegistry::RegisterKeyboardOutputDevice(
          tag, true, [=]() { return instance; }) != OK ||
      DeviceRegistry::RegisterScreenOutputDevice(
          tag, true, [=]() { return instance; }) != OK ||
      DeviceRegistry::RegisterLEDOutputDevice(
          tag, true, [=]() { return instance; }) != OK) {
    return ERROR;
  }
  return OK;
}

// Register all the devices

enum {
  JOYSTICK = 0,
  KEYSCAN,
  ENCODER,
  SSD1306,
  USB_KEYBOARD,
  USB_MOUSE,
  USB_INPUT,
  LED,
};

static Status register1 = RegisterConfigModifier(SSD1306);
static Status register2 =
    RegisterJoystick(JOYSTICK, 28, 27, 5, false, false, true, ALT_LY);
static Status register3 = RegisterKeyscan(KEYSCAN);
static Status register4 = RegisterEncoder(ENCODER, 19, 22, 2);
static Status register5 = RegisterCustomSSD1306(SSD1306, i2c0, 20, 21, 0x3c,
                                                SSD1306Display::R_64, true);
static Status register6 = RegisterUSBKeyboardOutput(USB_KEYBOARD);
static Status register7 = RegisterUSBMouseOutput(USB_MOUSE);
static Status register8 = RegisterUSBInput(USB_INPUT);
static Status register9 = RegisterWS2812(LED, 26, 17);
