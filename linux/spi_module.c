#include <asm/io.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "../ibp_lib.h"
#include "keyboard.h"
#include "mouse.h"

#define INTERVAL_MS 10
#define BUF_SIZE 132

static struct timer_list timer;
static bool timer_initialized = false;
static spinlock_t timer_initialized_lock;

static struct spi_device *spi_dev = NULL;
static uint8_t *buffer = NULL;
static struct spi_transfer *xs = NULL;

static void SPIPullFn(struct work_struct *work) {
  if (spi_dev == NULL || buffer == NULL || xs == NULL) {
    goto bad;
  }

  IBPSegment segment;
  int8_t bytes;

  memset(buffer, 0, BUF_SIZE);

  // Right now there's no auto-type so output buffer always has zero segment.
  bytes = SerializeSegments(&segment, /*num_segments=*/0, buffer, BUF_SIZE);
  if (bytes < 0) {
    printk(KERN_WARNING "Failed create output buffer");
    goto bad;
  }

  for (size_t i = 0; i < bytes; ++i) {
    memset(&xs[i], 0, sizeof(xs[i]));
    xs[i].len = 1;
    xs[i].tx_buf = &buffer[i];
    xs[i].rx_buf = &buffer[32];
    xs[i].cs_change = true;
    xs[i].delay.value = 0;
    xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
    xs[i].cs_change_delay.value = 0;
    xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
    xs[i].word_delay.value = 0;
    xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
  }
  xs[bytes - 1].cs_change = false;
  if (spi_sync_transfer(spi_dev, xs, bytes) < 0) {
    printk(KERN_WARNING "Failed to write");
    goto bad;
  }

  // Read 4 bytes to hopefully include the header if it's missed in the first
  // one.

  bytes = 4;
  memset(buffer, 0, BUF_SIZE);
  memset(xs, 0, sizeof(xs[0]) * bytes);
  for (size_t i = 0; i < bytes; ++i) {
    xs[i].len = 1;
    xs[i].tx_buf = &buffer[32];
    xs[i].rx_buf = &buffer[i];
    xs[i].cs_change = true;
    xs[i].delay.value = 0;
    xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
    xs[i].cs_change_delay.value = 0;
    xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
    xs[i].word_delay.value = 0;
    xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
  }
  xs[bytes - 1].cs_change = false;
  if (spi_sync_transfer(spi_dev, xs, bytes) < 0) {
    printk(KERN_WARNING "Failed to read");
    goto bad;
  }

  size_t write_offset = 0;
  bool found_header = false;
  for (size_t i = 0; i < bytes; ++i) {
    if (buffer[i] != 0 || found_header) {
      buffer[write_offset++] = buffer[i];
      found_header = true;
    }
  }

  bytes = GetTransactionTotalSize(buffer[0]);
  if (bytes < 0) {
    // Invalid response header.
    goto bad;
  }
  bytes -= write_offset;
  if (bytes > 0) {
    memset(xs, 0, sizeof(xs[0]) * bytes);
    for (size_t i = 0; i < bytes; ++i) {
      xs[i].len = 1;
      xs[i].tx_buf = &buffer[BUF_SIZE - 1];
      xs[i].rx_buf = &buffer[i + write_offset];
      xs[i].cs_change = true;
      xs[i].delay.value = 0;
      xs[i].delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].cs_change_delay.value = 0;
      xs[i].cs_change_delay.unit = SPI_DELAY_UNIT_SCK;
      xs[i].word_delay.value = 0;
      xs[i].word_delay.unit = SPI_DELAY_UNIT_SCK;
    }
    xs[bytes - 1].cs_change = false;
    if (spi_sync_transfer(spi_dev, xs, bytes) < 0) {
      printk(KERN_WARNING "Failed to receive remaining response.");
      goto bad;
    }
  }

  bytes += write_offset - 1;
  uint8_t *curr_buf = buffer + 1;
  int8_t consumed_bytes = DeSerializeSegment(curr_buf, bytes, &segment);
  if (consumed_bytes < 0) {
    goto bad;
  }
  while (consumed_bytes > 0) {
    if (consumed_bytes > 0) {
      switch (segment.field_type) {
        case IBP_KEYCODE: {
          OnNewKeycodes(&segment.field_data.keycodes);
          break;
        }
        default:
          break;
      }
    }
    bytes -= consumed_bytes;
    memset(&segment, 0, sizeof(segment));
    consumed_bytes = DeSerializeSegment(curr_buf, bytes, &segment);
  }
  // do {
  //   memset(&segment, 0, sizeof(segment));
  //   bytes -= consumed_bytes;
  //   consumed_bytes = ;
  //   if (consumed_bytes > 0) {
  //     switch (segment.field_type) {
  //       case IBP_KEYCODE: {
  //         printk(KERN_ALERT "DEBUG.");
  //         OnNewKeycodes(&segment.field_data.keycodes);
  //         break;
  //       }
  //       default:
  //         break;
  //     }
  //   }
  // } while (consumed_bytes > 0);

  IBPKeyCodes empty_keys;
  mod_timer(&timer, jiffies + msecs_to_jiffies(INTERVAL_MS));
  return;

