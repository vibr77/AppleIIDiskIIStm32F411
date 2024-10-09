#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"
//#include "fatfs_sdcard.h"

#include "driver_woz.h"
#include "main.h"
#include "log.h"

__uint8_t TMAP[160];
__uint16_t TRK_startingBlockOffset[41];
__uint16_t TRK_BlockCount[41];
__uint32_t TRK_BitCount[41];



const char logPrefix[]="[woz_driver]";

extern long database;                                            // start of the data segment in FAT
extern int csize;  
extern volatile enum FS_STATUS fsState;
unsigned int fatWozCluster[20];
char * woz1_256B_prologue;                                       // needed to store the potential overwrite
woz_info_t wozFile;
int getWozTrackFromPh(int phtrack){
   return TMAP[phtrack];
}
unsigned int getWozTrackSize(int trk){
  unsigned int B=TRK_BitCount[trk];
  return B;
}

long getSDAddrWoz(int trk,int block,int csize, long database){
  long rSector=-1;
  if (wozFile.version==2){
    int long_sector = TRK_startingBlockOffset[trk] + block;
    //int long_sector=3+trk*13;
    //log_debug("long_sector: %d",long_sector);
    int long_cluster = long_sector >> 6;
    int ft = fatWozCluster[long_cluster];
    rSector=database+(ft-2)*csize+(long_sector & (csize-1));

  }else if (wozFile.version==1){
    int long_sector = 13*trk;                                // 13 block of 512 per track
    int long_cluster = long_sector >> 6;
    int ft = fatWozCluster[long_cluster];
    rSector=database+(ft-2)*csize+(long_sector & (csize-1));
  
  }
  
  return rSector;
}

enum STATUS getWozTrackBitStream_fopen(int trk,unsigned char * buffer){
  
  int long_sector = TRK_startingBlockOffset[trk];
  FRESULT fres; 
  FIL fil;  

  while(fsState!=READY){};
  fsState=BUSY;
  char filename[]="/WOZ 2.0/Bouncing Kamungas - Disk 1, Side A.woz";
  fres = f_open(&fil,filename , FA_READ);    
  if(fres != FR_OK){
    log_error("File open Error: (%i)",fres);
    return RET_ERR;
  }
  f_lseek(&fil,long_sector*512);
  unsigned int pt;
  fres = f_read(&fil,buffer,6656,&pt);
  if(fres != FR_OK){
    log_error("File read Error: (%i)",fres);
    return RET_ERR;
  }
  f_close(&fil);
  fsState=READY;
  return RET_OK;

}


enum STATUS getWozTrackBitStream(int trk,unsigned char * buffer){
  const unsigned int blockNumber=13; 
  int addr=getSDAddrWoz(trk,0,csize,database);
  
  if (addr==-1){
    printf("%s:Error getting SDCard Address for woz\n",logPrefix);
    return RET_ERR;
  }
  
  if (wozFile.version==2){

    getDataBlocksBareMetal(addr,buffer,blockNumber);

  }else if (wozFile.version==1){
    unsigned char * tmp2=(unsigned char*)malloc((blockNumber+1)*512*sizeof(char));
    if (tmp2==NULL){
      log_error("Error memory alloaction getNicTrackBitStream: tmp2:8192 Bytes",logPrefix);
      return RET_ERR;
    }
    getDataBlocksBareMetal(addr,tmp2,blockNumber+1);
    while (fsState!=READY){}                                                          // we need to wait here... with the DMA
    memcpy(buffer,tmp2+256,blockNumber*512-10);                                       // Last 10 Bytes are not Data Stream Bytes
    woz1_256B_prologue=malloc(256*sizeof(char));
    memcpy(woz1_256B_prologue,tmp2,256);                                              // we need this to speed up the write process
    free(tmp2);
  }
        
  return 1;
}

enum STATUS setWozTrackBitStream(int trk,unsigned char * buffer){
  const unsigned int blockNumber=13; 
  int addr=getSDAddrWoz(trk,0,csize,database);
  
  if (addr==-1){
    log_error("Error getting SDCard Address for woz");
    return RET_ERR;
  }
  
  if (wozFile.version==2){
    //writeDataBlocks(addr,buffer,blockNumber);
    //cmd25SetDataBlocksBareMetal(addr,buffer,blockNumber);
  }else if (wozFile.version==1){
    
    unsigned char * tmp2=(unsigned char*)malloc(14*512*sizeof(char));
    if (tmp2==NULL){
      log_error("Error memory alloaction getNicTrackBitStream: tmp2:7168 Bytes");
      return RET_ERR;
    }

    // First we need to get the first 256 bytes of t
    memcpy(tmp2,woz1_256B_prologue,256);
    memcpy(tmp2+256,buffer,blockNumber*512);   
    //cmd25SetDataBlocksBareMetal(addr,tmp2,blockNumber+1);                           // <!> Last 10 Bytes are not Data Stream Bytes
    free(tmp2);
  }
        
  return 1;
}

#define WOZ_InfoChunk_offset 12
#define WOZ_TmapChunk_offset 80

enum STATUS mountWozFile(char * filename){
    
    FRESULT fres; 
    FIL fil;  

    for (int i=0;i<160;i++)
      TMAP[i]=255;

    for (int i=0;i<40;i++){
      TRK_BitCount[i]=0;
    }

    fres = f_open(&fil,filename , FA_READ);    
    if(fres != FR_OK){
        log_error("File open Error: (%i)",fres);
        return RET_ERR;
    } 

    long clusty=fil.obj.sclust;
    int i=0;
    fatWozCluster[i]=clusty;
    log_info("file cluster %d:%ld",i,clusty);
  
