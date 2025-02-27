
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_woz.h"
#include "driver_nic.h"
#include "driver_dsk.h"

#include "emul_diskii.h"
#include "display.h"
#include "favorites.h"
#include "configFile.h"
#include "main.h"
#include "log.h"

//static unsigned long t1,t2,diff1=0,maxdiff1=0;

volatile int ph_track=0;                                                // SDISK Physical track 0 - 139
volatile int ph_track_b=0;                                              // SDISK Physical track 0 - 139 for DISK B 

volatile int intTrk=0;                                                  // InterruptTrk                            
unsigned char prevTrk=35;                                               // prevTrk to keep track of the last head track

extern unsigned char read_track_data_bloc[RAW_SD_TRACK_SIZE];                  // 
extern volatile unsigned char DMA_BIT_TX_BUFFER[RAW_SD_TRACK_SIZE];            // DMA Buffer for READ Track

volatile int flgWeakBit=0;                                       // Activate WeakBit only for Woz File
uint8_t flgBitIndxCounter=0;                                    // Keep track of Bit Index Counter when changing track (only for WOZ)

extern enum action nextAction;

volatile unsigned char flgDeviceEnable=0;
unsigned char flgImageMounted=0;                            // Image file mount status flag
unsigned char flgBeaming=0;                                 // DMA SPI1 to Apple II Databeaming status flag
unsigned char flgWriteProtected=0;                          // Write Protected

uint8_t optimalBitTiming=32;                                // Read Timer Adjument 

extern woz_info_t wozFile;
image_info_t mountImageInfo;

// --------------------------------------------------------------------
// Extern Declaration
// --------------------------------------------------------------------

extern TIM_HandleTypeDef htim1;                             // Timer1 is managing buzzer pwm
extern TIM_HandleTypeDef htim2;                             // Timer2 is handling WR_DATA
extern TIM_HandleTypeDef htim3;                             // Timer3 is handling RD_DATA

extern uint8_t bootMode;                    

extern FATFS fs;                                            // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                     // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState;   

extern list_t * dirChainedList;
extern uint8_t flgSoundEffect;

#ifdef A2F_MODE
extern uint8_t rEncoder;
extern uint8_t re_aState;
extern uint8_t re_bState;
extern bool re_aChanged;
extern bool re_bChanged;
#endif

// --------------------------------------------------------------------
// Hook function for file type driver Woz,Dsk, Po, ...
// --------------------------------------------------------------------

enum STATUS (*getTrackBitStream)(int,unsigned char*);       // pointer to readBitStream function according to driver woz/nic
enum STATUS (*setTrackBitStream)(int,unsigned char*);       // pointer to writeBitStream function according to driver woz/nic
long (*getSDAddr)(int ,int ,int , long);                    // pointer to getSDAddr function
int  (*getTrackFromPh)(int);                                // pointer to track calculation function
unsigned int  (*getTrackSize)(int);    

char currentFullPath[MAX_FULLPATH_LENGTH];                    // current path from root
char currentPath[MAX_PATH_LENGTH];                            // current directory name max 64 char
char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];  // fullpath from root image filename
char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];      // fullpath from root image filename


// --------------------------------------------------------------------
// Read / Write Interrupt function variables
// --------------------------------------------------------------------

static volatile unsigned int WR_REQ_PHASE=0;
static volatile unsigned int wrBitCounter=0;
static volatile unsigned int wrBytes=0;

static volatile uint8_t bitPtr=0;
static volatile int bitCounter=0;
static volatile int bytePtr=0;
static volatile int bitSize=0;
static volatile int ByteSize=0;

static int wrStartPtr=0;
static int wrEndPtr=0;
static int wrBitWritten=0;
static int wr_attempt=0;                                             // temp variable to keep incremental counter of the debug dump to file

unsigned long cAlive=0;


const uint8_t weakBitTank[]   ={1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
                                1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
                                1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1,
                                0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0,
                                0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1,
                                1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
                                0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                                0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
                                0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
                                0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
                                0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0};

