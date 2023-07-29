#include "spi.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "utils.h"

#define GPIO_DEBUG_PIN_0 3
#define GPIO_DEBUG_PIN_1 4

namespace {

constexpr size_t kQueueSize = 128;
constexpr size_t kTicksToDelay = 2;

SleepQueue<uint8_t>* spi_rx_queues[2] = {NULL};
SleepQueue<uint8_t>* spi_tx_queues[2] = {NULL};

extern "C" {

void SPIDeviceTask(void* parameter) {
  reinterpret_cast<IBPSPIDevice*>(parameter)->DeviceTask();
}

void SPIHostTask(void* parameter) {
  reinterpret_cast<IBPSPIHost*>(parameter)->HostTask();
}

void SPIDeviceRXIRQ(spi_inst_t* spi_port, SleepQueue<uint8_t>* queue) {
gpio_put(GPIO_DEBUG_PIN_0, 1);
  spi_get_hw(spi_port)->imsc &= 0b1011;  // Mask IRQ
  while (spi_is_readable(spi_port)) {
    const uint8_t read = (uint8_t)spi_get_hw(spi_port)->dr;
    queue->Push(read);  // Don't care if push is unsuccessful.
  }
  queue->WakeupTaskISR();
  spi_get_hw(spi_port)->imsc |= 0b0100;  // Reenable IRQ
}

void SPIDeviceTXIRQ(spi_inst_t* spi_port, SleepQueue<uint8_t>* queue) {
  spi_get_hw(spi_port)->imsc &= 0b0111;  // Mask IRQ
  const uint8_t* head;
  for (head = queue->Peak(); spi_is_writable(spi_port) && head != NULL;
       queue->Pop(), head = queue->Peak()) {
    spi_get_hw(spi_port)->dr = *head;
  }
  if (head == NULL) {
    // Only wake up the task when the queue is empty.
    queue->WakeupTaskISR();
  }
  spi_get_hw(spi_port)->imsc |= 0b1000;  // Reenable IRQ
}

void SPI0DeviceIRQ() {
  if (spi_get_hw(spi0)->ris & 0b0100) {
    SPIDeviceRXIRQ(spi0, spi_rx_queues[0]);
  } else if (spi_get_hw(spi0)->ris & 0b1000) {
    SPIDeviceTXIRQ(spi0, spi_tx_queues[0]);
  }
}

void SPI0HostIRQ() {}

void SPI1DeviceIRQ() {
  if (spi_get_hw(spi1)->ris & 0b0100) {
    SPIDeviceRXIRQ(spi1, spi_rx_queues[1]);
  } else if (spi_get_hw(spi1)->ris & 0b1000) {
    SPIDeviceTXIRQ(spi1, spi_tx_queues[1]);
  }
}

void SPI1HostIRQ() {}
}
}  // namespace

IBPSPIBase::IBPSPIBase(IBPSPIArgs args)
    : task_handle_(NULL),
      spi_port_(args.spi_port),
      baud_rate_(args.baud_rate),
      rx_pin_(args.rx_pin),
      tx_pin_(args.tx_pin),
      cs_pin_(args.cs_pin),
      sck_pin_(args.sck_pin),
      rx_queue_(NULL),
      tx_queue_(NULL) {}

bool IBPSPIBase::TXEmpty() {
  return (spi_get_const_hw(spi_port_)->sr & SPI_SSPSR_TFE_BITS);
}

void IBPSPIBase::InitSPI(bool slave) {
  spi_init(spi_port_, baud_rate_);
  spi_set_slave(spi_port_, slave);
  gpio_set_function(rx_pin_, GPIO_FUNC_SPI);
  gpio_set_function(tx_pin_, GPIO_FUNC_SPI);
  gpio_set_function(cs_pin_, GPIO_FUNC_SPI);
  gpio_set_function(sck_pin_, GPIO_FUNC_SPI);
}

void IBPSPIBase::InitQueues(irq_handler_t irq_handler) {
  rx_queue_ = new SleepQueue<uint8_t>(
      spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
      kQueueSize);
  tx_queue_ = new SleepQueue<uint8_t>(
      spin_lock_init(spin_lock_claim_unused(/*required=*/true)), task_handle_,
      kQueueSize);
  const size_t spi_idx = spi_get_index(spi_port_);
  spi_rx_queues[spi_idx] = rx_queue_;
  spi_tx_queues[spi_idx] = tx_queue_;

  const int irq_nums[2] = {SPI0_IRQ, SPI1_IRQ};
  irq_set_exclusive_handler(irq_nums[spi_idx], irq_handler);
}

IBPSPIDevice::IBPSPIDevice(IBPSPIArgs args) : IBPSPIBase(args) {}

