#ifndef SYNC_H_
#define SYNC_H_

#include "utils.h"

Status StartSyncTasks();

class CoreBlockerSection {
 public:
  CoreBlockerSection();
  virtual ~CoreBlockerSection();
};

// Block the other core so that we can do flash operations
void DisableTheOtherCore();
void ReenableTheOtherCore();

#endif /* SYNC_H_ */