volatile unsigned int weakBitTankPosition=0;

// --------------------------------------------------------------------
// Phase Interrupt function variables
// --------------------------------------------------------------------


// Magnet States --> Stepper Motor Position
//
//                N
//               0001
//        NW      |      NE
//       1001     |     0011
//                |
// W 1000 ------- o ------- 0010 E
//                |
//       1100     |     0110
//        SW      |      SE
//               0100
//                S

volatile const int magnet2Position[16] = {
//   0000 0001 0010 0011 0100 0101 0110 0111 1000 1001 1010 1011 1100 1101 1110 1111
    -1,   0,   2,   1,   4,  -1,   3,  -1,   6,   7,  -1,  -1,   5,  -1,  -1,  -1
};

volatile const int position2Direction[8][8] = {               // position2Direction[X][Y] :X ROW Y: COLUMN 
//     N  NE   E  SE   S  SW   W  NW
//     0   1   2   3   4   5   6   7
    {  0,  1,  2,  3,  0, -3, -2, -1 }, // 0 N
    { -1,  0,  1,  2,  3,  0, -3, -2 }, // 1 NE
    { -2, -1,  0,  1,  2,  3,  0, -3 }, // 2 E
    { -3, -2, -1,  0,  1,  2,  3,  0 }, // 3 SE
    {  0, -3, -2, -1,  0,  1,  2,  3 }, // 4 S
    {  3,  0, -3, -2, -1,  0,  1,  2 }, // 5 SW
    {  2,  3,  0, -3, -2, -1,  0,  1 }, // 6 W
    {  1,  2,  3,  0, -3, -2, -1,  0 }, // 7 NW
};

/**
  * @brief Phase Management for DiskII
  * @param None
  * @retval None
  */
void DiskIIPhaseIRQ(){
    
    //log_info("1"); 
    volatile unsigned char stp=(GPIOA->IDR&0b0000000000001111);
    volatile int newPosition=magnet2Position[stp];

    if (newPosition>=0){

        if (flgDeviceEnable==1){
        
        int lastPosition=ph_track&7;
        int move=position2Direction[lastPosition][newPosition];
        
        ph_track+= move;
        
        if (ph_track<0)
            ph_track=0;

        else if (ph_track>159)                                                                 
            ph_track=159;                                             

        if (flgBeaming==1)                                     
            intTrk=getTrackFromPh(ph_track);                                        // Get the current track from the accroding driver  
        
        }else{
        
        int lastPosition=ph_track_b&7;
        int move=position2Direction[lastPosition][newPosition];

        ph_track_b+= move;
        
        if (ph_track_b<0)
            ph_track_b=0;
        else if (ph_track_b>159)                                                                 
            ph_track_b=159;
        }
    }

    return;
}

uint8_t itmp=0;

static volatile uint8_t pendingWriteTrk=0;
static volatile uint8_t wrData=0;
static volatile uint8_t prevWrData=0;
static volatile uint8_t xorWrData=0;
static volatile uint8_t wrBitPos=0;
static volatile unsigned char byteWindow=0x0;

int dbg_s[256];
int dbg_e[256];

/**
  * @brief Write Req Interrupt function, Manage start and end of the write process.
  *        it 
  * @param None
  * @retval None
  */
