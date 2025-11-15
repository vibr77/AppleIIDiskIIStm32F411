
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "driver_woz.h"
#include "driver_nic.h"
#include "driver_dsk.h"
#include "driver_smartloader.h"

#include "emul_diskii.h"
#include "display.h"
#include "favorites.h"
#include "configFile.h"
#include "main.h"
#include "log.h"

extern SD_HandleTypeDef hsd;
//static unsigned long t1,t2,diff1=0,maxdiff1=0;
const  char * diskIIImageExt[]={"PO","po","DSK","dsk","NIC","nic","WOZ","woz",NULL};

volatile int ph_track=80;                                                                        // SDISK Physical track 0 - 139
volatile int ph_track_b=0;                                                                      // SDISK Physical track 0 - 139 for DISK B 

volatile int intTrk=0;                                                                          // InterruptTrk                            
unsigned char prevTrk=35;                                                                       // prevTrk to keep track of the last head track

extern unsigned char read_track_data_bloc[RAW_SD_TRACK_SIZE];                  
extern volatile unsigned char DMA_BIT_TX_BUFFER[RAW_SD_TRACK_SIZE];                             // DMA Buffer for READ Track
extern const  char** ptrFileFilter;
//volatile int flgWeakBit=0;                                                                      // Activate WeakBit only for Woz File
uint8_t flgBitIndxCounter=0;                                                                    // Keep track of Bit Index Counter when changing track (only for WOZ)

extern enum action nextAction;

volatile unsigned char flgDeviceEnable=0;
unsigned char flgImageMounted=0;                                                                // Image file mount status flag
unsigned char flgBeaming=0;                                                                     // DMA SDIO Beaming
unsigned char flgWriteProtected=0;                                                              // Write Protected

uint8_t optimalBitTiming=32;                                                                    // Read Timer Adjument 

extern woz_info_t wozFile;
image_info_t mountImageInfo;

extern uint8_t flgSwitchEmulationType;

// --------------------------------------------------------------------
// Extern Declaration
// --------------------------------------------------------------------

extern TIM_HandleTypeDef htim1;                                                                 // Timer1 is managing buzzer pwm
extern TIM_HandleTypeDef htim2;                                                                 // Timer2 is handling WR_DATA
extern TIM_HandleTypeDef htim3;  
//extern TIM_HandleTypeDef htim5;                                                                 // Timer3 is handling RD_DATA

extern uint8_t bootMode;                    

extern FATFS fs;                                                                                // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                                                           // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState;   

extern list_t * dirChainedList;
extern uint8_t flgSoundEffect;
extern volatile uint8_t flgScreenOff;

extern uint8_t flgWeakBit;

extern uint8_t emulationType;

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

enum STATUS (*getTrackBitStream)(int,unsigned char*);                                           // pointer to readBitStream function according to driver woz/nic
enum STATUS (*setTrackBitStream)(int,unsigned char*);                                           // pointer to writeBitStream function according to driver woz/nic
long (*getSDAddr)(int ,int ,int , long);                                                        // pointer to getSDAddr function
int  (*getTrackFromPh)(int);                                                                    // pointer to track calculation function
unsigned int  (*getTrackSize)(int);     

char currentFullPath[MAX_FULLPATH_LENGTH];                                                      // current path from root
char currentPath[MAX_PATH_LENGTH];                                                              // current directory name max 64 char
char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];                                    // fullpath from root image filename
char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];                                        // fullpath from root image filename


// --------------------------------------------------------------------
// Read / Write Interrupt function variables
// --------------------------------------------------------------------


static volatile uint8_t flgWrRequest=1;                                                         // Important Active LOW so need to be HIGH

#ifdef A2F_MODE
static volatile uint8_t flgSelect=1;                                                            // Flag for Select line, PB8 High when A2 is power on
#else
static volatile uint8_t flgSelect=0;                                                            // Flag for Select line, PB8 High when A2 is power on
#endif

