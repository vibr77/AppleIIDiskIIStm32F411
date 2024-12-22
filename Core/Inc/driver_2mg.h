#ifndef img2_h
#define img2_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int get2mgTrackFromPh(int phtrack);
unsigned int get2mgTrackSize(int trk);

long get2mgSDAddr(int trk,int block,int csize, long database);
enum STATUS get2mgTrackBitStream(int trk,unsigned char * buffer);
enum STATUS set2mgTrackBitStream(int trk,unsigned char * buffer);
enum STATUS mount2mgFile(char * filename);
enum STATUS img22Nic(unsigned char *src,unsigned char *buffer,uint8_t trk);
enum STATUS nic22mg(char *rawByte,unsigned char *buffer,uint8_t trk);

#endif 