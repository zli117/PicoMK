#include "runner.h"

#include <memory>
#include <vector>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "base.h"
#include "configuration.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "usb.h"
#include "utils.h"

static std::vector<std::shared_ptr<GenericInputDevice>> input_devices;
static std::vector<std::shared_ptr<GenericOutputDevice>> output_devices;
static std::vector<std::shared_ptr<GenericOutputDevice>> slow_output_devices;

static TaskHandle_t input_task_handle = NULL;
static TimerHandle_t input_timer_handle = NULL;
static TaskHandle_t output_task_handle = NULL;
static TimerHandle_t output_timer_handle = NULL;
static TaskHandle_t slow_output_task_handle = NULL;
static TimerHandle_t slow_output_timer_handle = NULL;

static SemaphoreHandle_t semaphore;
static bool is_config_mode;
static bool update_config_flag;

namespace runner {

Status RunnerInit() {
  input_devices = DeviceRegistry::GetInputDevices();
  output_devices = DeviceRegistry::GetOutputDevices(
      /*is_slow=*/false);
  slow_output_devices = DeviceRegistry::GetOutputDevices(
      /*is_slow=*/true);
  volatile size_t output_size = output_devices.size();
  volatile size_t slowoutput_size = slow_output_devices.size();
  volatile size_t input_size = input_devices.size();
  if (USBInit() != OK) {
    return ERROR;
  }

  is_config_mode = false;
  update_config_flag = false;

  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);
  return OK;
}

extern "C" void InputDeviceTask(void* parameter);
extern "C" void InputDeviceTimerCallback(TimerHandle_t xTimer);
extern "C" void OutputDeviceTask(void* parameter);
extern "C" void OutputDeviceTimerCallback(TimerHandle_t xTimer);
extern "C" void SlowOutputDeviceTask(void* parameter);
extern "C" void SlowOutputDeviceTimerCallback(TimerHandle_t xTimer);

Status RunnerStart() {
  // Start USB task first
  if (StartUSBTask() != OK) {
    return ERROR;
  }

  // Start output device task

  BaseType_t status = xTaskCreate(&OutputDeviceTask, "output_device_task",
                                  CONFIG_TASK_STACK_SIZE, NULL,
                                  CONFIG_TASK_PRIORITY, &output_task_handle);
  if (status != pdPASS || output_task_handle == NULL) {
    return ERROR;
  }

  output_timer_handle = xTimerCreate("output_device_timer", CONFIG_SCAN_TICKS,
                                     pdTRUE,  // Auto reload
                                     NULL, &OutputDeviceTimerCallback);

  if (output_timer_handle == NULL) {
    return ERROR;
  }
  if (xTimerStart(output_timer_handle, 0) != pdPASS) {
    return ERROR;
  }

  // Start slow output device task

  status = xTaskCreate(&SlowOutputDeviceTask, "slow_output_device_task",
                       CONFIG_TASK_STACK_SIZE, NULL, CONFIG_TASK_PRIORITY - 1,
                       &slow_output_task_handle);
  if (status != pdPASS || slow_output_task_handle == NULL) {
    return ERROR;
  }

  slow_output_timer_handle =
      xTimerCreate("slow_output_device_timer", CONFIG_SLOW_TICKS,
                   pdTRUE,  // Auto reload
                   NULL, &SlowOutputDeviceTimerCallback);

  if (slow_output_timer_handle == NULL) {
    return ERROR;
  }
  if (xTimerStart(slow_output_timer_handle, 0) != pdPASS) {
    return ERROR;
  }

  // Start input device task

  // Pin input task to the same core as tick handler because there could be
  // flash writes that will disable the other core. Don't want to block the tick
  // handler as well as usb task (which is also pinned to the tick core) for too
  // long.
  status = xTaskCreateAffinitySet(
      &InputDeviceTask, "input_device_task", CONFIG_TASK_STACK_SIZE, NULL,
      CONFIG_TASK_PRIORITY, (1 << (configTICK_CORE)), &input_task_handle);
  if (status != pdPASS || input_task_handle == NULL) {
    return ERROR;
  }

  input_timer_handle = xTimerCreate("input_device_timer", CONFIG_SCAN_TICKS,
                                    pdTRUE,  // Auto reload
                                    NULL, &InputDeviceTimerCallback);

  if (input_timer_handle == NULL) {
    return ERROR;
  }
  if (xTimerStart(input_timer_handle, 0) != pdPASS) {
    return ERROR;
  }

  watchdog_enable(/*delay_ms=*/100, /*pause_on_debug=*/true);

  return OK;
}

