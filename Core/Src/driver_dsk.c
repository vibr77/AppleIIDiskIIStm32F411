
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_dsk.h"
#include "main.h"
#include "log.h"

extern long database;                                            // start of the data segment in FAT
extern int csize;  
extern volatile enum FS_STATUS fsState;
unsigned int fatDskCluster[20];
extern image_info_t mountImageInfo;

#define NIBBLE_BLOCK_SIZE  408 // Anything above 406 seems to work fine
#define ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86

enum BITSTREAM_PARSING_STAGE{N,SEARCH_ADDR,READ_ADDR,SEARCH_DATA,READ_DATA};

static enum STATUS nib2dsk(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError);
static enum STATUS dsk2Nib(unsigned char *rawByte,unsigned char *buffer,uint8_t trk);

static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack);
static enum STATUS decodeGCR6_2(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc);

static const unsigned char signatureAddrStart[]	={0xD5,0xAA,0x96};
static const unsigned char signatureDataStart[]	={0xD5,0xAA,0xAD};

static uint8_t  sectorCheckArray[16];
static uint8_t  dsk2nibSectorMap[]         = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
static uint8_t  po2nibSectorMap[]         =  {0, 8,  1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15};

static const uint8_t from_gcr_6_2_byte[128] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0x80-0x87
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0x88-0x8F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,     // 0x90-0x97
    0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,     // 0x98-0x9F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,     // 0xA0-0xA7
    0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,     // 0xA8-0xAF
    0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,     // 0xB0-0xB7
    0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,     // 0xB8-0xBF
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0xC0-0xC7
    0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,     // 0xC8-0xCF
    0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,     // 0xD0-0xD7
    0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,     // 0xD8-0xDF
    0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,     // 0xE0-0xE7
    0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,     // 0xE8-0xEF
    0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,     // 0xF0-0xF7
    0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f      // 0xF8-0xFF
};

