/* USER CODE BEGIN Header */
/*
__   _____ ___ ___        Author: Vincent BESSON
 \ \ / /_ _| _ ) _ \      Release: 0.61
  \ V / | || _ \   /      Date: 2024.08.23
   \_/ |___|___/_|_\      Description: Apple Disk II Emulator on STM32F4x
                2024      Licence: Creative Commons
______________________

Note 
+ CubeMX is needed to generate the code not included such as drivers folders and ...
+ An update of fatfs is required to manage long filename otherwise it crashes, v0.15 
+ any stm32fx would work, only available ram size is important >= 32 kbytes to manage 3 full track in // and also write buffer

Lessons learne:
- SDCard CMD17 is not fast enough due to wait before accessing to the bloc (140 CPU Cycle at 64 MHz), prefer CMD18 with multiple blocs read,
- Circular buffer with partial track is possible but needs complex coding to manage buffer copy and correct SDcard timing (I do not recommand),
- bitstream output is made via DMA SPI (best accurate option), do not use baremetal bitbanging with assemnbly (my first attempt) it is not accurate in ARM with internal interrupt,
- Use Interrupt for head move on Rising & Falling Edge => Capturing 1/4 moves

Current status: READ PARTIALLY WORKING / WRITE Experimental
+ woz file support : in progress first images are working
+ NIC file support : in progress first images are working


"tasks": [
			{
				"label": "FatFs CleanUP",
				"type": "shell",
				"command": "cd ./Middlewares/Third_Party/FatFs; pwd; dir; /bin/bash ./r015.sh",
				"options": {
					"cwd": "${workspaceRoot}"
				},
				"group": {
					"kind": "build",
					"isDefault": true
				},
				"problemMatcher": [
					"$gcc"
				]
			},

Architecture:

- TIM1 Timer 1 : Use to Manage the WR_DATA, ETR1 Slave Reset mode to resync with the A2 Write Pulse that is 3.958 uS instead of 4uS. Every Rising Edge resync
- TIM3 Timer 3 : Use to Manage the RD_DATA, 
- TIM4 Timer 4 : Internal no PWM, debouncer for the button

GPIO

BTN
- PC13 BTN_ENTR
- PC14 BTN_UP
- PC15 BTN_DOWN
- PB10 BTN_RET

STEP
- PA0 STEP0
- PA1 STEP1
- PA2 STEP2
- PA3 STEP3

- PB09 WR_REQ
- PB02 WR_PROTECT
- PA04 DEVICE_ENABLE
- PB10 SD_EJECT
- PA05 Manage A2 PWR 

I2C Screen SSD1306
- PB06
- PB07

*/ 

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "display.h"
//#include "fatfs_sdcard.h"
#include "list.h"
#include "driver_woz.h"
#include "driver_nic.h"
#include "configFile.h"
#include "log.h"
#include "dma_printf.h"
#include "dma_scanf.h"


//#include "parson.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_tx;

SD_HandleTypeDef hsd;
DMA_HandleTypeDef hdma_sdio_rx;
DMA_HandleTypeDef hdma_sdio_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile unsigned int *DWT_CYCCNT   = (volatile unsigned int *)0xE0001004;
volatile unsigned int *DWT_CONTROL  = (volatile unsigned int *)0xE0001000;
volatile unsigned int *DWT_LAR      = (volatile unsigned int *)0xE0001FB0;
volatile unsigned int *SCB_DHCSR    = (volatile unsigned int *)0xE000EDF0;
volatile unsigned int *SCB_DEMCR    = (volatile unsigned int *)0xE000EDFC;
volatile unsigned int *ITM_TER      = (volatile unsigned int *)0xE0000E00;
volatile unsigned int *ITM_TCR      = (volatile unsigned int *)0xE0000E80;
static int Debug_ITMDebug = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM1_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */


FATFS fs;

bool buttonDebounceState=true;
extern __uint32_t TRK_BitCount[160];

volatile int ph_track=0;                                    // SDISK Physical track 0 - 139
volatile int intTrk=0;                                      // InterruptTrk                                    
unsigned char prevTrk=0;                                    // prevTrk to keep track of the last head track

unsigned int DMABlockSize=6464;//6592; // 6656              // Size of the DMA Buffer => full track width with 13 block of 512
unsigned int RawSDTrackSize=6656;                           // Maximuum track size on NIC & WOZ to load from SD
unsigned char read_track_data_bloc[6656];                  // 3 adjacent track 3 x RawSDTrackSize
  
volatile unsigned char DMA_BIT_TX_BUFFER[6656];             // DMA Buffer from the SPI
volatile unsigned char DMA_BIT_RX_BUFFER[6656];             // DMA Buffer from the SPI

uint8_t optimalBitTiming=32;

extern woz_info_t wozFile;
volatile unsigned int WR_REQ_PHASE=0;
                              
long database=0;                                            // start of the data segment in FAT
int csize=0;                                                // Cluster size
extern uint8_t CardType;                                    // fatfs_sdcard.c type of SD card

volatile unsigned char flgDeviceEnable=0;
unsigned char flgImageMounted=0;                            // Image file mount status flag
unsigned char flgBeaming=0;                                 // DMA SPI1 to Apple II Databeaming status flag
volatile unsigned int  flgwhiteNoise=0;                     // White noise in case of blank 255 track to generate random bit
unsigned char flgWriteProtected=0;                                   // Write Protected

enum STATUS (*getTrackBitStream)(int,unsigned char*);       // pointer to readBitStream function according to driver woz/nic
enum STATUS (*setTrackBitStream)(int,unsigned char*);       // pointer to writeBitStream function according to driver woz/nic
long (*getSDAddr)(int ,int ,int , long);                    // pointer to getSDAddr function
int  (*getTrackFromPh)(int);                                // pointer to track calculation function
unsigned int  (*getTrackSize)(int);    

enum page currentPage=0;
enum action nextAction=NONE;

void (*ptrbtnUp)(void *);                                   // function pointer to manage Button Interupt according to the page
void (*ptrbtnDown)(void *);
void (*ptrbtnEntr)(void *);
void (*ptrbtnRet)(void *);



char selItem[256];                                           // select from chainedlist item;
char currentFullPath[1024];                                  // current path from root
char * currentImageFilename=NULL;                              // current mounted image filename;
char currentFullPathImageFilename[1024];                     // fullpath from root image filename

