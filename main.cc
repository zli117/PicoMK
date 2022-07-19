#include <stdio.h>

#include "FreeRTOS.h"
#include "irq.h"
#include "pico/stdlib.h"
#include "runner.h"
#include "storage.h"
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
  HeapStats_t xHeapStats1;
  vPortGetHeapStats(&xHeapStats1);

  StartIRQTasks();

  vPortGetHeapStats(&xHeapStats1);

  InitializeStorage();

  vPortGetHeapStats(&xHeapStats1);

  runner::RunnerInit();

  vPortGetHeapStats(&xHeapStats1);

  runner::RunnerStart();

  vPortGetHeapStats(&xHeapStats1);

  vTaskStartScheduler();

  while (true)
    ;

  return 0;
}
