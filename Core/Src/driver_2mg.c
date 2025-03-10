
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_2mg.h"
#include "emul_disk35.h"

#include "main.h"
#include "log.h"

extern long database;                                            // start of the data segment in FAT
extern int csize;  
extern volatile enum FS_STATUS fsState;
unsigned int fat2mgCluster[64];
extern image_info_t mountImageInfo;



#define NIBBLE_BLOCK_SIZE  416 //402
#define NIBBLE_SECTOR_SIZE 512

#define IMG2_HEADER_SIZE 64

/*
static const unsigned char scramble[] = {
	0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
	};
*/

static uint32_t get_u32le(const uint8_t *p) {
    uint32_t ret;
    ret  = ((uint32_t)(const uint8_t)p[0]) << 0;
    ret |= ((uint32_t)(const uint8_t)p[1]) << 8;
    ret |= ((uint32_t)(const uint8_t)p[2]) << 16;
    ret |= ((uint32_t)(const uint8_t)p[3]) << 24;
    return ret;
}

static const unsigned physical_to_prodos_sector_map_35[DISK_35_NUM_REGIONS][16] = {
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, 11, -1, -1, -1, -1},
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, 9, -1, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 1, 5, 2, 6, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1}
    };

static const char gcr_6_2_byte[] = {
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
static const char decTable[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x02,0x03,0x00,0x04,0x05,0x06,
	0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x08,0x00,0x00,0x00,0x09,0x0a,0x0b,0x0c,0x0d,
	0x00,0x00,0x0e,0x0f,0x10,0x11,0x12,0x13,0x00,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1b,0x00,0x1c,0x1d,0x1e,
	0x00,0x00,0x00,0x1f,0x00,0x00,0x20,0x21,0x00,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
	0x00,0x00,0x00,0x00,0x00,0x29,0x2a,0x2b,0x00,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,
	0x00,0x00,0x33,0x34,0x35,0x36,0x37,0x38,0x00,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
};
#pragma  GCC diagnostic pop


static unsigned maxSectorsPerRegion35[DISK_35_NUM_REGIONS] = {12, 11, 10, 9, 8};
static unsigned trackStartperRegion35[DISK_35_NUM_REGIONS + 1] = {0, 32, 64, 96, 128, 160};            // TODO if Single sided it does not work 

__uint32_t IMG2_DataBlockCount;
__uint32_t IMG2_DataOffset;
__uint32_t IMG2_DataByteCount;

static uint16_t diskGetLogicalSectorCountFromTrack(uint8_t diskType, uint8_t trackIndex);
static uint8_t diskGetRegionFromTrack(uint8_t disk_type, u_int8_t track_index);

int get2mgTrackFromPh(int phtrack){
    return phtrack >> 2;
}

unsigned int get2mgTrackSize(int trk){
    
    uint8_t diskRegion=diskGetRegionFromTrack(DISK_TYPE_3_5,trk);
    unsigned trackSectorCount = maxSectorsPerRegion35[diskRegion];
    int trackSize=1+DISK_35_BYTES_TRACK_GAP_1+782*trackSectorCount-55;
    return trackSize;
}

long get2mgSDAddr(int trk,int block,int csize, long database){
    long rSector=-1;
    int long_sector =  block;
    int long_cluster = long_sector >> 6;
    int ft = fat2mgCluster[long_cluster];
    rSector=database+(ft-2)*csize+(long_sector & (csize-1));

  return rSector;
}


enum STATUS get2mgTrackBitStream(int trk,unsigned char * buffer){
    
    uint16_t sectorOffset=diskGetLogicalSectorCountFromTrack(DISK_TYPE_3_5,trk);
    uint8_t diskRegion=diskGetRegionFromTrack(DISK_TYPE_3_5, trk);
	uint8_t trackSectors=maxSectorsPerRegion35[diskRegion];
    
    int addr=get2mgSDAddr(trk,sectorOffset,csize,database);
    const unsigned int blockNumber=trackSectors+1;                  // The start of the Data block is at offset 64 so we need another block to complete 64+sector*512  

    if (addr==-1){
        log_error("Error getting SDCard Address for 2MG\n");
        return RET_ERR;
    }
    int blkSize=blockNumber*512;
    unsigned char * tmp=(unsigned char *)malloc((blkSize)*sizeof(char));
    
    if (tmp==NULL){
        log_error("unable to allocate tmp for %d Bytes",blkSize);
        return RET_ERR;
    }

    getDataBlocksBareMetal(addr,tmp,blockNumber);          // Needs to be improved and to remove the zeros
    while (fsState!=READY){}
    
    //if (diskTrack2Nib(tmp+_2MG_DATA_START_OFFSET,buffer,trk)==RET_ERR){                   // TO BE FIXED
        log_error("diskTrack2Nib return an error");
        free(tmp);
        return RET_ERR;
    //}

    free(tmp);
    return RET_OK;
}

enum STATUS set2mgTrackBitStream(int trk,unsigned char * buffer){
    return RET_OK;
}

enum STATUS mount2mgFile(_2mg_t _2MG, char * filename){
    FRESULT fres; 
    FIL fil;  
    _2MG.blockCount=0;
    _2MG.mounted=0;
    fres = f_open(&fil,filename , FA_READ);     // Step 2 Open the file long naming

    if(fres != FR_OK){
        log_error("File open Error: (%i)\r\n", fres);
        return -1;
    }

    long clusty=fil.obj.sclust;
    int i=0;
    fat2mgCluster[i]=clusty;
    log_info("file cluster %d:%ld\n",i,clusty);
    
    while (clusty!=1 && i<64){
        i++;
        clusty=get_fat((FFOBJID*)&fil,clusty);
        log_info("file cluster %d:%ld",i,clusty);
        fat2mgCluster[i]=clusty;
    }

    unsigned int pt;
    unsigned char * img2_header=(unsigned char*)malloc(IMG2_HEADER_SIZE*sizeof(char));
    f_read(&fil,img2_header,IMG2_HEADER_SIZE,&pt);

    if (!memcmp(img2_header,"2IMG",4)){                    //57 4F 5A 31
        log_info("Image:2mg 2IMG signature");

    }else if (!memcmp(img2_header,"GMI2",4)){
        log_info("Image:2mg GIM2 Signature");
    }else{
        log_error("Error: not a 2mg file");
        return RET_ERR;
    }
    char * creator_id=(char*)malloc(5*sizeof(char));
    memcpy(creator_id,img2_header,4);
   
    creator_id[5]=0x0;

    log_info("creator ID:%s",creator_id);

    IMG2_DataBlockCount=get_u32le(img2_header+0x14);
    IMG2_DataOffset=get_u32le(img2_header+0x18);
    IMG2_DataByteCount=get_u32le(img2_header+0x1c);
    
    if(IMG2_DataBlockCount != 1600 && IMG2_DataBlockCount != 16390){
        log_error("Wrong 512x data block count:%d",IMG2_DataBlockCount);
        //return RET_ERR;
    }else{
        log_info("2MG 512 data blocks:%ld",IMG2_DataBlockCount);
        log_info("2MG Bytes size:%ld",IMG2_DataByteCount);
    }
		
    free(img2_header);
    f_close(&fil);
    _2MG.blockCount=IMG2_DataBlockCount;
    _2MG.isDoubleSided=1;
    _2MG.mounted=1;

    return RET_OK;
}


static uint8_t diskGetRegionFromTrack(uint8_t disk_type, u_int8_t track_index) {
    uint8_t diskRegion = 0;
    if (disk_type == DISK_TYPE_3_5) {
        diskRegion = 1;
        for (; diskRegion < DISK_35_NUM_REGIONS + 1; ++diskRegion) {
            if (track_index < trackStartperRegion35[diskRegion]) {
                diskRegion--;
                break;
            }
        }
    }
    return diskRegion;
}

static uint16_t diskGetLogicalSectorCountFromTrack(uint8_t diskType, uint8_t trackIndex){
    uint16_t sector=0;
    for (uint8_t i=0;i<trackIndex;i++){
        uint8_t diskRegion=diskGetRegionFromTrack(diskType,i);
        sector+=maxSectorsPerRegion35[diskRegion];
        //printf("trk:%d region:%d sectors:%d\n",i,diskRegion,sector);
    }
    return sector;
}


static void nibEncodeData35( const uint8_t *dataSrc,uint8_t * dataTarget, unsigned cnt) {
    /* decoded bytes are encoded to GCR 6-2 8-bit bytes*/
    uint8_t scratch0[175], scratch1[175], scratch2[175];
    uint8_t data[524];
    unsigned chksum[3];
    
    uint16_t dataTargetIndx=0;

    unsigned data_idx = 0, scratch_idx = 0;
    uint8_t v;

    //assert(cnt == 512);
    /* IIgs - 12 byte tag header is blank, but....
       TODO: what if it isn't??  */

    memset(data, 0, DISK_NIB_SECTOR_DATA_TAG_35);
    memcpy(data + DISK_NIB_SECTOR_DATA_TAG_35, dataSrc, 512);

    data_idx = 0;

    /* split incoming decoded nibble data into parts for encoding into the
       final encoded buffer

       shamelessly translated from Ciderpress Nibble35.cpp as the encoding
       scheme is quite involved - you stand on the shoulders of giants.
    */

    chksum[0] = chksum[1] = chksum[2] = 0;
    while (data_idx < 524) {
        chksum[0] = (chksum[0] & 0xff) << 1;
        if (chksum[0] & 0x100) {
            ++chksum[0];
        }
        v = data[data_idx++];
        chksum[2] += v;
        if (chksum[0] & 0x100) {
            ++chksum[2];
            chksum[0] &= 0xff;
        }
        scratch0[scratch_idx] = (v ^ chksum[0]) & 0xff;
        v = data[data_idx++];
        chksum[1] += v;
        if (chksum[2] > 0xff) {
            ++chksum[1];
            chksum[2] &= 0xff;
        }
        scratch1[scratch_idx] = (v ^ chksum[2]) & 0xff;

        if (data_idx < 524) {
            v = data[data_idx++];
            chksum[0] += v;
            if (chksum[1] > 0xff) {
                ++chksum[0];
                chksum[1] &= 0xff;
            }
            scratch2[scratch_idx] = (v ^ chksum[1]) & 0xff;
            ++scratch_idx;
        }
    }
    scratch2[scratch_idx++] = 0;

    for (data_idx = 0; data_idx < scratch_idx; ++data_idx) {
        v = (scratch0[data_idx] & 0xc0) >> 2;
        v |= (scratch1[data_idx] & 0xc0) >> 4;
        v |= (scratch2[data_idx] & 0xc0) >> 6;

        dataTarget[dataTargetIndx++]=gcr_6_2_byte[ v & 0x3f];
        dataTarget[dataTargetIndx++]=gcr_6_2_byte[ scratch0[data_idx] & 0x3f];
        dataTarget[dataTargetIndx++]=gcr_6_2_byte[ scratch1[data_idx] & 0x3f];

        if (data_idx < scratch_idx - 1) {
            dataTarget[dataTargetIndx++]=gcr_6_2_byte[ scratch2[data_idx] & 0x3f];
        }
    }

    /* checksum */
    
    v  = (chksum[0] & 0xc0) >> 6;
    v |= (chksum[1] & 0xc0) >> 4;
    v |= (chksum[2] & 0xc0) >> 2;
    
    dataTarget[dataTargetIndx++]=gcr_6_2_byte[ v & 0x3f];
    dataTarget[dataTargetIndx++]=gcr_6_2_byte[ chksum[2] & 0x3f];
    dataTarget[dataTargetIndx++]=gcr_6_2_byte[ chksum[1] & 0x3f];
    dataTarget[dataTargetIndx]=gcr_6_2_byte[ chksum[0] & 0x3f];

    //printf("dataTargetIndx:%d\n",dataTargetIndx+1);
    
}


/**
  * @brief Button debouncer that reset the Timer 4
  * @param img struct of the 2MG, buffer pointing to the start of the track, trk number
  * @retval None
  */
enum STATUS diskTrack2Nib(_2mg_t _2MG,unsigned char *buffer,unsigned char * nibBuffer,uint8_t trk){

    /*uint8_t qtr_tracks_per_track=0;
    
    if (_2MG.isDoubleSided) {   
        qtr_tracks_per_track = 1;
    } else {
        qtr_tracks_per_track = 2;
    }*/
    uint8_t diskRegion=diskGetRegionFromTrack(DISK_TYPE_3_5,trk);
    unsigned track_sector_count = maxSectorsPerRegion35[diskRegion];
    
    //  TRK 0: (0,1) , TRK 1: (2,3), and so on. and track encoded
    unsigned logical_track_index = trk / 2;
    unsigned logical_side_index = trk % 2;
    //unsigned nib_track_index = trk / qtr_tracks_per_track;
    
    uint8_t side_index_and_track_64 = (logical_side_index << 5) | (logical_track_index >> 6);
    uint8_t sector_format = (_2MG.isDoubleSided ? 0x20 : 0x00) | 0x2;
    
    // Now start to Nibblize
    //int nibBufferSize=1+DISK_35_BYTES_TRACK_GAP_1+782*track_sector_count-55;
    //char nibBuffer[nibBufferSize];                  // 1+100*5+(8+5+4+1+703+2+5+52)*sector-55                                                                         // TO be computed too dirty
    uint16_t nibByteIndx=0;                         //501+sector*782-55

    // PART 1 TRACK PROLOGUE SYNC 400*10 or 500*8
    const char gap50[]={ 0x3F,0xCF,0xF3,0xFC,0xFF};
    nibBuffer[nibByteIndx]= 0xFF;
    nibByteIndx++;

    for (int i=0;i<DISK_35_BYTES_TRACK_GAP_1/5;i++){
        memcpy(nibBuffer+nibByteIndx,gap50,5);
        nibByteIndx+=5;
    }

    // PART 2 
                                                   
    for (uint8_t sector= 0; sector < track_sector_count; sector++) {
        unsigned logical_sector = physical_to_prodos_sector_map_35[diskRegion][sector];
        uint8_t * dataSrc=buffer+ (logical_sector) * 512;
        
        unsigned temp;
        
        nibBuffer[nibByteIndx]= 0xFF;
        nibByteIndx++;
        
        nibBuffer[nibByteIndx++]=0xD5;                                                      // Address field start sginature
        nibBuffer[nibByteIndx++]=0xAA;
        nibBuffer[nibByteIndx++]=0x96;


        //  ADDRESS (prologue, header, epilogue) note the combined address
        //  segment differs from the 5.25" version
        //  track, sector, side, format (0x12 or 0x22 or 0x14 or 0x24)
        //  format = sides | interleave where interleave should always be 2
        
        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)(logical_track_index & 0xff) & 0x3f];
        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)(logical_sector & 0xff) & 0x3f];
        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)(side_index_and_track_64 & 0xff) & 0x3f];
        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)(sector_format) & 0x3f];
        
        temp = (logical_track_index ^ logical_sector ^ side_index_and_track_64 ^ sector_format);
        printf("lit:%d ls:%d sit64:%d sf:%d temp:%d\n",logical_track_index& 0xff,logical_sector & 0xff,side_index_and_track_64 & 0xff,sector_format,temp);
        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)(temp) & 0x3f];
        
        nibBuffer[nibByteIndx++]=0xDE;
        nibBuffer[nibByteIndx++]=0xAA;
        nibBuffer[nibByteIndx++]=0xFF;                                                      // Closing of the Address field signature

        memcpy(nibBuffer+nibByteIndx,gap50,5);
        nibByteIndx+=5;

        nibBuffer[nibByteIndx++]= 0xFF;

        nibBuffer[nibByteIndx++]=0xD5;                                                      // 0xD5AAAD Data block start signature
        nibBuffer[nibByteIndx++]=0xAA;
        nibBuffer[nibByteIndx++]=0xAD;

        nibBuffer[nibByteIndx++]=gcr_6_2_byte[ (uint8_t)logical_sector & 0x3f];             // Add the sector at the beginning
        
        uint8_t * dataTarget=(uint8_t *)malloc(703*sizeof(uint8_t));                        // 512 Datablock become a 702 Bytes with GCR6_2
        
        nibEncodeData35( dataSrc,dataTarget,512);                                           // GCR encode the block
        memcpy(nibBuffer+nibByteIndx,dataTarget,703);                                       // Copy the nibble into the target, this could be avoided by passing the address of nibBuffer
        free(dataTarget);                                                                   // Free the memory
        nibByteIndx+=703;                                                                   // Increment the index
        
        nibBuffer[nibByteIndx++]=0xDE;
        nibBuffer[nibByteIndx++]=0xAA;

        if (sector + 1 < track_sector_count) {
            nibBuffer[nibByteIndx++]=0xFF;
            nibBuffer[nibByteIndx++]=0xFF;
            nibBuffer[nibByteIndx++]=0xFF;

            for (int i=0;i<DISK_35_BYTES_TRACK_GAP_3/5;i++){
                memcpy(nibBuffer+nibByteIndx,gap50,5);
                nibByteIndx+=5;
            }
            nibBuffer[nibByteIndx++]=0x3F;
            nibBuffer[nibByteIndx++]=0xCF;
        }
    }
    //printf("nibByteIndx:%d\n",nibByteIndx);
    //print_packet2(nibBuffer,nibByteIndx,logical_track_index,0);
    return RET_OK;
}


enum STATUS nic22mg(char *rawByte,unsigned char *buffer,uint8_t trk){
return RET_OK;
}




