
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
#define ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86

enum BITSTREAM_PARSING_STAGE{N,ADDR_START,ADDR_END,DATA_START,DATA_END};
static const unsigned char signatureAddrStart[]	={0xD5,0xAA,0x96};
static const unsigned char signatureDataStart[]	={0xD5,0xAA,0xAD};

static  uint8_t sectorCheckArray[32];
//static  uint8_t dsk2nibSectorMap[]         = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
//static  uint8_t po2nibSectorMap[]          = {0, 8,  1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15};

static  uint8_t nib2dskSectorMap[]         = {0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10,8, 6, 4, 2, 15};
//static  uint8_t nib2poSectorMap[]          = {0,  2,  4, 6,  8,10, 12,14,  1, 3,  5, 7, 9, 11,13,15};

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

static enum STATUS nib2nic(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError);
static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack);
static enum STATUS decodeGcr62(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc);


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
  
  if (addr==-1){
    log_error("Error getting SDCard Address for nic\n");
    return RET_ERR;
  }
  
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



static enum STATUS nib2nic(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError){

  unsigned char ch=0x0;
  unsigned char byteWindow=0x0;                                                               // byte window of the data stream
  unsigned char byteFrameIndx=0;                                                              // simple counter to check if a complete signature is found
  unsigned char byteStreamRecord=0;                                                           // flag to record block data
  int byteStreamRecordIndx=0;                                                                 // buffer index when recording addr data and gcr data

  unsigned char tmpBuffer[346];                                                               // temp buffer for Addr, data block & GCR decoding

  int i=0;                                                                                    // Buffer index in the infinite while loop
  int indxOfFirstAddr=-1;                                                                     // Byte address on the stream for the first Addressblock, to check if a full stream loop is done before exiting
  
  uint8_t  logicalSector=0;                                                                   // nibSector from the trackstream
  uint8_t  physicalSector=0;                                                                  // disk Sector number of applying the DOS or PRODOS sector ordering
  uint8_t  sumSector=0x0;                                                                     // calculate the sum of sector to confirm a complete track is here
  uint8_t  dskTrack=0x0;                                                                      // track from the addr decoded block use to check with the function variable
  uint8_t  cksum_out, cksum_calc;                                                             // GCR 6_2 checksum value

  unsigned char * sectorMap=nib2dskSectorMap;
 
 const uint8_t checkSignatureLength=3;                                                       // length of the prologue to check changed it to const
 enum BITSTREAM_PARSING_STAGE stage=ADDR_START;                                              // Current stage of the processing to find the right data signature                                                            
 const unsigned char *ptrSearchSignature=&signatureAddrStart[0];                             // Start with the AddrSignature to be checked
 int blockLength=11;

 for (int8_t k=0;k<16;k++){                                                                  // Array to get which sector might be missing
     sectorCheckArray[k]=0;
 }

