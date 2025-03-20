#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_woz.h"
#include "main.h"
#include "log.h"

__uint8_t TMAP[160];
__uint16_t TRK_startingBlockOffset[MAX_TRACK];
__uint16_t TRK_BlockCount[MAX_TRACK];
__uint32_t TRK_BitCount[MAX_TRACK];

__uint8_t TRK_maxIndx=0;

extern FATFS fs;
extern long database;                                            // start of the data segment in FAT
extern int csize;  
extern volatile enum FS_STATUS fsState;
unsigned int fatWozCluster[20];
char * woz1_256B_prologue;                                       // needed to store the potential overwrite

woz_info_t wozFile;
//unsigned max_sectors_per_region_35[DISK_35_NUM_REGIONS] = {12, 11, 10, 9, 8};
//unsigned track_start_per_region_35[DISK_35_NUM_REGIONS + 1] = {0, 32, 64, 96, 128, 160};

static unsigned maxSectorsPerRegion35[DISK_35_NUM_REGIONS] = {19, 17, 16, 14, 13};
//static unsigned trackStartperRegion35DoubleHead[DISK_35_NUM_REGIONS + 1] = {0, 32, 64, 96, 128, 160};            // TODO if Single sided it does not work 
static unsigned trackStartperRegion35SingleHead[DISK_35_NUM_REGIONS + 1] = {0, 16, 32, 48, 64, 80};            // TODO if Single sided it does not work 

static uint16_t diskGetLogicalSectorCountFromTrack( uint8_t trackIndex,uint8_t head);
static uint16_t diskGetLogicalSectorNumberFromTrack( uint8_t trackIndex,uint8_t head);
static uint8_t diskGetRegionFromTrack( u_int8_t track_index,uint8_t head);


int getWozTrackFromPh(int phtrack){

  return TMAP[phtrack];
}
unsigned int getWozTrackSize(int trk){
  
  if (trk>(MAX_TRACK-1))
    trk=MAX_TRACK-1;

  unsigned int B=TRK_BitCount[trk];
  return B;
}

long getWozSDAddr(int trk,int block,int csize, long database){
  long rSector=-1;
  if (wozFile.version==2){
    int long_sector = TRK_startingBlockOffset[trk] + block;
    //int long_sector=3+trk*13;
    //log_debug("long_sector: %d",long_sector);                         // <!> TODO debug To be removed in production
    int long_cluster = long_sector >> 6;
    int ft = fatWozCluster[long_cluster];
    rSector=database+(ft-2)*csize+(long_sector & (csize-1));

  }else if (wozFile.version==1){
    int long_sector = 13*trk;                                         // 13 block of 512 per track
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
    fsState=READY;
    return RET_ERR;
  }
  f_lseek(&fil,long_sector*512);
  unsigned int pt;
  fres = f_read(&fil,buffer,6656,&pt);
  if(fres != FR_OK){
    log_error("File read Error: (%i)",fres);
    fsState=READY;
    return RET_ERR;
  }
  f_close(&fil);
  fsState=READY;
  return RET_OK;

}

enum STATUS getWozTrackBitStream(int trk,unsigned char * buffer){
  const unsigned int blockNumber=13; 
  int addr=getWozSDAddr(trk,0,csize,database);
  
  if (addr==-1){
    log_error("Error getting SDCard Address for woz\n");
    return RET_ERR;
  }
  
  if (wozFile.version==2){

    getDataBlocksBareMetal(addr,buffer,blockNumber);
    while (fsState!=READY){} 

  }else if (wozFile.version==1){
    
    unsigned char * tmp2=(unsigned char*)malloc((blockNumber+1)*512*sizeof(char));
    if (tmp2==NULL){
      log_error("Error memory alloaction getWozTrackBitStream: tmp2:8192 Bytes");
      return RET_ERR;
    }

    getDataBlocksBareMetal(addr,tmp2,blockNumber+1);
    while (fsState!=READY){} 
                                                                                      // we need to wait here... with the DMA
    memcpy(buffer,tmp2+256,blockNumber*512-10);                                       // Last 10 Bytes are not Data Stream Bytes
    woz1_256B_prologue=malloc(256*sizeof(char));
    memcpy(woz1_256B_prologue,tmp2,256);                                              // we need this to speed up the write process
    free(tmp2);
  }
        
  return 1;
}

enum STATUS setWozTrackBitStream(int trk,unsigned char * buffer){
  const unsigned int blockNumber=13; 
  int addr=getWozSDAddr(trk,0,csize,database);
  
  if (trk==255){
    log_error("Error can not write to track 255");
    return RET_ERR;
  }

