
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"
//#include "fatfs_sdcard.h"

#include "driver_nic.h"
#include "main.h"
#include "log.h"

extern long database;                                            // start of the data segment in FAT
extern int csize;  

unsigned int fatNicCluster[20];



int getNicTrackFromPh(int phtrack){
  return phtrack >> 2;
}

unsigned int getNicTrackSize(int trk){
  return 16*512*8;
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
  
  if (addr==-1){
    log_error("Error getting SDCard Address for nic\n");
    return RET_ERR;
  }

  getDataBlocksBareMetal(addr,buffer,blockNumber);

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
  log_info("file cluster %d:%ld\n",i,clusty);
  
  while (clusty!=1 && i<30){
    i++;
    clusty=get_fat((FFOBJID*)&fil,clusty);
    log_info("file cluster %d:%ld\n",i,clusty);
    fatNicCluster[i]=clusty;
  }

  f_close(&fil);

  return 0;
}

