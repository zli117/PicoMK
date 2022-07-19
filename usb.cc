#include "usb.h"

#include <stdint.h>

#include <algorithm>

#include "FreeRTOS.h"
#include "config.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "tusb.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
// USB descriptors
////////////////////////////////////////////////////////////////////////////////

// Device descriptor. There's only one
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),  // Size of the Descriptor in Bytes
    .bDescriptorType = TUSB_DESC_DEVICE,    // Device Descriptor (0x01)
    .bcdUSB = 0x0200,         // USB Specification Number 0x0200 for USB 2.0
    .bDeviceClass = 0x00,     // Each interface specifies its own class code
    .bDeviceSubClass = 0x00,  // Zero because bDeviceClass is 0
    .bDeviceProtocol = 0x00,  // No class specific protocols on a device basis
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = CONFIG_USB_VID,
    .idProduct = CONFIG_USB_PID,
    .bcdDevice = 0x0100,  // Device release number specified by developer.

    // Indices of string descriptors

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01  // Number of Configurations
};

// HID report descriptors. One for each interface. We use two interfaces so that
// we can support boot mode, and the reason for boot protocol to require two
// interfaces is that we can't use Report ID (boot protocol doesn't parse Report
// Descriptor)

// Custom keyboard report descriptor so that we can support both boot protocol
// and report protocol in one interface, even without SetProtocol request.
uint8_t const desc_hid_keyboard_report[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),      //
    HID_USAGE(HID_USAGE_DESKTOP_KEYBOARD),       //
    HID_COLLECTION(HID_COLLECTION_APPLICATION),  //

    // No report ID since we need to support boot protocol

    // Treat the first 8 as paddings in report protocol. In boot protocol,
    // this is the standard 6-key roll over format. For report protocol, the key
    // presses are reported in the bitmap after it. In this way we can support
    // BIOS even if it doesn't send SetProtocol request.
    HID_REPORT_COUNT(8),      // 8 reports
    HID_REPORT_SIZE(8),       // Each has size of 8 bytes
    HID_INPUT(HID_CONSTANT),  //

    // 255 bits bitmap for key state.
    HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),            //
    HID_USAGE_MIN(0),                                   // Starts from keycode 0
    HID_USAGE_MAX_N(255, 2),                            // Ends at keycode 255
    HID_LOGICAL_MIN(0),                                 //
    HID_LOGICAL_MAX(1),                                 //
    HID_REPORT_SIZE(1),                                 // 1 bit for each key
    HID_REPORT_COUNT_N(256, 2),                         // 256 keys
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),  //

    // Output (from host) 5-bit LED Indicator Kana | Compose | ScrollLock |
    // CapsLock | NumLock
    HID_USAGE_PAGE(HID_USAGE_PAGE_LED),                  //
    HID_USAGE_MIN(1),                                    //
    HID_USAGE_MAX(5),                                    //
    HID_REPORT_COUNT(5),                                 //
    HID_REPORT_SIZE(1),                                  //
    HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),  //

    // Led padding of 3 bits
    HID_REPORT_COUNT(1),       //
    HID_REPORT_SIZE(3),        //
    HID_OUTPUT(HID_CONSTANT),  //
    HID_COLLECTION_END};

// Use the standard mouse report descriptor.
uint8_t const desc_hid_mouse_report[] = {TUD_HID_REPORT_DESC_MOUSE()};
uint8_t const desc_hid_consumer_report[] = {TUD_HID_REPORT_DESC_CONSUMER()};

// Configuration descripter and all the interface, HID, endpoint descriptors.
// This is required by the USB protocol that all the

#define ENDPOINT_IN_ADDR(ENDPOINT) (0x80 | (((ENDPOINT) + 1) & 0x7))
#define ENDPOINT_OUT_ADDR(ENDPOINT) (((ENDPOINT) + 1) & 0x7)

#if CONFIG_DEBUG_ENABLE_USB_SERIAL
#define DESC_CONFIG_TOTAL_LEN \
  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * 3 + TUD_CDC_DESC_LEN)