  if (addr==-1){
    log_error("Error getting SDCard Address for woz");
    return RET_ERR;
  }
  
  if (wozFile.version==2){
    setDataBlocksBareMetal(addr,buffer,blockNumber);
    while (fsState!=READY){};
    
  }else if (wozFile.version==1){
    
    unsigned char * tmp2=(unsigned char*)malloc(14*512*sizeof(char));
    if (tmp2==NULL){
      log_error("Error memory alloaction getNicTrackBitStream: tmp2:7168 Bytes");
      return RET_ERR;
    }

    // First we need to get the first 256 bytes of t
    memcpy(tmp2,woz1_256B_prologue,256);
    memcpy(tmp2+256,buffer,blockNumber*512);
    setDataBlocksBareMetal(addr,tmp2,14); 
    while (fsState!=READY){};  
    
    free(tmp2);
  }
  
  return RET_OK;
}

#define WOZ_InfoChunk_offset 12
#define WOZ_TmapChunk_offset 80

enum STATUS mountWozFile(char * filename){
    
    FRESULT fres; 
    FIL fil;  
    TRK_maxIndx=0;

    for (int i=0;i<160;i++)
      TMAP[i]=255;

    for (int i=0;i<MAX_TRACK;i++){
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
          log_debug("woz TMAP %03d: %02d",i,TMAP[i]);
          
          if (TMAP[i]!=255 && TMAP[i]>TRK_maxIndx)
            TRK_maxIndx=TMAP[i];
          
          if (TRK_maxIndx>MAX_TRACK)
            TRK_maxIndx=MAX_TRACK;
        }

        log_info("woz TMAP max track %02d",TRK_maxIndx);
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

            for (int i=0;i<=TRK_maxIndx;i++){
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
        for (int i=0;i<=TRK_maxIndx;i++){
          TRK_startingBlockOffset[i]=13*i;
          TRK_BlockCount[i]=13;

          char * trk_chunk=(char *)malloc(10*sizeof(char));
          if (!trk_chunk)
            return RET_ERR;

          f_lseek(&fil,256+i*6656+6646);
          f_read(&fil,trk_chunk,10,&pt);
          //for (int i=0;i<10;i++){
          //  log_info("%2X;",trk_chunk[i]);
          //}
          TRK_BitCount[i] = ((trk_chunk[3] << 8) | trk_chunk[2]);
          //log_info("woz trk file offset trk:%03d offset:%02dx512 BlkCount:%d BitCount:%ld",i,TRK_startingBlockOffset[i],TRK_BlockCount[i],TRK_BitCount[i]);
          free(trk_chunk);
        }
        return RET_OK;
    }

    f_close(&fil);
    return RET_OK;
}


static uint8_t diskGetRegionFromTrack( u_int8_t track_index, uint8_t head) {
  uint8_t diskRegion = 0;
 
  diskRegion = 1;
  uint8_t compTrackIndex=track_index;
  if (head==2)
    compTrackIndex>>=1;

  for (; diskRegion < DISK_35_NUM_REGIONS + 1; ++diskRegion) {
      if (    compTrackIndex< trackStartperRegion35SingleHead[diskRegion]) {
          diskRegion--;
          break;
      }
  }

  return diskRegion;
}
static uint16_t diskGetLogicalSectorNumberFromTrack( uint8_t trackIndex, uint8_t head){
  uint16_t sector=0;
  uint8_t diskRegion=0;
  
  diskRegion=diskGetRegionFromTrack(trackIndex,head);

  sector=maxSectorsPerRegion35[diskRegion];
      
  return sector;
}
static uint16_t diskGetLogicalSectorCountFromTrack( uint8_t trackIndex,uint8_t head){
  uint16_t sector=0;
  uint8_t diskRegion=0;
  for (uint8_t i=0;i<trackIndex;i++){
     
      diskRegion=diskGetRegionFromTrack(i,head);
      sector+=maxSectorsPerRegion35[diskRegion];
      //printf("trk:%d region:%d sectors:%d\n",i,diskRegion,sector);
  }
  return sector;
}

enum STATUS createBlankWozFile(char * filename, uint8_t version,uint8_t diskFormat,uint8_t head){
  
  FRESULT fres; 
  FIL fil;

  if (version!=1 && version!=2){
    log_error("Woz version should 1 or 2");
    return RET_ERR;
  }

  //unlinkImageFile(filename);

  fsState=BUSY;
  fres = f_open(&fil,filename ,  FA_WRITE | FA_CREATE_ALWAYS );    
  
