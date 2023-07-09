#ifndef SSD1306_H_
#define SSD1306_H_

#include <array>
#include <memory>

#include "FreeRTOS.h"
#include "base.h"
#include "configuration.h"
#include "hardware/i2c.h"
#include "pico-ssd1306/ssd1306.h"
#include "semphr.h"
#include "utils.h"

class SSD1306Display : virtual public ScreenMixinBase<SSD1306Display> {
 public:
  enum NumRows { R_32 = 32, R_64 = 64 };

  SSD1306Display(i2c_inst_t* i2c, uint8_t sda_pin, uint8_t scl_pin,
                 uint8_t i2c_addr, NumRows num_rows, bool flip);

  void OnUpdateConfig(const Config* config) override;
  std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig()
      override;

  void SetConfigMode(bool is_config_mode) override;
  void OutputTick() override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  size_t GetNumRows() const override { return num_rows_; }
  size_t GetNumCols() const override { return num_cols_; }
  bool IsConfigMode() const override { return config_mode_; }
  void SetPixel(size_t row, size_t col, Mode mode) override;
  void DrawLine(size_t start_row, size_t start_col, size_t end_row,
                size_t end_col, Mode mode) override;
  void DrawRect(size_t start_row, size_t start_col, size_t end_row,
                size_t end_col, bool fill, Mode mode) override;
  void DrawText(size_t row, size_t col, const std::string& text, Font font,
                Mode mode) override;
  void DrawText(size_t row, size_t col, const std::string& text,
                const CustomFont& font, Mode mode) override;
  void DrawBuffer(const std::vector<uint8_t>& buffer, size_t start_row,
                  size_t start_col, size_t end_row, size_t end_col) override {}
  void Clear() override {
    display_->clear();
    buffer_changed_ = true;
  }

  // Don't need to do anything special since we already have the sleep
  void SuspendEvent(bool is_suspend) override {}

  static Status Register(uint8_t key, bool slow,
                         const std::shared_ptr<SSD1306Display> ptr);

 protected:
  void CMD(uint8_t cmd);

  i2c_inst_t* const i2c_;
  const uint8_t sda_pin_;
  const uint8_t scl_pin_;
  const uint8_t i2c_addr_;
  const size_t num_rows_;
  const size_t num_cols_;
  uint32_t sleep_s_;

  std::unique_ptr<pico_ssd1306::SSD1306> display_;

  std::array<uint8_t, FRAMEBUFFER_SIZE> buffer_;
  std::array<uint8_t, FRAMEBUFFER_SIZE> out_buffer_;

  bool buffer_changed_;
  bool send_buffer_;
  uint32_t last_active_s_;
  bool sleep_;
  bool config_mode_;

  SemaphoreHandle_t semaphore_;
};

#endif /* SSD1306_H_ */