 while(1){
  
    ch=buffer[i];                                                                           // for code clarity

    for (int8_t j=7;j>=0;j--){
         
      byteWindow<<=1;
         byteWindow|=ch >> j & 1;

         if (byteWindow & 0x80){
           
           if (byteStreamRecord==1){											                                    // Storing Addr or Data data 
             tmpBuffer[byteStreamRecordIndx]=byteWindow;
             byteStreamRecordIndx++;
                 
                 if (byteStreamRecordIndx==blockLength){                                     // end of the data block
                     byteFrameIndx=checkSignatureLength;
                 }

           }else{
                 if (byteWindow==ptrSearchSignature[byteFrameIndx]){
                     tmpBuffer[byteFrameIndx]=ptrSearchSignature[byteFrameIndx]; 
                     byteFrameIndx++;
                 }else{
                     byteFrameIndx=0;
                 }
             }

           if (byteFrameIndx==checkSignatureLength){                                          // if == 3 means that 3 succesive char of signature have been detected
              
            if (stage==ADDR_START){             
              //log_info("A-S byte:%d %04X bit:%d",i,i,j); 
              stage=ADDR_END;                                                         // Move to the next stage
                                                              
              if (indxOfFirstAddr==-1)                                                // Capture the starting point of the first ADDRESS bloc to make a full loop in the buffer
                indxOfFirstAddr=i;

              blockLength=11;                                                         // 11 char to capture 3 Prologue + 8 data char
              byteStreamRecord=1;
              byteStreamRecordIndx=3;
              // log_info("A-E byte:%d %04X bit:%d",i,i,j);
             }
            else if (stage==ADDR_END){
              //log_info("B-S byte:%d %04X bit:%d",i,i,j);                               // END ADDRESS bloc signature found
              stage=DATA_START;                                                       
              ptrSearchSignature=&signatureDataStart[0];
               
              if((decodeAddr(tmpBuffer,&logicalSector,&dskTrack))==RET_ERR){    // decode ADDRESS Bloc field
                  log_error("decodeAddr error end of the process");
                  *retError=0x02;
              
                  //dumpBuf(tmpBuffer,1,350);
                  return RET_ERR;    
              }

              if (dskTrack!=trk){                                                     // Check the track (because of timing constraint this should help detects errors)
                  log_error("track differs from the head:%d and the logical address data :%d",trk,dskTrack);
                  *retError=0x03;
                  return RET_ERR;
              }
              
              for (int8_t j=0;j<16;j++){
                  if (sectorMap[j]==logicalSector){
                      physicalSector=j;
                      break;
                  }
              }
               //physicalSector=sectorMap[logicalSector];                                  // Will be usefull to determine position in the dsk track buffer to vbe written in the file
               
              byteStreamRecord=0;                                                     // stop recording char in the buffer
               
                     //log_info("B-E byte:%d %04X bit:%d",i,i,j);
             }
             else if (stage==DATA_START){                                                // START DATA Block signature is detected 
               //log_info("C-S byte:%d %04X bit:%d",i,i,j); 
              stage=DATA_END;
              blockLength=346;
              byteStreamRecord=1; 
              byteStreamRecordIndx=3;                                                 // move the temporary buffer position by the size of the signature
             }

             else if (stage==DATA_END){                                                  // End of the DATA Bloc
               //log_info("D-S byte:%d %04X bit:%d",i,i,j); 
                     stage=ADDR_START;
               ptrSearchSignature=&signatureAddrStart[0];                              // Next signature search is ADDRESS BLOC
                                                                                       // Increment the sumSector as checksum with the value of the current sector number
               uint8_t * data_out=dskData+256*physicalSector;                               // send directly the right buffer address to avoid memcpy
                     
                     //if (decodeGcr62b(tmpBuffer+3,(unsigned char *)data_out)==RET_ERR){
                     if(decodeGcr62(tmpBuffer,(unsigned char *)data_out,&cksum_out,&cksum_calc)==RET_ERR){    // gcr6_2 decode and expect 256 Bytes in return;
                        log_error("GCR decoding trk:%02d, sector:%02d, bytePos:%d %02X",trk,physicalSector,i,i);
                         //dumpBuf(buffer,1,6657);
                         /*char filename[32];
                         sprintf(filename,"dmp_gcr_%d_%d_%d.bin",trk,physicalSector,i);
                         dumpBufFile(filename,buffer,6657);
                         */
                         *retError=0x04;
               
                     }else{
                         sectorCheckArray[logicalSector]=1;                                         // flag succesfull sector in the checkArray
                         sumSector+=physicalSector+1;
                     }
               
                     byteStreamRecord=0;                                                       // Stop recording data in the buffer;
                     memset(tmpBuffer,0,346);
                     //log_info("D-E byte:%d %04X bit:%d",i,i,j); 
             }

             byteFrameIndx=0;                                                            // reset the byteFrameIdx to 0 
           }

           byteWindow=0x0;
         }
    }
    
    i++;
       
   if (i==byteSize){                                                                       // Check the end of the  track stream data buffer
     if (indxOfFirstAddr!=-1)                                                            // If the address of the first ADDR Block is found continue
             i=0;                                                                            // the loop to this point
         else{
             *retError=0x05;                                                                               // otherwise break the loop and return with error
             log_error("exiting the loop without track processed");
             break;
         }
             
   }else if (i==indxOfFirstAddr){                                                          // Back to the starting point of the first Block
         //log_info("back to the starting point");                                             // break the loop
     break;
   }
 }
 
 if (sumSector!=136){ 
     *retError=0x06;                                                                       // Check if we have successfuly process all sector
     log_error("Missing sector: %d!=136",sumSector);          // a better approach as multiple sector would share the same number
     for (int8_t j=0;j<16;j++){
         if (sectorCheckArray[j]==0)
             log_warn("sector NIB:%d is missing",j);
     }
     //return RET_ERR;                                                                       // good enough
 }
 
