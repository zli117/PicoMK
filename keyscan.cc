#include "keyscan.h"

#include <stdint.h>
#include <stdio.h>  // Debug

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/gpio.h"
#include "layout.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include "status.h"
#include "task.h"
#include "timers.h"

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

namespace keyboard {

__weak_symbol void SinkGPIODelay() { sleep_us(CONFIG_GPIO_SINK_DELAY_US); }

status KeyScanInit() {
  // Initialize all the GPIO pins
  for (size_t i = 0; i < GetNumSinkGPIOs(); ++i) {
    const uint8_t pin = GetSinkGPIO(i);
    gpio_init(pin);
    gpio_set_dir(pin, true);
    // gpio_set_pulls(pin, false, true);
  }
  for (size_t i = 0; i < GetNumSourceGPIOs(); ++i) {
    const uint8_t pin = GetSourceGPIO(i);
    gpio_init(pin);

    // Set the source GPIO to be INPUT with pullup
    gpio_set_dir(pin, false);
    gpio_pull_up(pin);
  }

  debounce_timer = new DebounceTimer[GetNumSinkGPIOs() * GetNumSourceGPIOs()]();

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

  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);

  return OK;
}

}  // namespace keyboard

extern "C" void TimerCallback(TimerHandle_t xTimer) {
  using namespace keyboard;
  xTaskNotifyGive(keyscan_task_handle);
}

static constexpr size_t kMaxKeyPressed = 6;

extern "C" void KeyscanTask(void* parameter) {
  using namespace keyboard;
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

    Keycode pressed_keys[6] = {};
    size_t num_pressed = 0;

    // Set all sink GPIOs to input (tri-state)
    for (size_t sink = 0; sink < GetNumSinkGPIOs(); ++sink) {
      gpio_put(GetSinkGPIO(sink), true);
    }

    for (size_t sink = 0; sink < GetNumSinkGPIOs() && num_pressed < 6; ++sink) {
      // Set one sink pin to output and LOW
      // gpio_set_dir(sink, true);
      gpio_put(GetSinkGPIO(sink), false);
      SinkGPIODelay();

      for (size_t source = 0; source < GetNumSourceGPIOs() && num_pressed < 6;
           ++source) {
        DebounceTimer& d_timer =
            debounce_timer[sink * GetNumSourceGPIOs() + source];

        // gpio_get returns true when no button is pressed (because of pull up)
        // and  false when a button is pressed.
        const bool pressed = !gpio_get(GetSourceGPIO(source));
        // if (pressed) {
        //   printf("sink %d (%d) source %d (%d) pressed\n", sink,
        //          GetSinkGPIO(sink), source, GetSourceGPIO(source));
        // }
        if (pressed != d_timer.pressed) {
          d_timer.tick_count += CONFIG_SCAN_TICKS;
          if (d_timer.tick_count >= CONFIG_DEBOUNCE_TICKS) {
            d_timer.pressed = !d_timer.pressed;
            d_timer.tick_count = 0;
          }
        }
        if (d_timer.pressed) {
          pressed_keys[num_pressed++] = GetKeycode(layer, sink, source);
        }
      }

      // Set the sink back to tri-state
      // gpio_set_dir(sink, false);
      gpio_put(GetSinkGPIO(sink), true);
    }

    // Debug
    if (num_pressed > 0) {
      printf("Pressed: ");
      for (size_t i = 0; i < num_pressed; ++i) {
        if (pressed_keys[i].is_custom) {
          printf("(0x%x) ", pressed_keys[i].keycode);
        } else {
          printf("0x%x ", pressed_keys[i].keycode);
        }
      }
      printf("\n");
    }
  }
}
