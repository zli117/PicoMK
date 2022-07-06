#include "keyscan.h"

#include <stdint.h>
#include <stdio.h>  // Debug

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

static TaskHandle_t keyscan_task_handle = NULL;
static TimerHandle_t timer_handle = NULL;
static SemaphoreHandle_t semaphore = NULL;
static size_t current_layer = 0;
static DebounceTimer* debounce_timer = NULL;

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

static constexpr size_t kMaxKeyPressed = 6;

extern "C" void KeyscanTask(void* parameter) {
  (void)parameter;

  while (true) {
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    // TODO: properly get semaphore
    xSemaphoreTake(semaphore, portMAX_DELAY);
    const size_t layer = current_layer;
    xSemaphoreGive(semaphore);

    // The first 8 bytes in report buffer is the standard 6 key format for boot
    // protocol. In the report descriptor these are specified as paddings so if
    // the host has full USB HID support, they would be ignored. The remaining
    // 32 bytes are the bitmap for each keycode defined by USB standard.
    uint8_t report_buffer[8 + 256 / 8] = {0};
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

        const Keycode kc = GetKeycode(layer, sink, source);

        if (kc.is_custom) {
          // TODO: Dispatch custom keycode
        } else {
          const uint8_t keycode = kc.keycode;
          if (d_timer.pressed) {
            report_buffer[keycode / 8 + 8] |= (1 << (keycode % 8));
            if (boot_protocol_kc_count < 6) {
              report_buffer[2 + boot_protocol_kc_count++] = keycode;
            } else if (report_buffer[2] != 0x01) {
              for (size_t i = 2; i < 8; ++i) {
                report_buffer[i] = 0x01;  // ErrorRollOver
              }
            }
          }
        }
      }

      // Set the sink back to high
      gpio_put(GetSinkGPIO(sink), true);
    }

    if (!tud_hid_n_ready(ITF_KEYBOARD)) {
      continue;
    }

    tud_hid_n_report(ITF_KEYBOARD, /*report_id=*/0, report_buffer,
                     sizeof(report_buffer));

    printf("Printf works\n");
  }
}
