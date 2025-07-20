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


#endif 





#define A2_SPC 0xA0

uint8_t adb_keycode_to_hid[128] = {
/* 0x20 32 */ A2_SPC, 
/* 0x21 33 */ A2_SPC, 
/* 0x22 34 */ A2_SPC, 
/* 0x23 35 */ A2_SPC, 
/* 0x24 36 */ A2_SPC, 
/* 0x25 37 */ A2_SPC, 
/* 0x26 38 */ A2_SPC, 
/* 0x27 39 */ A2_SPC, 
/* 0x28 40 */ A2_SPC, 
/* 0x29 41 */ A2_SPC, 
/* 0x2A 42 */ A2_SPC, 
/* 0x2B 43 */ A2_SPC, 
/* 0x2C 44 */ A2_SPC, 
/* 0x2D 45 */ A2_SPC, 
/* 0x2E 46 */ A2_SPC, 
/* 0x2F 47 */ A2_SPC, 
/* 0x30 48 */ A2_SPC,
/* 0x31 49 */ A2_SPC, 
/* 0x32 50 */ A2_SPC, 
/* 0x33 51 */ A2_SPC, 
/* 0x34 52 */ A2_SPC, 
/* 0x35 53 */ A2_SPC,
/* 0x36 54 */ A2_SPC, 
/* 0x37 55 */ A2_SPC, 
/* 0x38 56 */ A2_SPC, 
/* 0x39 57 */ A2_SPC, 
/* 0x3A 58 */ A2_SPC,
/* 0x3B 59 */ A2_SPC,
/* 0x3C 60 */ A2_SPC,
/* 0x3D 61 */ A2_SPC,
/* 0x3E 62 */ A2_SPC,
/* 0x3F 63 */ A2_SPC,
/* 0x40 64 */ A2_SPC,
/* 0x41 65 */ A2_SPC, 
/* 0x42 66 */ A2_SPC, 
/* 0x43 67 */ A2_SPC, 
/* 0x44 68 */ A2_SPC, 
/* 0x45 69 */ A2_SPC,
/* 0x46 70 */ A2_SPC, 
/* 0x47 71 */ A2_SPC, 
/* 0x48 72 */ A2_SPC, 
/* 0x49 73 */ A2_SPC, 
/* 0x4A 74 */ A2_SPC,
/* 0x4B 75 */ A2_SPC,
/* 0x4C 76 */ A2_SPC,
/* 0x4D 77 */ A2_SPC,
/* 0x4E 78 */ A2_SPC,
/* 0x4F 79 */ A2_SPC,
/* 0x50 80 */ A2_SPC,
/* 0x51 81 */ A2_SPC, 
/* 0x52 82 */ A2_SPC, 
/* 0x53 83 */ A2_SPC, 
/* 0x54 84 */ A2_SPC, 
/* 0x55 85 */ A2_SPC,
/* 0x56 86 */ A2_SPC, 
/* 0x57 87 */ A2_SPC, 
/* 0x58 88 */ A2_SPC, 
/* 0x59 89 */ A2_SPC, 
/* 0x5A 90 */ A2_SPC,
/* 0x5B 91 */ A2_SPC,
/* 0x5C 92 */ A2_SPC,
/* 0x5D 93 */ A2_SPC,
/* 0x5E 94 */ A2_SPC,
/* 0x5F 95 */ A2_SPC,  
/* 0x60 96 */ A2_SPC,
/* 0x61 97 */ A2_SPC, 
/* 0x62 98 */ A2_SPC, 
/* 0x63 99 */ A2_SPC, 
/* 0x64 100 */ A2_SPC, 
/* 0x65 101 */ A2_SPC,
/* 0x66 102 */ A2_SPC, 
/* 0x67 103 */ A2_SPC, 
/* 0x68 104 */ A2_SPC, 
/* 0x69 105 */ A2_SPC, 
/* 0x6A 106 */ A2_SPC,
/* 0x6B 107 */ A2_SPC,
/* 0x6C 108 */ A2_SPC,
/* 0x6D 109*/ A2_SPC,
/* 0x6E 110 */ A2_SPC,
/* 0x6F 111 */ A2_SPC,
/* 0x70 112 */ A2_SPC,
/* 0x71 113 */ A2_SPC, 
/* 0x72 114 */ A2_SPC, 
/* 0x73 115 */ A2_SPC, 
/* 0x74 116 */ A2_SPC, 
/* 0x75 117 */ A2_SPC,
/* 0x76 118 */ A2_SPC, 
/* 0x77 119 */ A2_SPC, 
/* 0x78 120 */ A2_SPC, 
/* 0x79 121 */ A2_SPC, 
/* 0x7A 122 */ A2_SPC,
/* 0x7B 123 */ A2_SPC,
/* 0x7C 124 */ A2_SPC,
/* 0x7D 125 */ A2_SPC,
/* 0x7E 126 */      
};
