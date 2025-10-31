
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_dsk.h"
#include "main.h"
#include "log.h"
#include "emul_diskii.h"
#include "emul_smartport.h"
#include "display.h"

extern long database;                                            // start of the data segment in FAT
extern int csize;  
extern volatile enum FS_STATUS fsState;
unsigned int fatSmartloaderCluster[20];
extern image_info_t mountImageInfo;
extern char smartloader_bin[];

extern const  char * smartportImageExt[];
extern const  char * diskIIImageExt[];

#define NIBBLE_BLOCK_SIZE  416 // 400 51 200
#define NIBBLE_SECTOR_SIZE 512
#define ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86

 enum BITSTREAM_PARSING_STAGE{N,ADDR_START,ADDR_END,DATA_START,DATA_END};

static enum STATUS nib2dsk(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError);
static enum STATUS dsk2Nib(unsigned char *rawByte,unsigned char *buffer,uint8_t trk);

static enum STATUS decodeAddr(unsigned char *buf, uint8_t * retSector,uint8_t * retTrack);
static enum STATUS decodeGcr62(uint8_t * buffer,uint8_t * data_out,uint8_t *chksum_out, uint8_t *chksum_calc);

static enum STATUS decodeGcr62b(unsigned char * src,unsigned char * dst);

static const unsigned char signatureAddrStart[]	={0xD5,0xAA,0x96};
static const unsigned char signatureDataStart[]	={0xD5,0xAA,0xAD};

static uint8_t sectorCheckArray[32];
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

static unsigned char decTable[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x03, 0x00, 0x04, 0x05, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x08, 0x00, 0x00, 0x00, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x00, 0x00, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x00, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x1c, 0x1d, 0x1e,
    0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x20, 0x21, 0x00, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x2a, 0x2b, 0x00, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0x00, 0x00, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x00, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};


// for bit flip

static const unsigned char FlipBit1[4] = { 0, 2, 1, 3 };
static const unsigned char FlipBit2[4] = { 0, 8, 4, 12 };
static const unsigned char FlipBit3[4] = { 0, 32, 16, 48 };

extern list_t * dirChainedList;
extern list_t * favoritesChainedList;

extern char currentFullPath[MAX_FULLPATH_LENGTH]; 
extern char currentPath[MAX_PATH_LENGTH];
extern char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];  // fullpath from root image filename
extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];

extern uint8_t smartloaderEmulationType;

extern const  char** ptrFileFilter;  

static uint8_t  smtlCommand=0x0;                        // Command from Smartloader
static uint8_t  smtlValue=0x0;                          // Param from Smartloader
static uint8_t  smtlCurrentCategory=0x0;                // See list below
static uint8_t  smtlCurrentPage=0x0;                    // a page is a list of 16 (0x0F) items, Page 01 => Item from 17 -> 32                

enum SMTL_CATEGORY{CAT_ROOT,CAT_FAVORITE,CAT_FILE,CAT_SETTINGS,CAT_HELP};

//static uint8_t  smtlLevel=0x0;
static uint8_t  smtlReturnCode=0x0;
//static uint8_t  smtlErrorCode=0x0;

int getSmartloaderTrackFromPh(int phtrack){
    return phtrack >> 2;
}

unsigned int getSmartloaderTrackSize(int trk){
    return 16*NIBBLE_BLOCK_SIZE*8;
}

long getSmartloaderSDAddr(int trk,int block,int csize, long database){
    int long_sector = trk*8;                    // DSK & PO are 256 long and not 512 a track is 4096
    int long_cluster = long_sector >> 6;
    int ft = fatSmartloaderCluster[long_cluster];
    long rSector=database+(ft-2)*csize+(long_sector & (csize-1));
    return rSector;
}

enum STATUS getSmartloaderTrackBitStream(int trk,unsigned char * buffer){
    
    unsigned char * tmp=(unsigned char *)malloc(4096*sizeof(char));
    
    if (tmp==NULL){
        log_error("unable to allocate tmp for 4096 Bytes");
        return RET_ERR;
    }