static volatile unsigned int rdBitCounter=0;                                                    // read Bitcounter index to keep read head position on virtual floppy circle
volatile unsigned int wrBitCounter=0;                                                    // write Bitcounter index to keep write  head position on virtual floppy circle
static volatile unsigned int wrLastWriteStartPtr=0;                                                  // index position of the start position of write segment
static volatile unsigned int  wrDeltaLastWritePtr=0;                                                  // index position of the start position of write loop segment
static volatile unsigned char wrLoopFlg=0;                                                      // Write flag Loop when bytePtr reached wrLoopStartPtr
static volatile unsigned int wrTrack=0;                                                         // Track to write to avoid losing track info during step change

static volatile uint8_t pendingWriteTrk=0;                                                      // pending write to disc flag
                                                              // bit number of the current WR DATA char
static volatile uint8_t wrData=0;                                                               // GPIO WR DATA value
static volatile uint8_t prevWrData=0;                                                           // GPIO WR Data prev value to enable XOR (changing polarity) 
static volatile uint8_t xorWrData=0;                                                            // Result of XOR between WR Data and Prev WR DATA
static volatile unsigned char byteWindow=0x0;                                                   // Current Byte window with shifting bit of WR DATA and RD DATA

static volatile int8_t bitPos=0;                                                                // Read Bit number of the current read char
static volatile int bytePtr=0;                                                                  // BytePtr based on wrBitCounter and rdBitCounter

//static volatile int bytePtrWr=0;                                                                // BytePtr for WR DATA based on wrBitCounter
//static volatile unsigned char tmpBuffer[8192];

static volatile int bitSize=0;                                                                  // Number of bits for the current track
static volatile int ByteSize=0;                                                                 // Number of Bytes for the current track 
volatile unsigned char rByte=0x0;                                                               // Current Read Byte
static int wr_attempt=0;                                                                        // DEBUG only temp variable to keep incremental counter of the debug dump to file
static unsigned long cAlive=0;
static volatile uint8_t wrBitPos=0; 
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

volatile unsigned int weakBitTankPosition=0;                                                    // Index of the weakBitTank current position

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
    
    volatile unsigned char stp=(GPIOA->IDR&0b0000000000001111);
    volatile int newPosition=magnet2Position[stp];
    //printf("ph:%02d\n",ph_track);
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

/**
  * @brief Function to handle Select Line IRQ IDC 20 Pin 12 PB8
  *        it 
  * @param None
  * @retval None
  */
void DiskIISelectIRQ(){
    if ((SELECT_GPIO_Port->IDR & SELECT_Pin)==0)
        flgSelect=0;
    else
        flgSelect=1;
}

uint8_t pFlgWRRequest=0;
static volatile unsigned long maxt,t1,t2,diff1;
static volatile uint8_t  flgDebug=0;
static volatile int      dbg2=0,dbg1=0;
static volatile char     dbgchar1,dbgchar2,dbgchar3;   

/**
  * @brief Write Req Interrupt function, Manage start and end of the write process.
  *        it 
  * @param None
  * @retval None
***/