bad:
  // Clear keycode
  memset(&empty_keys, 0, sizeof(IBPKeyCodes));
  OnNewKeycodes(&empty_keys);
  mod_timer(&timer, jiffies + msecs_to_jiffies(INTERVAL_MS));
}

DECLARE_WORK(workqueue, SPIPullFn);

static void TimerCallback(struct timer_list *data) {
  schedule_work(&workqueue);
}

static int SetupTimer(void) {
  spin_lock(&timer_initialized_lock);
  if (timer_initialized) {
    goto done;
  }
  printk(KERN_ALERT "Setup timer");
  timer_setup(&timer, TimerCallback, 0);
  mod_timer(&timer, jiffies + msecs_to_jiffies(INTERVAL_MS));
  timer_initialized = true;
done:
  spin_unlock(&timer_initialized_lock);
  return 0;
}

static void DeleteTimer(void) {
  spin_lock(&timer_initialized_lock);
  if (!timer_initialized) {
    goto done;
  }
  printk(KERN_ALERT "Delete timer");
  del_timer_sync(&timer);
  timer_initialized = false;
done:
  spin_unlock(&timer_initialized_lock);
}

static const struct of_device_id picomk_of_match[] = {
    {
        .compatible = "picomk,spi_board_v0",
        .data = 0,
    },
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, picomk_of_match);

static const struct spi_device_id picomk_spi_table[] = {{"spi_board_v0", 0},
                                                        {/* sentinel */}};
MODULE_DEVICE_TABLE(spi, picomk_spi_table);

static void picomk_spi_remove(struct spi_device *spi) {
  printk(KERN_ALERT "SPI remove.");
  DeleteTimer();
  DestroyKeyboardDevice();
  if (spi_dev) {
    spi_dev = NULL;
  }
  if (buffer) {
    kfree(buffer);
    buffer = NULL;
  }
  if (xs) {
    kfree(xs);
    xs = NULL;
  }
}

static void picomk_spi_shutdown(struct spi_device *spi) {
  picomk_spi_remove(spi);
}

static int picomk_spi_probe(struct spi_device *spi) {
  printk(KERN_ALERT "PicoMK SPI IBP driver inserted.");
  spi_dev = spi;
  spi_dev->bits_per_word = 8;
  spi_dev->max_speed_hz = 1000000;
  spi_dev->mode = 0;

  if (spi_setup(spi_dev)) {
    printk(KERN_ERR "Failed to update word size.");
    return -ENODEV;
  }

  int error;
  buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
  xs = kmalloc(sizeof(struct spi_transfer) * BUF_SIZE, GFP_KERNEL);
  if (!buffer || !xs) {
    printk(KERN_ERR "Failed to allocate buffer");
    error = -ENOMEM;
    goto bad;
  }

  spin_lock_init(&timer_initialized_lock);

  error = CreateKeyboardDevice(NULL, NULL);
  if (error) {
    goto bad;
  }

  SetupTimer();

  return 0;

bad:
  if (buffer) {
    kfree(buffer);
  }
  if (xs) {
    kfree(xs);
  }
  buffer = NULL;
  xs = NULL;
  spi_dev = NULL;
  return error;
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

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("NoSegfault");
MODULE_DESCRIPTION("SPI Inter-Board Protocol (IBP) Driver for PicoMK.");
MODULE_VERSION("0.0.1");