int lastlistPos;
list_t * dirChainedList;

// DEBUG BLOCK
volatile struct ilog {
    uint16_t  tc;
    uint8_t type;
    uint16_t val1;
};

uint8_t ilog_indx=0;
struct ilog il[512];
uint16_t tc1=0;
unsigned long t1,t2,diff1;   

int wr_attempt=0;
 
#define weakBit 1
const uint8_t fakeBitTankInt[]={1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                                1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
                                1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1,
                                0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                                0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1,
                                1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
                                0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                                0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
                                0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
                                0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
                                0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0};
const char fakeBitTank[]="\xE4\x23\x61\x40\x04\x0C\x84\x1E\x69\x86\x21\xA0\x32\xA1\x42\x24\x10\xD7\x05\xC7\x00\x50\x91\x82\x83\x81\x12\x14\x38\x09\xA8\x96";

volatile unsigned char headWindow=0x0A;                         // 0b0000 1010 For initialisation and avoid weakbit at the very begining (first cycle)
volatile unsigned int fakeBitTankPosition=0;                    // bit position in the fakeBitTank

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


void EnableTiming(void){
  if ((*SCB_DHCSR & 1) && (*ITM_TER & 1)) // Enabled?
    Debug_ITMDebug = 1;
 
  *SCB_DEMCR |= 0x01000000;
  *DWT_LAR = 0xC5ACCE55;                                    // enable access
  *DWT_CYCCNT = 0;                                          // reset the counter
  *DWT_CONTROL |= 1 ;                                       // enable the counter
}

volatile uint8_t bitPtr=0;
volatile int bitCounter=0;
volatile int bytePtr=0;
volatile int bitSize=0;

uint8_t nextBit=0;
volatile uint8_t *bbPtr=0x0;


/*
*
* GPIO IRQ HANDLER
*
*/
void debounceBtn(int GPIO){
  if (buttonDebounceState==true){
    printf("here %d",GPIO);
    buttonDebounceState=false; 
    TIM4->CNT=0;
    TIM4->CR1 |= TIM_CR1_CEN; 
    //processBtnInterrupt(GPIO);
  }
}
   

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void TIM4_IRQHandler(void){

  if (TIM4->SR & TIM_SR_UIF){
	  buttonDebounceState = true;
    printf("debounced\n");
  }
  TIM4->SR = 0;
}

volatile int zeroBits=0;

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */

void TIM3_IRQHandler(void){

  if (TIM3->SR & TIM_SR_UIF){
    TIM3->SR &= ~TIM_SR_UIF; 
    //RD_DATA_GPIO_Port->BSRR=1; 
    RD_DATA_GPIO_Port->BSRR=nextBit;                      // start by outputing the nextBit and then due the internal cooking for the next one
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_SET);
    bytePtr=bitCounter/8;
    bitPtr=bitCounter%8;
    nextBit=(*(bbPtr+bytePtr)>>(7-bitPtr) ) & 1;          // Assuming it is on GPIO PORT B and Pin 0 (0x1 Set and 0x01 << Reset)
    
    // Looking for WeakBit
  
    if (nextBit==0){
      if (++zeroBits>2){
        nextBit=fakeBitTankInt[fakeBitTankPosition] & 1;    // 30% of fakebit in the buffer as per AppleSauce reco

        if (++fakeBitTankPosition>213)
          fakeBitTankPosition=0;
      }
    }else{
      zeroBits=0;
    }
   
  


    bitCounter++;
    if (bitCounter>=bitSize){
      bitCounter=0;
    }

                                 // Clear the overflow interrupt 
  }else if (TIM3->SR & TIM_SR_CC1IF){                     // Pulse compare interrrupt on Channel 1
    RD_DATA_GPIO_Port->BSRR=1U <<16;                      // Rest the RD_DATA GPIO
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);
    TIM3->SR &= ~TIM_SR_CC1IF;                            // Clear the compare interrupt flag
  }else
    TIM3->SR = 0;
}