static const char encTable[] = {
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

// for bit flip

static const unsigned char FlipBit1[4] = { 0, 2, 1, 3 };
static const unsigned char FlipBit2[4] = { 0, 8, 4, 12 };
static const unsigned char FlipBit3[4] = { 0, 32, 16, 48 };

int getDskTrackFromPh(int phtrack){
    return phtrack >> 2;
}

unsigned int getDskTrackSize(int trk){
    return 16*NIBBLE_BLOCK_SIZE*8;
}

long getDskSDAddr(int trk,int block,int csize, long database){
    int long_sector = trk*8;                                                                    // DSK & PO are 256 long and not 512 a track is 4096
    int long_cluster = long_sector >> 6;
    int ft = fatDskCluster[long_cluster];
    long rSector=database+(ft-2)*csize+(long_sector & (csize-1));
    return rSector;
}

enum STATUS getDskTrackBitStream(int trk,unsigned char * buffer){
    long addr=getDskSDAddr(trk,0,csize,database);
    const unsigned int blockNumber=8;                                                           // 8 blocks of 512 bytes to read a full track          

    if (addr==-1){
        log_error("Error getting SDCard Address for DSK\n");
        return RET_ERR;
    }

    unsigned char * tmp=(unsigned char *)malloc(4096*sizeof(char));
    if (tmp==NULL){
        log_error("unable to allocate tmp for 4096 Bytes");
        return RET_ERR;
    }

    while (fsState!=READY){}                                                                    // Wait for the FS to be READY                  
    getDataBlocksBareMetal(addr,tmp,blockNumber);                                               // Needs to be improved and to remove the zeros
    while (fsState!=READY){}                                                                    // Wait for the end of the read operation before processing data
    
    if (dsk2Nib(tmp,buffer,trk)==RET_ERR){
        log_error("dsk2nib return an error");
        free(tmp);
        return RET_ERR;
    }

    free(tmp);
    return RET_OK;
}

enum STATUS mountDskFile(char * filename){
    FRESULT fres; 
    FIL fil;  

    fres = f_open(&fil,filename , FA_READ);     // Step 2 Open the file long naming

    if(fres != FR_OK){
        log_error("File open Error: (%i)\r\n", fres);
        return -1;
    }

    long clusty=fil.obj.sclust;
    int i=0;
    fatDskCluster[i]=clusty;
    log_info("file cluster %d:%ld\n",i,clusty);
    
    while (clusty!=1 && i<30){
        i++;
        clusty=get_fat((FFOBJID*)&fil,clusty);
        log_info("file cluster %d:%ld",i,clusty);
        fatDskCluster[i]=clusty;
    }

    f_close(&fil);

    return 0;
}

static enum STATUS dsk2Nib(unsigned char *rawByte,unsigned char *buffer,uint8_t trk){

    const unsigned char volume = 0xfe;
    int i=0;
    char dst[512];
    char src[256+2];
    
    unsigned char * sectorMap;
    if (mountImageInfo.type==2)
        sectorMap=dsk2nibSectorMap;
    else if (mountImageInfo.type==3)
        sectorMap=po2nibSectorMap;
    else{
        log_error("Unable to match sectorMap with mountImageInfo.type");
        return RET_ERR;
    }
        
    for (i=0; i<0x16; i++){
            dst[i]=0xff;
    } 
    
    // sync header
    dst[0x16]=0xf3;
    dst[0x17]=0xfc;
    dst[0x18]=0xff;
    dst[0x19]=0x3f;
    dst[0x1a]=0xcf;
    dst[0x1b]=0xf3;
    dst[0x1c]=0xfc;
    dst[0x1d]=0xff;
    dst[0x1e]=0x3f;
    dst[0x1f]=0xcf;
    dst[0x20]=0xf3;
    dst[0x21]=0xfc;	

    // address header
    dst[0x22]=0xd5;
    dst[0x23]=0xaa;
    dst[0x24]=0x96;
    dst[0x2d]=0xde;
    dst[0x2e]=0xaa;
    dst[0x2f]=0xeb;
    
    // sync header
    for (i=0x30; i<0x35; i++) 
        dst[i]=0xff;

    // data
    dst[0x35]=0xd5;
    dst[0x36]=0xaa;
    dst[0x37]=0xad;
    dst[0x18f]=0xde;
    dst[0x190]=0xaa;
    dst[0x191]=0xeb;
    
    for (i=0x192; i<NIBBLE_BLOCK_SIZE; i++) 
        dst[i]=0xff;
    
    for (uint8_t sector=0;sector<16;sector++){
        uint8_t sm=sectorMap[sector];
        memcpy(src,rawByte+sm * 256,256);
        src[256] = src[257] = 0;

        unsigned char c, x, ox = 0;

        dst[0x25]=((volume>>1)|0xaa);
        dst[0x26]=(volume|0xaa);
        dst[0x27]=((trk>>1)|0xaa);
        dst[0x28]=(trk|0xaa);
        dst[0x29]=((sector>>1)|0xaa);
        dst[0x2a]=(sector|0xaa);

        c = (volume^trk^sector);
        dst[0x2b]=((c>>1)|0xaa);
        dst[0x2c]=(c|0xaa);

        for (i = 0; i < 86; i++) {
            x = (FlipBit1[src[i] & 3] | FlipBit2[src[i + 86] & 3] | FlipBit3[src[i + 172] & 3]);
			dst[i+0x38] = encTable[(x^ox)&0x3f];
            ox = x;
        }

        for (i = 0; i < 256; i++) {
            x = (src[i] >> 2);
            dst[i+0x8e] = encTable[(x ^ ox) & 0x3f];
            ox = x;
        }
        
        dst[0x18e]=encTable[ox & 0x3f];
        memcpy(buffer+sector*NIBBLE_BLOCK_SIZE,dst,NIBBLE_BLOCK_SIZE);
    }
    return RET_OK;
}

uint8_t wr_retry=0;                                                                             // DEBUG ONLY

enum STATUS setDskTrackBitStream(int trk,unsigned char * buffer){
    
    uint8_t retE=0x0;
    
    unsigned char * dskData=(unsigned char *)malloc(4096*sizeof(unsigned char));
    if (dskData==NULL){
        log_error("Unable to allocate dskData for 4096 Bytes");
        return RET_ERR;
    } 
    
    if (nib2dsk((unsigned char *)dskData,buffer,trk,16*NIBBLE_BLOCK_SIZE,&retE)==RET_ERR){
        printf("dsk e:%d\n",retE);
        //free(dskData);                                                    // Memory is not freed at this stage as the process continues later
        //return RET_ERR;                                                   // Even if there is an error the track is written partially
    }

    long addr=getDskSDAddr(trk,0,csize,database);
    while (fsState!=READY){};

    setDataBlocksBareMetal(addr,dskData,8); 
    while (fsState!=READY){};                                               // wait for write end, to avoid freeing the buffer too early
    free(dskData);  
    
    return RET_OK; 
}

static enum STATUS nib2dsk(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError){

   	unsigned char ch=0x0;
    unsigned char byteWindow=0x0;                                                               // byte window of the data stream
    unsigned char tmpBuffer[346];                                                               // temp buffer for Addr, data block & GCR decoding

   	int i=0;                                                                                    // Buffer index in the infinite while loop
   	int indxOfFirstAddr=-1;                                                                     // Byte address on the stream for the first Addressblock, to check if a full stream loop is done before exiting
   	
   	uint8_t  logicalSector=0;                                                                   // nibSector from the trackstream
   	uint8_t  physicalSector=0;                                                                  // disk Sector number of applying the DOS or PRODOS sector ordering
   	uint8_t  sumSector=0x0;                                                                     // calculate the sum of sector to confirm a complete track is here
   	uint8_t  dskTrack=0x0;                                                                      // track from the addr decoded block use to check with the function variable
    uint8_t  cksum_out, cksum_calc;                                                             // GCR 6_2 checksum value

    unsigned char byte1=0x0;                                                                    // last 3 bytes read from the bitstream              
    unsigned char byte2=0x0;
    unsigned char byte3=0x0;
    
    uint8_t flgBreakloop=0;                                                                     // flag to exit the infinite while loop         

    int bitPos=0;                                                                               // bit position in the stream        
    int counter=0;                                                                              // counter for the tempBuffer              
    int bufferLoop=0;                                                                           // count the number of time we have looped the buffer
    unsigned char * sectorMap;                                                                  // Sector skewing from nibble to DSK or PO 

    if( dskData == NULL || buffer == NULL|| retError == NULL ){
        log_error("nib2dsk: NULL pointer detected\n");
        *retError=0xFF;
        return RET_ERR;
    }

    if (mountImageInfo.type==2)
        sectorMap=dsk2nibSectorMap;
    else if (mountImageInfo.type==3)
        sectorMap=po2nibSectorMap;
    else{
        log_error("Unable to match sectorMap with mountImageInfo.type");
        *retError=0x01;
        return RET_ERR;
    }

    enum BITSTREAM_PARSING_STAGE stage=SEARCH_ADDR;                                              // Current stage of the processing to find the right data signature                                                            
    
    memset(sectorCheckArray,0,16*sizeof(uint8_t));

    while(1){
        
   		ch=buffer[i];                                                                           // for code clarity

   		for (int8_t j=7;j>=0;j--){
            
   			byteWindow<<=1;
            byteWindow|=ch >> j & 1;

            if (byteWindow & 0x80){

                byte3=byte2;
                byte2=byte1;
                byte1=byteWindow & 0xFF;

                if (stage==SEARCH_ADDR && byte1==signatureAddrStart[2] && byte2==signatureAddrStart[1] && byte3==signatureAddrStart[0]){
                    if (indxOfFirstAddr==-1)
                        indxOfFirstAddr=bitPos;                                                 // Capture the starting point of the first ADDRESS bloc to make a full loop in the buffer

                    stage=READ_ADDR;
                    counter=0;

                }else if (stage==READ_ADDR){
                    tmpBuffer[counter]=byte1;
                    counter++;

                    if (counter==11){
                        
                        if (decodeAddr(tmpBuffer,&logicalSector,&dskTrack)==RET_ERR){           // decode ADDRESS Bloc field
                            log_error("decodeAddr error trk:%02d",trk);
                            *retError=0x02;
                            return RET_ERR;    
                        }

                        if (dskTrack!=trk){                                                     // Check the track (because of timing constraint this should help detects errors)
                            log_error("track differs from the head:%d and the logical address data :%d",trk,dskTrack);
                            *retError=0x03;
                            return RET_ERR;
                        }

                        physicalSector=sectorMap[logicalSector];
                        
                        stage=SEARCH_DATA;
                    }
                }else if(stage==SEARCH_DATA && byte1==signatureDataStart[2] && byte2==signatureDataStart[1] && byte3==signatureDataStart[0]){
                    stage=READ_DATA;
                    counter=0;

                }else if(stage==READ_DATA){
                    tmpBuffer[counter]=byte1;
                    counter++;
                    if (counter==343){                                                                          // Full data block read                
                        uint8_t * data_out=dskData+256*physicalSector;                                          // send directly the right buffer address to avoid memcpy
                        
                        if(decodeGCR6_2(tmpBuffer,(unsigned char *)data_out,&cksum_out,&cksum_calc)==RET_ERR){    // gcr6_2 decode and expect 256 Bytes in return;
                            printf("GCR %d\n",physicalSector);
                            GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_SET);  
                            *retError=0x04;
                            //return RET_ERR;                                                                   // continue even if there is an error 
                        }else{
                            sectorCheckArray[logicalSector]=1;                                                  // flag succesfull sector in the checkArray
                            sumSector+=physicalSector+1;
                        }
                        stage=SEARCH_ADDR;
                    }
                }
                
                byteWindow=0x0;   
            }

            bitPos++;
            
            if (bitPos==indxOfFirstAddr){                                      // Check if we have done a full loop in the buffer starting from the first Address block found
                flgBreakloop=1;
                break;
            }    
        }

        if (flgBreakloop==1){                                                   // Exit the infinite while loop               
            break;
        }  
        
        i++;
        
        if (i>=byteSize){                                                       // loop back to the begining of the buffer
            i=0;                                                                // reset buffer index
            bitPos=0;                                                           // reset bit position                 

            if (++bufferLoop>1){                                                // safety to avoid infinite loop
                log_error("bufferLoop exceeded");                               // this should not happen as we check for full loop with indxOfFirstAddr
                *retError=0x05;
                return RET_ERR;                                                 // Exit with error
            }
        }
    }

    if (sumSector!=136){ 
                                                                                // Check if we have successfuly process all sector
        //printf("Mis trk:%02d sec%d!=136\n",trk, sumSector);                   // a better approach as multiple sector would share the same number
        /*for (int8_t j=0;j<16;j++){
            if (sectorCheckArray[j]==0)
                log_error("sector NIB:%d is missing\n",j);
        }*/
        *retError=0x06;
        return RET_ERR;  
        
    }
    
    *retError=0x00;
    return RET_OK;            

}

