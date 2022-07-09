#include "runner.h"

#include <vector>

#include "FreeRTOS.h"
#include "base.h"
#include "configuration.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "usb.h"
#include "utils.h"

static Configuration* config;
static std::vector<std::shared_ptr<GenericInputDevice>> input_devices;
static std::vector<std::shared_ptr<GenericOutputDevice>> output_devices;

static TaskHandle_t input_task_handle = NULL;
static TimerHandle_t input_timer_handle = NULL;
static TaskHandle_t output_task_handle = NULL;
static TimerHandle_t output_timer_handle = NULL;

Status RunnerInit() {
  config = Configuration::GetConfig();
  input_devices = DeviceRegistry::GetAllInputDevices(config);
  output_devices = DeviceRegistry::GetAllOutputDevices(config);
  if (USBInit() != OK) {
    return ERROR;
  }
  return OK;
}

extern "C" void InputDeviceTask(void* parameter);
extern "C" void InputDeviceTimerCallback(TimerHandle_t xTimer);
extern "C" void OutputDeviceTask(void* parameter);
extern "C" void OutputDeviceTimerCallback(TimerHandle_t xTimer);

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
    for (auto output_device : output_devices) {
      output_device->StartOfInputTick();
    }

    for (auto input_device : input_devices) {
      input_device->Tick();
    }

    for (auto output_device : output_devices) {
      output_device->FinalizeInputTickOutput();
    }
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
    for (auto output_device : output_devices) {
      output_device->Tick();
    }
  }
}

extern "C" void OutputDeviceTimerCallback(TimerHandle_t xTimer) {
  xTaskNotifyGive(output_task_handle);
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
