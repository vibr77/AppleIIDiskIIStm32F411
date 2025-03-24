#include <stdint.h>
#include "defines.h"

#ifndef screen_fs
#define screen_fs

void initFsScr(char * path);
void updateChainedListDisplay(int init, list_t * lst );

void initLabelInputScr();

#endif
