#ifndef dsk_h
#define dsk_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "main.h"

int getDskTrackFromPh(int phtrack);
unsigned int getDskTrackSize(int trk);

long getDskSDAddr(int trk,int block,int csize, long database);
enum STATUS getDskTrackBitStream(int trk,unsigned char * buffer);
enum STATUS setDskTrackBitStream(int trk,unsigned char * buffer);
enum STATUS mountDskFile(char * filename);

enum STATUS createNewEmptyDSK(char * filename);

#endif 