    switch(trk){
        case 0:
            memcpy(tmp,smartloader_bin,4096);
            break;
        case 1:
            memcpy(tmp,smartloader_bin+4096,4096);
            break;
    
        case 0x02: 
        
            memset((unsigned char *)tmp,0,sizeof(char)*4096);
            
            char header[32];
            char item[16][24];

            switch (smtlCommand){
                
                case 0x09:                                              // Managing back to Main
                case 0x10:
                case 0x00:                                              // Listing
                
                if (smtlCurrentCategory==CAT_ROOT){
                                                                        // HEADER
                                                                        // ---------------------------------------------------
                    header[0]=0x20;                                     // Byte [1]+0: Return code
                    header[1]=0x06;                                     // Byte [1]+1: Number of Item in the page
                    header[2]=smtlValue;                                // Byte [1]+2: Value
                    header[3]=0x0;                                      // Byte [1]+3: Max Page
                    header[4]=smartloaderEmulationType;                 // Byte [1]+4: Reserved
                                                                        // Byte [1]+5: Reserved
                                                                        // Byte [23]+6: Current Path
                                                                        // Byte [3]+30: Reserved
                    sprintf(header+6,"MAIN MENU");
                                                                        // Item prefix
                                                                        // ---------------------------------------------------
                                                                        // D - Directory
                                                                        // F - File
                                                                        // T - PAGE TITLE
                                                                        // M - MENU ITEM
                                                                        // E - EMPTY
                    sprintf(item[0],"TMAIN MENU");
                    sprintf(item[1],"MFAVORITES");
                    sprintf(item[2],"MFILE MANAGER");
                    sprintf(item[3],"E");

                    if (smartloaderEmulationType==DISKII)
                        sprintf(item[4],"VEMULATION|DISKII");
                    else if (smartloaderEmulationType==SMARTPORTHD)
                        sprintf(item[4],"VEMULATION|SMARTPORT");
                    else
                        sprintf(item[4],"VEMULATION|???");

                    sprintf(item[5],"MHELP");
                
                    memcpy(tmp,header,32);
                    for (uint8_t i=0;i<6;i++){
                        memcpy(tmp+32+i*24,item[i],24);
                    }

                }else if (smtlCurrentCategory==CAT_HELP){
                                                                        // ---------------------------------------------------
                    header[0]=0x20;                                     // Byte [1]+0: Return code
                    header[1]=0x0E;                                     // Byte [1]+1: Number of Item in the page
                    header[2]=0x0;                                      // Byte [1]+2: Value
                    header[3]=0x0;                                      // Byte [1]+3: Max Page
                    header[4]=0x0;                                      // Byte [1]+4: Reserved

                    sprintf(header+6,"HELP");
                                                                        // Item prefix
                                                                        // ---------------------------------------------------
                                                                        // D - Directory
                                                                        // F - File
                                                                        // T - PAGE TITLE
                                                                        // M - MENU ITEM
                                                                        // E - EMPTY
                    sprintf(item[0],"THELP");
                    sprintf(item[1],"X");
                    sprintf(item[2],"XKEY:");
                    sprintf(item[3],"X");
                    sprintf(item[4],"X[UP]    PREV ITEM");
                    sprintf(item[5],"X[DOWN]  NEXT ITEM");
                    sprintf(item[6],"X[RIGHT] NEXT PAGE");
                    sprintf(item[7],"X[LEFT]  PREV PAGEpage");
                    sprintf(item[8],"X[ENTER] SELECT ITEM");
                    sprintf(item[9],"X[M]     BACK TO MAIN");
                    sprintf(item[10],"X[R]     REFRESH PAGE");
                    sprintf(item[11],"X[B]     BOOT");
                    sprintf(item[12],"X");
                    sprintf(item[13],"M<- BACK TO MAIN");
                    
                    memcpy(tmp,header,32);
                    
                    for (uint8_t i=0;i<14;i++){
                        memcpy(tmp+32+i*24,item[i],24);
                    }
                }
                else if (smtlCurrentCategory==CAT_FAVORITE){


                    header[0]=0x20;                                     // Byte [1]+0: Return code
                //  header[1]=0x05;                                     // Byte [1]+1: Number of Item in the current page
                //  header[2]=smtlValue;                                // Byte [1]+2: Value
                    header[3]=0x0;                                      // Byte [1]+3: Max Page
                    header[4]=0x0;                                      // Byte [1]+4: Reserved
                                                                        // Byte [1]+5: Reserved
                                                                        // Byte [23]+6: Current Path
                                                                        // Byte [3]+30: Reserved
                    sprintf(header+6,"FAVORITES");
                    
                    const uint8_t maxItemPerPage=16;
                    uint8_t lstCount=favoritesChainedList->len;
                    uint8_t maxPage=lstCount/maxItemPerPage;
                    if ((lstCount % maxItemPerPage) !=0){
                        maxPage++;
                        log_info("Favorites MaxPage:%d",maxPage);
                    }
                    maxPage--;

                    uint8_t currentPageItemCount=lstCount-(smtlCurrentPage*maxItemPerPage);
                    if (currentPageItemCount>16)
                        currentPageItemCount=16;
                    
                    header[1]=currentPageItemCount;                     // Number of Items in the current page
                    header[2]=smtlCurrentPage;                          // Current Page (smtValue)
                    header[3]=maxPage;                                  // Max number of Page

                    list_node_t *pItem=NULL;
                    int offset=0;
                    
                    uint8_t startIndex=smtlCurrentPage*maxItemPerPage;
                    uint8_t endIndex=startIndex+currentPageItemCount;
                    int jj=0;

                    memcpy(tmp,header,32);

                    for (int i=startIndex;i<endIndex;i++){
                        offset=(jj)*24+32;                              // we start 32 Bytes after the initial buffer to keep room for return code and ...
                    
                        pItem=list_at(favoritesChainedList, i);
                        if (pItem!=NULL && pItem->val!=NULL){
                            listItem_t * li=pItem->val;
                            snprintf((char *)(tmp+offset),23,"F%s",li->title);
                        }else{
                            snprintf((char *)(tmp+offset),23,"(NULL)");
                        }
                        jj++;  
                    }
                    //dumpBuf(tmp,1,512);

                }else if (smtlCurrentCategory==CAT_FILE){

                    list_destroy(dirChainedList);                       // First free existing chainedlist if exists

                    if (smartloaderEmulationType==DISKII){
                        ptrFileFilter=diskIIImageExt;
                    }else if (smartloaderEmulationType==SMARTPORTHD){
                        ptrFileFilter=smartportImageExt;
                    }

                    walkDir(currentFullPath,ptrFileFilter);             // Build new File chained List

                    header[0]=0x20;                                     // Byte [1]+0: Return code

                    int len=strlen(currentFullPath);
                    currentPath[0]=0x0;
                    for (int i=len-1;i!=-1;i--){
                        if (currentFullPath[i]=='/'){
                            snprintf(currentPath,23,"%s",currentFullPath+i);
                            break;
                        }
                    }
                    snprintf(header+6,23,"%s",currentPath);

                    const uint8_t maxItemPerPage=16;
                    uint8_t lstCount=dirChainedList->len;
                    uint8_t maxPage=lstCount/maxItemPerPage;
                    if ((lstCount % maxItemPerPage) !=0){
                        maxPage++;
                        //log_info("MaxPage:%d",maxPage);
                    }
                    // Remember Max page Index = Page Count starting from 0 so -1
                    maxPage--;

                    if (smtlCurrentPage>maxPage)
                        smtlCurrentPage=maxPage;
                    
                    if (smtlCurrentPage<0)
                        smtlCurrentPage=0;

                    uint8_t currentPageItemCount=lstCount-(smtlCurrentPage*maxItemPerPage);
                    if (currentPageItemCount>16)
                        currentPageItemCount=16;
                
                    if (smtlReturnCode==0x0)
                        smtlReturnCode=0x20;
                    
                    header[1]=currentPageItemCount;                     // Number of Items in the current page
                    header[2]=smtlCurrentPage;                          // Current Page (smtValue)
                    header[3]=maxPage;                                  // Max number of Page
                    header[4]=smartloaderEmulationType; 

                    list_node_t *pItem=NULL;
                    int offset=0;
                    
                    uint8_t startIndex=smtlCurrentPage*maxItemPerPage;
                    uint8_t endIndex=startIndex+currentPageItemCount;
                    int jj=0;

                    //log_info("File startIndex:%d, endIndex:%d",startIndex,endIndex);
                    
                    memcpy(tmp,header,32);

                    for (int i=startIndex;i<endIndex;i++){
                        offset=(jj)*24+32;                              // we start 32 Bytes after the initial buffer to keep room for return code and ...
                        pItem=list_at(dirChainedList, i);
                        if (pItem!=NULL && pItem->val!=NULL){
                            char * val=pItem->val;
                            tmp[offset]=val[0];
                            snprintf((char *)(tmp+offset+1),23,"%s",val+2);
                            //log_info("val:%s",val);
                        }
                        jj++;
                    }
                    //dumpBuf(tmp,1,512);
                }else if (smtlCurrentCategory==CAT_SETTINGS){

                }

                break;
                case 0x01:                                              // Change Page
                
                break;
                
                case 0x02:                                              // Select  item
                break;
                                                        
            }
            break;
        default:
            memset(tmp,0x0,4096);    
        break;
    }
        
