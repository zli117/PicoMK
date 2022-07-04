#ifndef KEYSCAN_H_
#define KEYSCAN_H_

namespace keyboard {

void keyscan_init();

}

extern "C" void keyscan_task();

#endif /* KEYSCAN_H_ */