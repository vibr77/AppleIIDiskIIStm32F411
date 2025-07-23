#ifndef smartloader_h
#define smartloader_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "main.h"

int getSmartloaderTrackFromPh(int phtrack);
unsigned int getSmartloaderTrackSize(int trk);

long getSmartloaderSDAddr(int trk,int block,int csize, long database);
enum STATUS getSmartloaderTrackBitStream(int trk,unsigned char * buffer);
enum STATUS setSmartloaderTrackBitStream(int trk,unsigned char * buffer);
enum STATUS mountSmartloaderFile(char * filename);








#define A2_SPC 0xA0


#endif 