
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

#define NIBBLE_BLOCK_SIZE  416 //402
#define NIBBLE_SECTOR_SIZE 512
#define ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86


enum BITSTREAM_PARSING_STAGE{N,ADDR_START,ADDR_END,DATA_START,DATA_END};
enum STATUS nib2dsk(char *rawByte,unsigned char *buffer,uint8_t trk,int byteSize);
enum STATUS dsk2Nib(unsigned char *rawByte,unsigned char *buffer,uint8_t trk);

static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack);
static enum STATUS decodeGcr62(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc);
static void print_packet (unsigned char* data, int bytes);

const unsigned char signatureAddrStart[]	={0xD5,0xAA,0x96};
const unsigned char signatureAddrEnd[]		={0xDE,0xAA,0xEB};
const unsigned char signatureDataStart[]	={0xD5,0xAA,0xAD};
const unsigned char signatureDataEnd[]		={0xDE,0xAA,0xEB};

static  uint8_t  dsk2nibSectorMap[]         = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
static  uint8_t  po2nibSectorMap[]         =  {0, 8,  1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15};

static  uint8_t  nib2dskSectorMap[]         = {0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10,8, 6, 4, 2, 15};
static uint8_t   nib2poSectorMap[]          = {0,  2,  4, 6,  8,10, 12,14,  1, 3,  5, 7, 9, 11,13,15};

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
    int long_sector = trk*8;                    // DSK & PO are 256 long and not 512 a track is 4096
    int long_cluster = long_sector >> 6;
    int ft = fatDskCluster[long_cluster];
    long rSector=database+(ft-2)*csize+(long_sector & (csize-1));
    return rSector;
}

enum STATUS getDskTrackBitStream(int trk,unsigned char * buffer){
    int addr=getDskSDAddr(trk,0,csize,database);
    const unsigned int blockNumber=8; 

    if (addr==-1){
        log_error("Error getting SDCard Address for DSK\n");
        return RET_ERR;
    }

    unsigned char * tmp=(unsigned char *)malloc(4096*sizeof(char));
    
    if (tmp==NULL){
        log_error("unable to allocate tmp for 4096 Bytes");
        return RET_ERR;
    }

    getDataBlocksBareMetal(addr,tmp,blockNumber);          // Needs to be improved and to remove the zeros
    while (fsState!=READY){}
    
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

enum STATUS dsk2Nib(unsigned char *rawByte,unsigned char *buffer,uint8_t trk){

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
        
    for (i=0; i<0x16; i++) 
        dst[i]=0xff;

    // sync header
    dst[0x16]=0x03;
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
    
    for (i=0x192; i<0x1a0; i++) 
        dst[i]=0xff;
    
    for (i=0x1a0; i<0x200; i++) 
        dst[i]=0x00;

