#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include "base.h"
#include "utils.h"

class TemperatureInputDeivce : virtual public GenericInputDevice {
 public:
  TemperatureInputDeivce();

  void InputLoopStart() override;
  void InputTick() override;

  std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig()
      override;
  void OnUpdateConfig(const Config* config) override;

 protected:
  virtual int32_t ConvertTemperature();

  virtual void WriteTemp(int32_t temp);

  bool is_fahrenheit_;
  std::vector<uint16_t> buffer_;
  uint32_t buffer_idx_;
  int32_t sum_;
  int32_t prev_temp_;
};

Status RegisterTemperatureInput(uint8_t tag);

#endif /* TEMPERATURE_H_ */