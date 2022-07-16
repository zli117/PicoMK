#ifndef RUNNER_H_
#define RUNNER_H_

#include "utils.h"

namespace runner {

Status RunnerInit();
Status RunnerStart();

void SetConfigMode(bool is_config);
void NotifyConfigChange();

}  // namespace runner

#endif /* RUNNER_H_ */