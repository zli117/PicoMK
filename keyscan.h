#ifndef KEYSCAN_H_
#define KEYSCAN_H_

#include <status.h>

status keyscan_init(void);
void keyscan_task(void);

#endif /* KEYSCAN_H_ */