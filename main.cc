#include <stdio.h>

#include "FreeRTOS.h"
// #include "hardware/clocks.h"
// #include "hardware/i2c.h"
// #include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "runner.h"
#include "utils.h"

extern "C" void vApplicationMallocFailedHook(void) {
  LOG_ERROR("Failed malloc. OOM");
}
extern "C" void vApplicationIdleHook(void) {}
extern "C" void vApplicationStackOverflowHook(TaskHandle_t pxTask,
                                              char *pcTaskName) {
  LOG_ERROR("Stack overflow for task %s", pcTaskName);
}
extern "C" void vApplicationTickHook(void) {}

int main() {
  RunnerInit();
  RunnerStart();

  vTaskStartScheduler();

  while (true)
    ;

  return 0;
}