#else
#define DESC_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * 3)
#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1,                      // bConfigurationValue
                          ITF_TOTAL,              // bNumInterfaces
                          0,                      // iConfiguration (string idx
                          DESC_CONFIG_TOTAL_LEN,  // wTotalLength
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,  // Remote wakeup
                          500),                                // Max 500mA

    TUD_HID_DESCRIPTOR(ITF_KEYBOARD,               // bInterfaceNumber
                       4,                          // iInterface (string idx)
                       HID_ITF_PROTOCOL_KEYBOARD,  // Boot keyboard protocol
                       sizeof(desc_hid_keyboard_report),  // Keyboard HID size
                       ENDPOINT_IN_ADDR(ITF_KEYBOARD),    // Endpoint address
                       CFG_TUD_HID_EP_BUFSIZE,  // Endpoint buffer size
                       CONFIG_USB_POLL_MS),     // Pulling interval
    TUD_HID_DESCRIPTOR(ITF_MOUSE,               // bInterfaceNumber
                       5,                       // iInterface (string idx)
                       HID_ITF_PROTOCOL_MOUSE,  // Boot mouse protocol
                       sizeof(desc_hid_mouse_report),  // Mouse HID size
                       ENDPOINT_IN_ADDR(ITF_MOUSE),    // Endpoint address
                       CFG_TUD_HID_EP_BUFSIZE,         // Endpoint buffer size
                       CONFIG_USB_POLL_MS),            // Pulling interval
    TUD_HID_DESCRIPTOR(ITF_CONSUMER,                   // bInterfaceNumber
                       6,                      // iInterface (string idx)
                       HID_ITF_PROTOCOL_NONE,  // Non boot
                       sizeof(desc_hid_consumer_report),  // Mouse HID size
                       ENDPOINT_IN_ADDR(ITF_CONSUMER),    // Endpoint address
                       CFG_TUD_HID_EP_BUFSIZE,  // Endpoint buffer size
                       CONFIG_USB_POLL_MS),     // Pulling interval

#if CONFIG_DEBUG_ENABLE_USB_SERIAL
    TUD_CDC_DESCRIPTOR(ITF_CDC_CTRL,  // bInterfaceNumber
                       7,             // iInterface (string idx)
                       ENDPOINT_IN_ADDR(ITF_CDC_CTRL),  // Notification endpoint
                       CONFIG_DEBUG_USB_SERIAL_CDC_CMD_MAX_SIZE,  // Buffer size
                       ENDPOINT_OUT_ADDR(ITF_CDC_DATA),  // Avoid conflict
                       ENDPOINT_IN_ADDR(ITF_CDC_DATA),   //
                       CONFIG_DEBUG_USB_BUFFER_SIZE),
#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */
};

char const *string_desc_arr[] = {
    "",                       // 0: Index 0 is language ID. Handled in callback
    CONFIG_USB_VENDER_NAME,   // 1: Manufacturer
    CONFIG_USB_PRODUCT_NAME,  // 2: Product
    CONFIG_USB_SERIAL,        // 3: Serial number
    "Keyboard",               // 4: Keyboard interface
    "Mouse",                  // 5: Mouse interface
    "Consumer",               // 6: Consumer interface
    "Serial",                 // 7: CDC interface
};

////////////////////////////////////////////////////////////////////////////////
// USB callbacks
////////////////////////////////////////////////////////////////////////////////

extern "C" {
// Descriptor callbacks

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  switch (instance) {
    case ITF_KEYBOARD:
      return desc_hid_keyboard_report;
    case ITF_MOUSE:
      return desc_hid_mouse_report;
    case ITF_CONSUMER:
      return desc_hid_consumer_report;
    default:
      // Shouldn't reach here, unless something is horribly wrong.
      return NULL;
  }
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;  // We only have one configuration
  return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;  // We only support English

  if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
    return NULL;

  static uint16_t buffer[32];
  uint8_t str_len;

  if (index == 0) {
    buffer[1] = 0x0409;  // English only
    str_len = 1;
  } else {
    const char *str = string_desc_arr[index];

    str_len = strlen(str);
    if (str_len > 31) {
      str_len = 31;
    }

    // Convert ASCII string into UTF-16
    for (uint8_t i = 0; i < str_len; i++) {
      buffer[i + 1] = str[i];
    }
  }

  buffer[0] = (TUSB_DESC_STRING << 8) | (str_len + 1) * 2;

  return buffer;
}
}

// Request callbacks

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                          hid_report_type_t report_type,
                                          uint8_t *buffer, uint16_t reqlen) {
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t const *buffer, uint16_t bufsize) {
}

extern "C" void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {}

extern "C" bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate) {
  return false;
}

