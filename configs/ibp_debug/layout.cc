// A very simple setup with only one key for developing the IBP protocol.
#include "layout_helper.h"

#define CONFIG_NUM_PHY_ROWS 1
#define CONFIG_NUM_PHY_COLS 1

static constexpr GPIO kGPIOMatrix[CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] = {
    {G(1, 2)},
};

static constexpr Keycode kKeyCodes[][CONFIG_NUM_PHY_ROWS][CONFIG_NUM_PHY_COLS] =
    {
        [0] =
            {
                {K(K_2)},
            },
};

// clang-format on

// Compile time validation and conversion for the key matrix. Must include this.
#include "layout_internal.inc"

class IBPSPIDeviceDebug : public IBPSPIDevice {
 public:
  static std::shared_ptr<IBPSPIDeviceDebug> GetIBPSPIDeviceDebug(
      IBPSPIArgs args) {
    static std::shared_ptr<IBPSPIDeviceDebug> instance_ = NULL;
    if (instance_ == NULL) {
      instance_ =
          std::shared_ptr<IBPSPIDeviceDebug>(new IBPSPIDeviceDebug(args));
    }
    return instance_;
  }

 private:
  IBPSPIDeviceDebug(IBPSPIArgs args) : IBPSPIDevice(args) {}
};

Status RegisterSPIOut(uint8_t tag, IBPSPIArgs args) {
  auto creator_fn = [=]() {
    return IBPSPIDeviceDebug::GetIBPSPIDeviceDebug(args);
  };
  if (IBPDriverRegistry::RegisterDriver(tag, creator_fn) == OK &&
      DeviceRegistry::RegisterKeyboardOutputDevice(tag, false, creator_fn) ==
          OK) {
    return OK;
  }
  return ERROR;
}

static Status register1 = RegisterKeyscan(/*tag=*/0);
static Status register2 = RegisterUSBKeyboardOutput(/*tag=*/1);
static Status register3 =
    RegisterSPIOut(/*tag=*/2, {.spi_port = spi0,
                               .rx_pin = PICO_DEFAULT_SPI_RX_PIN,
                               .tx_pin = PICO_DEFAULT_SPI_TX_PIN,
                               .cs_pin = PICO_DEFAULT_SPI_CSN_PIN,
                               .sck_pin = PICO_DEFAULT_SPI_SCK_PIN,
                               .baud_rate = 100000});
