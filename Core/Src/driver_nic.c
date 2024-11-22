
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_nic.h"
#include "main.h"
#include "log.h"

extern long database;                                            // start of the data segment in FAT
extern int csize;
extern enum FS_STATUS fsState;

unsigned int fatNicCluster[20];

#define NIBBLE_BLOCK_SIZE  416 //402
#define NIBBLE_SECTOR_SIZE 512

int getNicTrackFromPh(int phtrack){
  return phtrack >> 2;
}

unsigned int getNicTrackSize(int trk){
  return 16*NIBBLE_BLOCK_SIZE*8;
  //return 16*512*8;
}

long getNicSDAddr(int trk,int block,int csize, long database){
  int long_sector = trk*16;
  int long_cluster = long_sector >> 6;
  int ft = fatNicCluster[long_cluster];
  long rSector=database+(ft-2)*csize+(long_sector & (csize-1));
  return rSector;
}

enum STATUS getNicTrackBitStream(int trk,unsigned  char* buffer){
  int addr=getNicSDAddr(trk,0,csize,database);
  const unsigned int blockNumber=16; 
  /*const char startSector[]= {
                            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
                            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,\
                            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0xFC,\
                            0xFF,0x3F,0xCF
                            } ;

  const char endSector[]={
                            0xDE,0xAA,0xEB,0xFF,0xFF,0xFF,0xFF,0xFF,
                            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                            0xFF
                            } ;
  */
  
  if (addr==-1){
    log_error("Error getting SDCard Address for nic\n");
    return RET_ERR;
  }

  //getDataBlocksBareMetal(addr,buffer,blockNumber);          // Needs to be improved and to remove the zeros
  
  unsigned char * tmp=(unsigned char *)malloc(8192*sizeof(unsigned char));
  
  if (tmp==NULL){
    log_error("can not allocate tmp for 8192 bytes");
    return RET_ERR;
  }

  getDataBlocksBareMetal(addr,tmp,blockNumber); 
  while(fsState!=READY){}

  for (int i=0;i<blockNumber;i++){
    memcpy(buffer+(i*NIBBLE_BLOCK_SIZE),tmp+(i*NIBBLE_SECTOR_SIZE),NIBBLE_BLOCK_SIZE);
  }

  free(tmp);
  
  return RET_OK;
}

enum STATUS mountNicFile(char * filename){

  FRESULT fres; 
  FIL fil;  

  fres = f_open(&fil,filename , FA_READ);     // Step 2 Open the file long naming

  if(fres != FR_OK){
    log_error("File open Error: (%i)\r\n", fres);
    return -1;
  }

  long clusty=fil.obj.sclust;
  int i=0;
  fatNicCluster[i]=clusty;
  log_info("file cluster %d:%ld",i,clusty);
  
  while (clusty!=1 && i<30){
    i++;
    clusty=get_fat((FFOBJID*)&fil,clusty);
    log_info("file cluster %d:%ld",i,clusty);
    fatNicCluster[i]=clusty;
  }

  f_close(&fil);

  return 0;
}

