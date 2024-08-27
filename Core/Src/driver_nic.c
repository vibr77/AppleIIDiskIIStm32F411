
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
  return phtrack>>2;
}

unsigned int getNicTrackSize(int trk){
  return 6464*8;
}


long getSDAddrNic(int trk,int block,int csize, long database){
  int long_sector = trk*16;
  int long_cluster = long_sector >> 6;
  int ft = fatNicCluster[long_cluster];
  long rSector=database+(ft-2)*csize+(long_sector & (csize-1));
  return rSector;
}

enum STATUS getNicTrackBitStream(int trk,unsigned  char* buffer){
  int addr=getSDAddrNic(trk,0,csize,database);
  if (addr==-1){
    printf("Error getting SDCard Address for nic\n");
    return RET_ERR;
  }
  
  unsigned char * tmp2=(unsigned char*)malloc(16*512*sizeof(char));
  if (tmp2==NULL){
    printf("Error memory alloaction getNicTrackBitStream: tmp2:8192 Bytes");
    return RET_ERR;
  }

  //cmd18GetDataBlocksBareMetal(addr,tmp2,16);
   getDataBlocksBareMetal(addr,tmp2,16);

  // 22xFF are not needed,
  // Right size is 404 Bytes => offset 6 athe begining 
  for (int i=0;i<16;i++){
    memcpy(buffer+i*404 /*416*/ ,tmp2+i*512+6,/*416*/ 404);
  }
  free(tmp2);
  return RET_OK;
}

enum STATUS mountNicFile(char * filename){
   
  FRESULT fres; 
  FIL fil;  

  fres = f_open(&fil,filename , FA_READ);     // Step 2 Open the file long naming

  if(fres != FR_OK){
    printf("File open Error: (%i)\r\n", fres);
   
    return -1;
  }

  long clusty=fil.obj.sclust;
  int i=0;
  fatNicCluster[i]=clusty;
  printf("file cluster %d:%ld\n",i,clusty);
  
  while (clusty!=1 && i<30){
    i++;
    clusty=get_fat((FFOBJID*)&fil,clusty);
    printf("file cluster %d:%ld\n",i,clusty);
    fatNicCluster[i]=clusty;
  }

  
  f_close(&fil);

  return 0;
}