    for (u_int8_t sector=0;sector<16;sector++){
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

static void print_packet (unsigned char* data, int bytes){
    int count, row;
    char xx;

    
    for (count = 0; count < bytes; count = count + 16) {
        
        printf("%04X: ", count);
        for (row = 0; row < 16; row++) {
            if (count + row >= bytes)
                printf("   ");
            else {
                printf("%02X ",data[count + row]);
            }
        }
        printf("- ");
        for (row = 0; row < 16; row++) {
            if ((data[count + row] > 31) && (count + row < bytes) && (data[count + row] < 129)){
                xx = data[count + row];
                printf("%c",xx);
            }
            else
                printf(".");
        }
        printf("\r\n");
    }
    printf(".\r\n");
    
}


enum STATUS setDskTrackBitStream(int trk,unsigned char * buffer){
    log_info("E1");
    if (nib2dsk(NULL,buffer,trk,16*416)==RET_ERR){
        log_error("nib2dsk return an error");
        return RET_ERR;
    }

    return RET_OK;
}

enum STATUS nib2dsk(char *rawByte,unsigned char *buffer,uint8_t trk,int byteSize){

   	unsigned char ch=0x0;
    unsigned char byteWindow=0x0;                                                               // byte window of the data stream
   	unsigned char byteFrameIndx=0;                                                              // simple counter to check if a complete signature is found
   	unsigned char byteStreamRecord=0;
   	int byteStreamRecordIndx=0;                                                                 // buffer index when recording addr data and gcr data

    unsigned char tmpBuffer[256];                                                               // temp buffer for Addr & GCR decoding
    unsigned char dskData[4096];                                                                // Target data bloc to be stored in the dsk file
        
    const unsigned char *ptrSearchSignature=&signatureAddrStart[0];

   	enum BITSTREAM_PARSING_STAGE stage=ADDR_START;                                              // Current stage of the processing to find the right data signature
   
   	int i=0;
   	int indxOfFirstAddr=-1;                                                                     // Byte address on the stream for the first Addressblock, to check if a full stream loop is done before exiting
   	
   	uint8_t  nibSector=0;                                                                       // nibSector from the trackstream
   	uint8_t  dskSector=0;                                                                       // dskSector number of applying the DOS or PRODOS sector ordering
   	uint8_t  sumSector=0x0;                                                                     // calculate the sum of sector to confirm a complete track is here
   	uint8_t  dskTrack=0x0;
    uint8_t  cksum_out, cksum_calc;                                                             // checksum value

    unsigned long t1,t2,diff1;
    DWT->CYCCNT = 0;                                                                            // Reset cpu cycle counter
    t1 = DWT->CYCCNT;
   	
    while(1){
   	
   		ch=buffer[i];

   		for (char j=7;j>=0;j--){

   			byteWindow<<=1;
            byteWindow|=ch>>j & 1;

            if (byteWindow & 0x80){
            	
            	if (byteStreamRecord==1){											            // Storing Addr or Data data 
            		tmpBuffer[byteStreamRecordIndx]=byteWindow;
            		byteStreamRecordIndx++;
            	}

            	if (byteWindow==ptrSearchSignature[byteFrameIndx]){
            		byteFrameIndx++;
            	}else{
            		byteFrameIndx=0;
            	}

            	if (byteFrameIndx==3){                                                          // if == 3 means that 3 succesive char of signature have been detected
            		printf("Found seq:0,Byte:%d,bit:%d,bitCounter:%d\n",i,j,i*8+j);
            		if (stage==ADDR_START){
            			stage=ADDR_END;
            			
            			if (indxOfFirstAddr==-1)                                                // Capture the starting point of the first ADDRESS bloc to make a full loop in the buffer
            				indxOfFirstAddr=i;

            			tmpBuffer[0]=ptrSearchSignature[0];                                     // Recopy the ADDRESS Bloc signature
            			tmpBuffer[1]=ptrSearchSignature[1];
            			tmpBuffer[2]=ptrSearchSignature[2];
            			byteStreamRecordIndx=3;
            			
            			ptrSearchSignature=&signatureAddrEnd[0];                                // Position the END ADDRESS bloc signature
            			byteStreamRecord=1;
            		}
            		else if (stage==ADDR_END){                                                  // END ADDRESS bloc signature found
            			stage=DATA_START;                                                       
            			ptrSearchSignature=&signatureDataStart[0];
            			tmpBuffer[byteStreamRecordIndx++]=0x0;
            			
                        if((nibSector=decodeAddr(tmpBuffer,&nibSector,&dskTrack))==RET_ERR){    // decode ADDRESS Bloc field
                            log_error("decodeAddr error end of the process");
                            return RET_ERR;
                        }
                        if (dskTrack!=trk){                                                     // Check the track (because of timing constraint this should help detects errors)
                            log_error("track differs from the head and the logical address data");
                            return RET_ERR;
                        }
            			dskSector=nib2dskSectorMap[nibSector];                                  // Will be usefull to determine position in the dsk track buffer to vbe written in the file
            			//print_packet(tmpBuffer,byteStreamRecordIndx);
            			byteStreamRecord=0;                                                     // stop recording char in the buffer
            			
            		}
            		else if (stage==DATA_START){                                                // START DATA Block signature is detected 
            			stage=DATA_END;
            			
            			tmpBuffer[0]=ptrSearchSignature[0];                                     // recopy signature (this can be removed)
            			tmpBuffer[1]=ptrSearchSignature[1];
            			tmpBuffer[2]=ptrSearchSignature[2];
            			byteStreamRecordIndx=3;                                                 // move the temporary buffer position by the size of the signature

            			ptrSearchSignature=&signatureDataEnd[0];
            			byteStreamRecord=1;                                                     // start recording in the temp buffer
            		}
            		else if (stage==DATA_END){                                                  // End of the DATA Bloc
            			stage=ADDR_START;
            			ptrSearchSignature=&signatureAddrStart[0];                              // Next signature search is ADDRESS BLOC
            			sumSector+=dskSector+1;                                                 // Increment the sumSector as checksum with the value of the current sector number
            			
            			uint8_t * data_out=dskData+256*dskSector;                               // send directly the right buffer address to avoid memcpy

            			if(decodeGcr62(tmpBuffer,data_out,&cksum_out,&cksum_calc)==RET_ERR){    // gcr6_2 decode and expect 256 Bytes in return;
            				log_error("error doing GCR decoding");
                            return RET_ERR;
            			}

            			byteStreamRecord=0;                                                     // Stop recording data in the buffer;
            			
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
            else{                                                                               // otherwise break the loop and return with error
                log_error("exiting the loop without track processed");
                break;
            }
                
	    }else if (i==indxOfFirstAddr){                                                          // Back to the starting point of the first Block
            log_info("back to the starting point");                                             // break the loop
	    	break;
	    }
    }

    if (sumSector!=136){                                                                        // Check if we have successfuly process all sector
        log_error("Not all sector have been processed");                                        // a better approach as multiple sector would share the same number
        return RET_ERR;                                                                         // good enough
    }
    t2 = DWT->CYCCNT;
    diff1=t2-t1;
    printf("cpu cycles elapsed %ld\n",diff1);
    
    //printf("\n.OUTPUT 4096 SumSector:%d=136\n",SumSector);
    //print_packet(dskData,4096);

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