void DiskIIWrReqIRQ(){

   // WR_REQ_PHASE=HAL_GPIO_ReadPin(WR_REQ_GPIO_Port, WR_REQ_Pin);
    
    if ((WR_REQ_GPIO_Port->IDR & WR_REQ_Pin)==0)
        WR_REQ_PHASE=0;
    else
        WR_REQ_PHASE=1;

    if (WR_REQ_PHASE==0 && flgDeviceEnable==1 && flgImageMounted==1){                       // WR_REQUEST IS ACTIVE LOW
          
        cAlive=0;
        //updateIMAGEScreen(1,intTrk);                                                      // Update Screen for Write

        HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);
        irqWriteTrack();  
        
        byteWindow=0;
        wrBitWritten=0;                                                                     // Count the number of bits sent from the A2
        wrBitPos=0;
        prevWrData=0;
        wrBitCounter=bitCounter+(8-bitCounter%8);                                           // Make it 8 Bit aligned

        wrBytes=(wrBitCounter)/8;
         
        HAL_TIM_PWM_Start_IT(&htim2,TIM_CHANNEL_3);                                         // Start the TIMER2 to get WR signals
        wrStartPtr=wrBitCounter;

    }else if (WR_REQ_PHASE==1 && flgDeviceEnable==1 && flgImageMounted==1){


        HAL_TIM_PWM_Stop_IT(&htim2,TIM_CHANNEL_3);                                          // Stop TIMER2 WRITE DATE
        irqReadTrack();
        HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
        
        wrEndPtr=wrBitCounter;
        bitCounter=wrBitCounter;

        pendingWriteTrk=1;                                                      
        cAlive=0;

        //printf("Write end total %d %d/8, started:%d, ended:%d  wrBitCounter:%d bitSize:%d /8:%d\n",wrBitWritten,wrBitWritten/8,wrStartPtr,wrEndPtr,wrBitCounter,bitSize,bitSize/8);
    }
}


/*
WRITE PART:
- TIMER2 is handling the write pulse from the Apple II and triggered by ETR1 => PA12
    
    PA12 : ETR1
    PA07 : WR_DATA
    PB09 : WR_REQ
    
    TIMER2 is 4 uS period (399)
    CC2 is 200 pulse (in the middle of the bit cell)

    0000 Reset state (WRDATA rising edge causing a short (i.e. 50ns) aysnchronous reset pulse of counter.
    0001 1us later
    0010 2 us later (half bit cell)
    0011 3 us later
    0100 4 us later (end bit cell, start new bit cell)
    0101 5 us later
    0110 6 us later (half bit cell)
    0111 7 us later
*/




/**
  * @brief 
  * @param None
  * @retval None
  */
void DiskIIReceiveDataIRQ(){
    
    if ((GPIOA->IDR & WR_DATA_Pin)==0)                                       // get WR_DATA DO NOT USE THE HAL function creating an overhead
        wrData=0;
    else
        wrData=1;
    
    wrData^= 0x01u;                                                           // get /WR_DATA
    xorWrData=wrData ^ prevWrData;                                            // Compute Magnetic polarity inversion
    prevWrData=wrData;                                                        // for next cycle keep the wrData

    byteWindow<<=1;                                                           // Shift left 1 bit to get the next one
    byteWindow|=xorWrData;                                                    // Write the next bit to the byteWindow
                                                                              // Keep track of the number of bits written to update the TMAP
    wrBitPos++;                                                               // Increase the WR Bit index
    if (wrBitPos==8){                                                         // After 8 Bits are received store the Byte
        
        DMA_BIT_TX_BUFFER[wrBytes]=byteWindow;
        byteWindow=0x0;
        wrBitPos=0;
        wrBytes++;

        if (wrBytes==ByteSize)
            wrBytes=0;
    }
        
    wrBitCounter++;                                                           // Next bit please ;)

    if (wrBitCounter>=bitSize)                                                // Same Size as the original track size
        wrBitCounter=0;                                                       // Start over at the beginning of the track

}


uint8_t nextBit=0;
volatile uint8_t *bbPtr=0x0;

volatile int zeroBits=0;                                  // Count number of consecutives zero bits
volatile unsigned char rByte=0x0;


/**
  * @brief 
  * @param None
  * @retval None
  */
