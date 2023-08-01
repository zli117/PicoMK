#include <asm/io.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "../ibp_lib.h"

#define INTERVAL_MS 800

static struct input_dev *button_dev;
static struct timer_list timer;
static struct spi_device *spi_device;

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

    if (!buf) {
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
      printk(KERN_ALERT "Failed to write");
      goto bad;
    }

    size_t write_offset = 0;
    bool found_header = false;
    for (size_t i = 0; i < bytes; ++i) {
      printk(KERN_ALERT "READ 0x%x", buf[i]);
      if (buf[i] != 0 || found_header) {
        buf[write_offset++] = buf[i];
        found_header = true;
      }
    }

    // // Now read the first byte of the response packet
    // memset(buf, 0, BUF_SIZE);
    // memset(&xs[0], 0, sizeof(xs[0]));
    // xs[0].len = 1;
    // xs[0].tx_buf = &buf[10];
    // xs[0].rx_buf = &buf[0];
    // if (spi_sync_transfer(spi_device, &xs[0], 1) < 0) {
    //   printk(KERN_ALERT "Failed to write");
    //   goto bad;
    // }
    bytes = GetTransactionTotalSize(buf[0]);
    if (bytes < 0) {
      printk(KERN_ALERT "Invalid response packet header %x", buf[0]);
      goto bad;
    }
    bytes -= write_offset;
    if (bytes > 0) {
      for (size_t i = 0; i < bytes; ++i) {
        memset(&xs[i], 0, sizeof(xs[i]));
        xs[i].len = 1;
        xs[i].rx_buf = &buf[i + write_offset];
        xs[i].tx_buf = &buf[BUF_SIZE - 1];
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

    for (size_t i = 0; i < bytes + write_offset; ++i) {
      printk(KERN_ALERT "Read 0x%x", buf[i]);
    }

    curr_buf = buf + 1;
    consumed_bytes = 0;
    do {
      memset(&segment, 0, sizeof(segment));
      bytes -= consumed_bytes;
      consumed_bytes = DeSerializeSegment(curr_buf, bytes, &segment);
      if (segment.field_type == IBP_KEYCODE) {
        input_report_key(button_dev, KEY_1, segment.field_data.keycodes.keycodes[0] == 0x1e);
        input_sync(button_dev);
      }
    } while (consumed_bytes > 0);

    /*
        /////////////

        buf[0] = 5;
        buf[1] = 0x1b;
        buf[2] = 0x2b;
        buf[3] = 0x3b;
        buf[4] = 0x4b;

        struct spi_transfer xs[32];
        struct spi_transfer x;

        for (size_t i = 0; i < 5; ++i) {
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
        if (spi_sync_transfer(spi_device, xs, 5) < 0) {
          printk(KERN_ALERT "Failed to write");
          goto bad;
        }

        uint8_t *read_buf = buf + 32;
        read_buf[0] = 0;

        for (size_t i = 0; i < 64; ++i) {
          memset(&x, 0, sizeof(x));
          x.len = 1;
          x.tx_buf = &buf[10];
          x.rx_buf = &read_buf[0];
          if (spi_sync_transfer(spi_device, &x, 1) < 0) {
            printk(KERN_ALERT "Failed to write");
            goto bad;
          }
          if (read_buf[0]) {
            break;
          }
        }
        if (read_buf[0] > 16) {
          read_buf[0] = 16;
        }
        for (size_t i = 0; i < read_buf[0]; ++i) {
          memset(&xs[i], 0, sizeof(xs[i]));
          xs[i].len = 1;
          xs[i].tx_buf = &buf[10];
          xs[i].rx_buf = &read_buf[i + 1];
          xs[i].cs_change = true;
          xs[i].delay.value = 0;
          xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
          xs[i].cs_change_delay.value = 0;
          xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
          xs[i].word_delay.value = 0;
          xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
        }
        xs[read_buf[0] - 1].cs_change = false;
        if (spi_sync_transfer(spi_device, xs, read_buf[0]) < 0) {
          printk(KERN_ALERT "Failed to write");
          goto bad;
        }

    #if INTERVAL_MS > 800
        printk(KERN_ALERT "NEW MESSAGE");
        for (size_t i = 0; i < read_buf[0] && i < 16; ++i) {
          printk(KERN_ALERT "Read value %02x", read_buf[i]);
        }
    #endif

        if (read_buf[2] == 0x2c) {
          input_report_key(button_dev, KEY_1, (int)read_buf[1] > 0);
          input_sync(button_dev);
        } else {
    #if INTERVAL_MS > 800
          printk(KERN_ALERT "Pico says magic number is not right");
    #endif
        }

    */
  bad:
    kfree(buf);
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

// module_init(button_init);
// module_exit(button_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A");
MODULE_DESCRIPTION("B");
MODULE_VERSION("C");