/*
WRITE PART:
- TIMER1 is handling the write pulse from the Apple II
         and triggered by ETR1 => PA12
    
    PA12 : ETR1
    PA07 : WR_DATA
    PB09 : WR_REQ
    
    TIMER1 is 4 uS period (399)
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

volatile uint8_t wrData=0;
volatile uint8_t prevWrData=0;
volatile uint8_t xorWrData=0;
volatile uint8_t wrBitPos=0;
volatile unsigned int wrBitCounter=0;
volatile unsigned int wrBytes=0;

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void TIM1_UP_TIM10_IRQHandler(void){
  if (TIM1->SR & TIM_SR_UIF){       
    TIM1->SR &= ~TIM_SR_UIF;                                                  // Reset the Interrupt
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_SET);
  }else{
    TIM1->SR=0;
  }     
}

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void TIM1_CC_IRQHandler(void){
  
  if (TIM1->SR & TIM_SR_CC2IF){                                               // The count & compare is on channel 2 to avoid issue with ETR1
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);

    wrData=HAL_GPIO_ReadPin(WR_DATA_GPIO_Port, WR_DATA_Pin);  // get WR_DATA
   
    wrData^= 0x01u;                                                           // get /WR_DATA
    xorWrData=wrData ^ prevWrData;                                            // Compute Magnetic polarity inversion
    prevWrData=wrData;                                                        // for next cycle keep the wrData

    wrBytes=wrBitCounter/8;
    wrBitPos=wrBitCounter%8;
    DMA_BIT_RX_BUFFER[wrBytes]|=xorWrData<<(7-wrBitPos);

    wrBitCounter++;                                                           // Next bit please ;)
    if (wrBitCounter>bitSize)                                                 // Same Size as the original track size
      wrBitCounter=0;                                                         // Start over at the beginning of the track

    TIM1->SR &= ~TIM_SR_CC2IF;                                                // clear the count & compare interrupt
  }
   TIM1->SR=0;
}


/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void irqReadTrack(){

  HAL_NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
  HAL_NVIC_DisableIRQ(TIM1_CC_IRQn);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);

  HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
  HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void irqWriteTrack(){
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
  HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
  HAL_NVIC_DisableIRQ(TIM3_IRQn);
  HAL_NVIC_DisableIRQ(TIM4_IRQn);
  HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void dumpIlog(){
  for (int i=0;i<ilog_indx;i++){
    if (i==0){
      log_info("%03d: %05d %c %d",i,il[i].tc,il[i].type,il[i].val1);
    }else{
      int d=(il[i].tc-il[i-1].tc);
      log_info("%03d: %05d %03d %c %d",i,il[i].tc,d,il[i].type,il[i].val1);
    } 
  }
}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
enum STATUS dumpBufFile(char * filename,unsigned char * buffer,int length){

  FATFS FatFs; 	//Fatfs handle
  FIL fil; 		  //File handle
  FRESULT fres; //Result after operations
  
  fres = f_mount(&FatFs, "", 1); //1=mount now
  if (fres != FR_OK) {
	  log_error("f_mount error (%i)\n", fres);
    return RET_ERR;
  }
  
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if(fres != FR_OK) {
	  log_error("f_open error (%i)\n", fres);
    return RET_ERR;
  }
 
  UINT bytesWrote;
  UINT totalBytes=0;

  for (int i=0;i<13;i++){

    fres = f_write(&fil, buffer+i*512, 512, &bytesWrote);
    if(fres == FR_OK) {
      totalBytes+=bytesWrote;
    }else{
	    log_error("f_write error (%i)\n",fres);
      return RET_ERR;
    }
  }

  log_info("Wrote %i bytes to '%s'!\n", totalBytes,filename);
  f_close(&fil);
  
  return RET_OK;
}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
enum STATUS writeTrkFile(char * filename,char * buffer,uint32_t offset){
  
  FATFS FatFs; 	//Fatfs handle
  FIL fil; 		  //File handle
  FRESULT fres; //Result after operations
  
  /*fres = f_mount(&FatFs, "", 1); //1=mount now
  if (fres != FR_OK) {
	  log_error("f_mount error (%i)\n",fres);
    return RET_ERR;
  }*/
  
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if(fres != FR_OK) {
	  log_error("f_open error (%i)\n",fres);
    return RET_ERR;
  }

  fres=f_lseek(&fil,offset);
  if(fres != FR_OK) {
    log_error("f_lseek error (%i)\n",fres);
    return RET_ERR;
  }

  UINT bytesWrote;
  UINT totalBytes=0;

  int blk=(DMABlockSize/512);
  int lst_blk_size=DMABlockSize%512;

  for (int i=0;i<blk/2;i++){
    fres = f_write(&fil, buffer+i*1024, 1024, &bytesWrote);
    if(fres == FR_OK) {
      totalBytes+=bytesWrote;
    }else{
	    log_error("f_write error (%i)\n",fres);
      return RET_ERR;
    }
  }
  if (lst_blk_size!=0){
    fres = f_write(&fil, buffer+blk*512, lst_blk_size, &bytesWrote);
    if(fres == FR_OK) {
      totalBytes+=bytesWrote;
    }else{
      log_error("f_write error (%i)\n",fres);
     
      return RET_ERR;
    }
  }

  log_debug("Wrote %i bytes to '%s' starting at %ld!\n", totalBytes,filename,offset);
  f_close(&fil);
  return RET_OK;

}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void dumpBuf(unsigned char * buf,long memoryAddr,int len){

  log_info("dump Buffer addr:%ld len:%d",memoryAddr,len);
 
  for (int i=0;i<len;i++){
      if (i%16==0){
        if (i%512==0)
          printf("\n-");
        printf("\n%03X: ",i);
        
      }
    printf("%02X ",buf[i]);
   
  }
  printf("\n");
}
/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
char *byte_to_binary(int x){
    char * b=(char*)malloc(9*sizeof(char));
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1){
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
    return b;
}

volatile enum FS_STATUS fsState=READY;

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void Custom_SD_WriteCpltCallback(void){
  
  if (fsState==WRITING){
    fsState=READY;                                                                                       // Reset cpu cycle counter
    t2 = DWT->CYCCNT;
    diff1 = t2 - t1;
    log_info(" Custom_SD_WriteCpltCallback diff %ld",diff1);
  }
}

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void Custom_SD_ReadCpltCallback(void){
  
  if (fsState==READING || fsState==BUSY){
    fsState=READY;                                                                                       // Reset cpu cycle counter
    t2 = DWT->CYCCNT;
    diff1 = t2 - t1;
    log_info(" Custom_SD_ReadCpltCallback diff %ld",diff1);
  }
}

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void getDataBlocksBareMetal(long memoryAdr,unsigned char * buffer,int count){
  fsState=READING;
  DWT->CYCCNT = 0; 
  t1 = DWT->CYCCNT;
  if (HAL_SD_ReadBlocks_DMA(&hsd, (uint8_t *)buffer, memoryAdr, count) != HAL_OK){
    log_error("Error HAL_SD_ReadBlocks_DMA");
  }
}

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void setDataBlocksBareMetal(long memoryAdr,unsigned char * buffer,int count){
  fsState=WRITING;
  DWT->CYCCNT = 0; 
  t1 = DWT->CYCCNT;
  if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t *)buffer, memoryAdr, count) != HAL_OK){
    log_error("Error HAL_SD_WriteBlocks_DMA");
  }
}

/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
void printBits(unsigned char x){
  for(int i=sizeof(x)<<3; i; i--)
    putchar('0'+((x>>(i-1))&1));
  putchar(' ');
}


/**
  * @brief sort a new chainedlist of item alphabetically
  * @param list_t * plst
  * @retval None
  */
list_t * sortLinkedList(list_t * plst){

  list_t *  sorteddirChainedList = list_new();
  list_node_t *pItem;
  list_node_t *cItem;
  int z=0;
  int i=0;
  do{
    pItem=list_at(plst,0);
    for (i=0;i<plst->len;i++){
      cItem=list_at(plst,i);
      z=strcmp(pItem->val,cItem->val);
      if (z>0){
        pItem=cItem;
        i=0;
      }
    }
    
    list_rpush(sorteddirChainedList, list_node_new(pItem->val));
    list_remove(plst,pItem);
  }while (plst->len>0);
  list_destroy(plst);
  return sorteddirChainedList; 
}

/**
  * @brief Build & sort a new chainedlist of file/dir item based on the current path
  * @param path
  * @retval RET_OK/RET_ERR
  */
enum STATUS walkDir(char * path){
  DIR dir;
  FRESULT     fres;  
  // FatFs; 	//Fatfs handle

  while(fsState!=READY){};

  /*fres = f_mount(&FatFs, "", 1); //1=mount now
  if (fres != FR_OK) {
    log_error("f_mount error (%i)\n", fres);
      fsState=READY;
      return RET_ERR;
  }*/


