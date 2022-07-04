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

constexpr size_t kNumInGPIO = ArraySize(kInGPIO);
constexpr size_t kNumOutGPIO = ArraySize(kOutGPIO);
constexpr size_t kNumLayers = ArraySize(kKeyCodes);

using KeyMatrix =
    std::array<std::array<std::array<Keycode, kNumOutGPIO>, kNumInGPIO>,
               kNumLayers>;

template <size_t L, size_t R, size_t C, size_t IN, size_t OUT>
constexpr KeyMatrix PostProcess(const GPIO (&gpio)[R][C],
                                const Keycode (&kc)[L][R][C],
                                const uint8_t (&gpio_in)[IN],
                                const uint8_t (&gpio_out)[OUT]) {
  if (OUT > IN) {
    for (size_t i = 0; i < OUT; ++i) {
      if (Find(gpio_in, gpio_out[i]) != -1) {
        throw std::invalid_argument(
            "Currently doesn't support GPIO pin to be both IN and OUT");
      }
    }
  } else {
    for (size_t i = 0; i < IN; ++i) {
      if (Find(gpio_out, gpio_in[i]) != -1) {
        throw std::invalid_argument(
            "Currently doesn't support GPIO pin to be both IN and OUT");
      }
    }
  }
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
        int in_gpio_idx = Find(gpio_in, gpio[r][c].in);
        int out_gpio_idx = Find(gpio_out, gpio[r][c].out);
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

constexpr KeyMatrix kKeyMatrix =
    PostProcess(kGPIOMatrix, kKeyCodes, kInGPIO, kOutGPIO);

}  // namespace

size_t GetKeyboardNumLayers() { return kNumLayers; }

size_t GetNumOutGPIOs() { return kNumOutGPIO; }

size_t GetNumInGPIOs() { return kNumInGPIO; }

uint8_t GetOutGPIO(size_t idx) { return kOutGPIO[idx]; }

uint8_t GetInGPIO(size_t idx) { return kInGPIO[idx]; }

Keycode GetKeycode(size_t layer, size_t in_gpio_idx, size_t out_gpio_idx) {
  return kKeyMatrix[layer][in_gpio_idx][out_gpio_idx];
}
