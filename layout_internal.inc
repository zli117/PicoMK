namespace {

template <typename T, size_t N>
constexpr size_t ArraySize(T (&)[N]) {
  return N;
}

template <size_t N>
constexpr int Find(const uint8_t (&arr)[N], uint8_t target) {
  for (size_t i = 0; i < N; ++i) {
    if (arr[i] == target) {
      return i;
    }
  }
  return -1;
}

constexpr size_t kNumRowGPIO = ArraySize(kRowGPIO);
constexpr size_t kNumColGPIO = ArraySize(kColGPIO);
constexpr size_t kNumLayers = ArraySize(kKeyCodes);

using KeyMatrix =
    std::array<std::array<std::array<Keycode, kNumColGPIO>, kNumRowGPIO>,
               kNumLayers>;

// Force non-const as error
void failure(const char*) {}

template <size_t L, size_t R, size_t C, size_t GP_ROW, size_t GP_COL>
constexpr KeyMatrix PostProcess(const GPIO (&gpio)[R][C],
                                const Keycode (&kc)[L][R][C],
                                const uint8_t (&gpio_row)[GP_ROW],
                                const uint8_t (&gpio_col)[GP_COL]) {
  if (GP_COL > GP_ROW) {
    for (size_t i = 0; i < GP_COL; ++i) {
      if (Find(gpio_row, gpio_col[i]) != -1) {
        failure("Currently doesn't support GPIO pin to be both row and col");
      }
    }
  } else {
    for (size_t i = 0; i < GP_ROW; ++i) {
      if (Find(gpio_col, gpio_row[i]) != -1) {
        failure("Currently doesn't support GPIO pin to be both row and col");
      }
    }
  }
  KeyMatrix output = {0};
  for (size_t l = 0; l < L; ++l) {
    for (size_t r = 0; r < R; ++r) {
      for (size_t c = 0; c < C; ++c) {
        if (!kc[l][r][c].is_custom && kc[l][r][c].keycode == HID_KEY_NONE) {
          continue;
        }

        if (gpio[r][c].row == gpio[r][c].col) {
          failure("Row and col GPIOs have to be different");
        }
        int row_gpio_idx = Find(gpio_row, gpio[r][c].row);
        int col_gpio_idx = Find(gpio_col, gpio[r][c].col);
        if (row_gpio_idx < 0) {
          failure("Row GPIO not found");
        }
        if (col_gpio_idx < 0) {
          failure("Col GPIO not found");
        }
        if (output[l][row_gpio_idx][col_gpio_idx].keycode != HID_KEY_NONE) {
          failure("Trying to assign two keycodes to one location");
          // throw std::invalid_argument(
          //     );
        }

        output[l][row_gpio_idx][col_gpio_idx] = kc[l][r][c];
      }
    }
  }
  return output;
}

constexpr KeyMatrix kKeyMatrix =
    PostProcess(kGPIOMatrix, kKeyCodes, kRowGPIO, kColGPIO);

}  // namespace

size_t GetKeyboardNumLayers() { return kNumLayers; }

size_t GetNumSinkGPIOs() { return kDiodeColToRow ? kNumRowGPIO : kNumColGPIO; }
size_t GetNumSourceGPIOs() {
  return kDiodeColToRow ? kNumColGPIO : kNumRowGPIO;
}
uint8_t GetSinkGPIO(size_t idx) {
  return kDiodeColToRow ? kRowGPIO[idx] : kColGPIO[idx];
}
uint8_t GetSourceGPIO(size_t idx) {
  return kDiodeColToRow ? kColGPIO[idx] : kRowGPIO[idx];
}
Keycode GetKeycode(size_t layer, size_t sink_gpio_idx, size_t source_gpio_idx) {
  return kKeyMatrix[layer][kDiodeColToRow ? sink_gpio_idx : source_gpio_idx]
                   [kDiodeColToRow ? source_gpio_idx : sink_gpio_idx];
}