void DiskIIWrReqIRQ(){
    // Read WR_REQ pin state once
    uint8_t currentWrRequest = ((WR_REQ_GPIO_Port->IDR & WR_REQ_Pin) != 0);
    
    // Early exit if device not enabled
    if (flgDeviceEnable == 0)
        return;

    // Falling edge: Start write mode
    if (currentWrRequest == 0 /*&& pFlgWRRequest == 1*/) {
        // Stop read timer (combined operations)
        TIM3->DIER &= ~TIM_DIER_CC4IE;
        TIM3->CCER &= ~TIM_CCER_CC4E;
        TIM3->CR1 &= ~TIM_CR1_CEN;

        wrBitPos = 0;
        wrBitCounter = bytePtr * 8;
        wrLastWriteStartPtr = bytePtr;
        wrDeltaLastWritePtr = 0;
        
        __DSB();

        // Start write timer (combined operations)
        TIM2->DIER |= TIM_DIER_CC2IE;
        TIM2->CR1 |= TIM_CR1_CEN;
    }
    // Rising edge: End write mode, start read mode
    else if (currentWrRequest == 1 && pFlgWRRequest == 0) {
        //GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin, GPIO_PIN_SET); 
        wrTrack=intTrk;  // Store the current track to write even if step change during write
        // Stop write timer (combined operations)
        TIM2->DIER &= ~TIM_DIER_CC2IE;
        TIM2->CCER &= ~TIM_CCER_CC2E;
        TIM2->CR1 &= ~TIM_CR1_CEN;

        bitPos = 0;
        rdBitCounter = bytePtr * 8;
        rByte = DMA_BIT_TX_BUFFER[bytePtr];
        pendingWriteTrk = 1;
        cAlive = 0;
        
        __DSB();

        // Start read timer (combined operations)
        TIM3->DIER |= TIM_DIER_CC4IE;
        TIM3->CCER |= TIM_CCER_CC4E;
        TIM3->CR1 |= TIM_CR1_CEN;
        
        //GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin, GPIO_PIN_RESET);     
    }
    
    pFlgWRRequest = currentWrRequest;
    flgWrRequest = currentWrRequest;
}
/* Old version
void DiskIIWrReqIRQ(){

    if ((WR_REQ_GPIO_Port->IDR & WR_REQ_Pin)==0)
        flgWrRequest=0;
    else
        flgWrRequest=1;

    if (flgDeviceEnable==0)                                                     // A2 is not connected do nothing or DeviceEnable is not LOW
        return;

    if (flgWrRequest==0 ){                          // Write Request is active Low 
        
        // <!> The order of the line below is important DO NOT CHANGE IT <!!>
        
        TIM3->DIER &= ~TIM_DIER_CC4IE;                                      // disable CC4 interrupt
        TIM3->CCER &= ~TIM_CCER_CC4E;                                       // disable CC4 output
        TIM3->CR1 &= ~TIM_CR1_CEN;                                          // stop the timer

        flgDebug=0;
        wrBitPos=0; 
        
        //dbgchar1=0x0;                                                       // Debug only
        //dbgchar2=0x0;
        //dbgchar3=0x0;
        
        //bytePtr++;                                                        // Reset the BitPos
        wrBitCounter=bytePtr*8;
        __DSB();

        //GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_SET);           // WR_REQ is active LOW                  
        TIM2->DIER |= TIM_DIER_CC2IE;                                      // enable CC2 Channel2 interrupt
        TIM2->CR1 |= TIM_CR1_CEN;                                          // start the timerHAL_TIM_PWM_Start_IT(&htim2,TIM_CHANNEL_2); 
    
        //endingWriteTrk=1;                                                // Pending write has to be here and not in the below section

        wrLastWriteStartPtr=bytePtr;                                      // set the write loop start pointer 256 bytes after the current position
        wrDeltaLastWritePtr=0;

    }else if (flgWrRequest==1 && pFlgWRRequest==0){
        
        GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin,GPIO_PIN_SET); 
        
        TIM2->DIER &= ~TIM_DIER_CC2IE;   // disable CC3 interrupt
        TIM2->CCER &= ~TIM_CCER_CC2E;    // disable CC3 output
        TIM2->CR1 &= ~TIM_CR1_CEN;       // stop the timer
        
        dbgchar3=byteWindow;
        
        bitPos=0;                                                                               // Reset the bitPos          
        rByte=DMA_BIT_TX_BUFFER[bytePtr];
        rdBitCounter=bytePtr*8;                                                                 // Prepare the next read byte after write            
        
        TIM3->DIER  |= TIM_DIER_CC4IE;     // enable CC4 interrupt
        TIM3->CCER  |= TIM_CCER_CC4E;      // enable CC4 output
        TIM3->CR1   |= TIM_CR1_CEN;         // start the timer 
        
        flgDebug=0;
        pendingWriteTrk=1;
        cAlive=0;
        __DSB();                                                                               // Reset the cAlive
        GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin,GPIO_PIN_RESET);     
    }
    pFlgWRRequest=flgWrRequest;   
}
*/

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
     
    // Read WR_DATA pin once and combine operations
    //for (uint8_t i=0;i<10;i++);
    uint8_t wrData = ((GPIOA->IDR & WR_DATA_Pin) == 0) ? 1 : 0;
     
    //GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,wrData);
    //GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_SET);

    // Compute XOR and shift in one expression
    byteWindow = (byteWindow << 1) | (wrData ^ prevWrData);
    prevWrData = wrData;
    
    if (++wrBitPos == 8) {
        DMA_BIT_TX_BUFFER[bytePtr++] = byteWindow;
        byteWindow = 0x0;
        wrBitPos = 0;
    }
    
    if (++wrBitCounter >= bitSize) {
        DMA_BIT_TX_BUFFER[bytePtr] = byteWindow;
        byteWindow = 0x0;
        wrBitCounter = 0;
        wrBitPos = 0;
        bytePtr = 0;
    }
    //GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_RESET);
}
    