  if(fres != FR_OK){
    log_error("File open Error: (%i)",fres);
    fsState=READY;
    return RET_ERR;
  }

  f_printf(&fil,"WOZ%d",version);
  f_putc(0xFF,&fil);
  f_putc(0x0A,&fil);
  f_putc(0x0D,&fil);
  f_putc(0x0A,&fil);
  f_putc(0x00,&fil);
  f_putc(0x00,&fil);
  f_putc(0x00,&fil);
  f_putc(0x00,&fil);

  // INFO CHUNK

  f_printf(&fil,"INFO");
  
  f_putc(60,&fil);                              // 16       uint32  Chunk Size  Size is always 60.
  f_putc(0x0,&fil);
  f_putc(0x0,&fil);
  f_putc(0x0,&fil);

  f_putc(version,&fil);                         // 20   +0  uint8   INFO Version        Version number of the INFO chunk.Current version is 3.
  f_putc(diskFormat,&fil);                      // 21   +1  uint8   Disk Type           1 = 5.25, 2 = 3.5
  f_putc(0,&fil);                               // 22   +2  uint8   Write Protection    1 = Floppy is write protected
  f_putc(0,&fil);                               // 23   +3  uint8   Synchronized        1 = Cross track sync was used during imaging
  f_putc(0,&fil);                               // 24   +4  uint8   Cleaned             1 = MC3470 fake bits have been removed
  f_printf(&fil,"%-32s","R3TR0.net VIBR2025");  // 25   +5  UTF-8 32 Creator            Name of software that created the WOZ file. 
                                                //                                      String in UTF-8. No BOM. Padded to 32 bytes using space character (0x20). ex: “Applesauce v1.0"
  f_putc(head,&fil);                            // 57   +37 uint8   Disk Sides          The number of disk sides contained within this image. A 5.25 disk will always be 1. A 3.5 disk can be 1 or 2.
  f_putc(0,&fil);                               // 58   +38 uint8   Boot Sector Format  The type of boot sector found on this disk. This is only for 5.25 disks. 3.5 disks should just set this to 0.
                                                //                          0 = Unknown
                                                //                          1 = Contains boot sector for 16-sector
                                                //                          2 = Contains boot sector for 13-sector
                                                //                          3 = Contains boot sectors for both
  if (diskFormat==1)                            // 59   +39 uint8   Optimal Bit Timing
    f_putc(32,&fil);                            
  else if (diskFormat==2)
    f_putc(16,&fil);
                                                // 60   +40 uint16  Compatible Hardware
  uint16_t hc= 0x0001 |                         //   = Apple ][
    0x0002         |                            //   = Apple ][ Plus
    0x0004         |                            //   = Apple //e (unenhanced)
    0x0008         |                            //   = Apple //c
    0x0010         |                            //   = Apple //e Enhanced
    0x0020         |                            //   = Apple IIgs
    0x0040;                                     //   = Apple //c Plus
                                                //    0x0080 = Apple ///
                                                //    0x0100 = Apple /// Plus 
    f_putc(hc,&fil);
    f_putc(0,&fil);

    f_putc(0,&fil);                             // 62   +42 uint16  Required RAM Minimum RAM size needed for this software. This value is in K (1024 bytes). If the minimum size is unknown, this value should be set to 0. So, a requirement of 64K would be indicated by the value 64 here.  
    f_putc(0,&fil);                             // 
    
    f_putc(0,&fil);                             // Largest Track
    f_putc(0,&fil);                             // 
    
    f_putc(0,&fil);                             // FLUX Block
    f_putc(0,&fil);                             // 
    
    f_putc(0,&fil);                             // Largest Flux Track
    f_putc(0,&fil);                             // 

    for (uint8_t i=0;i<10;i++){
      f_putc(0,&fil);
    }


  // TMAP CHUNK

  f_printf(&fil,"TMAP");
  
  f_putc(160,&fil);                         // 84   uint32    Size is always 160. 
  f_putc(0x0,&fil);
  f_putc(0x0,&fil);
  f_putc(0x0,&fil);

  if (diskFormat==1){                       // 5.25 TRK 
    f_putc(0,&fil);
    f_putc(0,&fil);
    f_putc(0xFF,&fil);                                        
    for (uint8_t trk=1;trk<35;trk++){
      f_putc(trk,&fil);
      f_putc(trk,&fil);
      f_putc(trk,&fil);
      f_putc(0xFF,&fil);
    }
    for (uint8_t trk=0;trk<21;trk++){
      f_putc(0xFF,&fil);
    }
  }else if (diskFormat==2){                 // 3.5 TRK 
    if (head==1){
      for (uint8_t trk=0;trk<80;trk++){
          f_putc(trk,&fil);
      }
      for (uint8_t trk=0;trk<80;trk++){
        f_putc(0xFF,&fil);
      }
    }else if (head==2){
      for (uint8_t trk=0;trk<160;trk++){
        f_putc(trk,&fil);
      }
    }

  }

