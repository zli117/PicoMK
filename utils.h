#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>

#include <queue>
#include <string>

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/sync.h"
#include "semphr.h"

// TODO: rename this
enum status { OK, ERROR };
using Status = status;

enum LogLevel { L_ERROR = 1, L_WARNING = 2, L_INFO = 3, L_DEBUG = 4 };

#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#define LOG(LEVEL, prefix, format, ...)                     \
  ({                                                        \
    if (CONFIG_DEBUG_LOG_LEVEL >= LEVEL) {                  \
      printf("%s %s:%d " format "\n", prefix, __FILENAME__, \
             __LINE__ __VA_OPT__(, ) __VA_ARGS__);          \
    }                                                       \
    0;                                                      \
  })

#define LOG_ERROR(format, ...) \
  LOG(LogLevel::L_ERROR, "E", format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARNING(format, ...) \
  LOG(LogLevel::L_WARNING, "W", format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(format, ...) \
  LOG(LogLevel::L_INFO, "I", format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_DEBUG(format, ...) \
  LOG(LogLevel::L_DEBUG, "D", format __VA_OPT__(, ) __VA_ARGS__)

class LockSemaphore {
 public:
  explicit LockSemaphore(SemaphoreHandle_t semaphore);

  virtual ~LockSemaphore();

 private:
  SemaphoreHandle_t semaphore_;
};

class LockSpinlock {
 public:
  explicit LockSpinlock(spin_lock_t* lock);

  virtual ~LockSpinlock();

 private:
  spin_lock_t* lock_;
  uint32_t irq_;
};

template <typename T>
class SleepQueue {
 public:
  SleepQueue(spin_lock_t* lock, TaskHandle_t task_handle, size_t max_size)
      : max_size_(max_size), task_handle_(task_handle), lock_(lock) {}

  void WakeupTask() const {
    if (task_handle_ != NULL) {
      xTaskNotifyGive(task_handle_);
    }
  }

  void WakeupTaskISR() const {
    if (task_handle_ != NULL) {
      vTaskNotifyGiveFromISR(task_handle_, /*pxHigherPriorityTaskWoken=*/NULL);
    }
  }

  bool Push(const T& value) {
    // LockSpinlock lock(lock_);
    if (data_.size() >= max_size_) {
      return false;
    }
    data_.push(value);
    return true;
  }

  bool Push(T&& value) {
    // LockSpinlock lock(lock_);
    if (data_.size() >= max_size_) {
      return false;
    }
    data_.push(std::move(value));
    return true;
  }

  const T* Peak() const {
    // LockSpinlock lock(lock_);
    if (data_.empty()) {
      return NULL;
    }
    return &data_.front();
  }

  bool Pop() {
    // LockSpinlock lock(lock_);
    if (data_.empty()) {
      return false;
    }
    data_.pop();
    return true;
  }

  size_t Size() const {
    // LockSpinlock lock(lock_);
    return data_.size();
  }

 private:
  const size_t max_size_;
  const TaskHandle_t task_handle_;
  spin_lock_t* lock_;
  std::queue<T> data_;
};

#endif /* UTILS_H_ */