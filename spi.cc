#include "spi.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "utils.h"

namespace {

constexpr size_t kQueueSize = 128;

SleepQueue<uint8_t>* spi_0_rx_queue = NULL;
SleepQueue<uint8_t>* spi_0_tx_queue = NULL;
SleepQueue<uint8_t>* spi_1_rx_queue = NULL;
SleepQueue<uint8_t>* spi_1_tx_queue = NULL;

extern "C" {

void SPIDeviceTask(void* parameter) {
  reinterpret_cast<IBPSPIDevice*>(parameter)->DeviceTask();
}

void SPIHostTask(void* parameter) {
  reinterpret_cast<IBPSPIHost*>(parameter)->HostTask();
}

void SPIDeviceRXIRQ(spi_inst_t* spi_port, SleepQueue<uint8_t>* queue) {
  spi_get_hw(spi_port)->imsc &= 0b1011;  // Mask IRQ
  while (spi_is_readable(spi_port)) {
    const uint8_t read = (uint8_t)spi_get_hw(spi_port)->dr;
    queue->Push(read);  // Don't care if push is unsuccessful.
  }
  queue->WakeupTask();
  spi_get_hw(spi_port)->imsc |= 0b0100;  // Reenable IRQ
}

void SPIDeviceTXIRQ(spi_inst_t* spi_port, SleepQueue<uint8_t>* queue) {
  spi_get_hw(spi_port)->imsc &= 0b0111;  // Mask IRQ
  for (const uint8_t* head = queue->Peak();
       spi_is_writable(spi_port) && head != NULL; head = queue->Peak()) {
    spi_get_hw(spi_port)->dr = *head;
    queue->Pop();
  }
  queue->WakeupTask();
  spi_get_hw(spi_port)->imsc |= 0b1000;  // Reenable IRQ
}

void SPI0DeviceIRQ() {
  if (spi_get_hw(spi0)->ris & 0b0100) {
    SPIDeviceRXIRQ(spi0, spi_0_rx_queue);
  } else if (spi_get_hw(spi0)->ris & 0b1000) {
    SPIDeviceTXIRQ(spi0, spi_0_tx_queue);
  }
}

void SPI0HostIRQ() {}

void SPI1DeviceIRQ() {
  if (spi_get_hw(spi1)->ris & 0b0100) {
    SPIDeviceRXIRQ(spi1, spi_1_rx_queue);
  } else if (spi_get_hw(spi1)->ris & 0b1000) {
    SPIDeviceTXIRQ(spi1, spi_1_tx_queue);
  }
}

void SPI1HostIRQ() {}
}
}  // namespace

IBPSPIDevice::IBPSPIDevice(IBPSPIArgs args)
    : task_handle_(NULL),
      spi_port_(args.spi_port),
      rx_pin_(args.rx_pin),
      tx_pin_(args.tx_pin),
      cs_pin_(args.cs_pin),
      sck_pin_(args.sck_pin) {}

Status IBPSPIDevice::IBPInitialize() {
  spi_init(spi_port_, 100000);      // Baud rate doesn't matter for device.
  spi_get_hw(spi_port_)->imsc = 0;  // Disable the interrupt for now.
  spi_set_slave(spi_port_, /*slave=*/true);
  gpio_set_function(rx_pin_, GPIO_FUNC_SPI);
  gpio_set_function(tx_pin_, GPIO_FUNC_SPI);
  gpio_set_function(cs_pin_, GPIO_FUNC_SPI);
  gpio_set_function(sck_pin_, GPIO_FUNC_SPI);

  if (spi_port_ == spi0) {
    spi_0_rx_queue = new SleepQueue<uint8_t>(
        spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
        kQueueSize);
    spi_0_tx_queue = new SleepQueue<uint8_t>(
        spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
        kQueueSize);
    irq_set_exclusive_handler(SPI0_IRQ, SPI0DeviceIRQ);
    irq_set_enabled(SPI0_IRQ, true);
  } else {
    spi_1_rx_queue = new SleepQueue<uint8_t>(
        spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
        kQueueSize);
    spi_1_tx_queue = new SleepQueue<uint8_t>(
        spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
        kQueueSize);
    irq_set_exclusive_handler(SPI1_IRQ, SPI1DeviceIRQ);
    irq_set_enabled(SPI1_IRQ, true);
  }

  BaseType_t status =
      xTaskCreate(&SPIDeviceTask, "spi_device_task", CONFIG_TASK_STACK_SIZE,
                  this, CONFIG_TASK_PRIORITY, &task_handle_);
  if (status != pdPASS || task_handle_ == NULL) {
    return ERROR;
  }
  return OK;
}

void IBPSPIDevice::DeviceTask() {}