  // TRKS CHUNK

  f_printf(&fil,"TRKS");
  unsigned int *bw=0;
  long chunkSize=0;
  if (diskFormat==1){
    chunkSize=13*35*512+1024+256;
     
  }else if (diskFormat==2){
    chunkSize=(diskGetLogicalSectorCountFromTrack(80,1)+(12))*512;                 // 13 is last track -3 is  
  }
  
  f_write(&fil,&chunkSize,4,bw);

  if (diskFormat==1){                     // 5.25 TRK 
    for (uint16_t trk=0;trk<35;trk++){
      int sectorIndx=3+trk*13;
      f_write(&fil,&sectorIndx,2,bw);
      
      f_putc(13,&fil);
      f_putc(0x0,&fil);

      f_putc(0x50,&fil);
      f_putc(0xC7,&fil);
      f_putc(0x0,&fil);
      f_putc(0x0,&fil);
    }

  }else if (diskFormat==2){
    u_int8_t numTrk=80;
    if (head==2)
      numTrk=160;
    
    int sectorIndx=3;
    f_write(&fil,&sectorIndx,2,bw);
    uint8_t sectorNum=diskGetLogicalSectorNumberFromTrack(0,head);
    
    f_putc(sectorNum,&fil);
    f_putc(0x0,&fil);
    
    uint32_t bitNum=sectorNum*512;
    
    f_write(&fil,&bitNum,4,bw);

    for (uint8_t trk=1;trk<numTrk;trk++){

        sectorIndx=3+diskGetLogicalSectorCountFromTrack(trk,head);

        //printf("trk:%d sectorIndx:%d hex:%04X\n",trk,sectorIndx,sectorIndx);
        
        f_write(&fil,&sectorIndx,2,bw);
        
        sectorNum=diskGetLogicalSectorNumberFromTrack(trk,head);
        
        f_putc(sectorNum,&fil);
        f_putc(0x0,&fil);
         bitNum=sectorNum*512;
        f_write(&fil,&bitNum,4,bw);
    }
  }

  // LAST PART WRITE WHITE SPACE CORRESPONDING TO THE NUMBER OF SECTORS * 512

  int sectorIndex=0;
  if (diskFormat==1){
    sectorIndex=13*35;
   
  }else if (diskFormat==2){
    u_int8_t numTrk=80;
    if (head==2)
      numTrk=160;
    
    sectorIndex=diskGetLogicalSectorCountFromTrack(numTrk,head)+12;     // Do not forget the last one track (13-1)
  }

  for (int i=0;i<sectorIndex;i++){
    for (int b=0;b<512;b++){
        f_putc(0x0,&fil);
    }
  }

  f_close(&fil);
  
  fres = f_open(&fil,filename , FA_READ | FA_WRITE | FA_OPEN_EXISTING);    
  
  if(fres != FR_OK){
    log_error("File open Error: (%i)",fres);
    fsState=READY;
    return RET_ERR;
  }
  
  f_lseek(&fil,12);
  uint32_t crc32=getWozCrc32(&fil);
  f_lseek(&fil,8);
  f_write(&fil,&crc32,4,bw);
  log_info("crc:%04X",crc32);
  f_close(&fil);
  fsState=READY;
  /*
    char tmp[1536];
    fres = f_open(&fil,filename , FA_READ );    
    int br=0;
    if(fres != FR_OK){
      log_error("File open Error: (%i)",fres);
      return RET_ERR;
    }
    f_read(&fil,&tmp,1536,&br);

    dumpBuf(tmp,0,1536);
  */
  return RET_OK;
}

static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t getWozCrc32(FIL * fil){
	
  /*
  Due to meory limitation this function from Applesauce needs to be adapted uisng the file pointer from fat_fs
  */

	char ch[64];                                  // Chunk of 64 instead of single char is obviously faster ;)
	uint32_t crc =0;
  crc = crc ^ ~0U;
  unsigned int br=0;
	while (!f_eof(fil)){
    f_read(fil,&ch,64,&br);
    for (uint8_t i=0;i<br;i++){
	    crc = crc32_tab[(crc ^ ch[i]) & 0xFF] ^ (crc >> 8);
    }

  }
  return crc ^ ~0U;
}
