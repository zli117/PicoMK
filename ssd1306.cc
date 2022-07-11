#include "ssd1306.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "pico-ssd1306/shapeRenderer/ShapeRenderer.h"
#include "pico-ssd1306/textRenderer/12x16_font.h"
#include "pico-ssd1306/textRenderer/16x32_font.h"
#include "pico-ssd1306/textRenderer/5x8_font.h"
#include "pico-ssd1306/textRenderer/8x8_font.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

// Debug
#include "hardware/timer.h"

using pico_ssd1306::Size;
using pico_ssd1306::SSD1306;
using pico_ssd1306::WriteMode;

SSD1306Display::SSD1306Display(i2c_inst_t* i2c, uint8_t sda_pin,
                               uint8_t scl_pin, uint8_t i2c_addr,
                               NumRows num_rows, bool flip)
    : i2c_(i2c),
      sda_pin_(sda_pin),
      scl_pin_(scl_pin),
      i2c_addr_(i2c_addr),
      num_rows_(num_rows),
      num_cols_(128),
      buffer_idx_(0),
      is_config_mode_(false) {
  i2c_init(i2c_, 400 * 1000);
  gpio_set_function(sda_pin_, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin_, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin_);
  gpio_pull_up(scl_pin_);

  busy_wait_ms(250);

  display_ = std::make_unique<SSD1306>(
      i2c_, i2c_addr_, num_rows_ == 64 ? Size::W128xH64 : Size::W128xH32);
  if (flip) {
    display_->setOrientation(0);
  }

  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);

  busy_wait_ms(250);
}

void SSD1306Display::Tick() {
  const uint64_t start_time = time_us_64();

  // Manually send the buffer to avoid blocking other tasks

  CMD(pico_ssd1306::SSD1306_PAGEADDR);
  CMD(0x00);
  CMD(0x07);
  CMD(pico_ssd1306::SSD1306_COLUMNADDR);
  CMD(0x00);
  CMD(127);

  std::array<uint8_t, FRAMEBUFFER_SIZE + 1> local_copy;
  {
    LockSemaphore lock(semaphore_);
    std::copy(double_buffer_[buffer_idx_].begin(),
              double_buffer_[buffer_idx_].end(), local_copy.begin() + 1);
  }
  local_copy[0] = pico_ssd1306::SSD1306_STARTLINE;

  i2c_write_blocking(i2c_, i2c_addr_, local_copy.data(), local_copy.size(),
                     false);

  const uint64_t end_time = time_us_64();
}

void SSD1306Display::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  is_config_mode_ = is_config_mode;
}

void SSD1306Display::StartOfInputTick() {
  // No need to lock as Tick() does not modify reads buffer_idx_.
  const uint8_t buf_idx = (buffer_idx_ + 1) % 2;
  std::fill(double_buffer_[buf_idx].begin(), double_buffer_[buf_idx].end(), 0);
  display_->setBuffer(double_buffer_[buf_idx].data());
}

void SSD1306Display::FinalizeInputTickOutput() {
  LockSemaphore lock(semaphore_);
  buffer_idx_ = (buffer_idx_ + 1) % 2;
}

void SSD1306Display::ActiveLayers(const std::vector<bool>& layers) {
  bool is_config_mode;
  {
    LockSemaphore lock(semaphore_);
    is_config_mode = is_config_mode_;
  }
  const uint8_t buf_idx = (buffer_idx_ + 1) % 2;
  if (is_config_mode) {
    return;
  }
  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i]) {
      DrawRect(/*start_row=*/1, 1 + i * 8, /*end_row=*/7, 7 + i * 8,
               /*fill=*/true, ADD);
    }
  }
}

void SSD1306Display::SetPixel(size_t row, size_t col, Mode mode) {
  display_->setPixel(col, row, (WriteMode)mode);
}

void SSD1306Display::DrawLine(size_t start_row, size_t start_col,
                              size_t end_row, size_t end_col, Mode mode) {
  pico_ssd1306::drawLine(display_.get(), start_col, start_row, end_col, end_row,
                         (WriteMode)mode);
}

void SSD1306Display::DrawRect(size_t start_row, size_t start_col,
                              size_t end_row, size_t end_col, bool fill,
                              Mode mode) {
  if (fill) {
    pico_ssd1306::fillRect(display_.get(), start_col, start_row, end_col,
                           end_row, (WriteMode)mode);
  } else {
    pico_ssd1306::drawRect(display_.get(), start_col, start_row, end_col,
                           end_row, (WriteMode)mode);
  }
}

class BuiltinWrapper : public ScreenOutputDevice::CustomFont {
 public:
  BuiltinWrapper(const uint8_t* buffer) : buffer_(buffer) {}
  const uint8_t* GetFont() const override { return buffer_; }

 private:
  const uint8_t* const buffer_;
};

void SSD1306Display::DrawText(size_t row, size_t col, const std::string& text,
                              Font font, Mode mode) {
  switch (font) {
    case F5X8: {
      DrawText(row, col, text, BuiltinWrapper(font_5x8), mode);
      break;
    }
    case F8X8: {
      DrawText(row, col, text, BuiltinWrapper(font_8x8), mode);
      break;
    }
    case F12X16: {
      DrawText(row, col, text, BuiltinWrapper(font_12x16), mode);
      break;
    }
    case F16X32: {
      DrawText(row, col, text, BuiltinWrapper(font_16x32), mode);
      break;
    }
    default:
      break;
  }
}

void SSD1306Display::DrawText(size_t row, size_t col, const std::string& text,
                              const CustomFont& font, Mode mode) {
  const uint8_t* font_buf = font.GetFont();
  const uint8_t width = font_buf[0];
  size_t col_offset = 0;
  for (char c : text) {
    pico_ssd1306::drawChar(display_.get(), font_buf, c, row, col + col_offset,
                           (WriteMode)mode);
    col_offset += width;
  }
}

void SSD1306Display::CMD(uint8_t cmd) {
  uint8_t data[2] = {0x00, cmd};
  i2c_write_blocking(i2c_, i2c_addr_, data, 2, false);
}

static std::shared_ptr<SSD1306Display> singleton;

static std::shared_ptr<SSD1306Display> GetSSD1306Display(const Configuration*) {
  if (singleton == NULL) {
    singleton = std::make_shared<SSD1306Display>(i2c0, 20, 21, 0x3C,
                                                 SSD1306Display::R_64, true);
  }
  return singleton;
}

static Status usb_keyboard_out =
    DeviceRegistry::RegisterKeyboardOutputDevice(3, true, GetSSD1306Display);
static Status screen_out =
    DeviceRegistry::RegisterScreenOutputDevice(1, true, GetSSD1306Display);
