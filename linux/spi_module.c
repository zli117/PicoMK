#include <asm/io.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "../ibp_lib.h"

#define INTERVAL_MS 10

// Copied from drivers/hid/usbhid/usbkbd.c
static const unsigned char usb_kbd_keycode[256] = {
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

static struct input_dev *button_dev;
static struct timer_list timer;
static struct spi_device *spi_device;

static bool pressed;

#define BUF_SIZE 256

static void spi_keyboard_fn(struct work_struct *work) {
  if (spi_device) {
    uint8_t *buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    uint8_t *curr_buf;
    struct spi_transfer *xs =
        kmalloc(sizeof(struct spi_transfer) * 128, GFP_KERNEL);
    IBPSegment segment;
    int8_t bytes;
    int8_t consumed_bytes;

    if (!buf || !xs) {
      printk(KERN_ALERT "Failed to allocate buffer");
      return;
    }

    memset(buf, 0, BUF_SIZE);
    bytes = SerializeSegments(&segment, 0, buf, 128);
    if (bytes < 0) {
      printk(KERN_ALERT "Failed create output buffer");
      goto bad;
    }

    for (size_t i = 0; i < bytes; ++i) {
      memset(&xs[i], 0, sizeof(xs[i]));
      xs[i].len = 1;
      xs[i].tx_buf = &buf[i];
      xs[i].rx_buf = &buf[32];
      xs[i].cs_change = true;
      xs[i].delay.value = 0;
      xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].cs_change_delay.value = 0;
      xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].word_delay.value = 0;
      xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
    }
    xs[bytes - 1].cs_change = false;
    if (spi_sync_transfer(spi_device, xs, bytes) < 0) {
      printk(KERN_ALERT "Failed to write");
      goto bad;
    }

    // Read 4 bytes to hopefully include the header if it's missed in the first
    // one.

    memset(buf, 0, BUF_SIZE);
    bytes = 4;
    for (size_t i = 0; i < bytes; ++i) {
      memset(&xs[i], 0, sizeof(xs[i]));
      xs[i].len = 1;
      xs[i].tx_buf = &buf[32];
      xs[i].rx_buf = &buf[i];
      xs[i].cs_change = true;
      xs[i].delay.value = 0;
      xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].cs_change_delay.value = 0;
      xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].word_delay.value = 0;
      xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
    }
    xs[bytes - 1].cs_change = false;
    if (spi_sync_transfer(spi_device, xs, bytes) < 0) {
      printk(KERN_ALERT "Failed to read");
      goto bad;
    }

    size_t write_offset = 0;
    bool found_header = false;
    for (size_t i = 0; i < bytes; ++i) {
      if (buf[i] != 0 || found_header) {
        buf[write_offset++] = buf[i];
        found_header = true;
      }
    }

    bytes = GetTransactionTotalSize(buf[0]);
    if (bytes < 0) {
      printk(KERN_ALERT "Invalid response packet header %x", buf[0]);
      goto bad;
    }
    bytes -= write_offset;
    if (bytes > 0) {
      memset(xs, 0, sizeof(xs[0]) * bytes);
      for (size_t i = 0; i < bytes; ++i) {
        // memset(&xs[i], 0, sizeof(xs[i]));
        xs[i].len = 1;
        xs[i].tx_buf = &buf[BUF_SIZE - 1];
        xs[i].rx_buf = &buf[i + write_offset];
        xs[i].cs_change = true;
        xs[i].delay.value = 0;
        xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
        xs[i].cs_change_delay.value = 0;
        xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
        xs[i].word_delay.value = 0;
        xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
      }
      xs[bytes - 1].cs_change = false;
      if (spi_sync_transfer(spi_device, xs, bytes) < 0) {
        printk(KERN_ALERT "Failed to transmit");
        goto bad;
      }
    }

    bytes += write_offset;
    curr_buf = buf + 1;
    consumed_bytes = 0;
    bool has_key = false;
    do {
      memset(&segment, 0, sizeof(segment));
      bytes -= consumed_bytes;
      consumed_bytes = DeSerializeSegment(curr_buf, bytes, &segment);
      // printk("Segment type: %d, consumed_bytes: %d", segment.field_type,
      //        consumed_bytes);
      if (consumed_bytes > 0) {
        if (segment.field_type == IBP_KEYCODE) {
          has_key = true;
          if (pressed && segment.field_data.keycodes.keycodes[0] != 0x1e) {
            pressed = false;
            input_report_key(button_dev, KEY_1, 0);
          } else if (!pressed &&
                     segment.field_data.keycodes.keycodes[0] == 0x1e) {
            input_report_key(button_dev, KEY_1, 1);
            pressed = true;
          }
          input_sync(button_dev);
        }
      } else {
        if (!has_key && pressed) {
          pressed = false;
          input_report_key(button_dev, KEY_1, 0);
          input_sync(button_dev);
        }
      }
    } while (consumed_bytes > 0);

  bad:
    kfree(buf);
    kfree(xs);
  }
  mod_timer(&timer, jiffies + msecs_to_jiffies(INTERVAL_MS));
}