static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack){

	unsigned char volume=0x0;
	unsigned char track=0x0;
	unsigned char sector=0x0;
	unsigned char checksum=0x0;

	unsigned char compute_checksum=0x0;

	volume	=((buf[0] << 1) & 0xAA) | (buf[1]  & 0x55);
	track	=((buf[2] << 1) & 0xAA) | (buf[3] & 0x55);
	sector	=((buf[4] << 1) & 0xAA) | (buf[5] & 0x55);
	checksum=((buf[6] << 1) & 0xAA) | (buf[7]  & 0x55);

	compute_checksum^=volume;
	compute_checksum^=track;
	compute_checksum^=sector;
    *retSector=sector;
    *retTrack=track;
	//printf("Address decoding volume:%d, track:%d, sector:%d, checksum:%02X, compute checksum:%02X\n",volume,track,sector,checksum,compute_checksum);
	if (checksum!=compute_checksum){
		printf("Address field decoding checksum error %02X!=%02x\n",compute_checksum,checksum);
        return RET_ERR;
    }

	return RET_OK;

}

static enum STATUS decodeGCR6_2(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc) {
    
    const uint8_t *disk_bytes = buffer;
    uint8_t enc2_unpacked[ENCODE_525_6_2_RIGHT_BUFFER_SIZE * 3];
    unsigned chksum, i2, i6;
    uint8_t rbyte;
    
