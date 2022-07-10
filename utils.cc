#include "utils.h"

#include "task.h"

LockSemaphore::LockSemaphore(SemaphoreHandle_t semaphore)
    : semaphore_(semaphore) {
  xSemaphoreTake(semaphore_, portMAX_DELAY);
}

LockSemaphore::~LockSemaphore() { xSemaphoreGive(semaphore_); }