 //t2 = DWT->CYCCNT;
 //diff1=t2-t1;
 //printf("cpu cycles:%ld\n",diff1);
 *retError=0x00;
 return RET_OK;
}

static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack){

	unsigned char volume=0x0;
	unsigned char track=0x0;
	unsigned char sector=0x0;
	unsigned char checksum=0x0;

	unsigned char compute_checksum=0x0;

	volume	=((buf[3] << 1) & 0xAA) | (buf[4]  & 0x55);
	track	=((buf[5] << 1) & 0xAA) | (buf[6] & 0x55);
	sector	=((buf[7] << 1) & 0xAA) | (buf[8] & 0x55);
	checksum=((buf[9] << 1) & 0xAA) | (buf[10]  & 0x55);

	compute_checksum^=volume;
	compute_checksum^=track;
	compute_checksum^=sector;
    *retSector=sector;
    *retTrack=track;
	//printf("Address decoding volume:%d, track:%d, sector:%d, checksum:%02X, compute checksum:%02X\n",volume,track,sector,checksum,compute_checksum);
	if (checksum!=compute_checksum){
		log_error("Address field decoding checksum error %02X!=%02x\n",compute_checksum,checksum);
        return RET_ERR;
    }

	return RET_OK;

}

static enum STATUS decodeGcr62(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc) {
    
  const uint8_t *disk_bytes = buffer+3;
  uint8_t enc2_unpacked[ENCODE_525_6_2_RIGHT_BUFFER_SIZE * 3];
  unsigned chksum, i2, i6;
  uint8_t rbyte;
 // uint8_t tmp;

  chksum = 0;
  
  for (i2 = 0; i2 < ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2++) {
    rbyte=from_gcr_6_2_byte[(*disk_bytes++)-0x80];
      
      if (rbyte == 0x80){
          log_error("rbyte1 error ==0x80");
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
          log_error("rbyte2 error ==0x80");
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
      log_error("gcrDeocding checksum error %02X!=%02X",*chksum_out,*chksum_calc);
      return RET_ERR;
  }
  return RET_OK;
}

enum STATUS createNewDiskNic(char * filename){

  FIL fil; 		  //File handle
  FRESULT fres; //Result after operations

  const unsigned char volume = 0xfe;
  uint8_t track;
  uint8_t sector;

 
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
  
  for (int i=0;i<22;i++){
    buffer[i]=0xFF;
  }

  buffer[23]=0x03;
  buffer[24]=0xFC;
  buffer[25]=0xFF;
  buffer[26]=0x3F;
  buffer[27]=0xCF;
  buffer[28]=0xF3;
  buffer[26]=0xFC;
  buffer[27]=0xFF;
  buffer[28]=0x3F;
  buffer[29]=0xCF;
  buffer[30]=0xF3;
  buffer[31]=0xFC;

  buffer[32]=0xD5;
  buffer[33]=0xAA;
  buffer[34]=0x96;

  buffer[43]=0xDE;
  buffer[44]=0xAA;
  buffer[45]=0xEB;

  buffer[43]=0xD5;
  buffer[44]=0xAA;
  buffer[45]=0xAD;

  buffer[399]=0xDE;
  buffer[400]=0xAA;
  buffer[401]=0xEB;

  for (int i=0;i<14;i++){
    buffer[402+i]=0xFF;
  }

  for (track=0;track<35;track++){
    for (sector=0;sector<16;sector++){
      buffer[35]=(volume>>1 ) | 0xAA;
			buffer[36]=(volume) | 0xAA;

			buffer[37]=(track>>1 )| 0xAA;
			buffer[38]=(track) | 0xAA;

			buffer[39]=(sector>>1 )| 0xAA;
			buffer[40]=(sector) | 0xAA;

			uint8_t cksum=volume^track^sector;

			buffer[41]=(cksum>>1 )| 0xAA;
			buffer[42]=(cksum) | 0xAA;
    
      fsState=WRITING;
      fres = f_write(&fil, (unsigned char *)buffer, 512, &bytesWrote);
      if(fres == FR_OK) {
        //totalBytes+=bytesWrote;
      }else{
        log_error("f_write error (%i) block:%d\n",fres,sector);
        fsState=READY;
        return RET_ERR;
      }

      while(fsState!=READY){};
    }
  }
  f_close(&fil);
  return RET_OK;
}
 

