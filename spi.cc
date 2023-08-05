#include "spi.h"

#include <cstring>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "utils.h"

#define GPIO_DEBUG_PIN_0 3
#define GPIO_DEBUG_PIN_1 4
#define GPIO_DEBUG_PIN_2 5

namespace {

constexpr size_t kTXTicksToWait = 1;

extern "C" {

static IBPIRQData __not_in_flash("irq") irq_data[2];

void SPIDeviceTask(void* parameter) {
  reinterpret_cast<IBPSPIDevice*>(parameter)->DeviceTask();
}

void SPIHostTask(void* parameter) {
  reinterpret_cast<IBPSPIHost*>(parameter)->HostTask();
}

void __no_inline_not_in_flash_func(SPIDeviceRXIRQ)(IBPIRQData* irq_data) {
  gpio_put(GPIO_DEBUG_PIN_0, 1);

  // Local copy to reduce pointer dereference.
  IBPIRQData irq_data_local = *irq_data;
  spi_get_hw(irq_data_local.spi_port)->imsc &= 0b1011;  // Mask RX IRQ

  while (spi_is_readable(irq_data_local.spi_port)) {
    const uint8_t read = (uint8_t)spi_get_hw(irq_data_local.spi_port)->dr;
    if (irq_data_local.rx_buf_idx == 0) {
      irq_data_local.rx_packet_size = GetTransactionTotalSize(read);
    }
    (*irq_data_local.rx_buffer)[irq_data_local.rx_buf_idx++] = read;
    if (irq_data_local.rx_packet_size < 0) {
      *irq_data = irq_data_local;
      xSemaphoreGiveFromISR(irq_data_local.rx_handle,
                            /*pxHigherPriorityTaskWoken=*/NULL);
      return;
    }
    if (irq_data_local.rx_buf_idx >= irq_data_local.rx_packet_size) {
      while (spi_is_writable(irq_data_local.spi_port) &&
             irq_data_local.tx_buf_idx < irq_data_local.tx_packet_size) {
        spi_get_hw(irq_data_local.spi_port)->dr =
            (*irq_data_local.tx_buffer)[irq_data_local.tx_buf_idx++];
      }
      *irq_data = irq_data_local;

      spi_get_hw(irq_data_local.spi_port)->imsc |= 0b1000;  // Enable TX IRQ
      xSemaphoreGiveFromISR(irq_data_local.rx_handle,
                            /*pxHigherPriorityTaskWoken=*/NULL);
      return;
    }
  }

  *irq_data = irq_data_local;
  spi_get_hw(irq_data_local.spi_port)->imsc |= 0b0100;  // Reenable IRQ
}

void SPIDeviceTXIRQ(IBPIRQData* irq_data) {
  gpio_put(GPIO_DEBUG_PIN_1, 1);
  IBPIRQData irq_data_local = *irq_data;
  spi_get_hw(irq_data_local.spi_port)->imsc &= 0b0111;  // Mask IRQ

  while (spi_is_writable(irq_data_local.spi_port) &&
         irq_data_local.tx_buf_idx < irq_data_local.tx_packet_size) {
    spi_get_hw(irq_data_local.spi_port)->dr =
        (*irq_data_local.tx_buffer)[irq_data_local.tx_buf_idx++];
  }

  *irq_data = irq_data_local;

  if (irq_data_local.tx_buf_idx >= irq_data_local.tx_packet_size) {
    // Only wake up the task when the queue is empty.
    // queue->WakeupTaskISR();
    xSemaphoreGiveFromISR(irq_data_local.tx_handle,
                          /*pxHigherPriorityTaskWoken=*/NULL);
    // Leave TX IRQ disabled.
  } else {
    spi_get_hw(irq_data_local.spi_port)->imsc |= 0b1000;  // Reenable IRQ
  }
}

void SPI0DeviceIRQ() {
  if (spi_get_hw(spi0)->mis & 0b0100) {
    SPIDeviceRXIRQ(&irq_data[0]);
  } else if (spi_get_hw(spi0)->mis & 0b1000) {
    SPIDeviceTXIRQ(&irq_data[0]);
  }
}

void SPI0HostIRQ() {}

void SPI1DeviceIRQ() {
  if (spi_get_hw(spi1)->mis & 0b0100) {
    SPIDeviceRXIRQ(&irq_data[1]);
  } else if (spi_get_hw(spi1)->mis & 0b1000) {
    SPIDeviceTXIRQ(&irq_data[1]);
  }
}

void SPI1HostIRQ() {}
}
}  // namespace

void IBPIRQData::Clear() {
  rx_buf_idx = 0;
  rx_packet_size = -1;
  tx_buf_idx = 0;
  tx_packet_size = 0;
  xSemaphoreGive(rx_handle);
  xSemaphoreGive(tx_handle);
}