  fres = f_opendir(&dir, path);
 
  log_info("directory listing:%s",path);

  if (fres != FR_OK){
    log_error("f_opendir error (%i)\n",fres);
    return RET_ERR;
  }
    
  char * fileName;
  int len;
  lastlistPos=0;
  dirChainedList=list_new();

  if (fres == FR_OK){
      if (strcmp(path,"") && strcmp(path,"/")){
        fileName=malloc(128*sizeof(char));
        sprintf(fileName,"D|..");
        list_rpush(dirChainedList, list_node_new(fileName));
        lastlistPos++;
      }
      
      while(1){
        FILINFO fno;

        fres = f_readdir(&dir, &fno);
 
        if (fres != FR_OK){
          log_error("Error f_readdir:%d path:%s\n", fres,path);
          return RET_ERR;
        }
        if ((fres != FR_OK) || (fno.fname[0] == 0))
          break;
                                                                          // 256+2
        len=(int)strlen(fno.fname);                                       // Warning strlen
        
        if (((fno.fattrib & AM_DIR) && 
            !(fno.fattrib & AM_HID) && len>2 && fno.fname[0]!='.' ) ||     // Listing Directories & File with NIC extension
            (len>5 &&
            (!memcmp(fno.fname+(len-4),"\x2E\x4E\x49\x43",4)  ||           // .NIC
             !memcmp(fno.fname+(len-4),"\x2E\x6E\x69\x63",4)  ||           // .nic
             !memcmp(fno.fname+(len-4),"\x2E\x57\x4F\x5A",4)  ||           // .WOZ
             !memcmp(fno.fname+(len-4),"\x2E\x77\x6F\x7A",4)) &&           // .woz
             !(fno.fattrib & AM_SYS) &&                                    // Not System file
             !(fno.fattrib & AM_HID)                                       // Not Hidden file
             )
             
             ){
              
          fileName=malloc(64*sizeof(char));
          if (fno.fattrib & AM_DIR){
            fileName[0]='D';
            fileName[1]='|';
            strcpy(fileName+2,fno.fname);
          }else{
            fileName[0]='F';
            fileName[1]='|';
            memcpy(fileName+2,fno.fname,len);
            fileName[len+2]=0x0;
          }
            list_rpush(dirChainedList, list_node_new(fileName));
            lastlistPos++;
          }
       
        
       log_info("%c%c%c%c %10d %s/%s",
          ((fno.fattrib & AM_DIR) ? 'D' : '-'),
          ((fno.fattrib & AM_RDO) ? 'R' : '-'),
          ((fno.fattrib & AM_SYS) ? 'S' : '-'),
          ((fno.fattrib & AM_HID) ? 'H' : '-'),
          (int)fno.fsize, path, fno.fname);
        
      }
    }
  
  dirChainedList=sortLinkedList(dirChainedList);
  f_closedir(&dir);
  return RET_OK;
}

/**
  * @brief  Check if SD Card is ejected or not
  * @param None
  * @retval None
  */
char processSdEject(uint16_t GPIO_PIN){
  log_info("processSdeject");
  
  int sdEject=HAL_GPIO_ReadPin(SD_EDJECT_GPIO_Port, GPIO_PIN);        // Check if SDCard is ejected
  if (sdEject==1){  
    flgImageMounted=0;

    initSdEjectScreen();                                                                      // Display the message on screen
    
    do {  
      sdEject=HAL_GPIO_ReadPin(SD_EDJECT_GPIO_Port, GPIO_PIN);                 // wait til it has changed
    }while(sdEject==1);
  
  }
  return sdEject;
}


/**
  * @brief  Check if DiskII is Enable / Disable => Device Select is active low
  * @param None
  * @retval None
  */