extern "C" void tud_hid_report_complete_cb(uint8_t instance,
                                           uint8_t const *report, uint8_t len) {
}

extern "C" void tud_mount_cb(void) {}

extern "C" void tud_umount_cb(void) {}

extern "C" void tud_suspend_cb(bool remote_wakeup_en) {}

extern "C" void tud_resume_cb(void) {}

extern "C" void USBTask(void *parameter);

// Semaphore for data accessed between USB task and other tasks. Should not be
// used between callbacks.
static SemaphoreHandle_t semaphore = NULL;

#if CONFIG_DEBUG_ENABLE_USB_SERIAL

extern "C" {
static void stdio_usb_out_chars(const char *buf, int length) {
  static uint64_t last_avail_time;

  LockSemaphore lock(semaphore);

  if (tud_cdc_connected()) {
    for (int i = 0; i < length;) {
      int n = length - i;
      int avail = (int)tud_cdc_write_available();
      if (n > avail) n = avail;
      if (n) {
        int n2 = (int)tud_cdc_write(buf + i, (uint32_t)n);
        tud_cdc_write_flush();
        i += n2;
        last_avail_time = time_us_64();
      } else {
        tud_cdc_write_flush();
        if (!tud_cdc_connected() ||
            (!tud_cdc_write_available() &&
             time_us_64() > last_avail_time + CONFIG_DEBUG_USB_TIMEOUT_US)) {
          break;
        }
      }
    }
  } else {
    // reset our timeout
    last_avail_time = 0;
  }
}

stdio_driver_t stdio_usb = {
    .out_chars = stdio_usb_out_chars,
    .in_chars = NULL,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF,
#endif
};
}

#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

status USBInit() {
  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);
  return OK;
}

static TaskHandle_t usb_task_handle = NULL;

status StartUSBTask() {
  // BaseType_t status = xTaskCreate(&USBTask, "usb_task", CONFIG_TASK_STACK_SIZE,
  //                                 NULL, CONFIG_TASK_PRIORITY, &usb_task_handle);
  BaseType_t status = xTaskCreateAffinitySet(
      &USBTask, "usb_task", CONFIG_TASK_STACK_SIZE, NULL,
      CONFIG_TASK_PRIORITY, (1 << 1), &usb_task_handle);
  if (status != pdPASS || usb_task_handle == NULL) {
    return ERROR;
  }
  return OK;
}

extern "C" void USBTask(void *parameter) {
  (void)parameter;

  tusb_init();

#if CONFIG_DEBUG_ENABLE_USB_SERIAL
  stdio_set_driver_enabled(&stdio_usb, true);
#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

  while (true) {
    tud_task();
  }
}

USBOutputAddIn::USBOutputAddIn() {
  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);
}

void USBOutputAddIn::SetIdle(uint8_t idle_rate) {
  LockSemaphore lock(semaphore_);
  idle_rate_ = idle_rate;
}

void USBOutputAddIn::SetBoot(bool is_boot_protocol) {
  LockSemaphore lock(semaphore_);
  is_boot_protocol_ = is_boot_protocol;
}

std::shared_ptr<USBKeyboardOutput> USBKeyboardOutput::GetUSBKeyboardOutput() {
  static std::shared_ptr<USBKeyboardOutput> singleton = NULL;
  if (singleton == NULL) {
    singleton = std::shared_ptr<USBKeyboardOutput>(new USBKeyboardOutput());
  }
  return singleton;
}

void USBKeyboardOutput::OutputTick() {
  LockSemaphore lock(semaphore_);
  if (is_config_mode_) {
    // Don't report key strokes to host if in config mode
    return;
  }
  if (!tud_hid_n_ready(ITF_KEYBOARD)) {
    return;
  }
  if (tud_suspended() && has_key_output_) {
    tud_remote_wakeup();
  }
  auto &buffer = double_buffer_[active_buffer_];
  tud_hid_n_report(ITF_KEYBOARD, /*report_id=*/0, buffer.data(), buffer.size());
  tud_hid_n_report(ITF_CONSUMER, /*report_id=*/0, &consumer_keycode_, 2);
}

void USBKeyboardOutput::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  is_config_mode_ = is_config_mode;
}

void USBKeyboardOutput::StartOfInputTick() {
  // No need to lock as Tick() does not modify reads active_buffer_.
  const uint8_t buf_idx = (active_buffer_ + 1) % 2;
  std::fill(double_buffer_[buf_idx].begin(), double_buffer_[buf_idx].end(), 0);
  LockSemaphore lock(semaphore_);
  consumer_keycode_ = 0;
}