DECLARE_WORK(workqueue, spi_keyboard_fn);

static void timer_callback(struct timer_list *data) {
  schedule_work(&workqueue);
}

static int open(struct input_dev *dev) {
  timer_setup(&timer, timer_callback, 0);
  mod_timer(&timer, jiffies + msecs_to_jiffies(INTERVAL_MS));
  pressed = false;
  return 0;
}

static void close(struct input_dev *dev) { del_timer(&timer); }

static int /*__init*/ button_init(void) {
  int error;

  button_dev = input_allocate_device();
  if (!button_dev) {
    printk(KERN_ERR "button.c: Not enough memory\n");
    error = -ENOMEM;
    goto err_free_irq;
  }

  button_dev->evbit[0] = BIT_MASK(EV_KEY);
  button_dev->keybit[BIT_WORD(KEY_1)] = BIT_MASK(KEY_1);
  button_dev->open = open;
  button_dev->close = close;
  button_dev->name = "Test Keyboard";

  error = input_register_device(button_dev);
  if (error) {
    printk(KERN_ERR "button.c: Failed to register device\n");
    goto err_free_dev;
  }

  printk(KERN_ALERT "Button module inserted");

  return 0;

err_free_dev:
  input_free_device(button_dev);
err_free_irq:
  return error;
}

static void /*__exit*/ button_exit(void) {
  del_timer(&timer);
  input_unregister_device(button_dev);
}

static const struct of_device_id picomk_of_match[] = {
    {
        .compatible = "picomk,spi_board_v0",
        .data = 0,
    },
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, picomk_of_match);

// static const struct spi_device_id picomk_spi_table[] = {
//     {"picomk,spi_board_v0", 0}, {/* sentinel */}};
static const struct spi_device_id picomk_spi_table[] = {{"spi_board_v0", 0},
                                                        {/* sentinel */}};
MODULE_DEVICE_TABLE(spi, picomk_spi_table);

static void picomk_spi_remove(struct spi_device *spi) {
  if (spi_device) {
    button_exit();
    spi_device = NULL;
  }
}

static void picomk_spi_shutdown(struct spi_device *spi) {
  if (spi_device) {
    button_exit();
    spi_device = NULL;
  }
}

static int picomk_spi_probe(struct spi_device *spi) {
  printk(KERN_ALERT "SPI probe called.");
  spi_device = spi;
  spi_device->bits_per_word = 8;
  spi_device->max_speed_hz = 1000000;
  spi_device->mode = 0;

  if (spi_setup(spi_device)) {
    printk(KERN_ALERT "Failed to update word size.");
    return -ENODEV;
  }
  return button_init();
}

static struct spi_driver picomk_spi_driver = {
    .id_table = picomk_spi_table,
    .driver =
        {
            .name = "PicoMK",
            .of_match_table = picomk_of_match,
        },
    .probe = picomk_spi_probe,
    .remove = picomk_spi_remove,
    .shutdown = picomk_spi_shutdown,
};
module_spi_driver(picomk_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A");
MODULE_DESCRIPTION("B");
MODULE_VERSION("C");