char processDeviceEnableInterrupt(uint16_t GPIO_Pin){
  
  // The DEVICE_ENABLE signal from the Disk controller is activeLow
   
  uint8_t  a=HAL_GPIO_ReadPin(DEVICE_ENABLE_GPIO_Port,GPIO_Pin);
  //uint8_t  b=HAL_GPIO_ReadPin(A2_PWR_GPIO_Port,A2_PWR_Pin);
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (a==0){
    flgDeviceEnable=1;

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
      HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);  // WRITE_PROTECT is enable
    else
      HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);
  
  }else if (flgDeviceEnable==1 && a==1 /*&& b==1*/){
    
    flgDeviceEnable=0;
    
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
  * @brief  Trigger by External GPIO Interrupt 
  *         pointer to function according the page are linked to relevant function;   
  * @param GPIO_Pin
  * @retval None
  */
void processBtnInterrupt(uint16_t GPIO_Pin){     

  switch (GPIO_Pin){
    case BTN_UP_Pin:
      ptrbtnUp(NULL);
      log_debug("BTN UP"); 
      break;
    case BTN_DOWN_Pin:
      ptrbtnDown(NULL);
      log_debug("BTN DOWN");
      break;
    case BTN_ENTR_Pin:
      ptrbtnEntr(NULL);
      log_info("BTN ENT");
      break;
    case BTN_RET_Pin:

      ptrbtnRet(NULL);
     log_info("BTN RET");
      break;
    default:
      break;
  }       
}

/**
  * @brief  processPrevFSItem(), processNextFSItem(), processSelectFSItem()
  *         are functions linked to button DOWN/UP/ENTER to manage fileSystem information displayed,
  *         and trigger the action   
  * @param  Node
  * @retval None
  */

int currentClistPos;
int nextClistPos;
extern uint8_t dispSelectedIndx;
extern uint8_t selectedFsIndx;

#define MAX_LINE_ITEM 4
void processPrevFSItem(){
    
    uint8_t lstCount=dirChainedList->len;
    
    if (lstCount<=MAX_LINE_ITEM && dispSelectedIndx==0){
      dispSelectedIndx=lstCount-1;
    }else if (dispSelectedIndx==0)
      if (currentClistPos==0)
        currentClistPos=lstCount-1;
      else
        currentClistPos=(currentClistPos-1)%lstCount;
    else{
      dispSelectedIndx--;
    }
    log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
    updateFSDisplay(-1);
}

void processNextFSItem(){ 
    
    uint8_t lstCount=dirChainedList->len;
    
    if (lstCount<=MAX_LINE_ITEM && dispSelectedIndx==lstCount-1){
      dispSelectedIndx=0;
    }else if (dispSelectedIndx==(MAX_LINE_ITEM-1))
      currentClistPos=(currentClistPos+1)%lstCount;
    else{
      dispSelectedIndx++;
    }
    log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
    updateFSDisplay(-1);
}

void processSelectFSItem(){
  // Warning Interrupt can not trigger Filesystem action otherwise deadlock can occured !!!
  
  if (nextAction==FSDISP)     // we need to wait for the previous action to complete (a deadlock might have happened)
    return;

  list_node_t *pItem=NULL;
  
  pItem=list_at(dirChainedList, selectedFsIndx);
  sprintf(selItem,"%s",(char*)pItem->val);
  
  int len=strlen(currentFullPath);
  if (selItem[2]=='.' && selItem[3]=='.'){                // selectedItem is [UpDir];
    for (int i=len-1;i!=-1;i--){
      if (currentFullPath[i]=='/'){
        currentFullPath[i]=0x0;
        dispSelectedIndx=0;
        currentClistPos=0;
        nextAction=FSDISP;
        break;
      }
      if (i==0)
        currentFullPath[0]=0x0;
    }
  }else if (selItem[0]=='D' && selItem[1]=='|'){        // selectedItem is a directory
    sprintf(currentFullPath+len,"/%s",selItem+2);
    dispSelectedIndx=0;
    currentClistPos=0;
    nextAction=FSDISP;
    printf("OK1 %s\n",currentFullPath);
  }else{
    swithPage(MOUNT,selItem);
  }
  log_debug("result |%s|",currentFullPath);
  
}

int toggle=1;
void processToogleOption(){
  
  if (toggle==1){
    toggle=0;
    toggleMountOption(0);
  }else{
    toggle=1;
    toggleMountOption(1);
  }
}

void processMountOption(){
  if (toggle==0){
    swithPage(FS,NULL);
    toggle=1;                               // rearm toggle switch
  }else{
    nextAction=IMG_MOUNT;                       // Mounting can not be done via Interrupt, must be done via the main thread
    //swithPage(FS,NULL);
  }
}

void nothing(){
  __NOP();
}

void processBtnRet(){
  if (currentPage==MOUNT || currentPage==IMAGE){
    swithPage(FS,NULL);
  }else if (currentPage==FS){

  }
}

enum STATUS swithPage(enum page newPage,void * arg){

// Manage with page to display and the attach function to button Interrupt  
  
  switch(newPage){
    case FS:
      initFSScreen(currentFullPath);
      updateFSDisplay(-1);
      ptrbtnUp=processNextFSItem;
      ptrbtnDown=processPrevFSItem;
      ptrbtnEntr=processSelectFSItem;
      //ptrbtnRet=nothing;
      ptrbtnRet=processBtnRet;
      currentPage=FS;
      break;
    case MENU:
      break;
    case IMAGE:
       initIMAGEScreen(currentImageFilename,0);
       ptrbtnUp=nothing;
       ptrbtnDown=nothing;
       ptrbtnEntr=nothing;
       ptrbtnRet=processBtnRet;
       currentPage=IMAGE;
      break;
    case MOUNT:
      mountImageScreen((char*)arg+2);
      ptrbtnEntr=processMountOption;
      ptrbtnUp=processToogleOption;
      ptrbtnDown=processToogleOption;
      ptrbtnRet=processBtnRet;
      currentPage=MOUNT;
      break;
    default:
      return RET_ERR;
      break;
  }
  return RET_OK;
}

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

void processDiskHeadMoveInterrupt(uint16_t GPIO_Pin){

 
 volatile unsigned char stp=(GPIOA->IDR&0b0000000000001111);

  volatile int newPosition=magnet2Position[stp];

  if (newPosition>=0){
      
    int lastPosition=ph_track&7;
    int move=position2Direction[lastPosition][newPosition];
    
    ph_track+= move;
  
    if (ph_track<0)
      ph_track=0;

    if (ph_track>160)                                                                 
      ph_track=160;                                             
                                          
    intTrk=getTrackFromPh(ph_track);                                        // Get the current track from the accroding driver                   
  }
}

/**
  * @brief  Mount the image 
  * @param  filename: full path to the image file
  * @retval STATUS RET_ERR/RET_OK
*/
enum STATUS mountImagefile(char * filename){
  int l=0;
  
  flgImageMounted=0;
  if (filename==NULL)
    return RET_ERR;

  FRESULT fr;
  FILINFO fno;
  


  log_info("Mounting image: %s",filename);
  while(fsState!=READY){};



  fsState=BUSY;
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
      (!memcmp(filename+(l-4),"\x2E\x4E\x49\x43",4)  ||           // .NIC
       !memcmp(filename+(l-4),"\x2E\x6E\x69\x63",4))){            // .nic

     if (mountNicFile(filename)!=RET_OK)
        return RET_ERR;
    
     getSDAddr=getSDAddrNic;
     getTrackBitStream=getNicTrackBitStream;
     getTrackFromPh=getNicTrackFromPh;
     getTrackSize=getNicTrackSize;
     optimalBitTiming=32;
     flgWriteProtected=0;
  }else if (l>4 && 
      (!memcmp(filename+(l-4),"\x2E\x57\x4F\x5A",4)  ||           // .WOZ
       !memcmp(filename+(l-4),"\x2E\x77\x6F\x7A",4))) {           // .woz

    if (mountWozFile(filename)!=RET_OK)
      return RET_ERR;

    getSDAddr=getSDAddrWoz;
    getTrackBitStream=getWozTrackBitStream;
    setTrackBitStream=setWozTrackBitStream;
    getTrackFromPh=getWozTrackFromPh;
    getTrackSize=getWozTrackSize;
    

    optimalBitTiming=wozFile.opt_bit_timing;
    flgWriteProtected=wozFile.is_write_protected;
    
  }else{
    return RET_ERR;
  }

  log_info("Mount image:OK");
  flgImageMounted=1;
  return RET_OK;
}


/**
  * @brief  Inite Buffer to be send to the Apple II 
  * @retval enum STATUS of the request
  */
enum STATUS initeBeaming(){
  
  if (flgImageMounted!=1){
    return RET_ERR;
  }

  flgBeaming=0;

