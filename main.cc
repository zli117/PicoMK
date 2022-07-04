#include <FreeRTOS.h>
#include <hardware/clocks.h>
#include <hardware/i2c.h>
#include <hardware/watchdog.h>
#include <layout.h>
#include <pico/stdlib.h>
#include <task.h>

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for
// information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

extern "C" void vApplicationMallocFailedHook(void) {}
extern "C" void vApplicationIdleHook(void) {}
extern "C" void vApplicationStackOverflowHook(TaskHandle_t pxTask,
                                              char *pcTaskName) {}
extern "C" void vApplicationTickHook(void) {}

// Overrides for C++ so that new and delete will call the correct functions
void *operator new(size_t size) { return pvPortMalloc(size); }
void *operator new[](size_t size) { return pvPortMalloc(size); }
void operator delete(void *ptr) { vPortFree(ptr); }
void operator delete[](void *ptr) { vPortFree(ptr); }

int main() {
  int i = keyboard::GetKeyboardNumLayers();
  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  vTaskStartScheduler();

  return 0;
}