volatile uint8_t nextBit=0;
volatile uint8_t *bbPtr=0x0;

volatile int zeroBits=0;                                                                        // Count number of consecutives zero bits



/**
  * @brief 
  * @param None
  * @retval None
  */


void DiskIISendDataIRQ(){
    
    // Pre-calculate next bit and update GPIO immediately (most time-critical operation)
    nextBit = rByte >> 7;  // Simpler than ternary operator
                                
    if (nextBit == 0 && flgWeakBit == 1) {
        if (++zeroBits > 3) {
            nextBit = weakBitTank[weakBitTankPosition] & 1;
            if (++weakBitTankPosition > 208)
                weakBitTankPosition = 0;
        }
    } else {
        zeroBits = 0;
    }        
    
    RD_DATA_GPIO_Port->BSRR = nextBit;  // Output bit ASAP
    
    rByte <<= 1; 
    
    if (++bitPos == 8) {
        bitPos = 0;
        rByte = DMA_BIT_TX_BUFFER[++bytePtr];

        if (pendingWriteTrk == 1) {
            if (++wrDeltaLastWritePtr > 64 && wrLoopFlg == 0) {
                wrLoopFlg = 1;
            }  
        }
    }

    if (++rdBitCounter >= bitSize) {
        rdBitCounter = 0;
        bytePtr = 0;
        bitPos = 0;
        rByte = DMA_BIT_TX_BUFFER[0];
    }
}


