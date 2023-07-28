#ifndef SPI_H_
#define SPI_H_

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

class IBPSPIBase : public virtual IBPDeviceBase {
 protected:
  IBPSPIBase(IBPSPIArgs args);

  bool TXEmpty();

  void InitSPI(bool slave);
  void InitQueues(irq_handler_t irq_handler);

  TaskHandle_t task_handle_;
  spi_inst_t* spi_port_;
  const uint32_t baud_rate_;
  const uint32_t rx_pin_;
  const uint32_t tx_pin_;
  const uint32_t cs_pin_;
  const uint32_t sck_pin_;
  SleepQueue<uint8_t>* rx_queue_;
  SleepQueue<uint8_t>* tx_queue_;
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

  spi_inst_t* spi_port_;
  uint32_t rx_pin_;
  uint32_t tx_pin_;
  uint32_t cs_pin_;
  uint32_t sck_pin_;
  uint32_t baud_rate_;
};

#endif /* SPI_H_ */