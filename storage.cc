#include "storage.h"

#include <stdint.h>

#include <memory>

#include "FreeRTOS.h"
#include "config.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "irq.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "semphr.h"

// #define LFS_THREADSAFE 1
// #define LFS_NO_MALLOC 1

extern "C" {
#include "littlefs/lfs.h"

static SemaphoreHandle_t __not_in_flash("storage") semaphore;
static lfs_t __not_in_flash("storage") lfs;

#define READ_SIZE 64
#define CACHE_SIZE READ_SIZE
#define PROG_SIZE READ_SIZE
#define LOOKAHEAD_SIZE READ_SIZE
#define FS_OFFSET (PICO_FLASH_SIZE_BYTES - CONFIG_FLASH_FILESYSTEM_SIZE)

static int read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                void* buffer, lfs_size_t size);
static int prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                const void* buffer, lfs_size_t size);
static int erase(const struct lfs_config* c, lfs_block_t block);
static int sync(const struct lfs_config* c);
static int lock(const struct lfs_config* c);
static int unlock(const struct lfs_config* c);

static uint8_t __not_in_flash("storage") read_buffer[CACHE_SIZE]
    __attribute__((aligned(4)));
static uint8_t __not_in_flash("storage") prog_buffer[CACHE_SIZE]
    __attribute__((aligned(4)));
static uint8_t __not_in_flash("storage") lookahead_buffer[LOOKAHEAD_SIZE]
    __attribute__((aligned(4)));

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
      .lock = lock,
      .unlock = unlock,

      // block device configuration
      .read_size = READ_SIZE,
      .prog_size = PROG_SIZE,
      .block_size = FLASH_SECTOR_SIZE,
      .block_count = CONFIG_FLASH_FILESYSTEM_SIZE / FLASH_SECTOR_SIZE,
      .block_cycles = 100,
      .cache_size = CACHE_SIZE,
      .lookahead_size = LOOKAHEAD_SIZE,
      .read_buffer = read_buffer,
      .prog_buffer = prog_buffer,
      .lookahead_buffer = lookahead_buffer,
  };
  return cfg;
}

static const struct lfs_config __not_in_flash("storage1")
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

static int lock(const struct lfs_config* c) {
  xSemaphoreTake(semaphore, portMAX_DELAY);
  return LFS_ERR_OK;
}

static int unlock(const struct lfs_config* c) {
  xSemaphoreGive(semaphore);
  return LFS_ERR_OK;
}

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

  // // Mount the filesystem or format it
  // int err = lfs_mount(&lfs, &kLFSConfig);
  // if (err) {
  //   lfs_format(&lfs, &kLFSConfig);
  //   lfs_mount(&lfs, &kLFSConfig);
  // }
  return OK;
}

static void __no_inline_not_in_flash_func(write)(const char* buffer,
                                                 size_t length) {
  const uint32_t irq = save_and_disable_interrupts();
  flash_range_erase(FS_OFFSET, 4096);
  flash_range_program(FS_OFFSET, (const uint8_t*)&length, sizeof(size_t));
  flash_range_program(FS_OFFSET + 256, (const uint8_t*)buffer, length);
  restore_interrupts(irq);
}

Status WriteStringToFile(const std::string& content, const std::string& name) {
  // uint8_t buffer[CACHE_SIZE];

  // lfs_file_t file;
  // struct lfs_file_config file_cfg = {
  //     .buffer = buffer,
  //     .attrs = NULL,
  //     .attr_count = 0,
  // };
  // if (lfs_file_opencfg(&lfs, &file, name.c_str(), LFS_O_RDWR | LFS_O_CREAT,
  //                      &file_cfg) < 0) {
  //   return ERROR;
  // }
  EnterGlobalCriticalSection();
  // // const uint32_t irq = save_and_disable_interrupts();
  // const lfs_ssize_t written =
  //     lfs_file_write(&lfs, &file, content.c_str(), content.size());
  write(content.c_str(), content.size());
  ExitGlobalCriticalSection();
  // // restore_interrupts(irq);
  // if (written < 0 || written != content.size()) {
  //   return ERROR;
  // }
  // if (lfs_file_close(&lfs, &file) < 0) {
  //   return ERROR;
  // }
  return OK;
}

Status ReadFileContent(const std::string& name, std::string* output) {
  size_t length = *(size_t*)((XIP_NOCACHE_NOALLOC_BASE) + FS_OFFSET);
  if (length > 1024) {
    return ERROR;
  }
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
  memcpy(buffer.get(),
         (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE) + FS_OFFSET + 256,
         length);
  *output = std::string((char*)buffer.get(), length);
  return OK;

  // uint8_t buffer[CACHE_SIZE];

  // lfs_file_t file;
  // struct lfs_file_config file_cfg = {
  //     .buffer = buffer,
  //     .attrs = NULL,
  //     .attr_count = 0,
  // };
  // if (lfs_file_opencfg(&lfs, &file, name.c_str(), LFS_O_RDONLY, &file_cfg) <
  //     0) {
  //   return ERROR;
  // }

  // const lfs_soff_t file_size = lfs_file_size(&lfs, &file);
  // if (file_size < 0) {
  //   return ERROR;
  // }

  // std::unique_ptr<uint8_t[]> read_buffer(new uint8_t[file_size]);

  // const lfs_ssize_t read_bytes =
  //     lfs_file_read(&lfs, &file, read_buffer.get(), file_size);
  // if (read_bytes < 0 || read_bytes != file_size) {
  //   return ERROR;
  // }
  // *output = std::string((char*)read_buffer.get(), (uint32_t)file_size);

  // if (lfs_file_close(&lfs, &file) < 0) {
  //   return ERROR;
  // }
  // return OK;
}

Status GetFileSize(const std::string& name, size_t* output) {
  // uint8_t buffer[CACHE_SIZE];

  // lfs_file_t file;
  // struct lfs_file_config file_cfg = {
  //     .buffer = buffer,
  //     .attrs = NULL,
  //     .attr_count = 0,
  // };
  // if (lfs_file_opencfg(&lfs, &file, name.c_str(), LFS_O_RDONLY, &file_cfg) <
  //     0) {
  //   return ERROR;
  // }

  // const lfs_soff_t file_size = lfs_file_size(&lfs, &file);
  // if (file_size < 0) {
  //   return ERROR;
  // }
  // *output = file_size;

  // return OK;
  return ERROR;
}

Status RemoveFile(const std::string& name) {
  return ERROR;
  // if (lfs_remove(&lfs, name.c_str()) < 0) {
  //   return ERROR;
  // }
  // return OK;
}
