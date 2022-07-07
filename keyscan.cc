#include "keyscan.h"

#include <stdio.h>

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/gpio.h"
#include "layout.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "tusb.h"
#include "usb.h"
#include "utils.h"

struct DebounceTimer {
  uint8_t tick_count : 7;
  uint8_t pressed : 1;

  DebounceTimer() : tick_count(0), pressed(0) {}
};

// Inaccessible by other threads

static TaskHandle_t keyscan_task_handle = NULL;
static TimerHandle_t timer_handle = NULL;
static SemaphoreHandle_t semaphore = NULL;
static DebounceTimer* debounce_timer = NULL;
static std::map<size_t, CustomKeycodeHandlerCreator> custom_handlers;

// Accessible by other threads

static std::vector<Keycode> keycode_requests;
static std::vector<bool> active_layers;

status RegisterCustomKeycodeHandler(uint8_t keycode,
                                    CustomKeycodeHandlerCreator creator) {
  custom_handlers[keycode] = creator;
  return OK;
}

// Keyscan APIs

status SetLayerStatus(uint8_t layer, bool active) {
  xSemaphoreTake(semaphore, portMAX_DELAY);
  if (layer > active_layers.size()) {
    return ERROR;
  }
  if (layer == 0) {
    return OK;
  }
  active_layers[layer] = active;
  xSemaphoreGive(semaphore);
  return OK;
}

status ToggleLayerStatus(uint8_t layer) {
  xSemaphoreTake(semaphore, portMAX_DELAY);
  if (layer > active_layers.size()) {
    return ERROR;
  }
  if (layer == 0) {
    return OK;
  }
  active_layers[layer] = !active_layers[layer];
  xSemaphoreGive(semaphore);
  return OK;
}

std::vector<uint8_t> GetActiveLayers() {
  std::vector<uint8_t> output;
  xSemaphoreTake(semaphore, portMAX_DELAY);
  for (int16_t i = active_layers.size() - 1; i >= 0; --i) {
    if (active_layers[i]) {
      output.push_back(i);
    }
  }
  xSemaphoreGive(semaphore);
  return output;
}

status PressThenReleaseKey(Keycode kc) {
  xSemaphoreTake(semaphore, portMAX_DELAY);
  keycode_requests.push_back(kc);
  xSemaphoreGive(semaphore);
  return OK;
}

status SendStandardKeycode(uint8_t keycode) {
  xSemaphoreTake(semaphore, portMAX_DELAY);
  keycode_requests.push_back(
      {.keycode = keycode, .is_custom = false, .custom_info = 0});
  xSemaphoreGive(semaphore);
  return OK;
}

// Implementations

extern "C" void KeyscanTask(void* parameter);
extern "C" void TimerCallback(TimerHandle_t xTimer);

__weak_symbol void SinkGPIODelay() { sleep_us(CONFIG_GPIO_SINK_DELAY_US); }

status KeyScanInit() {
  // Initialize all the GPIO pins
  for (size_t i = 0; i < GetNumSinkGPIOs(); ++i) {
    const uint8_t pin = GetSinkGPIO(i);
    gpio_init(pin);
    gpio_set_dir(pin, true);
  }
  for (size_t i = 0; i < GetNumSourceGPIOs(); ++i) {
    const uint8_t pin = GetSourceGPIO(i);
    gpio_init(pin);

    // Set the source GPIO to be INPUT with pullup
    gpio_set_dir(pin, false);
    gpio_pull_up(pin);
  }

  debounce_timer = new DebounceTimer[GetNumSinkGPIOs() * GetNumSourceGPIOs()]();

  active_layers.resize(GetKeyboardNumLayers());
  active_layers[0] = true;

  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);

  return OK;
}

status StartKeyScanTask() {
  BaseType_t status =
      xTaskCreate(&KeyscanTask, "keyscan_task", CONFIG_TASK_STACK_SIZE, NULL,
                  CONFIG_TASK_PRIORITY, &keyscan_task_handle);
  if (status != pdPASS || keyscan_task_handle == NULL) {
    return ERROR;
  }

  timer_handle = xTimerCreate("keyscan_timer", CONFIG_SCAN_TICKS,
                              pdTRUE,  // Auto reload
                              NULL, TimerCallback);

  if (timer_handle == NULL) {
    return ERROR;
  }
  if (xTimerStart(timer_handle, 0) != pdPASS) {
    return ERROR;
  }

  return OK;
}

extern "C" void TimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(keyscan_task_handle);
}

class MouseButtonHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed) override {}

  std::string GetName() const override { return "Mouse key handler"; }
};

class LayerButtonHandler : public CustomKeycodeHandler {
 public:
  void ProcessKeyState(Keycode kc, bool is_pressed) override {
    const bool toggle = kc.custom_info & 0x40;
    const uint8_t layer = kc.custom_info & 0x3f;
    if (is_pressed) {
      if (toggle) {
        ToggleLayerStatus(layer);
      } else {
        SetLayerStatus(layer, true);
      }
    } else {
      if (!toggle) {
        SetLayerStatus(layer, false);
      }
    }
  }

  std::string GetName() const override { return "Layer switch key handler"; }
};

