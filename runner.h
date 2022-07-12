#ifndef RUNNER_H_
#define RUNNER_H_

#include "utils.h"

Status RunnerInit();
Status RunnerStart();

void SetConfigMode(bool is_config);
void NotifyConfigChange();

#endif /* RUNNER_H_ */