void DiskIISendDataIRQ(){
    RD_DATA_GPIO_Port->BSRR=nextBit;                          // start by outputing the nextBit and then due the internal cooking for the next one

    if (intTrk==255){
        weakBitTankPosition=bitCounter%256;
        nextBit=weakBitTank[weakBitTankPosition] & 1;
    }else{
        bytePtr=bitCounter/8;
        bitPtr=bitCounter%8;
        nextBit=(*(bbPtr+bytePtr)>>(7-bitPtr) ) & 1;          // Assuming it is on GPIO PORT B and Pin 0 (0x1 Set and 0x01 << Reset)
    
      // ************  WEAKBIT ****************


#if WEAKBIT ==1

        if ( nextBit==0 && flgWeakBit==1 ){
            if (++zeroBits>3){
                nextBit=weakBitTank[weakBitTankPosition] & 1;    // 30% of fakebit in the buffer as per AppleSauce reco      
                
                if (++weakBitTankPosition>208)
                weakBitTankPosition=0;
            }
        }else{
            zeroBits=0;
        }
        
#endif
      // ************  WEAKBIT ****************

    }

    bitCounter++;
    if (bitCounter>=bitSize){
        bitCounter=0;
    }
}

/**
  * @brief 
  * @param None
  * @retval None
  */
int DiskIIDeviceEnableIRQ(uint16_t GPIO_Pin){
    // The DEVICE_ENABLE signal from the Disk controller is activeLow
    
    uint8_t  a=0;
    if ((GPIOA->IDR & GPIO_Pin)==0)
        a=0;
    else
        a=1;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (a==0 && flgBeaming==1){                                                                 // <!> TO BE TESTED 24/10
        flgDeviceEnable=1;
#ifdef A2F_MODE
    if (flgImageMounted==1){  
      HAL_GPIO_WritePin(AB_GPIO_Port,AB_Pin,GPIO_PIN_SET);
    }
#endif
        GPIO_InitStruct.Pin   = RD_DATA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
        HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

        HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);

        if (flgWriteProtected==1)
            HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                // WRITE_PROTECT is enable
        else
            HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);
    
    }else if (flgDeviceEnable==1 && a==1 ){

        flgDeviceEnable=0;
#ifdef A2F_MODE        
        HAL_GPIO_WritePin(AB_GPIO_Port,AB_Pin,GPIO_PIN_RESET);
#endif
        GPIO_InitStruct.Pin   = RD_DATA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);

        HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);                                                // Stop the Timer
        
    }
    log_info("flgDeviceEnable==%d",flgDeviceEnable);
    return flgDeviceEnable;
}


/**
  * @brief  Mount the image 
  * @param  filename: full path to the image file
  * @retval STATUS RET_ERR/RET_OK
*/
enum STATUS DiskIIMountImagefile(char * filename){
    int l=0;
    
    flgImageMounted=0;
    if (filename==NULL)
        return RET_ERR;

    FRESULT fr;
    FILINFO fno;
    
    int len=strlen(filename);
    int i=0;

    if (len==0){
        log_error("mount error filename is empty");
        return RET_ERR;
    }

    for (i=len-1;i!=0;i--){
        if (filename[i]=='/')
        break;
    }

    snprintf(mountImageInfo.title,20,"%s",filename+i+1);

    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    
    log_info("Mounting image: %s",filename);
    while(fsState!=READY){};
    fsState=BUSY;

    //fr = f_mount(&fs, "", 1);                 // to be checked 