void USBKeyboardOutput::FinalizeInputTickOutput() {
  LockSemaphore lock(semaphore_);
  active_buffer_ = (active_buffer_ + 1) % 2;
  has_key_output_ = boot_protocol_kc_count_ > 0;
  boot_protocol_kc_count_ = 0;
}

void USBKeyboardOutput::SendKeycode(uint8_t keycode) {
  auto &buffer = double_buffer_[(active_buffer_ + 1) % 2];
  buffer[keycode / 8 + 8] |= (1 << (keycode % 8));
  if (boot_protocol_kc_count_ < 6) {
    buffer[2 + (boot_protocol_kc_count_)++] = keycode;
  } else if (buffer[2] != 0x01) {
    for (size_t i = 2; i < 8; ++i) {
      buffer[i] = 0x01;  // ErrorRollOver
    }
  }
}

void USBKeyboardOutput::SendKeycode(const std::vector<uint8_t> &keycode) {
  for (auto code : keycode) {
    SendKeycode(code);
  }
}

void USBKeyboardOutput::SendConsumerKeycode(uint16_t keycode) {
  LockSemaphore lock(semaphore_);
  consumer_keycode_ = keycode;
}

std::shared_ptr<USBMouseOutput> USBMouseOutput::GetUSBMouseOutput() {
  static std::shared_ptr<USBMouseOutput> singleton = NULL;
  if (singleton == NULL) {
    singleton = std::shared_ptr<USBMouseOutput>(new USBMouseOutput());
  }
  return singleton;
}

USBKeyboardOutput::USBKeyboardOutput()
    : USBOutputAddIn(),
      active_buffer_(0),
      boot_protocol_kc_count_(0),
      is_config_mode_(false),
      has_key_output_(false) {}

void USBMouseOutput::OutputTick() {
  LockSemaphore lock(semaphore_);
  if (is_config_mode_) {
    // Don't report key strokes to host if in config mode
    return;
  }
  if (!tud_hid_n_ready(ITF_MOUSE)) {
    return;
  }
  auto &buffer = double_buffer_[active_buffer_];
  tud_hid_n_mouse_report(ITF_MOUSE, /*report_id=*/0, buffer[0], buffer[1],
                         buffer[2], buffer[3], buffer[4]);
}

void USBMouseOutput::SetConfigMode(bool is_config_mode) {
  LockSemaphore lock(semaphore_);
  is_config_mode_ = is_config_mode;
}

void USBMouseOutput::StartOfInputTick() {
  const uint8_t buf_idx = (active_buffer_ + 1) % 2;
  std::fill(double_buffer_[buf_idx].begin(), double_buffer_[buf_idx].end(), 0);
}

void USBMouseOutput::FinalizeInputTickOutput() {
  LockSemaphore lock(semaphore_);
  active_buffer_ = (active_buffer_ + 1) % 2;
}

void USBMouseOutput::MouseKeycode(uint8_t keycode) {
  if (keycode > MSE_FORWARD) {
    return;
  }

  double_buffer_[(active_buffer_ + 1) % 2][0] |= (1 << keycode);
}

void USBMouseOutput::MouseMovement(int8_t x, int8_t y) {
  double_buffer_[(active_buffer_ + 1) % 2][1] = x;
  double_buffer_[(active_buffer_ + 1) % 2][2] = y;
}

void USBMouseOutput::Pan(int8_t x, int8_t y) {
  double_buffer_[(active_buffer_ + 1) % 2][3] = x;
  double_buffer_[(active_buffer_ + 1) % 2][4] = y;
}

USBMouseOutput::USBMouseOutput()
    : USBOutputAddIn(), active_buffer_(0), is_config_mode_(false) {}

// Registration

static Status usb_keyboard_out = DeviceRegistry::RegisterKeyboardOutputDevice(
    2, false, []() -> std::shared_ptr<USBKeyboardOutput> {
      return USBKeyboardOutput::GetUSBKeyboardOutput();
    });
static Status usb_mouse_out = DeviceRegistry::RegisterMouseOutputDevice(
    2, false, []() -> std::shared_ptr<USBMouseOutput> {
      return USBMouseOutput::GetUSBMouseOutput();
    });
