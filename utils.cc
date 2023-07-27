#include "utils.h"

#include "task.h"

LockSemaphore::LockSemaphore(SemaphoreHandle_t semaphore)
    : semaphore_(semaphore) {
  xSemaphoreTake(semaphore_, portMAX_DELAY);
}

LockSemaphore::~LockSemaphore() { xSemaphoreGive(semaphore_); }

LockSpinlock::LockSpinlock(spin_lock_t* lock)
    : lock_(lock), irq_(spin_lock_blocking(lock_)) {}

LockSpinlock::~LockSpinlock() { spin_unlock(lock_, irq_); }