    fr = f_stat(filename, &fno);
    switch (fr) {
        case FR_OK:
            log_info("Size: %lu", fno.fsize);
            break;
        case FR_NO_FILE:
        case FR_NO_PATH:
            log_error("\"%s\" does not exist.", filename);
            fsState=READY;
            return RET_ERR;
            break;
        default:
            log_error("An error occured. (%d)", fr);
            fsState=READY;
            return RET_ERR;
    }
    fsState=READY;
    l=strlen(filename);
    if (l>4 && 
        (!memcmp(filename+(l-4),".NIC",4)  ||           // .NIC
        !memcmp(filename+(l-4),".nic",4))){            // .nic

        if (mountNicFile(filename)!=RET_OK)
            return RET_ERR;
        
        getSDAddr=getNicSDAddr;
        getTrackBitStream=getNicTrackBitStream;
        getTrackFromPh=getNicTrackFromPh;
        getTrackSize=getNicTrackSize;
        
        mountImageInfo.optimalBitTiming=32;
        mountImageInfo.writeProtected=0;
        mountImageInfo.version=0;
        mountImageInfo.cleaned=0;
        mountImageInfo.type=0;

        flgWriteProtected=0;

    }else if (l>4 && 
        (!memcmp(filename+(l-4),".WOZ",4)  ||           // .WOZ
        !memcmp(filename+(l-4),".woz",4))) {           // .woz

        if (mountWozFile(filename)!=RET_OK)
        return RET_ERR;

        getSDAddr=getWozSDAddr;
        getTrackBitStream=getWozTrackBitStream;
        setTrackBitStream=setWozTrackBitStream;
        getTrackFromPh=getWozTrackFromPh;
        getTrackSize=getWozTrackSize;
        
        mountImageInfo.optimalBitTiming=wozFile.opt_bit_timing;
        mountImageInfo.writeProtected=wozFile.is_write_protected;
        mountImageInfo.synced=wozFile.sync;
        mountImageInfo.version=wozFile.version;
        mountImageInfo.cleaned=wozFile.cleaned;
        mountImageInfo.type=1;

        flgWriteProtected=wozFile.is_write_protected;
        
    }else if (l>4 && 
        (!memcmp(filename+(l-4),".DSK",4)  ||           // .DSK & PO
        !memcmp(filename+(l-4),".dsk",4) ||
        !memcmp(filename+(l-3),".po",3) ||
        !memcmp(filename+(l-3),".PO",3))){ 
    
        if (mountDskFile(filename)!=RET_OK)
        return RET_ERR;

        getSDAddr=getDskSDAddr;
        getTrackBitStream=getDskTrackBitStream;
        setTrackBitStream=setDskTrackBitStream;
        getTrackFromPh=getDskTrackFromPh;
        getTrackSize=getDskTrackSize;
        
        mountImageInfo.optimalBitTiming=32;
        mountImageInfo.writeProtected=0;
        mountImageInfo.synced=0;
        mountImageInfo.version=0;
        mountImageInfo.cleaned=0;
        if (!memcmp(filename+(l-4),".DSK",4) || !memcmp(filename+(l-4),".dsk",4))
        mountImageInfo.type=2;                                // DSK type
        else
        mountImageInfo.type=3; 

        flgWriteProtected=0;

    }else{
        return RET_ERR;
    }

    log_info("Mount image:OK");
    flgImageMounted=1;
    
    if (mountImageInfo.cleaned==1)
        flgWeakBit=1;

    sprintf(currentFullPathImageFilename,"%s",filename);
    log_info("currentFullPathImageFilename:%s",currentFullPathImageFilename);

    if (isFavorite(currentFullPathImageFilename)==1)
        mountImageInfo.favorite=1;
    else
        mountImageInfo.favorite=0;

    return RET_OK;
    }

enum STATUS DiskIIUnmountImage(){
    flgImageMounted=0;
    flgBeaming=0;
    // TODO add stop timer
    return RET_OK;
}


/**
  * @brief  Inite Buffer to be send to the Apple II 
  * @retval enum STATUS of the request
  */
enum STATUS DiskIIiniteBeaming(){

    if (flgImageMounted!=1){
    log_error("initeBeaming error imageFile is not mounted");
    return RET_ERR;
    }

    flgBeaming=0;

    memset((unsigned char *)&DMA_BIT_TX_BUFFER,0,sizeof(char)*RAW_SD_TRACK_SIZE);
    //memset((unsigned char *)&DMA_BIT_RX_BUFFER,0,sizeof(char)*RAW_SD_TRACK_SIZE);
    memset(read_track_data_bloc,0,sizeof(char)*RAW_SD_TRACK_SIZE);

    DWT->CYCCNT = 0;                              // Reset cpu cycle counter
    //t1 = DWT->CYCCNT; 

