#include "peripheral.h"

#include "FreeRTOS.h"
#include "config.h"
#include "joystick.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "tusb.h"
#include "usb.h"

static TaskHandle_t peripheral_task_handle = NULL;
static TimerHandle_t timer_handle = NULL;
static SemaphoreHandle_t semaphore = NULL;

#if CONFIG_ENABLE_JOYSTICK

static CenteringPotentialMeterDriver* x_pot = NULL;
static CenteringPotentialMeterDriver* y_pot = NULL;

#endif /* CONFIG_ENABLE_JOYSTICK */

// Variables needs lock protection

static uint8_t mouse_button_state = 0;

#if CONFIG_ENABLE_JOYSTICK

static JoystickMode joystick_mode = MOUSE;

#endif /* CONFIG_ENABLE_JOYSTICK */

// API

status SetMouseButtonState(BuiltInCustomKeyCode mouse_key, bool is_pressed) {
  if (mouse_key > MSE_FORWARD) {
    return ERROR;
  }

  xSemaphoreTake(semaphore, portMAX_DELAY);
  if (is_pressed) {
    mouse_button_state |= (1 << mouse_key);
  } else {
    mouse_button_state &= ~(1 << mouse_key);
  }
  xSemaphoreGive(semaphore);
  return OK;
}

status SetJoystickMode(JoystickMode mode) {
#if CONFIG_ENABLE_JOYSTICK
  xSemaphoreTake(semaphore, portMAX_DELAY);
  joystick_mode = mode;
  xSemaphoreGive(semaphore);
#endif /* CONFIG_ENABLE_JOYSTICK */
  return OK;
}

extern "C" void PeripheralTask(void* parameter);
extern "C" void PeripheralTimerCallback(TimerHandle_t xTimer);

status PeripheralInit() {
#if CONFIG_ENABLE_JOYSTICK
  x_pot = new CenteringPotentialMeterDriver(CONFIG_JOYSTICK_GPIO_X,
                                            CONFIG_JOYSTICK_SMOOTH_BUFFER_LEN,
                                            CONFIG_JOYSTICK_FLIP_X_DIR);
  y_pot = new CenteringPotentialMeterDriver(CONFIG_JOYSTICK_GPIO_Y,
                                            CONFIG_JOYSTICK_SMOOTH_BUFFER_LEN,
                                            CONFIG_JOYSTICK_FLIP_Y_DIR);
#endif /* CONFIG_ENABLE_JOYSTICK */

  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);
  return OK;
}

status StartPeripheralTask() {
  BaseType_t status =
      xTaskCreate(&PeripheralTask, "peripheral_task", CONFIG_TASK_STACK_SIZE,
                  NULL, CONFIG_TASK_PRIORITY, &peripheral_task_handle);
  if (status != pdPASS || peripheral_task_handle == NULL) {
    return ERROR;
  }
  timer_handle = xTimerCreate("peripheral_timer", CONFIG_SCAN_TICKS,
                              pdTRUE,  // Auto reload
                              NULL, PeripheralTimerCallback);

  if (timer_handle == NULL) {
    return ERROR;
  }
  if (xTimerStart(timer_handle, 0) != pdPASS) {
    return ERROR;
  }

  return OK;
}

extern "C" void PeripheralTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(peripheral_task_handle);
}

extern "C" void PeripheralTask(void* parameter) {
  (void)parameter;

  while (true) {
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);

    xSemaphoreTake(semaphore, portMAX_DELAY);
    const uint8_t mouse_buttons = mouse_button_state;

#if CONFIG_ENABLE_JOYSTICK
    const JoystickMode js_mode = joystick_mode;
#endif /* CONFIG_ENABLE_JOYSTICK */
    xSemaphoreGive(semaphore);

    int8_t x = 0;
    int8_t y = 0;
    int8_t horizontal = 0;
    int8_t vertical = 0;

#if CONFIG_ENABLE_JOYSTICK
    x_pot->Tick();
    y_pot->Tick();
    switch (js_mode) {
      case MOUSE: {
        x = x_pot->GetValue();
        y = y_pot->GetValue();
        break;
      }
      case XY_SCROLL: {
        horizontal = x_pot->GetValue();
        vertical = y_pot->GetValue();
        break;
      }
      default:
        LOG_ERROR("Unsupported joystick mode (%d)", js_mode);
        break;
    }
#endif /* CONFIG_ENABLE_JOYSTICK */

    if (!tud_hid_n_ready(ITF_MOUSE)) {
      continue;
    }

    tud_hid_n_mouse_report(ITF_MOUSE, /*report_id=*/0, mouse_buttons, x, y,
                           vertical, horizontal);
  }
}
