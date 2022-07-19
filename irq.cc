#include "irq.h"

#include <atomic>

#include "FreeRTOS.h"
#include "hardware/regs/sio.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "queue.h"
#include "task.h"

struct TaskInfo {
  const uint8_t core_id;
  std::atomic<bool> entered;
  spin_lock_t* sync_wait_lock;
};

static spin_lock_t* __not_in_flash("sync") critical_section_lock;

static TaskHandle_t __not_in_flash("sync") task_handles[2] = {NULL, NULL};

static TaskInfo __not_in_flash("sync") core_info[2] = {
    {.core_id = 0, .entered = false, .sync_wait_lock = NULL},
    {.core_id = 1, .entered = false, .sync_wait_lock = NULL}};

extern "C" void __no_inline_not_in_flash_func(CPUHoggerTask)(void* parameter);

Status StartIRQTasks() {
  HeapStats_t xHeapStats;
  vPortGetHeapStats(&xHeapStats);

  if (xTaskCreateAffinitySet(&CPUHoggerTask, "core_0_hogger",
                             configMINIMAL_STACK_SIZE, (void*)&core_info[0],
                             // Higher priority to make sure it can preempt
                             // whatever is current running on this core
                             CONFIG_TASK_PRIORITY + 1, (1 << 0),
                             &task_handles[0]) != pdPASS ||
      task_handles[0] == NULL) {
    return ERROR;
  }

  vPortGetHeapStats(&xHeapStats);

  if (xTaskCreateAffinitySet(&CPUHoggerTask, "core_1_hogger",
                             configMINIMAL_STACK_SIZE, (void*)&core_info[1],
                             CONFIG_TASK_PRIORITY + 1, (1 << 1),
                             &task_handles[1]) != pdPASS ||
      &task_handles[1] == NULL) {
    return ERROR;
  }

  vPortGetHeapStats(&xHeapStats);

  core_info[0].sync_wait_lock = spin_lock_init(0);
  core_info[1].sync_wait_lock = spin_lock_init(1);
  critical_section_lock = spin_lock_init(3);
  if (core_info[0].sync_wait_lock == NULL ||
      core_info[1].sync_wait_lock == NULL || critical_section_lock) {
    return ERROR;
  }

  return OK;
}

extern "C" void __no_inline_not_in_flash_func(CPUHoggerTask)(void* parameter) {
  TaskInfo& task_info = *(TaskInfo*)parameter;
  const uint32_t cpuid = *(uint32_t*)((SIO_BASE) + (SIO_CPUID_OFFSET));
  assert(cpuid == task_info.core_id);

  while (true) {
    task_info.entered = false;
    xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                    /*clear notification on exit*/ 0xffffffff,
                    /*pulNotificationValue=*/NULL, portMAX_DELAY);
    task_info.entered = true;

    // Here we want to disable IRQ until the other core tells us to stop (by
    // releasing the lock)
    uint32_t core_irq = spin_lock_blocking(task_info.sync_wait_lock);
    spin_unlock(task_info.sync_wait_lock, core_irq);
  }
}

void EnterGlobalCriticalSection() {
  // Don't disable IRQ here since flash writes we don't need to disable IRQ
  // until we actually write it.
  spin_lock_unsafe_blocking(critical_section_lock);

  const uint32_t cpuid = *(uint32_t*)((SIO_BASE) + (SIO_CPUID_OFFSET));
  const uint32_t the_other_core = (cpuid + 1) % 2;

  // Don't hold the IRQ since we don't want to disable it for current core yet.
  spin_lock_unsafe_blocking(core_info[the_other_core].sync_wait_lock);
  xTaskNotifyGive(task_handles[the_other_core]);  // Notify the other core to
                                                  // enter blocking as well

  // We need to be sure the other core has entered the hogger task which is SRAM
  // mapped before we proceed.
  while (!core_info[the_other_core].entered)
    ;
}

void ExitGlobalCriticalSection() {
  const uint32_t cpuid = *(uint32_t*)((SIO_BASE) + (SIO_CPUID_OFFSET));
  const uint32_t the_other_core = (cpuid + 1) % 2;
  spin_unlock_unsafe(core_info[the_other_core].sync_wait_lock);
  spin_unlock_unsafe(critical_section_lock);
}