  memset(DMA_BIT_TX_BUFFER,0,sizeof(char)*DMABlockSize);
  memset(DMA_BIT_RX_BUFFER,0,sizeof(char)*DMABlockSize);
  memset(read_track_data_bloc,0,sizeof(char)*RawSDTrackSize);


  DWT->CYCCNT = 0;                              // Reset cpu cycle counter
  t1 = DWT->CYCCNT; 
 
  //flgWriteProtected=1;

  if (flgWriteProtected==1)
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                              // WRITE_PROTECT is enable
  else
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_RESET);  

  HAL_GPIO_WritePin(RD_DATA_GPIO_Port,RD_DATA_GPIO_Port,GPIO_PIN_RESET); 

  //memcpy(DMA_BIT_TX_BUFFER,read_track_data_bloc,RawSDTrackSize);
  
  bbPtr=(volatile u_int8_t*)&DMA_BIT_TX_BUFFER;
  bitSize=6656*8;
  bitCounter=0;

  TIM3->ARR=(optimalBitTiming*125/10)-1;

  log_info("initeBeaming optimalBitTiming:%d",optimalBitTiming);
  
  flgBeaming=1;
  return RET_OK; 
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  //FATFS fs;
  FRESULT fres;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_FATFS_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_SDIO_SD_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */

  log_set_level(LOG_INFO);

  log_info("************** BOOTING ****************");                      // Data to send
  log_info("**     This is the sound of sea !    **");
  log_info("***************************************");
  
 
  EnableTiming();                                                          // Enable WatchDog to get precise CPU Cycle counting
  memset(DMA_BIT_TX_BUFFER,0,6656*sizeof(char));

  int T1_DIER=0x0;
  T1_DIER|=TIM_DIER_CC2IE;
  T1_DIER|=TIM_DIER_UIE;
  TIM1->DIER|=T1_DIER;                                                     // Enable Output compare Interrupt
  
  int T4_DIER=0x0;
  T4_DIER|=TIM_DIER_CC2IE;
  T4_DIER|=TIM_DIER_UIE;
  TIM4->DIER|=T4_DIER;   

  int dier=0x0;
  dier|=TIM_DIER_CC1IE;
  dier|=TIM_DIER_UIE;
  TIM3->DIER=dier;


  //initScreen();  
                                                                  // I2C Screen init
  HAL_Delay(1000);
  //processSdEject(SD_EJECT_Pin);
  
  processDeviceEnableInterrupt(DEVICE_ENABLE_Pin);
  flgDeviceEnable=1;

  dirChainedList = list_new();                                                      // ChainedList to manage File list in current path
  currentClistPos=0;                                                                // Current index in the chained List
  lastlistPos=0;                                                                    // Last index in the chained list

  int trk=35;
  ph_track=160;
                                                                           
  char *imgFile=NULL;
  

  fres = f_mount(&fs, "", 1);                                       
  
  if (fres == FR_OK) {
    /*log_info("Loading config file");
    if (loadConfigFile()==RET_ERR){
      log_error("Error in loading configFile");
      //setConfigFileDefaultValues();
      //saveConfigFile();
    }

    //setConfigFileDefaultValues();
    //saveConfigFile();
    imgFile=(char*)getConfigParamStr("lastFile");
    char *tmp=(char*)getConfigParamStr("currentPath");
    
    if (tmp)
      sprintf(currentFullPath,"%s",tmp);
    else
      sprintf(currentFullPath,"");
    if (imgFile!=NULL){
      log_info("lastFile:%s",imgFile);
      currentImageFilename=(char*)malloc(256*sizeof(char));
      sprintf(currentImageFilename,"%s",imgFile);
    }
  
    walkDir(currentFullPath);
    //walkDir("");
  */
  }else{
    log_error("Error mounting sdcard %d",fres);
  }

  csize=fs.csize;
  database=fs.database;
  
  /*
  if (mountImagefile(currentImageFilename)==RET_OK){
    swithPage(IMAGE,NULL);

    if (flgImageMounted==1){
      initeBeaming();
      processDeviceEnableInterrupt(DEVICE_ENABLE_Pin);
    }
  }else{
    if (currentImageFilename!=NULL)
      log_error("error Mounting file:%s",imgFile);
    else
       log_error("error no file to mount");
    swithPage(FS,NULL);
  }
  */

    irqReadTrack();
    char filename[128];
    //sprintf(filename,"/Blank(4).woz");
    //sprintf(filename,"/spiradisc/Frogger.woz");
    //sprintf(filename,"/spiradisc/Lunar Leepers.woz");
    
    //sprintf(filename,"/Locksmith v6.0B.woz");
    //sprintf(filename,"/DK.woz");                                                                      // WORKS only with 6656 DMABuf size;
    //sprintf(filename,"/Bouncing Kamungas crk.woz");
    //sprintf(filename,"/ER16.woz");                                                                    // WORKING
    //sprintf(filename,"/MH.woz");
    //sprintf(filename,"Locksmithcrk.nic");
    //sprintf(filename,"Locksmith 6.0 Fast Disk.nic");
    //sprintf(filename,"Locksmith.woz");
    //sprintf(filename,"/Zaxxon.woz");
    //sprintf(filename,"/WOZ 2.0/Blazing Paddles (Baudville).woz");                                     // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Border Zone - Disk 1, Side A.woz");                                    // 22/08 NOT WORKING
    //sprintf(filename,"/WOZ 2.0/Bouncing Kamungas - Disk 1, Side A.woz");                              // 22/08 NOT WORKING
    //sprintf(filename,"/WOZ 2.0/Commando - Disk 1, Side A.woz");                                       // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Crisis Mountain - Disk 1, Side A.woz");                                  // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/DOS 3.3 System Master.woz");                                           // 15/07 WORKING
    //sprintf(filename,"/WOZ 2.0/Dino Eggs - Disk 1, Side A.woz");                                      // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/First Math Adventures - Understanding Word Problems.woz");             // 20/07 WORKING
    //sprintf(filename,"/WOZ 2.0/Hard Hat Mack - Disk 1, Side A.woz");                                  // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Miner 2049er II - Disk 1, Side A.woz");                                // 22/08 WORKING 
    //sprintf(filename,"/WOZ 2.0/Planetfall - Disk 1, Side A.woz");                                     // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Rescue Raiders - Disk 1, Side B.woz");                                 // 21/08 WORKING
    sprintf(filename,"/WOZ 2.0/Sammy Lightfoot - Disk 1, Side A.woz");                                // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Stargate - Disk 1, Side A.woz");                                       // 21/08 NOT WORKING playing with /ENABLE
    //sprintf(filename,"/WOZ 2.0/Stickybear Town Builder - Disk 1, Side A.woz");                        // 22/08 WORKING 
    //sprintf(filename,"/WOZ 2.0/Take 1 (Baudville).woz");                                              // 21/08 WORKING
    //sprintf(filename,"/WOZ 2.0/The Apple at Play.woz");                                               // 15/07 WORKING 
    //sprintf(filename,"/WOZ 2.0/The Bilestoad - Disk 1, Side A.woz");                                    // 20/08 WORKING
    //sprintf(filename,"/WOZ 2.0/The Print Shop Companion - Disk 1, Side A.woz");                       // 22/08 WORKING
    //sprintf(filename,"/WOZ 2.0/Wings of Fury - Disk 1, Side A.woz");                                  // NOT Working missing 128K of RAM
    //sprintf(filename,"/Monster Smash - Disk 1, Side A.woz");                                          // NOT WORKING 22/08
    
    //sprintf(filename,"diversidos_src working.woz");
    //sprintf(currentImageFilename,"%s",filename);
    
    if ((mountImagefile(filename))==RET_ERR){
     log_error("Mount Image Error");
    }

    if (flgImageMounted==1){
      initeBeaming();
      //swithPage(IMAGE,NULL);
    }


  unsigned long cAlive=0;

  volatile int newBitSize=0;
  volatile uint32_t oldBitSize=0;
  volatile uint32_t oldBitCounter=0;
  volatile uint32_t newBitCounter=0;

