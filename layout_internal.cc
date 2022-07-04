namespace {

template <typename T, size_t N>
inline constexpr size_t ArraySize(T (&)[N]) {
  return N;
}

template <size_t N>
inline constexpr int Find(const uint8_t (&arr)[N], uint8_t target) {
  for (size_t i = 0; i < N; ++i) {
    if (arr[i] == target) {
      return i;
    }
  }
  return -1;
}

inline constexpr size_t kNumInGPIO = ArraySize(kInGPIO);
inline constexpr size_t kNumOutGPIO = ArraySize(kOutGPIO);
inline constexpr size_t kNumLayers = ArraySize(kKeyCodes);

using KeyMatrix =
    std::array<std::array<std::array<Keycode, kNumOutGPIO>, kNumInGPIO>,
               kNumLayers>;

template <size_t L, size_t R, size_t C>
inline constexpr KeyMatrix PostProcess(const GPIO (&gpio)[R][C],
                                       const Keycode (&kc)[L][R][C]) {
  KeyMatrix output = {0};
  for (size_t l = 0; l < L; ++l) {
    for (size_t r = 0; r < R; ++r) {
      for (size_t c = 0; c < C; ++c) {
        if (kc[l][r][c].keycode == HID_KEY_NONE) {
          continue;
        }

        if (gpio[r][c].in == gpio[r][c].out) {
          throw std::invalid_argument("In and out GPIOs have to be different");
        }
        int in_gpio_idx = Find(kInGPIO, gpio[r][c].in);
        int out_gpio_idx = Find(kOutGPIO, gpio[r][c].out);
        if (in_gpio_idx < 0) {
          throw std::invalid_argument("In GPIO not found");
        }
        if (out_gpio_idx < 0) {
          throw std::invalid_argument("Out GPIO not found");
        }
        if (output[l][in_gpio_idx][out_gpio_idx].keycode != HID_KEY_NONE) {
          throw std::invalid_argument(
              "Trying to assign two keycodes to one location");
        }

        output[l][in_gpio_idx][out_gpio_idx] = kc[l][r][c];
      }
    }
  }
  return output;
}

constexpr KeyMatrix kKeyMatrix = PostProcess(kGPIOMatrix, kKeyCodes);

}  // namespace

size_t GetKeyboardNumLayers() { return kNumLayers; }

size_t GetNumOutGPIOs() { return kNumOutGPIO; }

size_t GetNumInGPIOs() { return kNumInGPIO; }

uint8_t GetOutGPIO(size_t idx) { return kOutGPIO[idx]; }

uint8_t GetInGPIO(size_t idx) { return kInGPIO[idx]; }

Keycode GetKeycode(size_t layer, size_t in_gpio_idx, size_t out_gpio_idx) {
  return kKeyMatrix[layer][in_gpio_idx][out_gpio_idx];
}
