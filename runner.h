#ifndef RUNNER_H_
#define RUNNER_H_

#include "utils.h"

Status RunnerInit();
Status RunnerStart();

void SetConfigModeAllDevice(bool is_config_mode);
void NotifyConfigChange();

#endif /* RUNNER_H_ */