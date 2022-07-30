#ifndef CONFIG_H_
#define CONFIG_H_

// Required
#include "FreeRTOSConfig.h"

// Name of the keyboard. It'll be reported to USB host and show up when running
// lsusb
#define CONFIG_KEYBOARD_NAME "Pico Keyboard V0.4"

// Ticks on key scan frequency and debouncing time. Tick here refers to FreeRTOS
// tick, which is 1ms.
#define CONFIG_SCAN_TICKS 5
#define CONFIG_DEBOUNCE_TICKS 15

// How frequent should we run the slow output tasks
#define CONFIG_SLOW_TICKS 50

// USB identifiers. Modify if necessary.
#define CONFIG_USB_VID 0xeceb
#define CONFIG_USB_PID 0x3026
#define CONFIG_USB_VENDER_NAME "PicoMK"
#define CONFIG_USB_SERIAL_NUM "1234"

// Configure the filesystem size and config file name
#define CONFIG_FLASH_FILESYSTEM_SIZE (32 * 4096)
#define CONFIG_FLASH_JSON_FILE_NAME "config.json"

////////////////////////////////////////////////////////////////////////////////
// Advanced options (usually no need to modify)
////////////////////////////////////////////////////////////////////////////////

// How many us should we wait between each GPIO sink row scan, for the wires to
// fully discharge. Usually 1 us is good enough.
#define CONFIG_GPIO_SINK_DELAY_US 1

// Stack size of the tasks. Usually no need to change
#define CONFIG_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)

// Priority of the tasks. Usually no need to change
#define CONFIG_TASK_PRIORITY (configMAX_PRIORITIES - 2)

// Frequency for USB polls. 1 ms is usually good
#define CONFIG_USB_POLL_MS 1

// Enable or disable USB serial debug

// Set to 1 to enable USB serial debug.
#define CONFIG_DEBUG_ENABLE_USB_SERIAL 0

#if CONFIG_DEBUG_ENABLE_USB_SERIAL

// Log level for debugging logs
// L_ERROR = 1, L_WARNING = 2, L_INFO = 3, L_DEBUG = 4
// Level <= CONFIG_DEBUG_LOG_LEVEL will be shown
#define CONFIG_DEBUG_LOG_LEVEL 2

// Don't change these three unless you know what you're doing
#define CONFIG_DEBUG_USB_SERIAL_CDC_CMD_MAX_SIZE 8
#define CONFIG_DEBUG_USB_BUFFER_SIZE 64
#define CONFIG_DEBUG_USB_TIMEOUT_US 500000

#else

#define CONFIG_DEBUG_LOG_LEVEL 0  // Disable all logs

#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

#endif /* CONFIG_H_ */