extern "C" void InputDeviceTask(void* parameter) {
  (void)parameter;

  bool local_is_config_mode = false;

  while (true) {
    // Initialization

    DeviceRegistry::UpdateConfig();
    for (auto output_device : output_devices) {
      output_device->StartOfInputTick();
    }
    for (auto output_device : slow_output_devices) {
      output_device->StartOfInputTick();
    }

    for (auto input_device : input_devices) {
      input_device->InputLoopStart();
    }

    for (auto output_device : output_devices) {
      output_device->FinalizeInputTickOutput();
    }
    for (auto output_device : slow_output_devices) {
      output_device->FinalizeInputTickOutput();
    }

    while (true) {
      const uint64_t sleep_time = time_us_64();
      // Wait for the timer callback to wake it up. Running this outside the
      // timer context to avoid overflowing the timer task.
      xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                      /*clear notification on exit*/ 0xffffffff,
                      /*pulNotificationValue=*/NULL, portMAX_DELAY);
      const uint64_t start_time = time_us_64();
      if (start_time - sleep_time < 1000) {
        LOG_WARNING(
            "Input task didn't sleep enough. Remaining time budget less than "
            "is less than 1ms.");
      }
      bool should_change_config_mode;
      bool should_update_config;
      {
        LockSemaphore lock(semaphore);
        should_change_config_mode = local_is_config_mode != is_config_mode;
        should_update_config = update_config_flag;
        update_config_flag = false;
      }
      if (should_change_config_mode) {
        local_is_config_mode = !local_is_config_mode;
        for (auto device : output_devices) {
          device->SetConfigMode(local_is_config_mode);
        }
        for (auto device : input_devices) {
          device->SetConfigMode(local_is_config_mode);
        }
        for (auto device : slow_output_devices) {
          device->SetConfigMode(local_is_config_mode);
        }
      }
      if (should_update_config) {
        // Rerun the initialization
        break;
      }

      for (auto output_device : output_devices) {
        output_device->StartOfInputTick();
      }
      for (auto output_device : slow_output_devices) {
        output_device->StartOfInputTick();
      }

      for (auto input_device : input_devices) {
        input_device->InputTick();
      }

      for (auto output_device : output_devices) {
        output_device->FinalizeInputTickOutput();
      }
      for (auto output_device : slow_output_devices) {
        output_device->FinalizeInputTickOutput();
      }
      const uint64_t end_time = time_us_64();
      LOG_DEBUG("Input task per iteration takes %d us", end_time - start_time);
      LOG_INFO("End input tick");
      watchdog_update();
    }
  }
}

extern "C" void InputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(input_task_handle);
}

extern "C" void OutputDeviceTask(void* parameter) {
  (void)parameter;

  while (true) {
    const uint64_t sleep_time = time_us_64();
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    const uint64_t start_time = time_us_64();
    if (start_time - sleep_time < 1000) {
      LOG_WARNING(
          "Output task didn't sleep enough. Remaining time budget less than "
          "1ms.");
    }
    for (auto output_device : output_devices) {
      output_device->OutputTick();
    }
    const uint64_t end_time = time_us_64();
    LOG_DEBUG("Output task per iteration takes %d us", end_time - start_time);
  }
}

extern "C" void OutputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(output_task_handle);
}

extern "C" void SlowOutputDeviceTask(void* parameter) {
  (void)parameter;

  while (true) {
    const uint64_t sleep_time = time_us_64();
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    const uint64_t start_time = time_us_64();
    if (start_time - sleep_time < 1000) {
      LOG_WARNING(
          "Slow output task didn't sleep enough. Remaining time budget is less "
          "than 1ms.");
    }
    for (auto output_device : slow_output_devices) {
      output_device->OutputTick();
    }
    const uint64_t end_time = time_us_64();
    LOG_DEBUG("Slow output task per iteration takes %d us",
              end_time - start_time);
  }
}

extern "C" void SlowOutputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(slow_output_task_handle);
}

void SetConfigMode(bool is_config) {
  LockSemaphore lock(semaphore);
  is_config_mode = is_config;
}

void NotifyConfigChange() {
  LockSemaphore lock(semaphore);
  update_config_flag = true;
}

}  // namespace runner