/* 
  // Use to test a specific track
  bitSize=getTrackSize(0); 
  getTrackBitStream(0,DMA_BIT_TX_BUFFER);
        
  while(fsState!=READY){}
  HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
  while(1){}

*/
  HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);
  log_info("here1\n");
  while (1){

    if (flgDeviceEnable==1 && prevTrk!=intTrk && flgImageMounted==1){

     
  
      trk=intTrk;                                   // Track has changed, but avoid new change during the process
      
      DWT->CYCCNT = 0;                              // Reset cpu cycle counter
      t1 = DWT->CYCCNT;                                       
      
      if (trk==255){
       /*
          bitSize=253;
          memcpy(DMA_BIT_TX_BUFFER,fakeBitTank,bitSize/8);
          printf("ph:%02d newTrack:255 \n",ph_track);
        */
        prevTrk=trk;
        continue;
      }
     
      // --------------------------------------------------------------------
      // PART 1 MAIN TRACK & RESTORE AS QUICKLY AS POSSIBLE THE DMA
      // --------------------------------------------------------------------
      
      HAL_Delay(2);                                                                                                                                                      
      
      HAL_NVIC_EnableIRQ(SDIO_IRQn);
      HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
      HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

      getTrackBitStream(trk,read_track_data_bloc);
        
      while(fsState!=READY){}

      HAL_NVIC_DisableIRQ(SDIO_IRQn);
      HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
      HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
      
      memcpy(DMA_BIT_TX_BUFFER,read_track_data_bloc,RawSDTrackSize);
      if (intTrk==0){
        HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_SET);
        //bitCounter=0;
      }else{
         HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);
      }


      t2 = DWT->CYCCNT;
      diff1 = t2 - t1;
      
      oldBitSize=bitSize;
      oldBitCounter=bitCounter;
      newBitSize=getTrackSize(trk); 
          
      newBitCounter = (int) (oldBitCounter * oldBitSize) / newBitSize;
      bitSize=newBitSize;

      prevTrk=trk;
      printf("trk %d %d newBitCounter:%ld bitCounter:%d\n",trk,bitSize,newBitCounter,bitCounter);

    }else if (nextAction!=NONE){                                         // Several action can not be done on Interrupt
      
      switch(nextAction){
        case UPDIMGDISP:
          updateIMAGEScreen(WR_REQ_PHASE,trk);
        case WRITE_TRK:
          long offset=getSDAddr(trk,0,csize,database);
          //writeTrkFile("/Blank.woz",DMA_BIT_TX_BUFFER,offset);
          setDataBlocksBareMetal(offset,DMA_BIT_TX_BUFFER,13);
          nextAction=NONE;

          break;
        case DUMP_TX:
          char filename[128];
          sprintf(filename,"dump_rx_trk_%d_%d.bin",intTrk,wr_attempt);
          wr_attempt++;
          //dumpBufFile(filename,DMA_BIT_RX_BUFFER,DMABlockSize);
          //dumpBuf(DMA_BIT_RX_BUFFER,1,bitSize/8);
          printf("bitsize %d\n",bitSize);
          memcpy(DMA_BIT_TX_BUFFER,DMA_BIT_RX_BUFFER,DMABlockSize);
          memset(DMA_BIT_RX_BUFFER,0,DMABlockSize);                      // WARNING TO BE TESTED
   
          nextAction=NONE;
          break;
        case FSDISP:
          log_info("FSDISP fsState:%d",fsState);
          list_destroy(dirChainedList);
          log_info("FSDISP: currentFullPath:%s",currentFullPath);
          walkDir(currentFullPath);
          setConfigParamStr("currentPath",currentFullPath);
          saveConfigFile();
          currentClistPos=0;
          initFSScreen("");
          updateFSDisplay(-1); 
          nextAction=NONE;
          break;
        
        case IMG_MOUNT:

          int len=strlen(currentFullPath)+strlen(selItem)+1;
          char * tmp=(char *)malloc(len*sizeof(char));
          sprintf(tmp,"%s/%s",currentFullPath,selItem+2);
          sprintf(currentImageFilename,"%s",selItem+2);
          
          if (setConfigParamStr("lastFile",tmp)==RET_ERR){
            log_error("Error in saving param to configFie:lastImageFile %s",tmp);
          }

          mountImagefile(tmp);
          initeBeaming();
          
          dumpConfigParams();
          if (saveConfigFile()==RET_ERR){
            log_error("Error in saving JSON to file");
          }

          free(tmp);
          swithPage(IMAGE,NULL);
          nextAction=NONE;
          break;
        default:
          break;
      }
    }else{
      cAlive++;
      if (cAlive==50000000){
        printf(".\n");
        cAlive=0;
      }
    }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 240;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* EXTI0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  /* EXTI1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  /* EXTI2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
  /* EXTI3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
  /* EXTI4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
  /* TIM1_UP_TIM10_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
  /* TIM1_CC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM1_CC_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
  /* TIM3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  /* SDIO_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SDIO_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SDIO_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
  /* EXTI9_5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  /* EXTI15_10_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 13, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_4B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDIO_Init 2 */
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  if (HAL_SD_Init(&hsd) != HAL_OK){
    Error_Handler();
  }
  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK){
    Error_Handler();
  }

  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 399;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
  sSlaveConfig.InputTrigger = TIM_TS_ETRF;
  sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
  sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
  sSlaveConfig.TriggerFilter = 0;
  if (HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 200;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_2);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 399;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_OC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 132;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_1);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 120;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 50000;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 400;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OnePulse_Init(&htim4, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, RD_DATA_Pin|WR_PROTECT_Pin|DEBUG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BTN_ENTR_Pin BTN_DOWN_Pin BTN_UP_Pin */
  GPIO_InitStruct.Pin = BTN_ENTR_Pin|BTN_DOWN_Pin|BTN_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : STEP0_Pin STEP1_Pin STEP2_Pin STEP3_Pin */
  GPIO_InitStruct.Pin = STEP0_Pin|STEP1_Pin|STEP2_Pin|STEP3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DEVICE_ENABLE_Pin */
  GPIO_InitStruct.Pin = DEVICE_ENABLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DEVICE_ENABLE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : WR_DATA_Pin */
  GPIO_InitStruct.Pin = WR_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(WR_DATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RD_DATA_Pin */
  GPIO_InitStruct.Pin = RD_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : WR_PROTECT_Pin DEBUG_Pin */
  GPIO_InitStruct.Pin = WR_PROTECT_Pin|DEBUG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_EDJECT_Pin */
  GPIO_InitStruct.Pin = SD_EDJECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SD_EDJECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_RET_Pin */
  GPIO_InitStruct.Pin = BTN_RET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_RET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : WR_REQ_Pin */
  GPIO_InitStruct.Pin = WR_REQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(WR_REQ_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Callback function for External interrupt
  * @param  GPIO_Pin
  * @retval None
  */

int rx_start;
int rx_end;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

 // printf("startr here 0 %d\n",GPIO_Pin);
  if( GPIO_Pin == STEP0_Pin   ||               // Step 0 PB8
      GPIO_Pin == STEP1_Pin   ||               // Step 1 PB9
      GPIO_Pin == STEP2_Pin   ||               // Step 2 PB10
      GPIO_Pin == STEP3_Pin 
     ){            // Step 3 PB11
    
    processDiskHeadMoveInterrupt(GPIO_Pin);
   
  }else if (GPIO_Pin==DEVICE_ENABLE_Pin){
    processDeviceEnableInterrupt(DEVICE_ENABLE_Pin);
  }else if (GPIO_Pin==SD_EDJECT_Pin){
    processSdEject(GPIO_Pin);
  }
  
  else if ((GPIO_Pin == BTN_RET_Pin   ||      // BTN_RETURN
            GPIO_Pin == BTN_ENTR_Pin  ||      // BTN_ENTER
            GPIO_Pin == BTN_UP_Pin    ||      // BTN_UP
            GPIO_Pin == BTN_DOWN_Pin          // BTN_DOWN
            ) && buttonDebounceState==true){
    
    buttonDebounceState=false;

    printf("startr here 0 %d\n",GPIO_Pin);
    TIM4->CNT=0;
    TIM4->CR1 |= TIM_CR1_CEN;

    processBtnInterrupt(GPIO_Pin);

  } else if (GPIO_Pin == WR_REQ_Pin){
  
    if (WR_REQ_PHASE==0){                             // WR_REQUEST IS ACTIVE LOW
      irqWriteTrack();
      HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);
      

      HAL_TIM_PWM_Start_IT(&htim1,TIM_CHANNEL_3);                  // start the timer1
      WR_REQ_PHASE=1;                                 // Write has begun :)
      
      rx_start=bitCounter;
      wrBitCounter=bitCounter-bitCounter%8;     
      
      printf("Write started wrBitCounter:%d\n",wrBitCounter);             
      
    }else{
      WR_REQ_PHASE=0;                                 // Write has just stopped
     
      HAL_TIM_PWM_Stop_IT(&htim1,TIM_CHANNEL_3); 

      irqReadTrack(); 
      HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
      
      rx_end=wrBitCounter;
      bitCounter=wrBitCounter;
      
      /*
       int Bstart=rx_start_indx/8;
       int Bend;
       if (rx_end_indx%8!=0)
         Bend =rx_end_indx/8+1;
        else
         Bend =rx_end_indx/8;

        int Bsize;
        if (Bsize%8!=0)
          Bsize=bitSize/8+1;
        else
          Bsize=bitSize/8;
    
      if (rx_end_indx>rx_start_indx){
        int len=Bend-Bstart+1;
        printf("Case 1 bStart:%d bEnd:%d Bsize:%d len:%d\n",Bstart,Bend,Bsize,len);
        memcpy(DMA_BIT_TX_BUFFER+Bstart,DMA_BIT_RX_BUFFER+Bstart,len);
      }else{
        
        int len=Bsize-Bstart;
        printf("case 2 bStart:%04d bEnd:%04d Bsize:%04d len:%04d\n",Bstart,Bend,Bsize,len);
        printf("       +start:%04d len:%04d\n",Bstart,len);
        printf("        start:0 len:%04d\n",Bend+1);
        memcpy(DMA_BIT_TX_BUFFER+Bstart,DMA_BIT_RX_BUFFER+Bstart,len);
        memcpy(DMA_BIT_TX_BUFFER,DMA_BIT_RX_BUFFER,Bend+1);

      }

      */
      memcpy(DMA_BIT_TX_BUFFER,DMA_BIT_RX_BUFFER,6656);
      memset(DMA_BIT_RX_BUFFER,0,6656);
      nextAction=WRITE_TRK;
      //nextAction=DUMP_TX;
      printf("Write end started:%d, end:%d  wrBitCounter:%d bitSize:%d /8:%d\n",rx_start,rx_end,wrBitCounter,bitSize,bitSize/8);


     
    }

  }else {
      __NOP();
  }
}


/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);  
  return ch;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
   log_error("Oups I lost my mind, and then I crashed with no inspiration\n");
  while (1)
  {
   
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
