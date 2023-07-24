#include "layout_helper.h"

#define CONFIG_NUM_PHY_ROWS 1
#define CONFIG_NUM_PHY_COLS 1

static constexpr GPIO kGPIOMatrix[CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  {G(1, 2)},
};

static constexpr Keycode kKeyCodes[][CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
  [0]={
    {K(K_1)},
  },
};

// clang-format on

// Compile time validation and conversion for the key matrix. Must include this.
#include "layout_internal.inc"

// Only register the key scanner to save binary size

static Status register1 = RegisterKeyscan(/*tag=*/0);
static Status register2 = RegisterUSBKeyboardOutput(/*tag=*/1);
