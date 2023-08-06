#include "keyboard.h"

#include <linux/input.h>

// Copied from drivers/hid/usbhid/usbkbd.c
static const uint8_t usb_kbd_keycode[256] = {
    0,   0,   0,   0,   30,  48,  46,  32,  18,  33,  34,  35,  23,  36,  37,
    38,  50,  49,  24,  25,  16,  19,  31,  20,  22,  47,  17,  45,  21,  44,
    2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  28,  1,   14,  15,  57,
    12,  13,  26,  27,  43,  43,  39,  40,  41,  51,  52,  53,  58,  59,  60,
    61,  62,  63,  64,  65,  66,  67,  68,  87,  88,  99,  70,  119, 110, 102,
    104, 111, 107, 109, 106, 105, 108, 103, 69,  98,  55,  74,  78,  96,  79,
    80,  81,  75,  76,  77,  71,  72,  73,  82,  83,  86,  127, 116, 117, 183,
    184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 134, 138, 130, 132,
    128, 129, 131, 137, 133, 135, 136, 113, 115, 114, 0,   0,   0,   121, 0,
    89,  93,  124, 92,  94,  95,  0,   0,   0,   122, 123, 90,  91,  85,  0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   29,
    42,  56,  125, 97,  54,  100, 126, 164, 166, 165, 163, 161, 115, 114, 113,
    150, 158, 159, 128, 136, 177, 178, 176, 142, 152, 173, 140};

struct KeyboardDevice {
  struct input_dev* dev;
  IBPKeyCodes old_keys;
  IBPConsumer old_consumer_keys;
  int (*open)(void);
  void (*close)(void);
};

static struct KeyboardDevice keyboard_device = {0};
static spinlock_t keyboard_device_lock;

static int keyboard_open(struct input_dev* dev) {
  if (keyboard_device.open != NULL) {
    return keyboard_device.open();
  }
  return 0;
}

static void keyboard_close(struct input_dev* dev) {
  if (keyboard_device.close) {
    keyboard_device.close();
  }
}

int CreateKeyboardDevice(int (*open)(void), void (*close)(void)) {
  int error;

  if (keyboard_device.dev != NULL) {
    error = -EEXIST;
    goto err;
  }

  keyboard_device.dev = input_allocate_device();
  if (!keyboard_device.dev) {
    printk(KERN_ERR "%s:%d Not enough memory\n", __FILE__, __LINE__);
    error = -ENOMEM;
    goto err;
  }

  keyboard_device.dev->evbit[0] = BIT_MASK(EV_KEY);
  keyboard_device.dev->open = keyboard_open;
  keyboard_device.dev->close = keyboard_close;
  keyboard_device.dev->name = "PicoMK Keyboard";
  keyboard_device.open = open;
  keyboard_device.close = close;
  for (int i = 0; i < 255; i++) {
    set_bit(usb_kbd_keycode[i], keyboard_device.dev->keybit);
  }
  clear_bit(0, keyboard_device.dev->keybit);

  error = input_register_device(keyboard_device.dev);
  if (error) {
    printk(KERN_ERR "%s:%d Failed to register device\n", __FILE__, __LINE__);
    goto err_free_dev;
  }

  memset(&keyboard_device.old_keys, 0, sizeof(IBPKeyCodes));
  memset(&keyboard_device.old_consumer_keys, 0, sizeof(IBPConsumer));

  spin_lock_init(&keyboard_device_lock);

  return 0;

err_free_dev:
  input_free_device(keyboard_device.dev);
  keyboard_device.dev = NULL;
  keyboard_device.open = NULL;
  keyboard_device.close = NULL;
err:
  return error;
}

int DestroyKeyboardDevice(void) {
  spin_lock(&keyboard_device_lock);
  if (keyboard_device.dev) {
    input_unregister_device(keyboard_device.dev);
    input_free_device(keyboard_device.dev);
    keyboard_device.dev = NULL;
  }
  keyboard_device.open = NULL;
  keyboard_device.close = NULL;
  spin_unlock(&keyboard_device_lock);
  return 0;
}

int OnNewKeycodes(IBPKeyCodes* keys) {
  spin_lock(&keyboard_device_lock);
  if (keyboard_device.dev == NULL) {
    spin_unlock(&keyboard_device_lock);
    return 0;
  }
  spin_unlock(&keyboard_device_lock);

  for (int i = 0; i < 8; i++) {
    input_report_key(keyboard_device.dev, usb_kbd_keycode[i + 224],
                     (keys->modifier_bitmask >> i) & 1);
  }
  for (int i = 0; i < keyboard_device.old_keys.num_keycodes; ++i) {
    if (memscan(keys->keycodes, keyboard_device.old_keys.keycodes[i],
                keys->num_keycodes) == keys->keycodes + keys->num_keycodes) {
      const uint8_t keycode =
          usb_kbd_keycode[keyboard_device.old_keys.keycodes[i]];
      if (keycode) {
        input_report_key(keyboard_device.dev, keycode, 0);
      }
    }
  }
  for (int i = 0; i < keys->num_keycodes; ++i) {
    if (memscan(keyboard_device.old_keys.keycodes, keys->keycodes[i],
                keyboard_device.old_keys.num_keycodes) ==
        keyboard_device.old_keys.keycodes +
            keyboard_device.old_keys.num_keycodes) {
      const uint8_t keycode = usb_kbd_keycode[keys->keycodes[i]];
      if (keycode) {
        input_report_key(keyboard_device.dev, keycode, 1);
      }
    }
  }
  input_sync(keyboard_device.dev);
  keyboard_device.old_keys = *keys;
  return 0;
}

int OnNewConsumerKeycodes(IBPConsumer* keys) { return 0; }
