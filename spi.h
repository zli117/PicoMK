#ifndef SPI_H_
#define SPI_H_

#include <array>
#include <memory>

#include "FreeRTOS.h"
#include "base.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "ibp.h"
#include "utils.h"

struct IBPSPIArgs {
  spi_inst_t* spi_port;
  uint32_t rx_pin;
  uint32_t tx_pin;
  uint32_t cs_pin;
  uint32_t sck_pin;
  uint32_t baud_rate;
};

struct IBPIRQData {
  std::shared_ptr<std::array<uint8_t, IBP_MAX_PACKET_LEN>> rx_buffer;
  std::shared_ptr<std::array<uint8_t, IBP_MAX_PACKET_LEN>> tx_buffer;
  uint8_t rx_buf_idx;
  int8_t rx_packet_size;
  uint8_t tx_buf_idx;
  uint8_t tx_packet_size;
  SemaphoreHandle_t rx_handle;
  SemaphoreHandle_t tx_handle;
  spi_inst_t* spi_port;

  void Clear();
};

class IBPSPIBase : public virtual IBPDeviceBase {
 protected:
  IBPSPIBase(IBPSPIArgs args);

  bool TXEmpty();

  void InitSPI(bool slave);
  void InitIRQData(irq_handler_t irq_handler);

  TaskHandle_t task_handle_;
  spi_inst_t* spi_port_;
  const uint32_t baud_rate_;
  const uint32_t rx_pin_;
  const uint32_t tx_pin_;
  const uint32_t cs_pin_;
  const uint32_t sck_pin_;
  IBPIRQData* irq_data_; 
};

class IBPSPIDevice : public IBPSPIBase {
 public:
  Status IBPInitialize() override;

  void DeviceTask();

 protected:
  IBPSPIDevice(IBPSPIArgs args);
};

class IBPSPIHost : public IBPSPIBase {
 public:
  Status IBPInitialize() override;

  void HostTask();

 protected:
  IBPSPIHost(IBPSPIArgs args);
};

#endif /* SPI_H_ */