    while (clusty!=1 && i<30){
        i++;
        clusty=get_fat((FFOBJID*)&fil,clusty);
        fatWozCluster[i]=clusty;
    }

    unsigned int pt;
    char * woz_header=(char*)malloc(6*sizeof(char));
    f_read(&fil,woz_header,4,&pt);
    if (!memcmp(woz_header,"\x57\x4F\x5A\x31",4)){                    //57 4F 5A 31
        log_info("Image:woz version 1");
        wozFile.version=1;
    }else if (!memcmp(woz_header,"\x57\x4F\x5A\x32",4)){
        log_info("Image:woz version 2");
        wozFile.version=2;
    }else{
        log_error("Error: not a woz file");
        return RET_ERR;
    }
    free(woz_header);

    // Getting the Info Chunk
    char *info_chunk=(char*)malloc(66*sizeof(char));
    f_lseek(&fil,12);
    f_read(&fil,info_chunk,60,&pt);

    log_info("info_chunk read:%d",pt);
    
    if (!memcmp(info_chunk,"\x49\x4E\x46\x4F",4)){                  // Little Indian 0x4F464E49  
        
        wozFile.disk_type=(uint8_t)info_chunk[1+8];
        log_info("woz file disk type:%d",wozFile.disk_type);
        
        
        wozFile.is_write_protected=info_chunk[2+8];
        log_info("woz file write protected:%d",wozFile.is_write_protected);
        
        
        wozFile.sync=(int)info_chunk[3+8];
        log_info("woz file write synced:%d",wozFile.sync);
        
        wozFile.cleaned=(int)info_chunk[4+8];
        log_info("woz cleaned:%d",wozFile.cleaned);
        
        if (wozFile.version==2){
          
          wozFile.opt_bit_timing=(int)info_chunk[39+8];
          wozFile.largest_track= info_chunk[44+8] | info_chunk[45+8] << 8;
          log_info("woz opt_bit_timing %d",wozFile.opt_bit_timing);
          log_info("woz largest_track %d",wozFile.largest_track);
        }else{
          wozFile.opt_bit_timing=32;
        }
        
        memcpy(wozFile.creator,info_chunk+5+8,32);
        log_info("woz creator:%s",wozFile.creator);
        
    }else{
        log_error("Error woz info Chunk is not valid");
        return RET_ERR;
    }
    free(info_chunk);

    // Start reading TMAP
    
    char * tmap_chunk=(char *)malloc(168*sizeof(char));
    if (!tmap_chunk){
        log_error("tmap_chunk error");
        return RET_ERR;
    }

    f_lseek(&fil,80);
    f_read(&fil,tmap_chunk,168,&pt);
    
    if (!memcmp(tmap_chunk,"\x54\x4D\x41\x50",4)){          // 0x50414D54          
        for (int i=0;i<160;i++){
            TMAP[i]=tmap_chunk[i+8];
            //log_info("woz TMAP %03d: %02d",i,TMAP[i]);
        }
    }else{
        log_error("Error tmp Chunk is not valid");
        free(tmap_chunk);
        return RET_ERR;
    }
    
    free(tmap_chunk);
    
    if (wozFile.version==2){
        char * trk_chunk=(char *)malloc(1280*sizeof(char));
        if (!trk_chunk)
            return RET_ERR;
        
        f_lseek(&fil,248);
        f_read(&fil,trk_chunk,1280,&pt);

        if (!memcmp(trk_chunk,"\x54\x52\x4B\x53",4)){                                                                 // 0x534B5254          // ERREUR A FIXER ICI

            for (int i=0;i<40;i++){
                TRK_startingBlockOffset[i]=(((unsigned short)trk_chunk[i*8+8+1] << 8) & 0xF00) | trk_chunk[i*8+8];
                TRK_BlockCount[i]=(((unsigned short)trk_chunk[i*8+8+1+2] << 8) & 0xF00) | trk_chunk[i*8+8+2];
                TRK_BitCount[i] = (trk_chunk[i*8+8+3+4]  << 24) | (trk_chunk[i*8+8+2+4] << 16) | (trk_chunk[i*8+8+1+4] << 8) | trk_chunk[i*8+8+4];
                //if (TRK_BlockCount[i]!=0)
                //  log_info("woz trk file offset trk:%03d offset:%02dx512 BlkCount:%d BitCount:%ld",i,TRK_startingBlockOffset[i],TRK_BlockCount[i],TRK_BitCount[i]);
            }
        }else{
            log_error("Error trk Chunk is not valid\n");
            free(trk_chunk);
            return RET_ERR;
        }

        free(trk_chunk);
    }else if (wozFile.version==1){
        log_info("woz file type 1 no trk_chunk\n");
        for (int i=0;i<40;i++){
          TRK_startingBlockOffset[i]=13*i;
          TRK_BlockCount[i]=13;

          char * trk_chunk=(char *)malloc(10*sizeof(char));
          if (!trk_chunk)
            return RET_ERR;

          f_lseek(&fil,256+i*6656+6646);
          f_read(&fil,trk_chunk,10,&pt);

          TRK_BitCount[i] = ((trk_chunk[1] << 8) | trk_chunk[0])*8;
          //log_info("woz trk file offset trk:%03d offset:%02dx512 BlkCount:%d BitCount:%ld",i,TRK_startingBlockOffset[i],TRK_BlockCount[i],TRK_BitCount[i]);
          free(trk_chunk);
        }
        return RET_OK;
    }

    f_close(&fil);
    return RET_OK;
}