    if (flgWriteProtected==1)
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                              // WRITE_PROTECT is enable
    else
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);  

    HAL_GPIO_WritePin(RD_DATA_GPIO_Port,RD_DATA_Pin,GPIO_PIN_RESET); 

    bbPtr=(volatile u_int8_t*)&DMA_BIT_TX_BUFFER;
    bitSize=6656*8;
    bitCounter=0;

    TIM3->ARR=(mountImageInfo.optimalBitTiming*12)-1;

    log_info("initeBeaming optimalBitTiming:%d",mountImageInfo.optimalBitTiming);

    flgBeaming=1;

    return RET_OK;
}

/**
  * @brief Emulation Driver initilization function 
  * @param None
  * @retval None
  */
void DiskIIInit(){
    
    ph_track=0;

    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_RESET);                     // DISK II PIN 8 IS GND

    mountImageInfo.optimalBitTiming=32;
    mountImageInfo.writeProtected=0;
    mountImageInfo.version=0;
    mountImageInfo.cleaned=0;
    mountImageInfo.type=0;
    
    createBlankWozFile("test.woz",2,1,1);

    if (bootMode==0){
        if (DiskIIMountImagefile(tmpFullPathImageFilename)==RET_OK){
        
            switchPage(IMAGE,currentFullPathImageFilename);

            if (flgImageMounted==1){                                            // <!> TO BE TESTED
                if (DiskIIiniteBeaming()==RET_OK)
                    DiskIIDeviceEnableIRQ(DEVICE_ENABLE_Pin);
            }
            
        }else{
            if (tmpFullPathImageFilename[0]!=0x0)
                log_error("imageFile mount error: %s",tmpFullPathImageFilename);
            else
                log_error("no imageFile to mount");
            
            switchPage(FS,currentFullPath);
        }
    }else if (bootMode==1){
        switchPage(FS,currentFullPath);
    }else if (bootMode==2){
        switchPage(FAVORITE,NULL);
    }
    HAL_TIM_PWM_Start_IT(&htim2,TIM_CHANNEL_3); 
    
    irqReadTrack();

    //sprintf(filename,"/WOZ 2.0/Blazing Paddles (Baudville).woz");                                     // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Border Zone - Disk 1, Side A.woz");                                    // 22/08 NOT WORKING
    //sprintf(filename,"/WOZ 2.0/Bouncing Kamungas - Disk 1, Side A.woz");                              // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Commando - Disk 1, Side A.woz");                                       // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Crisis Mountain - Disk 1, Side A.woz");                                // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/DOS 3.3 System Master.woz");                                           // 15/07 WORKING
    //sprintf(filename,"/WOZ 2.0/Dino Eggs - Disk 1, Side A.woz");                                      // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/First Math Adventures - Understanding Word Problems.woz");             // 20/07 WORKING
    //sprintf(filename,"/WOZ 1.0/Hard Hat Mack - Disk 1, Side A.woz");                                  // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Miner 2049er II - Disk 1, Side A.woz");                                // 22/08 WORKING 
    //sprintf(filename,"/WOZ 2.0/Planetfall - Disk 1, Side A.woz");                                     // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Rescue Raiders - Disk 1, Side B.woz");                                 // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Sammy Lightfoot - Disk 1, Side A.woz");                                // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Stargate - Disk 1, Side A.woz");                                       // 21/08 NOT WORKING playing with /ENABLE
    //sprintf(filename,"/WOZ 2.0/Stickybear Town Builder - Disk 1, Side A.woz");                        // 22/08 WORKING 
    //sprintf(filename,"/WOZ 2.0/Take 1 (Baudville).woz");                                              // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/The Apple at Play.woz");                                               // 15/07 WORKING 
    //sprintf(filename,"/WOZ 2.0/The Bilestoad - Disk 1, Side A.woz");                                  // 20/08 WORKING
    //sprintf(filename,"/WOZ 2.0/The Print Shop Companion - Disk 1, Side A.woz");                       // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Wings of Fury - Disk 1, Side A.woz");                                  // NOT Working missing 128K of RAM
    //sprintf(filename,"/Monster Smash - Disk 1, Side A.woz");                                          // NOT WORKING 22/08

    /*
    currentImageFilename=(char *)malloc(128*sizeof(char));
    sprintf(currentImageFilename,"%s",filename);
    
    if ((mountImagefile(filename))==RET_ERR){
        log_error("Mount Image Error");
    }
    */

    if (flgBeaming==1){
        switchPage(IMAGE,currentFullPathImageFilename);
    }

    // ONLY FOR DEBUG
    /*
        byteWindow=0;
        wrBitWritten=0;                                                                     // Count the number of bits sent from the A2
        wrBitPos=0;
        prevWrData=0;
        wrBitCounter=bitCounter+(8-bitCounter%8);                                           // Make it 8 Bit aligned
        wrBytes=(wrBitCounter)/8;
        ByteSize=6384;

        for (int i=0;i<8;i++){
            DiskIIReceiveDataIRQ();
        }
    */
    // ONLY FOR DEBUG
}