    chksum = 0;
    
    for (i2 = 0; i2 < ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2++) {
    	rbyte=from_gcr_6_2_byte[(*disk_bytes++)-0x80];
        
        if (rbyte == 0x80){
            //log_error("rbyte1 error ==0x80");
            //dumpBuf((char *)buffer,1,343);
            return RET_ERR;
        }
        
        chksum ^= rbyte;
        /* bits 0,1   2,3   4,5  switched and shifted to the first two bits*/
        enc2_unpacked[i2] = ((chksum & 0x1) << 1) | ((chksum & 0x2) >> 1);
        enc2_unpacked[i2 + ENCODE_525_6_2_RIGHT_BUFFER_SIZE] =  ((chksum & 0x4) >> 1) | ((chksum & 0x8) >> 3);
        enc2_unpacked[i2 + ENCODE_525_6_2_RIGHT_BUFFER_SIZE * 2] = ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
    }
    
    for (i6 = 0; i6 < 256; ++i6) {
    	rbyte=from_gcr_6_2_byte[(*disk_bytes++)-0x80];
        if (rbyte == 0x80){
            //log_error("rbyte2 error ==0x80");
            //dumpBuf((char *)buffer,1,343);
            return RET_ERR;
        }
        
        chksum ^= rbyte;
        *(data_out+i6) = (((chksum & 0xff) << 2) | enc2_unpacked[i6]);
    }

