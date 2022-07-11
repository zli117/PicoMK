#include "runner.h"

#include <memory>
#include <vector>

#include "FreeRTOS.h"
#include "base.h"
#include "configuration.h"
#include "hardware/timer.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "usb.h"
#include "utils.h"

static Configuration* config;
static std::vector<std::shared_ptr<GenericInputDevice>> input_devices;
static std::vector<std::shared_ptr<GenericOutputDevice>> output_devices;
static std::vector<std::shared_ptr<GenericOutputDevice>> slow_output_devices;

static TaskHandle_t input_task_handle = NULL;
static TimerHandle_t input_timer_handle = NULL;
static TaskHandle_t output_task_handle = NULL;
static TimerHandle_t output_timer_handle = NULL;
static TaskHandle_t slow_output_task_handle = NULL;
static TimerHandle_t slow_output_timer_handle = NULL;

Status RunnerInit() {
  config = Configuration::GetConfig();
  if (DeviceRegistry::GetAllDevices(config, &input_devices, &output_devices,
                                    &slow_output_devices) != OK) {
    return ERROR;
  }
  if (USBInit() != OK) {
    return ERROR;
  }
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

  status = xTaskCreate(
      &SlowOutputDeviceTask, "slow_output_device_task", CONFIG_TASK_STACK_SIZE,
      NULL, CONFIG_TASK_PRIORITY - 1, &slow_output_task_handle);
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

  status =
      xTaskCreate(&InputDeviceTask, "input_device_task", CONFIG_TASK_STACK_SIZE,
                  NULL, CONFIG_TASK_PRIORITY, &input_task_handle);
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

  return OK;
}

extern "C" void InputDeviceTask(void* parameter) {
  (void)parameter;

  while (true) {
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    const uint64_t start_time = time_us_64();
    for (auto output_device : output_devices) {
      output_device->StartOfInputTick();
    }
    for (auto output_device : slow_output_devices) {
      output_device->StartOfInputTick();
    }

    for (auto input_device : input_devices) {
      input_device->Tick();
    }

    for (auto output_device : output_devices) {
      output_device->FinalizeInputTickOutput();
    }
    for (auto output_device : slow_output_devices) {
      output_device->FinalizeInputTickOutput();
    }
    const uint64_t end_time = time_us_64();
    LOG_DEBUG("Input task per iteration takes %d us", end_time - start_time);
  }
}

extern "C" void InputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(input_task_handle);
}

extern "C" void OutputDeviceTask(void* parameter) {
  (void)parameter;

  while (true) {
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    const uint64_t start_time = time_us_64();
    for (auto output_device : output_devices) {
      output_device->Tick();
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
    // Wait for the timer callback to wake it up. Running this outside the timer
    // context to avoid overflowing the timer task.
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    const uint64_t start_time = time_us_64();
    for (auto output_device : slow_output_devices) {
      output_device->Tick();
    }
    const uint64_t end_time = time_us_64();
    LOG_DEBUG("Slow output task per iteration takes %d us",
              end_time - start_time);
  }
}

extern "C" void SlowOutputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(slow_output_task_handle);
}

void SetConfigModeAllDevice(bool is_config_mode) {
  for (auto device : input_devices) {
    device->SetConfigMode(is_config_mode);
  }
  for (auto device : output_devices) {
    device->SetConfigMode(is_config_mode);
  }
}

void NotifyConfigChange() {
  for (auto device : input_devices) {
    device->OnUpdateConfig();
  }
  for (auto device : output_devices) {
    device->OnUpdateConfig();
  }
}