/**
  * @brief 
  * @param None
  * @retval None
  */
void DiskIIMainLoop(){

    
    int trk=0;
    while(1){
        if (flgDeviceEnable==1 && prevTrk!=intTrk && flgBeaming==1){              // <!> TO Be tested

            trk=intTrk;                                   // Track has changed, but avoid new change during the process
            DWT->CYCCNT = 0;                              // Reset cpu cycle counter
            //t1 = DWT->CYCCNT;                                       
            cAlive=0;
            if (pendingWriteTrk==1){
                irqEnableSDIO();
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
                setTrackBitStream(prevTrk,DMA_BIT_TX_BUFFER);
                #pragma  GCC diagnostic pop

                irqDisableSDIO();
                pendingWriteTrk=0;
                //printf("Wr");
            }
            

            if (trk==255 ){
                bitSize=51200;
                ByteSize=6400;
                printf("ph:%02d fakeTrack:255\n",ph_track);
                prevTrk=trk;
                continue;
            }

            // --------------------------------------------------------------------
            // PART 1 MAIN TRACK & RESTORE AS QUICKLY AS POSSIBLE THE DMA
            // --------------------------------------------------------------------
            
            if (flgSoundEffect==1){
                TIM1->PSC=1000;
                HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
            }

            irqEnableSDIO();
            getTrackBitStream(trk,read_track_data_bloc);
            while(fsState!=READY){}
            irqDisableSDIO();

            memcpy((unsigned char *)&DMA_BIT_TX_BUFFER,read_track_data_bloc,RAW_SD_TRACK_SIZE);
            
            updateIMAGEScreen(0,trk);                                   // Put here otherwise Spiradisc is not working

            bitSize=getTrackSize(trk);
            ByteSize=bitSize/8; 
            prevTrk=trk;
        }else if (nextAction!=NONE){                                            // Several action can not be done on Interrupt
        
            switch(nextAction){
                case UPDIMGDISP:
                updateIMAGEScreen(WR_REQ_PHASE,trk);
                nextAction=NONE;
                break;

              
                case MKFS:
                    processMakeFsConfirmed();
                    nextAction=NONE;
                    break;
                
                case DUMP_TX:
                
                char filename[128];
                sprintf(filename,"dump_rx_trk_%d_%d.bin",intTrk,wr_attempt);
                wr_attempt++;
                
                irqEnableSDIO();
                dumpBufFile(filename,DMA_BIT_TX_BUFFER,RAW_SD_TRACK_SIZE);
                irqDisableSDIO();
                
                nextAction=NONE;
                break;

                /*case SYSRESET:
                NVIC_SystemReset();
                nextAction=NONE;
                break;
                */
                case FSMOUNT:
                // FSMOUNT will not work if SDCard is remove & reinserted...
                // system reset is preferred
                irqEnableSDIO();
            
                FRESULT fres = f_mount(&fs, "", 1);
                if (fres == FR_OK) {
                    log_info("FS mounting: OK");
                }else{
                    log_error("FS mounting: KO fres:%d",fres);
                }

                csize=fs.csize;
                database=fs.database;
                nextAction=FSDISP;
                break;

                case FSDISP:
                log_info("FSDISP fsState:%d",fsState);
                list_destroy(dirChainedList);
                log_info("FSDISP: currentFullPath:%s",currentFullPath);
                walkDir(currentFullPath);
                setConfigParamStr("currentPath",currentFullPath);
                saveConfigFile();
                initFSScreen(currentPath);
                updateChainedListDisplay(-1,dirChainedList);
                nextAction=NONE;
                break;
                
                case IMG_MOUNT:
                DiskIIUnmountImage();
                
                if (setConfigParamStr("lastFile",tmpFullPathImageFilename)==RET_ERR){
                    log_error("Error in setting param to configFie:lastImageFile %s",tmpFullPathImageFilename);
                }

                if(DiskIIMountImagefile(tmpFullPathImageFilename)==RET_OK){
                    if (DiskIIiniteBeaming()==RET_OK){  
                    DiskIIDeviceEnableIRQ(DEVICE_ENABLE_Pin);                                       // <!> TO Be tested
                    switchPage(IMAGE,tmpFullPathImageFilename);
                    }

                    if (saveConfigFile()==RET_ERR){
                    log_error("Error in saving JSON to file");
                    }else{
                    log_info("saving configFile: OK");
                    }
                }
                sprintf(currentFullPathImageFilename,"%s",tmpFullPathImageFilename);
                nextAction=NONE;
                break;

                default:
                    execAction(nextAction);
                    nextAction=NONE;
                break;
            }
        }else{
            cAlive++;
            if (flgSoundEffect==1){
                HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
            }
            
            if (cAlive==30000 && pendingWriteTrk==1){
                
                irqEnableSDIO();
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
                setTrackBitStream(prevTrk,DMA_BIT_TX_BUFFER);
                #pragma  GCC diagnostic pop
                irqDisableSDIO();
                pendingWriteTrk=0;
                //printf("W");
            }
            
            if (cAlive==5000000){
                printf(".\n");
                //printf("%ld",maxdiff1);
                cAlive=0;
                
                /*if (itmp!=0){
                    nextAction=DUMP_TX;
                    //dumpBuf(DMA_BIT_TX_BUFFER,0,6378);
                    for (int j=0;j<itmp;j++){
                        printf("dbg s:%05d e:%05d len:%d\n",dbg_s[j],dbg_e[j],dbg_e[j]-dbg_s[j]);
                        if (dbg_e[j]-dbg_s[j]>0)
                        dumpBuf(DMA_BIT_TX_BUFFER+dbg_s[j]/8,0,(dbg_e[j]-dbg_s[j])/8);
                    }
                    for (int j=0;j<itmp;j++){
                        printf("dbg s:%05d e:%05d len:%d\n",dbg_s[j],dbg_e[j],dbg_e[j]-dbg_s[j]);
                    }
                    itmp=0;
                   
                }*/
            }

#ifdef A2F_MODE
            if (HAL_GPIO_ReadPin(SD_EJECT_GPIO_Port, SD_EJECT_Pin)){// SD-Card removed!
                unlinkImageFile(currentFullPathImageFilename);
                NVIC_SystemReset();
            }

            rEncoder = HAL_GPIO_ReadPin(RE_A_GPIO_Port, RE_A_Pin);// handle rotary encoder
            if (rEncoder != re_aState){
                re_aState = rEncoder;
                re_aChanged = true;
                if (re_bChanged){
                    re_aChanged = false;
                    re_bChanged = false;
                    debounceBtn(BTN_DOWN_Pin);
                }
            }
            rEncoder = HAL_GPIO_ReadPin(RE_B_GPIO_Port, RE_B_Pin);
            if (rEncoder != re_bState){
                re_bState = rEncoder;
                re_bChanged = true;
                if (re_aChanged){
                    re_aChanged = false;
                    re_bChanged = false;
                    debounceBtn(BTN_UP_Pin);
                }
            }
#endif

        }
    }
}