Status IBPSPIDevice::IBPInitialize() {
  InitSPI(/*slave=*/true);
  spi_get_hw(spi_port_)->imsc = 0;  // Disable the interrupt for now.
  if (spi_port_ == spi0) {
    irq_set_enabled(SPI0_IRQ, true);
  } else {
    irq_set_enabled(SPI1_IRQ, true);
  }

  BaseType_t status =
      xTaskCreate(&SPIDeviceTask, "spi_device_task", CONFIG_TASK_STACK_SIZE,
                  this, CONFIG_TASK_PRIORITY, &task_handle_);
  if (status != pdPASS || task_handle_ == NULL) {
    return ERROR;
  }

  const irq_handler_t irq_handlers[2] = {SPI0DeviceIRQ, SPI1DeviceIRQ};
  InitQueues(irq_handlers[spi_get_index(spi_port_)]);


  gpio_init(GPIO_DEBUG_PIN_0);
  gpio_set_dir(GPIO_DEBUG_PIN_0, GPIO_OUT);
  gpio_init(GPIO_DEBUG_PIN_1);
  gpio_set_dir(GPIO_DEBUG_PIN_1, GPIO_OUT);

  return OK;
}

void IBPSPIDevice::DeviceTask() {
  while (true) {
gpio_put(GPIO_DEBUG_PIN_0, 0);
gpio_put(GPIO_DEBUG_PIN_1, 0);
    LOG_INFO("IBP: A");
    // Clear RX FIFO
    while (spi_is_readable(spi_port_)) {
      (void)spi_get_hw(spi_port_)->dr;
    }

    spi_get_hw(spi_port_)->imsc |= 0b0100;  // Enable RX IRQ
    uint8_t input_buffer[kQueueSize];
    uint8_t buffer_bytes = 0;
    int8_t expected_bytes = 0;

    LOG_INFO("IBP: B");

    // Wait for inbound packet
    do {
      xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                      /*clear notification on exit*/ 0xffffffff,
                      /*pulNotificationValue=*/NULL, portMAX_DELAY);
gpio_put(GPIO_DEBUG_PIN_1, 1);
      LOG_INFO("IBP: C");
      for (const uint8_t* queue_head = rx_queue_->Peak();
           queue_head != NULL && buffer_bytes < kQueueSize;
           rx_queue_->Pop(), queue_head = rx_queue_->Peak()) {
        input_buffer[buffer_bytes] = *queue_head;
        LOG_INFO("IBP READ %x", *queue_head);
        if (buffer_bytes == 0) {
          expected_bytes = GetTransactionTotalSize(input_buffer[0]);
        }
        ++buffer_bytes;
      }
    } while (expected_bytes > 0 && buffer_bytes < expected_bytes);

    LOG_INFO("IBP: D");

    // Invalid packet or something is wrong (when expected_bytes is 0).

    if (expected_bytes <= 0) {
      LOG_ERROR("Invalid in bound packet");
      vTaskDelay(kTicksToDelay);
      // Drain the queue
      while (rx_queue_->Pop())
        ;
      continue;
    }

    LOG_INFO("IBP: E");

    // Everything is ok, first disable the RX IRQ and send out the packet to the
    // input task.

    spi_get_hw(spi_port_)->imsc &= 0b1011;  // Mask RX IRQ
    SetInPacket(std::string(input_buffer, input_buffer + buffer_bytes));

    // Prepare the response.

    const std::string out_packet = GetOutPacket();
    if (out_packet.size() > kQueueSize || out_packet.empty()) {
      LOG_ERROR("Invalid out bound packet");
      continue;
    }

    LOG_INFO("IBP: F");

    // Send out the packet.

    for (const uint8_t byte : out_packet) {
      tx_queue_->Push(byte);
    }

    // Put some initial data to the TX FIFO so that the IRQ can trigger.
    const uint8_t* head;
    for (head = tx_queue_->Peak(); spi_is_writable(spi_port_) && head != NULL;
         tx_queue_->Pop(), head = tx_queue_->Peak()) {
      spi_get_hw(spi_port_)->dr = *head;
      LOG_INFO("Write: %x", *head);
    }

    LOG_INFO("IBP: G");

    // Only use IRQ if we still have more data to send.
    if (head != NULL) {
      spi_get_hw(spi_port_)->imsc |= 0b1000;  // Enable TX IRQ

      // We don't sleep indefinitely here. kTicksToDelay is the timeout.
      xTaskNotifyWait(/*do not clear notification on enter*/ 0,
                      /*clear notification on exit*/ 0xffffffff,
                      /*pulNotificationValue=*/NULL, kTicksToDelay);
      spi_get_hw(spi_port_)->imsc &= 0b0111;  // Mask TX IRQ
    }

    LOG_INFO("IBP: H");

    // Clear the TX queue in case anything goes wrong.
    while (tx_queue_->Pop())
      ;

    LOG_INFO("IBP: I");

    // If there are data stuck in TX FIFO, reset the SPI controller. Seems like
    // the only way to clear up the TX FIFO.
    if (!TXEmpty()) {
      InitSPI(/*slave=*/true);
      LOG_INFO("IBP: LLL");
    }

    LOG_INFO("IBP: J");
  }
}