IBPSPIBase::IBPSPIBase(IBPSPIArgs args)
    : task_handle_(NULL),
      spi_port_(args.spi_port),
      baud_rate_(args.baud_rate),
      rx_pin_(args.rx_pin),
      tx_pin_(args.tx_pin),
      cs_pin_(args.cs_pin),
      sck_pin_(args.sck_pin),
      irq_data_(NULL) {}

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

void IBPSPIBase::InitIRQData(irq_handler_t irq_handler) {
  const uint8_t spi_idx = spi_get_index(spi_port_);
  irq_data_ = &irq_data[spi_idx];
  irq_data_->rx_buffer =
      std::make_shared<std::array<uint8_t, IBP_MAX_PACKET_LEN>>();
  irq_data_->tx_buffer =
      std::make_shared<std::array<uint8_t, IBP_MAX_PACKET_LEN>>();
  irq_data_->spi_port = spi_port_;
  irq_data_->rx_handle = xSemaphoreCreateBinary();
  irq_data_->tx_handle = xSemaphoreCreateBinary();
  irq_data_->Clear();

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
  InitIRQData(irq_handlers[spi_get_index(spi_port_)]);

  gpio_init(GPIO_DEBUG_PIN_0);
  gpio_set_dir(GPIO_DEBUG_PIN_0, GPIO_OUT);
  gpio_init(GPIO_DEBUG_PIN_1);
  gpio_set_dir(GPIO_DEBUG_PIN_1, GPIO_OUT);
  gpio_init(GPIO_DEBUG_PIN_2);
  gpio_set_dir(GPIO_DEBUG_PIN_2, GPIO_OUT);

  return OK;
}

void IBPSPIDevice::DeviceTask() {
  if (irq_data_ == NULL) {
    LOG_ERROR("irq_data_ is NULL");
    return;
  }

  while (true) {
    irq_data_->Clear();

    // Get the new out-bound packet.
    const std::string out_packet = GetOutPacket();
    if (out_packet.size() > IBP_MAX_PACKET_LEN || out_packet.empty()) {
      LOG_ERROR("Invalid out bound packet");
      vTaskDelay(pdMS_TO_TICKS(IBP_INVALID_PACKET_DELAY_MS));
      continue;
    }

    std::memcpy(irq_data_->tx_buffer->data(), out_packet.data(),
                out_packet.size());
    irq_data_->tx_packet_size = out_packet.size();

    gpio_put(GPIO_DEBUG_PIN_0, 0);
    gpio_put(GPIO_DEBUG_PIN_1, 0);
    gpio_put(GPIO_DEBUG_PIN_2, 0);

    // Clear RX FIFO
    while (spi_is_readable(spi_port_)) {
      (void)spi_get_hw(spi_port_)->dr;
    }

    // If there are still data stuck in TX FIFO, reset the SPI controller. Seems
    // like the only way to clear up the TX FIFO.
    if (!TXEmpty()) {
      gpio_put(GPIO_DEBUG_PIN_1, 1);
      InitSPI(/*slave=*/true);
      gpio_put(GPIO_DEBUG_PIN_1, 0);
    }

    xSemaphoreTake(irq_data->rx_handle, /*xTicksToWait=*/0);
    xSemaphoreTake(irq_data->tx_handle, /*xTicksToWait=*/0);
    spi_get_hw(spi_port_)->imsc &= 0b0111;  // Mask TX IRQ
    spi_get_hw(spi_port_)->imsc |= 0b0100;  // Enable RX IRQ

    xSemaphoreTake(irq_data->rx_handle, portMAX_DELAY);

    gpio_put(GPIO_DEBUG_PIN_0, 0);

    // At this point RX should be disabled. Remask just to be sure.
    spi_get_hw(spi_port_)->imsc &= 0b1011;

    if (irq_data_->rx_packet_size < 0) {
      LOG_ERROR("Invalid in bound packet");
      // gpio_put(GPIO_DEBUG_PIN_2, 1);
      vTaskDelay(pdMS_TO_TICKS(IBP_INVALID_PACKET_DELAY_MS));
      continue;
    }

    SetInPacket(
        std::string(irq_data->rx_buffer->data(),
                    irq_data->rx_buffer->data() + irq_data_->rx_packet_size));

    const bool tx_done = xSemaphoreTake(irq_data->tx_handle, kTXTicksToWait);

    gpio_put(GPIO_DEBUG_PIN_1, 0);

    // At this point both RX and TX IRQs should be masked. Remask just to be
    // sure.
    spi_get_hw(spi_port_)->imsc = 0;

    if (tx_done && !TXEmpty()) {
      // If the semaphore is successfully acquired, then we know that TX IRQ has
      // pushed all the bytes. If the TX FIFO is still not empty then let's wait
      // a bit for it to clear up.
      vTaskDelay(kTXTicksToWait);
    }
  }
}