/*
void DiskIISendDataIRQ(){
    
    nextBit=rByte & 0x80 ? 1:0;
                                
    if ( nextBit==0 && flgWeakBit==1 ){
        if (++zeroBits>3){
            nextBit=weakBitTank[weakBitTankPosition] & 1;    // 30% of fakebit in the buffer as per AppleSauce reco      
                
            if (++weakBitTankPosition>208)
                weakBitTankPosition=0;
        }
    }else{
        zeroBits=0;
    }        
                                                                                                // Get the MSB bit to output
                                                                                                // Shift left the byteWindow for next bit
    RD_DATA_GPIO_Port->BSRR=nextBit;                                                            // start by outputing the nextBit and then due the internal cooking for the next one
    
    rByte<<=1; 
    
    bitPos++;
    if (bitPos==8){
        bitPos=0;
        bytePtr++;
        rByte=DMA_BIT_TX_BUFFER[bytePtr];

        if (pendingWriteTrk==1){
            wrDeltaLastWritePtr++;
            if (wrDeltaLastWritePtr>256 && wrLoopFlg==0){
                wrLoopFlg=1;
                //GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_SET); 
            }  
        }
    }

    

    rdBitCounter++;
    if (rdBitCounter>=bitSize){
        
        rdBitCounter=0;
        bytePtr=0;
        bitPos=0;
        rByte=DMA_BIT_TX_BUFFER[0];
    }


    
}
*/
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

    if (a==0){                                                                 // <!> TO BE TESTED 24/10
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

        TIM3->DIER |= TIM_DIER_CC4IE;
        TIM3->CCER |= TIM_CCER_CC4E;
        TIM3->CR1 |= TIM_CR1_CEN;

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);

        GPIOWritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,flgWriteProtected);  
        /*if (flgWriteProtected==1)
            GPIOWritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                     // WRITE_PROTECT is enable
        else
            GPIOWritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);
        */
    
    }else if (flgDeviceEnable==1 && a==1 ){

        pendingWriteTrk=0;                                                                       // We do that on purpose to avoid writing on extern power
        flgDeviceEnable=0;
        //prevTrk=36;                                                                             // TODO To be tested to check if track overlap is solved 
                                                                                                  // It should force track to be reread next enable request...
#ifdef A2F_MODE
        GPIOWritePin(AB_GPIO_Port,AB_Pin,GPIO_PIN_RESET);
#endif
        GPIO_InitStruct.Pin   = RD_DATA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);
        
        TIM3->DIER &= ~TIM_DIER_CC4IE;                                                          // disable CC4 interrupt
        TIM3->CCER &= ~TIM_CCER_CC4E;                                                           // disable CC4 output
        TIM3->CR1 &= ~TIM_CR1_CEN;                                                              // stop the timer
        
    }

    /*
  
    <!> The Below part is extremly important for the timing of the IIGS  
  
    */

    for (int i=0;i<1500;i++){
        __NOP();
    } 

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
    if (filename==NULL){
        log_error("filename is null");
        return RET_ERR;
    }

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

    fr = f_stat(filename, &fno);
    log_info("fr stat %d",fr);
    switch (fr) {
        case FR_OK:
            log_info("mount file size: %lu", fno.fsize);
            break;
        case FR_NO_FILE:
            log_error("\"%s\" does not exist.", filename);
            fsState=READY;
            return RET_ERR;
        case FR_NO_PATH:
            log_error("\"%s\" does not exist.", filename);
            fsState=READY;
            return RET_ERR;
            break;
        default:
            log_error("f_stat An error occured. (%d)", fr);
            fsState=READY;
            return RET_ERR;
    }
    fsState=READY;
    l=strlen(filename);
    
    if (!strcasecmp(filename+i+1,"smartloader.dsk")){
        
        log_info("special mode smartloader");

        getSDAddr=getSmartloaderSDAddr;
        getTrackBitStream=getSmartloaderTrackBitStream;
        setTrackBitStream=setSmartloaderTrackBitStream;
        getTrackFromPh=getSmartloaderTrackFromPh;
        getTrackSize=getSmartloaderTrackSize;
        
        /*
        getSDAddr=getSmartloaderSDAddr;
        getTrackBitStream=getDskTrackBitStream;
        setTrackBitStream=setDskTrackBitStream;
        getTrackFromPh=getDskTrackFromPh;
        getTrackSize=getDskTrackSize;
        */

        if (mountSmartloaderFile(filename)!=RET_OK){
            fsState=READY;
            return RET_ERR;
        }
    
        mountImageInfo.optimalBitTiming=32;
        mountImageInfo.writeProtected=0;
        mountImageInfo.synced=0;
        mountImageInfo.version=0;
        mountImageInfo.cleaned=0;
        mountImageInfo.type=2; 

    }else if(l>4 && 
        (!memcmp(filename+(l-4),".NIC",4)  ||           // .NIC
        !memcmp(filename+(l-4),".nic",4))){            // .nic

        if (mountNicFile(filename)!=RET_OK){
            fsState=READY;
            return RET_ERR;
        }
        
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
        !memcmp(filename+(l-4),".woz",4))) {            // .woz

        if (mountWozFile(filename)!=RET_OK){
            fsState=READY;
            return RET_ERR;
        }

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
    
        if (mountDskFile(filename)!=RET_OK){
            fsState=READY;
            return RET_ERR;
        }
    
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
        fsState=READY;
        return RET_ERR;
    }

    log_info("Mount image:OK");
    flgImageMounted=1;
    
    //if (mountImageInfo.cleaned==1)
    //    flgWeakBit=1;

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
    memset(read_track_data_bloc,0,sizeof(char)*RAW_SD_TRACK_SIZE);

    if (flgWriteProtected==1)
        GPIOWritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                              // WRITE_PROTECT is enable
    else
        GPIOWritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);  

    GPIOWritePin(RD_DATA_GPIO_Port,RD_DATA_Pin,GPIO_PIN_RESET); 

    bbPtr=(volatile uint8_t*)&DMA_BIT_TX_BUFFER;
    bitSize=6656*8;
    rdBitCounter=0;

    TIM3->ARR=(mountImageInfo.optimalBitTiming*12)-1;
   // TIM2->ARR=(mountImageInfo.optimalBitTiming*12)-1-3;

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
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};                                             // This Pin should be High on IIGS but connected to Ground Disk II 
    GPIO_InitStruct.Pin = SELECT_Pin;                                                   // 
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(SELECT_GPIO_Port, &GPIO_InitStruct);
    
    //HAL_GPIO_WritePin(_35DSK_GPIO_Port,_35DSK_Pin,GPIO_PIN_RESET);
    //HAL_Delay(500);
    HAL_GPIO_WritePin(SELECT_GPIO_Port,SELECT_Pin,GPIO_PIN_RESET);
    /*
    GPIO_InitTypeDef GPIO_InitStruct = {0};                                             // This Pin should be High on IIGS but connected to Ground Disk II 
    GPIO_InitStruct.Pin = _35DSK_Pin;                                                   // 
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(_35DSK_GPIO_Port, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(_35DSK_GPIO_Port,_35DSK_Pin,GPIO_PIN_RESET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(_35DSK_GPIO_Port,_35DSK_Pin,GPIO_PIN_SET);
    */

    //TIM2->ARR=(32*12)-1;

    ph_track=0;
   
    ptrFileFilter=diskIIImageExt;
    mountImageInfo.optimalBitTiming=32;
    mountImageInfo.writeProtected=0;
    mountImageInfo.version=0;
    mountImageInfo.cleaned=0;
    mountImageInfo.type=0;
    
    if (emulationType==SMARTLOADER){

        log_info("special mode smartloader");

        getSDAddr=getSmartloaderSDAddr;
        getTrackBitStream=getSmartloaderTrackBitStream;
        setTrackBitStream=setSmartloaderTrackBitStream;
        getTrackFromPh=getSmartloaderTrackFromPh;
        getTrackSize=getSmartloaderTrackSize;
            
        mountImageInfo.optimalBitTiming=32;
        mountImageInfo.writeProtected=0;
        mountImageInfo.synced=0;
        mountImageInfo.version=0;
        mountImageInfo.cleaned=0;
        mountImageInfo.type=99; 

        sprintf(mountImageInfo.title,"SMARTLOADER");
        flgImageMounted=1;
        flgBeaming=1;

        //sprintf(tmpFullPathImageFilename,"SmartLoader");
        //sprintf(currentFullPathImageFilename,"SmartLoader");
                    
        switchPage(DISKIIIMAGE,tmpFullPathImageFilename);
                                         
        if (DiskIIiniteBeaming()==RET_OK)
            DiskIIDeviceEnableIRQ(DEVICE_ENABLE_Pin);
    
    }

    else if (bootMode==0){
        if (DiskIIMountImagefile(tmpFullPathImageFilename)==RET_OK){
        
            switchPage(DISKIIIMAGE,currentFullPathImageFilename);

            if (flgImageMounted==1){                                            // <!> TO BE TESTED
                if (DiskIIiniteBeaming()==RET_OK)
                    DiskIIDeviceEnableIRQ(DEVICE_ENABLE_Pin);
            }
            
        }else{
            if (tmpFullPathImageFilename[0]!=0x0)
                log_error("imageFile mount error: %s",tmpFullPathImageFilename);
            else
                log_error("no imageFile to mount");
            
            switchPage(MENU,0);
        }

    }else if (bootMode==1){
        walkDir(currentFullPath,ptrFileFilter);
        switchPage(FS,currentFullPath);
    }else if (bootMode==2){
        switchPage(FAVORITES,NULL);
    }

    

    //irqEnableSDIO();
    //getTrackBitStream(22,read_track_data_bloc);
     
    //irqReadTrack();
    //createBlankWozFile("/test.woz",2,2,1);

    //sprintf(filename,"/WOZ 2.0/Blazing Paddles (Baudville).woz");                                     // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Border Zone - Disk 1, Side A.woz");                                    // 22/08 NOT WORKING
    //sprintf(filename,"/WOZ 2.0/Bouncing Kamungas - Disk 1, Side A.woz");                              // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Commando - Disk 1, Side A.woz");                                       // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Crisis Mountain - Disk 1, Side A.woz");                                // 21/08 WORKING
    //sprintf(filename,"/WOZ 1.0/DOS 3.3 System Master.woz");                                           // 15/07 WORKING
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
        switchPage(DISKIIIMAGE,currentFullPathImageFilename);
    }

    /*

    irqEnableSDIO();
    FIL fil; 
    FRESULT fr;   
    fr = f_open(&fil, "dump_rx_trk_0_0.bin", FA_READ);
    char buffer[6656];
    int br;
    f_read(&fil,buffer,6656,&br);
    while(fsState!=READY){};
    log_info("br:%d",br);
    uint8_t retE=0x0;
    if (nib2dsk(buffer,0,6656,&retE)==RET_ERR)
        log_error("e");
        //dumpBuf(buffer,1,6656);
    
    log_info("outside error:%d",retE);
    f_close(&fil);
    irqDisableSDIO();
    
    HAL_Delay(10000);
    */

   flgBeaming=1;
   DiskIISelectIRQ();                                                                       // Important at the end of Init
   flgSelect=1;
}

