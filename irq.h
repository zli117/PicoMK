#ifndef IRQ_H_
#define IRQ_H_

#include "utils.h"

// enum IRQ { ENABLE = 0, DISABLE };

Status StartIRQTasks();

// Not recursive
void EnterGlobalCriticalSection();
void ExitGlobalCriticalSection();

// void SetIRQBothCores(IRQ irq);

#endif  /* IRQ_H_ */
