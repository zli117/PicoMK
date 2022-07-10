#ifndef SSD1306_H_
#define SSD1306_H_

#include <array>
#include <memory>

#include "FreeRTOS.h"
#include "base.h"
#include "hardware/i2c.h"
#include "pico-ssd1306/ssd1306.h"
#include "semphr.h"

class SSD1306Display : public KeyboardOutputDevice, public ScreenOutputDevice {
 public:
  enum NumRows { R_32 = 32, R_64 = 64 };

  SSD1306Display(i2c_inst_t* i2c, uint8_t sda_pin, uint8_t scl_pin,
                 uint8_t i2c_addr, NumRows num_rows);

  void Tick() override;
  void OnUpdateConfig() override {}
  void SetConfigMode(bool is_config_mode) override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  void SendKeycode(uint8_t keycode) override {}
  void SendKeycode(const std::vector<uint8_t>& keycode) override {}
  void ActiveLayers(const std::vector<bool>& layers) override;

  size_t GetNumRows() const override { return num_rows_; }
  size_t GetNumCols() const override { return num_cols_; }
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

 protected:
  void CMD(uint8_t cmd);

  i2c_inst_t* const i2c_;
  const uint8_t sda_pin_;
  const uint8_t scl_pin_;
  const uint8_t i2c_addr_;
  const size_t num_rows_;
  const size_t num_cols_;

  // pico_ssd1306::SSD1306 currently has memory leak issue. See
  // https://github.com/Harbys/pico-ssd1306/issues/8
  std::unique_ptr<pico_ssd1306::SSD1306> display_;

  // The first byte is startline command (0x40)
  std::array<std::array<uint8_t, 1024 + 1>, 2> double_buffer_;
  uint8_t buffer_idx_;
  bool is_config_mode_;

  SemaphoreHandle_t semaphore_;
};

#endif /* SSD1306_H_ */
