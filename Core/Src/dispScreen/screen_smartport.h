#include <stdint.h>
#include "defines.h"

#ifndef screen_smartport
#define screen_smartport

void initSmartPortHDScr();
void setImageTabSmartPortHD(char * fileTab[4],uint8_t bootImageIndex);
void updateImageSmartPortHD();

void updateCommandSmartPortHD(uint8_t imageIndex,uint8_t cmd);

void initSmartPortHDImageOptionScr();
void initSmartportMountImageScr(char * filename);
void initSmartPortHDImageOption2Scr();
#endif 

