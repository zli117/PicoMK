#ifndef SPI_H_
#define SPI_H_

#include "FreeRTOS.h"
#include "base.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "ibp.h"

struct IBPSPIArgs {
  spi_inst_t* spi_port;
  uint32_t rx_pin;
  uint32_t tx_pin;
  uint32_t cs_pin;
  uint32_t sck_pin;
  uint32_t baud_rate;
};

class IBPSPIDevice : public virtual IBPDeviceBase {
 public:
  IBPSPIDevice(IBPSPIArgs args);

  Status IBPInitialize() override;

  void DeviceTask();

 protected:
  TaskHandle_t task_handle_;
  spi_inst_t* spi_port_;
  uint32_t rx_pin_;
  uint32_t tx_pin_;
  uint32_t cs_pin_;
  uint32_t sck_pin_;
};

class IBPSPIHost : public virtual IBPDeviceBase {
 public:
  IBPSPIHost(IBPSPIArgs args);

  Status IBPInitialize() override;

  void HostTask();

 protected:
  spi_inst_t* spi_port_;
  uint32_t rx_pin_;
  uint32_t tx_pin_;
  uint32_t cs_pin_;
  uint32_t sck_pin_;
  uint32_t baud_rate_;
};

#endif /* SPI_H_ */