    *chksum_calc = chksum;

    
    rbyte=from_gcr_6_2_byte[(*disk_bytes++)-0x80];
    
    
    if (rbyte == 0x80){
        log_error("rbyte3 error ==0x80");
    	return RET_ERR;
    }
    *chksum_out = rbyte;
    if (*chksum_out!=*chksum_calc){
        //printf("gcrDeocding checksum error %02X!=%02X",*chksum_out,*chksum_calc);
        printf("GCR ck err\n");
        return RET_ERR;
    }
    return RET_OK;
}

enum STATUS createNewDiskDSK(char * filename,int blockNumber){
  
  FIL fil; 		    //File handle
  FRESULT fres;     //Result after operations
 
  while(fsState!=READY){};
  fsState=BUSY;
  
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if (fres != FR_OK){
	  log_error("f_open error (%i)", fres);
    fsState=READY;
    return RET_ERR;
  }
 
  UINT bytesWrote;

  char buffer[512];
  memset(buffer,0x0,512);
             // 256 * 16 * 35 => 280  Block of 512 Bytes

    for (int i=0;i<blockNumber;i++){
        fsState=WRITING;
        fres = f_write(&fil, (unsigned char *)buffer, 512, &bytesWrote);
        if(fres == FR_OK) {
        //totalBytes+=bytesWrote;
        }else{
            log_error("f_write error (%i) block:%d\n",fres,i);
            fsState=READY;
            f_close(&fil);
            return RET_ERR;
        }

        while(fsState!=READY){};
  }
  f_close(&fil);

  return RET_OK;
}
