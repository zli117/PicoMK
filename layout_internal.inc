namespace {

template <typename T, size_t N>
constexpr size_t ArraySize(T (&)[N]) {
  return N;
}

constexpr size_t kNumLayers = ArraySize(kKeyCodes);

template <size_t L>
struct Scan {
  GPIO gpio;
  std::array<Keycode, L> keycodes;
};

constexpr size_t kGPIONumMax = 40;  // Pico has at most 40 pins

template <size_t N>
using KeyScanOrder = std::array<Scan<kNumLayers>, N>;
using AllGPIOs = std::array<uint8_t, kGPIONumMax>;

// Force non-constexpr as error
void failure(const char*) {}

template <size_t L, size_t R, size_t C>
constexpr KeyScanOrder<R * C> ConvertKeyScan(const GPIO (&gpio)[R][C],
                                             const Keycode (&kc)[L][R][C]) {
  size_t end_idx[kGPIONumMax] = {0};
  size_t source_count[kGPIONumMax] = {0};

  // Counting sort the source GPIOs

  for (size_t r = 0; r < R; ++r) {
    for (size_t c = 0; c < C; ++c) {
      bool is_none = true;
      for (size_t l = 0; l < L; ++l) {
        if (kc[l][r][c].is_custom || kc[l][r][c].keycode != HID_KEY_NONE) {
          is_none = false;
          break;
        }
      }
      if (is_none) {
        continue;
      }
      if (gpio[r][c].source == gpio[r][c].sink) {
        failure("Source and sink GPIOs have to be different");
      }
      if (gpio[r][c].source >= kGPIONumMax) {
        failure("Invalid source GPIO number");
      }
      if (gpio[r][c].sink >= kGPIONumMax) {
        failure("Invalid sink GPIO number");
      }
      ++end_idx[gpio[r][c].source];
      ++source_count[gpio[r][c].source];
    }
  }

  for (size_t i = 1; i < kGPIONumMax; ++i) {
    end_idx[i] += end_idx[i - 1];
  }

  KeyScanOrder<R* C> output = {0};

  for (size_t r = 0; r < R; ++r) {
    for (size_t c = 0; c < C; ++c) {
      bool is_none = true;
      for (size_t l = 0; l < L; ++l) {
        if (kc[l][r][c].is_custom || kc[l][r][c].keycode != HID_KEY_NONE) {
          is_none = false;
          break;
        }
      }
      if (is_none) {
        continue;
      }
      const size_t idx =
          end_idx[gpio[r][c].source] - source_count[gpio[r][c].source];
      auto& key_slot = output[idx];
      key_slot.gpio = gpio[r][c];
      for (size_t i = gpio[r][c].source > 0 ? end_idx[gpio[r][c].source - 1]
                                            : 0;
           i < idx; ++i) {
        if (key_slot.gpio.sink == output[i].gpio.sink) {
          failure("Duplicate source sink combination.");
        }
      }
      for (size_t l = 0; l < L; ++l) {
        if (!kc[l][r][c].is_custom && kc[l][r][c].keycode == HID_KEY_NONE) {
          continue;
        }
        key_slot.keycodes[l] = kc[l][r][c];
      }
      --source_count[gpio[r][c].source];
    }
  }

  return output;
}

template <size_t N>
constexpr size_t CountKeyScans(const KeyScanOrder<N>& key_scans) {
  size_t i = 0;
  for (; i < key_scans.size(); ++i) {
    if (key_scans[i].gpio.source == 0 && key_scans[i].gpio.sink == 0) {
      return i;
    }
  }

  return i;
}

template <size_t N>
constexpr AllGPIOs CollectAllGPIOs(const KeyScanOrder<N>& scan_order) {
  bool exist[kGPIONumMax] = {0};

  for (size_t i = 0; i < CountKeyScans(scan_order); ++i) {
    const auto& scan = scan_order[i];
    exist[scan.gpio.source] = true;
    exist[scan.gpio.sink] = true;
  }

  AllGPIOs output;
  output.fill(255);
  size_t idx = 0;
  for (size_t i = 0; i < kGPIONumMax; ++i) {
    if (exist[i]) {
      output[idx++] = i;
    }
  }
  return output;
}

constexpr size_t CountGPIOs(const AllGPIOs& gpios) {
  size_t i = 0;
  for (; i < gpios.size(); ++i) {
    if (gpios[i] == 255) {
      return i;
    }
  }

  return i;
}

static constexpr auto __not_in_flash("keyscan") kKeys =
    ConvertKeyScan(kGPIOMatrix, kKeyCodes);
static constexpr AllGPIOs __not_in_flash("keyscan") kGPIOPins =
    CollectAllGPIOs(kKeys);

}  // namespace

size_t GetKeyboardNumLayers() { return kNumLayers; }

size_t GetTotalNumGPIOs() { return CountGPIOs(kGPIOPins); }

uint8_t GetGPIOPin(size_t gpio_idx) { return kGPIOPins.at(gpio_idx); }

size_t GetTotalScans() { return CountKeyScans(kKeys); }

uint8_t GetSourceGPIO(size_t scan_idx) {
  return kKeys.at(scan_idx).gpio.source;
}

uint8_t GetSinkGPIO(size_t scan_idx) { return kKeys.at(scan_idx).gpio.sink; }

bool IsSourceChange(size_t scan_idx) {
  if (scan_idx == 0) {
    return true;
  }
  return GetSourceGPIO(scan_idx - 1) != GetSourceGPIO(scan_idx);
}

Keycode GetKeycodeAtLayer(uint8_t layer, size_t scan_idx) {
  return kKeys.at(scan_idx).keycodes.at(layer);
}
