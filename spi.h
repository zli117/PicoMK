#ifndef SPI_H_
#define SPI_H_

#include "base.h"
#include "hardware/spi.h"
#include "ibp.h"

class IBPSPIDevice : public virtual IBPDeviceBase {
 public:
  IBPSPIDevice(spi_inst_t spi_port, uint32_t rx_pin, uint32_t tx_pin,
               uint32_t cs_pin, uint32_t sck_pin);

  Status IBPInitialize() override;
};

class IBPSPIHost : public virtual IBPDeviceBase {
 public:
  IBPSPIHost(spi_inst_t spi_port, uint32_t baud_rate, uint32_t rx_pin,
             uint32_t tx_pin, uint32_t cs_pin, uint32_t sck_pin);

  Status IBPInitialize() override;
};

#endif /* SPI_H_ */