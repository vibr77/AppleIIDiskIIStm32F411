#include <stdint.h>
#include "defines.h"

#ifndef screen_diskii
#define screen_diskii


void initDiskIIImageScr(char * imageName,int type);
void updateDiskIIImageScr(uint8_t status,uint8_t trk);

void initDiskIIImageMenuScr(int i);
void initMountImageScr(char * filename);

#endif