CustomKeycodeHandler* BuiltInHandlerFactory(uint8_t keycode) {
  static std::array<std::unique_ptr<CustomKeycodeHandler>, TOTAL_BUILT_IN_KC>
      singleton_cache;

  switch (keycode) {
    case MSE_L:
    case MSE_M:
    case MSE_R:
    case MSE_BACK:
    case MSE_FORWARD: {
      if (singleton_cache[MSE_L] == NULL) {
        auto mouse_btn_handler = std::make_unique<MouseButtonHandler>();
        singleton_cache[MSE_L] = std::move(mouse_btn_handler);
      }
      return singleton_cache[MSE_L].get();
    }
    case LAYER_SWITCH: {
      if (singleton_cache[LAYER_SWITCH] == NULL) {
        singleton_cache[LAYER_SWITCH] = std::make_unique<LayerButtonHandler>();
      }
      return singleton_cache[LAYER_SWITCH].get();
    }
    default:
      return NULL;
  };
}

CustomKeycodeHandler* RegisteredHandlerFactor(uint8_t keycode) {
  static std::map<uint8_t, std::unique_ptr<CustomKeycodeHandler>>
      singleton_cache;
  auto it = singleton_cache.find(keycode);
  if (it != singleton_cache.end()) {
    return it->second.get();
  } else {
    auto creator_it = custom_handlers.find(keycode);
    if (creator_it == custom_handlers.end()) {
      return NULL;
    }
    auto handler = creator_it->second();
    auto* output = handler.get();
    singleton_cache[keycode] = std::move(handler);
    return output;
  }
}

void ProcessCustomKeycode(Keycode kc, bool is_pressed) {
  auto* handler = RegisteredHandlerFactor(kc.keycode);
  if (handler == NULL) {
    handler = BuiltInHandlerFactory(kc.keycode);
  }
  if (handler != NULL) {
    handler->ProcessKeyState(kc, is_pressed);
  } else {
    LOG_WARNING("Custom Keycode (%d) missing handler", kc.keycode);
  }
}

constexpr size_t kReportBufferSize = 8 + 256 / 8;

void ProcessKeycode(Keycode kc, bool is_pressed, uint8_t (&buffer)[],
                    uint8_t* boot_protocol_kc_count) {
  if (kc.is_custom) {
    ProcessCustomKeycode(kc, is_pressed);
  } else {
    const uint8_t keycode = kc.keycode;
    if (is_pressed) {
      buffer[keycode / 8 + 8] |= (1 << (keycode % 8));
      if (*boot_protocol_kc_count < 6) {
        buffer[2 + (*boot_protocol_kc_count)++] = keycode;
      } else if (buffer[2] != 0x01) {
        for (size_t i = 2; i < 8; ++i) {
          buffer[i] = 0x01;  // ErrorRollOver
        }
      }
    }
  }
}

extern "C" void KeyscanTask(void* parameter) {
  (void)parameter;

  while (true) {
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);

    const std::vector<uint8_t> active_layers = GetActiveLayers();

    // The first 8 bytes in report buffer is the standard 6 key format for boot
    // protocol. In the report descriptor these are specified as paddings so if
    // the host has full USB HID support, they would be ignored. The remaining
    // 32 bytes are the bitmap for each keycode defined by USB standard.
    uint8_t report_buffer[kReportBufferSize] = {0};
    uint8_t boot_protocol_kc_count = 0;

    // Set all sink GPIOs to HIGH
    for (size_t sink = 0; sink < GetNumSinkGPIOs(); ++sink) {
      gpio_put(GetSinkGPIO(sink), true);
    }

    for (size_t sink = 0; sink < GetNumSinkGPIOs(); ++sink) {
      // Set one sink pin to LOW
      gpio_put(GetSinkGPIO(sink), false);
      SinkGPIODelay();

      for (size_t source = 0; source < GetNumSourceGPIOs(); ++source) {
        DebounceTimer& d_timer =
            debounce_timer[sink * GetNumSourceGPIOs() + source];

        // gpio_get returns true when no button is pressed (because of pull up)
        // and  false when a button is pressed.
        const bool pressed = !gpio_get(GetSourceGPIO(source));
        if (pressed != d_timer.pressed) {
          d_timer.tick_count += CONFIG_SCAN_TICKS;
          if (d_timer.tick_count >= CONFIG_DEBOUNCE_TICKS) {
            d_timer.pressed = !d_timer.pressed;
            d_timer.tick_count = 0;
          }
        }

        Keycode kc = {0};
        for (uint8_t l : active_layers) {
          Keycode layer_kc = GetKeycodeAtLayer(l, sink, source);
          if (layer_kc.is_custom || layer_kc.keycode != HID_KEY_NONE) {
            kc = layer_kc;
            break;
          }
        }

        ProcessKeycode(kc, d_timer.pressed, report_buffer,
                       &boot_protocol_kc_count);
      }

      // Set the sink back to high
      gpio_put(GetSinkGPIO(sink), true);
    }

    // Process the requested keys
    std::vector<Keycode> local_keycode_requests;
    xSemaphoreTake(semaphore, portMAX_DELAY);
    std::swap(local_keycode_requests, keycode_requests);
    xSemaphoreGive(semaphore);
    for (const auto& kc : local_keycode_requests) {
      ProcessKeycode(kc, /*is_pressed=*/true, report_buffer,
                     &boot_protocol_kc_count);
    }

    if (!tud_hid_n_ready(ITF_KEYBOARD)) {
      continue;
    }

    tud_hid_n_report(ITF_KEYBOARD, /*report_id=*/0, report_buffer,
                     sizeof(report_buffer));
  }
}