static void processWriteTrack(uint8_t rTrk){
    // Placeholder for future write processing logic
    wrLoopFlg=0;
    pendingWriteTrk=0;
    cAlive=0;

    /*
    char filename[32];
    sprintf(filename,"rawdmp_trk_%d.bin",rTrk);
    irqEnableSDIO();
    dumpBufFile(filename,DMA_BIT_TX_BUFFER,6657);
    irqDisableSDIO();
    */

    GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin,GPIO_PIN_SET);
    
    // updateDiskIIImageScr(1,rTrk);
    
    if (flgSoundEffect==1){
        play_buzzer_ms(50);
    }
    
    irqEnableSDIO();
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    if (setTrackBitStream(rTrk,DMA_BIT_TX_BUFFER)==RET_OK){
        __NOP();
    }
    #pragma GCC diagnostic pop
    irqDisableSDIO();
    GPIOWritePin(DEBUG2_GPIO_Port, DEBUG2_Pin,GPIO_PIN_RESET);
}

/**
  * @brief 
  * @param None
  * @retval None
  */


void DiskIIMainLoop(){
    int trk=0;
    TIM2->ARR=32*12-1;
    TIM2->CCR2= 5;
    //TIM3->CCR3=32*6;
    getTrackBitStream(0,read_track_data_bloc);
    memcpy((unsigned char *)&DMA_BIT_TX_BUFFER,read_track_data_bloc,RAW_SD_TRACK_SIZE);
                
    //dumpBuf(DMA_BIT_TX_BUFFER,1,6656);
    while(1){
        //pendingWriteTrk=0;
        if (flgDeviceEnable==0){  
            
            if (flgWrRequest==1 && pendingWriteTrk==1 ){ 
                //uint8_t rTrk=intTrk;
                processWriteTrack(wrTrack);
            }
        }
        
        if (flgDeviceEnable==1){                                                             // A2 is Powered (Select Line HIGH) & DeviceEnable is active LOW
            
            
            if (flgWrRequest==1 && pendingWriteTrk==1 && wrLoopFlg==1){                     // Reading Mode, pending track to be written after a full revolution
                //uint8_t rTrk=intTrk;
                processWriteTrack(wrTrack);
            }

            if (prevTrk!=intTrk && flgBeaming==1){                                                  // <!> TO Be tested

                trk=intTrk;                                                                         // Track has changed, but avoid new change during the process                            
                cAlive=0;
            
                if (pendingWriteTrk==1){
                    processWriteTrack(wrTrack);
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
                    play_buzzer_ms(150);
                }

                irqEnableSDIO();
                while (fsState!=READY){
                    __NOP();
                }
                getTrackBitStream(trk,read_track_data_bloc);
                while(fsState!=READY);
                irqDisableSDIO();

                memcpy((unsigned char *)&DMA_BIT_TX_BUFFER,read_track_data_bloc,RAW_SD_TRACK_SIZE);
                updateDiskIIImageScr(0,trk);                                                        // Put here otherwise Spiradisc is not working

                bitSize=getTrackSize(trk);
                bytePtr=rdBitCounter/8;
                ByteSize=bitSize/8; 
                prevTrk=trk;

            }
        }

        if (nextAction!=NONE){                                                            // Several action can not be done on Interrupt
        
            switch(nextAction){
               
                case DUMP_TX:
                
                    char filename[128];
                    sprintf(filename,"dump_rx_trk_%d_%d.bin",intTrk,wr_attempt);
                    wr_attempt++;
                    
                    irqEnableSDIO();
                    dumpBufFile(filename,DMA_BIT_TX_BUFFER,RAW_SD_TRACK_SIZE);
                    irqDisableSDIO();
                    
                    nextAction=NONE;
                break;

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

                case IMG_MOUNT:
                   
                    DiskIIUnmountImage();
                    
                    if (setConfigParamStr("lastFile",tmpFullPathImageFilename)==RET_ERR){
                        log_error("Error in setting param to configFie:lastImageFile %s",tmpFullPathImageFilename);
                    }
                    
                    if(DiskIIMountImagefile(tmpFullPathImageFilename)==RET_OK){
                        if (DiskIIiniteBeaming()==RET_OK){  
                            DiskIIDeviceEnableIRQ(DEVICE_ENABLE_Pin);                                       // <!> TO Be tested
                            switchPage(DISKIIIMAGE,tmpFullPathImageFilename);
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
                    execAction(&nextAction);
                break;
            }
        }


        if (flgWrRequest==1){
            cAlive++;

            if (cAlive==5000000){ 
                HAL_SD_CardStateTypeDef state;
                state = HAL_SD_GetCardState(&hsd);                                              // To avoid SDCard to sleep and trigger an error
                printf(".%d %lu\n",fsState,state);                                              // This is ugly but no better way
                cAlive=0;
            }
        }

        pSdEject();

        if (flgSwitchEmulationType==1){
            //flgSwitchEmulationType=0;
            break;
        }


#ifdef A2F_MODE
            rEncoder = HAL_GPIO_ReadPin(RE_A_GPIO_Port, RE_A_Pin);// handle rotary encoder
            if (rEncoder != re_aState){
                re_aState = rEncoder;
                re_aChanged = true;
                if (re_bChanged){
                    re_aChanged = false;
                    re_bChanged = false;
                    debounceBtn(BTN_UP_Pin);
                }
            }
            rEncoder = HAL_GPIO_ReadPin(RE_B_GPIO_Port, RE_B_Pin);
            if (rEncoder != re_bState){
                re_bState = rEncoder;
                re_bChanged = true;
                if (re_aChanged){
                    re_aChanged = false;
                    re_bChanged = false;
                    debounceBtn(BTN_DOWN_Pin);
                }
            }
#endif
        
    }
}