    if (dsk2Nib(tmp,buffer,trk)==RET_ERR){
        log_error("dsk2nib return an error");
        free(tmp);
        return RET_ERR;
    }

    free(tmp);
    return RET_OK;
}

enum STATUS setSmartloaderTrackBitStream(int trk,unsigned char * buffer){
    
    uint8_t retE=0x0;
    log_info("wr ok %d",trk);
    unsigned char * dskData=(unsigned char *)malloc(4096*sizeof(unsigned char)); 
    if (nib2dsk(dskData,buffer,trk,16*416,&retE)==RET_ERR){
        log_error("nib2dsk error:%d",retE);
        free(dskData);
        return RET_ERR;
    }
    
    if (trk==0x3){
      
        /* Sector 0 & 1*/
        /* assuming we will process only 256 bytes corresponding of 1st sector*/

        smtlCommand=dskData[0];
        smtlValue=dskData[1];
        //dumpBuf(dskData,1,512);
        log_info("cmd:%02X, value:%d",smtlCommand,smtlValue);

        list_node_t *pItem=NULL;
        char *tmp;

        if (smtlCommand==0x09){
            smtlCurrentCategory=CAT_ROOT;
            smtlReturnCode=0x20;
        }
        else if (smtlCurrentCategory==CAT_HELP){                // Selection in Help menu lead to MAIN
            smtlCommand=0x09;
            smtlCurrentCategory=CAT_ROOT;
            smtlReturnCode=0x20;
        }                                                       // We go to Main Menu
        else if (smtlCurrentCategory==CAT_ROOT){
            
            if (smtlCommand==0x10 && smtlValue==4){
                log_info("Change the emulation");
                smtlCurrentCategory=CAT_ROOT;
                currentFullPath[0]=0x0;
                

                if (smartloaderEmulationType==DISKII){
                    smartloaderEmulationType=SMARTPORTHD;
                }else{
                    smartloaderEmulationType=DISKII;
                }
                setConfigParamInt("smartloaderEmulationType",smartloaderEmulationType);
                saveConfigFile();
                smtlValue=0;
            }
            
            if (smtlCommand==0x10 && smtlValue==5){
                log_info("getting help text");
                smtlCurrentCategory=CAT_HELP;
                sprintf(currentFullPath,"HELP");
            }

            if (smtlCommand==0x10 && smtlValue==2){             // We go to File Listing
                log_info("getting root file");
                smtlCurrentCategory=CAT_FILE;
                currentFullPath[0]=0x0;

            }else if (smtlCommand==0x10 && smtlValue==1){
                smtlCurrentCategory=CAT_FAVORITE;
            }

        }else if  (smtlCurrentCategory==CAT_FAVORITE && smtlCommand==0x10){
            
            pItem=list_at(favoritesChainedList, smtlValue);
            if (pItem && pItem->val!=NULL){
                listItem_t * pFavItem=pItem->val;
                DiskIIMountImagefile(pFavItem->cval);
                free(dskData);
                return RET_OK;

            }else{
                log_error("pItem is null Argh# !");
                free(dskData);
                return RET_ERR;
            } 

        }else if  (smtlCurrentCategory==CAT_FILE && smtlCommand==0x10){
            
            uint8_t ilen=strlen(currentFullPath);
            pItem=list_at(dirChainedList, smtlValue);                                                           // Get the item in the list of value
            tmp=pItem->val;
            log_info("tmp:%s",tmp);
            
            smtlReturnCode=0x20;

            if (!strcmp(tmp,"D|..")){   
                log_info("updir %s",currentFullPath);                                                           // selectedItem is [UpDir];
                for (int i=ilen-1;i!=-1;i--){
                    if (currentFullPath[i]=='/'){
                        snprintf(currentPath,MAX_PATH_LENGTH,"%s",currentFullPath+i);
                        currentFullPath[i]=0x0;
                        log_info("updir new: %s",currentFullPath);
                        break;
                    }
                    if (i==0)
                        currentFullPath[0]=0x0;
                }

            }else if (tmp[0]=='D'){
                                                                                                                // 0x02 Process item
                pItem=list_at(dirChainedList, smtlValue);
                tmp=pItem->val;
                log_info("tmp2:%s %d",tmp,ilen);

                sprintf(currentFullPath+ilen,"/%s",tmp+2);
            
                //list_destroy(dirChainedList);
                //walkDir(currentFullPath,ptrFileFilter);                                                         // TODO Check if needed as WalkDir is done on the reading side
            
            }else if (tmp[0]=='F'){

                sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,tmp+2);                                // Not a File issue an error
                
                // We need to put a bit of magik Here to switch the emulation if needed and load the image...
                // not easy...
                
                if (smartloaderEmulationType==DISKII){
                    setEmulationPtr(DISKII);
                    DiskIIMountImagefile(tmpFullPathImageFilename);
                    switchPage(DISKIIIMAGE,tmpFullPathImageFilename);
                    free(dskData);
                    return RET_OK;
                }else if (smartloaderEmulationType==SMARTPORTHD){
                    setEmulationPtr(SMARTPORTHD);
                    SmartPortInitWithImage(tmpFullPathImageFilename);
                    free(dskData);
                    return RET_OK;
                }
                    

                DiskIIMountImagefile(tmpFullPathImageFilename);
                switchPage(DISKIIIMAGE,tmpFullPathImageFilename);
                free(dskData);
                return RET_OK;
            }

        }else if  (smtlCurrentCategory==CAT_FILE && smtlCommand==0x11){
            smtlCurrentPage=smtlValue;
            smtlCommand=0x10;
        }
        
    }
        
    free(dskData);  
    return RET_OK;
}

