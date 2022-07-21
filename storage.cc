#include "storage.h"

#include <stdint.h>

#include <memory>

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "semphr.h"
#include "sync.h"

extern "C" {
#include "littlefs/lfs.h"

static SemaphoreHandle_t __not_in_flash("storage") semaphore;
static lfs_t __not_in_flash("storage") lfs;

#define FS_OFFSET (PICO_FLASH_SIZE_BYTES - CONFIG_FLASH_FILESYSTEM_SIZE)

static int read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                void* buffer, lfs_size_t size);
static int prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                const void* buffer, lfs_size_t size);
static int erase(const struct lfs_config* c, lfs_block_t block);
static int sync(const struct lfs_config* c);
static int lock(const struct lfs_config* c);
static int unlock(const struct lfs_config* c);

void failure(const char*) {}

// Compile time validation
constexpr struct lfs_config CreateLFSConfig() {
  if ((CONFIG_FLASH_FILESYSTEM_SIZE) % (FLASH_SECTOR_SIZE) != 0) {
    failure("Filesystem size has to be a multiple of FLASH_SECTOR_SIZE");
  }
  if ((CONFIG_FLASH_FILESYSTEM_SIZE) > PICO_FLASH_SIZE_BYTES) {
    failure("Filesystem is larger than PICO_FLASH_SIZE_BYTES");
  }

  const struct lfs_config cfg = {
      // block device operations
      .read = read,
      .prog = prog,
      .erase = erase,
      .sync = sync,

      // block device configuration
      .read_size = 64,
      .prog_size = FLASH_PAGE_SIZE,
      .block_size = FLASH_SECTOR_SIZE,
      .block_count = CONFIG_FLASH_FILESYSTEM_SIZE / FLASH_SECTOR_SIZE,
      .block_cycles = 500,
      .cache_size = FLASH_SECTOR_SIZE / 4,
      .lookahead_size = 32,
  };
  return cfg;
}

constexpr struct lfs_config __not_in_flash("storage1")
    kLFSConfig = CreateLFSConfig();

static int read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                void* buffer, lfs_size_t size) {
  memcpy(buffer,
         (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE) + FS_OFFSET +
             (block * c->block_size) + off,
         size);
  return LFS_ERR_OK;
}

static int __no_inline_not_in_flash_func(prog)(const struct lfs_config* c,
                                               lfs_block_t block, lfs_off_t off,
                                               const void* buffer,
                                               lfs_size_t size) {
  const uint32_t irq = save_and_disable_interrupts();
  volatile uint32_t offset = FS_OFFSET + (block * c->block_size) + off;
  flash_range_program(FS_OFFSET + (block * c->block_size) + off,
                      (const uint8_t*)buffer, size);
  restore_interrupts(irq);
  return LFS_ERR_OK;
}

static int __no_inline_not_in_flash_func(erase)(const struct lfs_config* c,
                                                lfs_block_t block) {
  const uint32_t irq = save_and_disable_interrupts();
  flash_range_erase(FS_OFFSET + (block * c->block_size), c->block_size);
  restore_interrupts(irq);
  return LFS_ERR_OK;
}

static int sync(const struct lfs_config* c) { return LFS_ERR_OK; }

// TODO implement the USB mass storage class

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void* buffer, uint32_t bufsize) {
  return 0;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t* buffer, uint32_t bufsize) {
  return 0;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {}

bool tud_msc_test_unit_ready_cb(uint8_t lun) { return true; }

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count,
                         uint16_t* block_size) {}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                        uint16_t bufsize) {
  return 0;
}
}

Status InitializeStorage() {
  semaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore);

  // Mount the filesystem or format it
  int err = lfs_mount(&lfs, &kLFSConfig);
  if (err) {
    lfs_format(&lfs, &kLFSConfig);
    lfs_mount(&lfs, &kLFSConfig);
  }
  return StartSyncTasks();
}

Status WriteStringToFile(const std::string& content, const std::string& name) {
  LockSemaphore lock(semaphore);

  // Block the other core to avoid executing flash code when writing to flash
  std::unique_ptr<CoreBlockerSection> blocker;
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    blocker = std::make_unique<CoreBlockerSection>();
  }

  lfs_file_t file;
  if (lfs_file_open(&lfs, &file, name.c_str(), LFS_O_RDWR | LFS_O_CREAT) < 0) {
    return ERROR;
  }
  const lfs_ssize_t written =
      lfs_file_write(&lfs, &file, content.c_str(), content.size());
  if (written < 0 || written != content.size()) {
    return ERROR;
  }
  if (lfs_file_close(&lfs, &file) < 0) {
    return ERROR;
  }
  return OK;
}

Status ReadFileContent(const std::string& name, std::string* output) {
  LockSemaphore lock(semaphore);

  // Block the other core to avoid executing flash code when writing to flash
  std::unique_ptr<CoreBlockerSection> blocker;
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    blocker = std::make_unique<CoreBlockerSection>();
  }

  lfs_file_t file;
  if (lfs_file_open(&lfs, &file, name.c_str(), LFS_O_RDONLY) < 0) {
    return ERROR;
  }

  const lfs_soff_t file_size = lfs_file_size(&lfs, &file);
  if (file_size < 0) {
    return ERROR;
  }

  std::unique_ptr<uint8_t[]> read_buffer(new uint8_t[file_size]);

  const lfs_ssize_t read_bytes =
      lfs_file_read(&lfs, &file, read_buffer.get(), file_size);
  if (read_bytes < 0 || read_bytes != file_size) {
    return ERROR;
  }
  *output = std::string((char*)read_buffer.get(), (uint32_t)file_size);

  if (lfs_file_close(&lfs, &file) < 0) {
    return ERROR;
  }
  return OK;
}

Status GetFileSize(const std::string& name, size_t* output) {
  LockSemaphore lock(semaphore);

  // Block the other core to avoid executing flash code when writing to flash
  std::unique_ptr<CoreBlockerSection> blocker;
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    blocker = std::make_unique<CoreBlockerSection>();
  }

  lfs_file_t file;
  if (lfs_file_open(&lfs, &file, name.c_str(), LFS_O_RDONLY) < 0) {
    return ERROR;
  }

  const lfs_soff_t file_size = lfs_file_size(&lfs, &file);
  if (file_size < 0) {
    return ERROR;
  }
  *output = file_size;

  return OK;
}

Status RemoveFile(const std::string& name) {
  LockSemaphore lock(semaphore);

  // Block the other core to avoid executing flash code when writing to flash
  std::unique_ptr<CoreBlockerSection> blocker;
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    blocker = std::make_unique<CoreBlockerSection>();
  }

  if (lfs_remove(&lfs, name.c_str()) < 0) {
    return ERROR;
  }
  return OK;
}