enum STATUS mountSmartloaderFile(char * filename){
    FRESULT fres; 
    FIL fil;  

    fres = f_open(&fil,filename , FA_READ);     // Step 2 Open the file long naming

    if(fres != FR_OK){
        log_error("File open Error: (%i)\r\n", fres);
        return -1;
    }

    long clusty=fil.obj.sclust;
    int i=0;
    fatSmartloaderCluster[i]=clusty;
    log_info("file cluster %d:%ld\n",i,clusty);
    
    while (clusty!=1 && i<30){
        i++;
        clusty=get_fat((FFOBJID*)&fil,clusty);
        log_info("file cluster %d:%ld",i,clusty);
        fatSmartloaderCluster[i]=clusty;
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
    //if (mountImageInfo.type==2)
        sectorMap=dsk2nibSectorMap;
   // else if (mountImageInfo.type==3)
   //     sectorMap=po2nibSectorMap;
   // else{
   //     log_error("Unable to match sectorMap with mountImageInfo.type");
   //     return RET_ERR;
   // }
        
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

//static uint8_t wr_retry=0;                                                                             // DEBUG ONLY



static enum STATUS nib2dsk(unsigned char * dskData,unsigned char *buffer,uint8_t trk,int byteSize,uint8_t * retError){

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

    /*
    unsigned long t1,t2,diff1;
    DWT->CYCCNT = 0;                                                                            // Reset cpu cycle counter
    t1 = DWT->CYCCNT;
    */
    unsigned char * sectorMap;                                                                  // Sector skewing from nibble to DSK or PO 
    //if (mountImageInfo.type==2)
       sectorMap=nib2dskSectorMap;
    //else if (mountImageInfo.type==3)
    //   sectorMap=nib2poSectorMap;
    //else{
    //   log_error("Unable to match sectorMap with mountImageInfo.type");
    //   return RET_ERR;
    //}

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
            	
            	if (byteStreamRecord==1){											            // Storing Addr or Data data 
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

            	if (byteFrameIndx==checkSignatureLength){                                       // if == 3 means that 3 succesive char of signature have been detected
            		
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
    
                       // log_info("C-E byte:%d %04X bit:%d",i,i,j);                            // start recording in the temp buffer
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static enum STATUS decodeGcr62b(unsigned char * src,unsigned char * dst){
   
    int i, j;
    unsigned char x, ox = 0;
    static unsigned char FlipBit[4] = { 0, 2, 1, 3 };
    uint8_t cksum=0x0;

    for (j=0, i=0x03; i<0x59; i++, j++) {
        x = ((ox^decTable[src[i]])&0x3f);
        cksum^=decTable[src[i]]&0x3f;
        dst[j+172] = FlipBit[(x>>4)&3];
        dst[j+86] = FlipBit[(x>>2)&3];
        dst[j] = FlipBit[(x)&3];
        ox = x;
    }

    for (j=0, i=0x59; i<0x159; i++, j++) {
        x = ((ox^decTable[src[i]])&0x3f);
        cksum^=decTable[src[i]]&0x3f;
        dst[j]|=(x<<2);
        ox = x;
    }
    
    if (cksum!=(decTable[src[345]]&0x3f)){
        log_error("cgcrDeocding checksum error cksum:%02X byte343:%02X",cksum,decTable[src[345]]&0x3f);
        return RET_ERR;
    }

    return RET_OK;
}
#pragma GCC diagnostic pop
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

