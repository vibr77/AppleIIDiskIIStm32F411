/* USER CODE BEGIN Header */
/*
__   _____ ___ ___        Author: Vincent BESSON
 \ \ / /_ _| _ ) _ \      Release: 0.80.7
  \ V / | || _ \   /      Date: 2025.03.26
   \_/ |___|___/_|_\      Description: Apple Disk II Emulator on STM32F4x
                2025      Licence: Creative Commons
______________________


Todo:
- Add the screen PWR on a pin and not directly on the +3.3V
- Add 74LS125 to protect the STM32 against AII over current
- Add .2Mg file support
- Add feature request from Retro-Device A / B Head positioning 
- Adding on PCB jumper solder
- Adding on PCB Power Pin 11


Note 
+ CubeMX is needed to generate the code not included such as drivers folders and ...
+ An update of fatfs is required to manage long filename otherwise it crashes, v0.15 
+ stm32f411 blackpill v3 if SDIO Sdcard Management otherwise, any STM32fx would work with SPI / DMA,
+ Adding a manual procedure to simulate the headsync with noise

Navigation:
- Config items:
  - makefs
  - boot last or boot favorite
  - fs save current dir
  - wipe favorites
  - wipe config
  
Lessons learned:
- IIGS in Fast mode, emulation diskII does not work, suspecting phase signal to be shorted
- Warning screen update with DMA while SDIO is running can perform SDIO error
- When SDIO appears one way is to investigate is to disable all interrupt & renable one by one
- Using a clock divider is also helping, clockDIV by 2 is solving the SDCard issue
- SDCard need to be formated with 64 sectors of 512 Bytes each / cluster mkfs.fat -F 32 -s64 
- SDCard CMD17 is not fast enough due to wait before accessing to the bloc (140 CPU Cycle at 64 MHz), prefer CMD18 with multiple blocs read,

- Bitstream output is made via DMA SPI (best accurate option), do not use baremetal bitbanging with assemnbly (my first attempt) it is not accurate in ARM with internal interrupt,
- Use Interrupt for head move on Rising & Falling Edge => Capturing 1/4 moves
- Warning Bootloader might take too much time to load and thus missing the headsync procedure,
- When there is a filesystem issue at start up it is sometime due to f_stat file to take long and generate an error for an image in config that does not exists on the filesystem


Current status: READ PARTIALLY WORKING / WRITE Experimental
+ Woz file support : in progress first images are working
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
- TIM1 Timer 1 : Use to Manage passive buzzer on PIN 14 (Channel 2N), via a PWM 
- TIM2 Timer 2 : Use to Manage the WR_DATA, ETR1 Slave Reset mode to resync with the A2 Write Pulse that is 3.958 uS instead of 4uS. Every Rising Edge resync
- TIM3 Timer 3 : Use to Manage the RD_DATA, 
- TIM4 Timer 4 : Internal no PWM, debouncer for the control button
- TIM5 Timer 5 : Deadlock timer check
- TIM9 Timer 9 : Screen Saver timer (if enable)

GPIO
  - PA10 DEBUG 

BTN
  - PC13 BTN_DOWN
  - PC14 BTN_UP
  - PC15 BTN_ENTR
  - PB12 BTN_RET

STEP
  - PA0 STEP0
  - PA1 STEP1
  - PA2 STEP2
  - PA3 STEP3

  - PB09 WR_REQ
  - PB02 WR_PROTECT
  - PA04 DEVICE_ENABLE
  - PB13 SD_EJECT

SDIO
  - PB4 DO
  - PA8 D1
  - PA9 D2
  - PB5 D3
  - PA6 CMD
  - PB15 CK

I2C Screen SSD1306
  - PB06 SCL
  - PB07 SDA

UART1
  - PA15 TX
  - PB3 RX

*/ 

// Changelog

/*
12.08.25 v0.80.18
  + [Main] Adding ScreenSaver feature to save the OLED, switch off every 60 min if activated in setting
  + [Settings] Adding ScreenSaver Option
  + [SmartLoader] Change of Address range for the new version of smartloas based on RWTS
03.08.25 v0.80.17
  + [Emulation] Adding a new setting in emulation type with smartloader to launch smartloader at startup
27.07.25 v0.80.16
  + [Smartloader] Add multipage feature, left and right arrow
  + [Smartloader] Add last folder saved for next reboot
  + [Smartloader] fix loading of image (sometime it was crashing)
  + [Smartloader] adding 20ms delay after reading block to avoid dead lock
  + [Smartloader] move arm track by one to enable easy reread in case of error
20.05.25 v0.80.13
  + [SMARTPORT] Fixing deadlock on FS access
  + [SMARTPORT] Manage Eject of Image and Disk present / not present
19.05.25 v0.80.12
  + [FATFS] changing strfunc in conf
  + [Smartport] adding ROM03 reinit devicelist
  + [Smartport] adding break on timeout
  + [Smartport] adding break on checksum failed to avoid corruption of sdcard block
  + [MAIN] adding TIM5 to manage deadlock
  + [PCB] beta release of PCB v6
10.04.25 v0.80.11
  + [Smartport] adding support for hdv file extension
  + [diskII] fix corrupted track on successive basic file writing
  + [diskII] fix empty directory when booting last directory
04.04.25 v0.80.9
  + Fixing instability on SDCard Access (Removing the constraints on DMA & SDIO IRQ)
26.03.25 v0.80.7
  + Fixing Favorites Screen
  + Adding Favorites to Smartport HD
  + Adding option to select boot index directly from Smartport Image
  + Adding toogle favorites SmartportHD
  + Display Fav Icon
26.03.25 v0.80.6
  + [SDEJECT] Disk Creation from Filesystem (PO,DSK,...)
  + [SDEJECT] Fixing SD Eject function for all emulator
  + [MAINMENU] Making the main menu dynamic according to emulation type
  + [UNIDISK / HD EDJECT] Manage Eject request from Smartport on update screen on SmartDisk
  + [SMARTPORT ] checksum and write fix
  + [SMARTPORT ] deadlock on some read query to Ack not asserted, workaround
22.03.25 v0.81
  + [SDEJECT] Fixing SD Eject function for all emulator
  + [MAINMENU] Making the main menu dynamic according to emulation type
  - [UNIDISK / HD EDJECT] Manage Eject request from Smartport on update screen on SmartDisk
  - [SMARTPORT WRITE KO] error on checksum to be fixed 
22.03.25 v0.80.5d
  + [DSK/PO] write process
  + [DSK/PO] sector skewing
  + [READ] optimize function timing duration
  + [WRITE] GCR_6_2 chksum comp
  + [WEAKBIT] option in setting
  + [UNIDISK 2MG] Full emulation of SMartport Unidisk with 2MG image file
  + [DISPLAY] Huge code clean up
06.03.25
  + [ALL] code refactoring
  + [FS] Adding directory option
  + [SDEJECT] Removing HAL, bitbanging to GPIO
  + [SDEJECT] Add function trigger to emul loop
  + [SDEJECT] Add sysreset after sdcard reinserted
03.03.25 v0.80.5
  + Add extension filter to walkdir depending of the type of emulator 
26.02.25 v0.80.4
  + [Smartport] mount regression fix
  + [Smartport] 2mg file ext disp fix
  + [WOZ] Correcting head / sector / region calculation
  + [FS]  Adding "." item on every location to add future access to option
  + [FATFS] Change conf of FATFS to manage STRFUNC
  + [WOZ] Passing the wozardry verify & dump test for both 3.5 & 5.25
  + [WOZ] Manage CRC32
  + [WOZ] Adding Blank woz file creation for 3.5
  + [WOZ] Adding Blank woz file creation for 5.25
  - [Smartport IIGS] TODO: Fix Splashscreen duration with Animation
  - [WOZ] TODO: blank WOZ 3.5 1 Byte slip to fix

23.02.25 v0.80.3
  + [All] Code cleaning, compilation warning cleaning
  + [Smartport] 2MG Extension checking fixed
  + [2MG] Fixing 2MG mount blockCount function
  + [Smartport] Fixing Missing Extended DIB Status
  + [SmartPort] Managing errorCode on CMD
  + [SmartPort] code refactoring
  + [SmartPort] Working on 2MG file support with Smartport
  + [SmartPort] Changed deviceType & subType according to the type of diskFormat (PO & 2MG)
  + [SmartPort] Added Standard Extended Function Call
  + [Merged] Merge code with A2F Contributor JoSch
  + [Display] Fixing some code on display
20.02.25 v0.80.2
  + Add confirmation screen MakeFS
  + Change display of MakeFS
  + MKFS on the Main Thread and not by interrupt
  + Deep work on 2MG & disk35
06.02.25 v0.80.1
  + [SmartPort] Fixing Smartport write process, and it WORKS!!!
  + [SmartPort] Managing incomming checksum command
  + [SmartPort] Optimizing read time 

02.02.25 v0.79.6
  + [Smartport] Fixing boot duration adjust to 500ms IIGS boot issue
02.02.25 v0.79.5
  + [ALL] Changing Timer 2 period -1
  + [SmartPort] Fixing mounting image
  + [SmartPort] Fixing SmartPort boot order
  + [SmartPort] Fixing SmartPort boot index
  + [SmartPort] Fixing display Image
  + [SmartPort] Adding display status
30.01.25 v0.79.4
  + Fix Menu item (Emulation / Sound)
  + Fix Menu <4 item navigation 
26.01.25 v0.79.3
  + Smartport HD list of loaded image on screen
  + Smartport HD remove HAL GPIO overhead on WRITE Process
  + Case v3 
24.01.25 v0.79.2 
  + Add Boot image index for Smartport
  + Woz write process working
  + bug fix on DMA management
  + Smartport fix (still need to work on the write process...)
  + Fix the TIMER perid to receive data
  + Move away ffrom HAL function and use baremetal GPIO 
  + Freeup unused buffer 6K
  + Init DOS3.3 working with delay write
26.12.24 v0.79.1
  +Remove hardcoded emulation type
13.12.24  v0.79
  +Fix: complete code restructuring to enable different emulation type
  +Feat: Add Smartport HD emulation type
  +Feat: Add Config menu to manage emulation type
  +Feat: Manage physical head positioning for Disk A & B

25.11.24: v0.78.4
  +Feat: Locksmith Certify works and Fast copy also
25.11.24: v0.78.3
  +Feat: makefs in the menu config
  +Fix: compilation option for fatfs mkfs
  +Fix : add fatfs label option
  +Feat: add confirmation question on mkfs from menu config
  +Fix: variable casting uint8 instead of int in getConfigParamUInt8
  +Fix: Empty SDCard crash, sortlst check for empty chainedlist
  +Feat: writing part 1 setting the woz driver function
  +Feat: writing part 2 correct write bitstream and fix bit shift
24.11.24: v0.78.2
  +Fix: bootloader delay providing a coldstart issue on several images
23.11.24: v0.78.1
  +Fix: Spiradisc fix, screenupdate was before memcopy was generation an extra delay and thus failing with cross track sync
22.11.24: v0.78
  +fix: WOZ more than 40 trk disk such as lode runner, add Max TRK

22.11.24: v0.77
  +fix: DSK reading, wrong NIBBLE_BLOCK_SIZE from 512 to 416
  +fix: NIC reading, wrong NIBBLE_BLOCK_SIZE from 512 to 416

20.11.24: v0.76
  +fix: weakbit issue, changing the threshold to 4 (instead of 2), Bouncing Kamungast is working
  +fix: weakbit the Print shop is now working, wrong variable used
  +fix: buzzer sound change after button is pressed (prescaler was not reset)
  +fix: woz file info properties not correctly propagated to flg variable
  +fix: harvey ball to be fixed

04.11.24: v0.72
  +fix: directory with name of 1 char not read by the emulator
  +fix: PO/po file extension add to the list of extension
  +feat: add menu image (toggle Favorite, unmount, unlink)
  +fix SDIO IRQ for unlink

01.11.24: v0.71
  +feat: config Menu
  +feat: favorites
  +Add: btn repetition for up/down (end of debouncer timer, check for btn state)
  +Adjust: TIM4 period on repeat and back to 400
  +Change: SDIO precaler to 1 (instead of 2) for performance purpose
  +Add: favorites primitives functions
  +refactor: rationnalization of display function
  +add: toggle add/remove favorite from image screen
  +add: favorite parsing on Image Mounting
  +fix: issue with SDIO IRQ on saveConfiguration
  +fix: issue with SDIO IRQ on favoriteSaveConfiguration
  +fix: issue with favorite filename displaying full path
  +fix: missing icon for favorite in ImageScreen
  +add: define.h
  +fix: init chainedlist based screen with 0 and first row selected
  +refactor: display function
31.10.24: v0.69
  +Add PO file support
  +Fix DSK driver getSdAddr 8*512 instead of 16*512, a track in DSK is 4096,8192
30.10.24: v0.68
  + Fix Woz v1.0 file reading (wrong offset to read the double byte and conversion to uint_16)
28.10.24: v0.67
  + Fix Reset on startup
  + Fix Lack of display at startup (same issue as Reset)
  + Fix lost mount ref (due to IRQ disable between read)
28.10.24: v0.66
  + Fix NIC Drive not handling the right number of block to read
  + Mod the readme 
24.10.24:
  + Modification of the flow from mount to beaming
  + Remove hardcoded critical variable
  + Adding new MainMenu screen 

24.10.23: 
  +Change Makefile, 
  +Add option for bootloader,
  +Modifiy issue with button order,
  +Change timer setting to match USB max setting / 96Mhz
  +Add UF2 file management with dedicated bootloader
  +Add (experimental mkfs option)
*/

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "display.h"
#include "list.h"

#include "emul_smartport.h"
#include "emul_diskii.h"
#include "emul_disk35.h"
#include "configFile.h"
#include "favorites.h"
#include "log.h"

#include "driver_dsk.h"
#include "driver_woz.h"
#include "driver_nic.h"
#include "driver_2mg.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#ifdef A2F_MODE
#pragma message("...building with A2F mode!")
#endif
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_tx;

SD_HandleTypeDef hsd;
DMA_HandleTypeDef hdma_sdio_rx;
DMA_HandleTypeDef hdma_sdio_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim9;

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
static void MX_SDIO_SD_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM9_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/*
* 4 Physical button function
*/
void (*ptrbtnUp)(void *);                                 
void (*ptrbtnDown)(void *);
void (*ptrbtnEntr)(void *);
void (*ptrbtnRet)(void *);

// Hook function for different type emulation
void (*ptrPhaseIRQ)();                                                                          // Pointer to function managing Ph0 to Ph3
void (*ptrReceiveDataIRQ)();                                                                    // Pointer to Received Data function with WR_DATA
void (*ptrSendDataIRQ)();                                                                       // Pointer to Send Data function with RD_DATA
void (*ptrWrReqIRQ)();                                                                          // Pointer to Interrupt function for WR_REQUEST
void (*ptrSelectIRQ)();                                                                         // Pointer to Select line IRQ function PB8 and IDC line 12 
int (*ptrDeviceEnableIRQ)(uint16_t GPIO_Pin);                                                   // Pointer to Interrupt function for DEVICE_ENABLE
enum STATUS (*ptrUnmountImage)();                           
enum STATUS (*ptrMountImagefile)(char * filename);
void (*ptrMainLoop)();                                                                          // Main Loop function pointer
void (*ptrInit)();                                                                              // Init function pointer

bool buttonDebounceState=true;

FATFS fs;                                                                                       // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
long database=0;                                                                                // start of the data segment in FAT
int csize=0;                                                                                    // Cluster size

const char ** ptrFileFilter=NULL;
unsigned char read_track_data_bloc[RAW_SD_TRACK_SIZE];                  
volatile unsigned char DMA_BIT_TX_BUFFER[RAW_SD_TRACK_SIZE];                                    // DMA Buffer for READ Track

extern char currentFullPath[MAX_FULLPATH_LENGTH];                                               // current path from root
extern char currentPath[MAX_PATH_LENGTH];                                                       // current directory name max 64 char
extern char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];                             // fullpath from root image filename
extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];                                 // fullpath from root image filename


char sTmp[256];
uint8_t iTmp=0;

uint8_t flgWeakBit=0;
uint8_t flgSoundEffect=0;                                                                       // Activate Buzze
volatile uint8_t flgScreenSaver=0;
volatile uint8_t flgDisplaySleep=0;
uint8_t bootMode=0;
uint8_t emulationType=0;
uint8_t bootImageIndex=0;
list_t * dirChainedList;

volatile uint8_t flgBreakLoop=0;
volatile enum action nextAction=NONE;

#ifdef A2F_MODE
// rotary encoder state
uint8_t rEncoder;
uint8_t re_aState;
uint8_t re_bState;
bool re_aChanged;
bool re_bChanged;
#endif

// DEBUG BLOCK

unsigned long t1,t2,diff1;   

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

int ui16NothingHook(uint16_t GPIO_Pin){
  return 0;
}

enum STATUS statusNothingHook(char * ptr){
  return RET_ERR;
}

void nothingHook(void*){
  return;
}

/**
  * @brief Button debouncer that reset the Timer 4
  * @param GPIO
  * @retval None
  */
void debounceBtn(int GPIO){

  if (flgDisplaySleep==1){
    nextAction=DISPLAY_WAKEUP;
    return;
  }

  buttonDebounceState=false;
  TIM4->CNT=0;
  TIM4->CR1 |= TIM_CR1_CEN;
  processBtnInterrupt(GPIO);
  return;
}



void TIM1_BRK_TIM9_IRQHandler(void){

  if (TIM9->SR & TIM_SR_UIF){
    TIM9->SR &= ~TIM_SR_UIF;        
  } 
  
  else if (TIM9->SR & TIM_SR_CC1IF){                     // Pulse compare interrrupt on Channel 1
    TIM9->SR &= ~TIM_SR_CC1IF;
    TIM9->SR=0;
    if (flgScreenSaver==1)
      nextAction=DISPLAY_OFF;
    return;
                                   // Clear the compare interrupt flag
  }else
    TIM9->SR = 0;
    return;
}

void TIM5_IRQHandler(void){

  if (TIM5->SR & TIM_SR_UIF){
    
  } if (TIM5->SR & TIM_SR_CC1IF){                         // Pulse compare interrrupt on Channel 1
    flgBreakLoop=1;
    TIM5->SR &= ~TIM_SR_CC1IF;                            // Clear the compare interrupt flag
  }else
    TIM5->SR = 0;
}

/**
  * @brief TIMER4 IRQ Handler, manage the debouncer of the control button (UP,DWN,RET,ENTER)
  * @param 
  * @retval None
  */
void TIM4_IRQHandler(void){

  if (TIM4->SR & TIM_SR_UIF){
    buttonDebounceState = true;
    log_debug("debounced\n");
    if(HAL_GPIO_ReadPin(BTN_UP_GPIO_Port, BTN_UP_Pin)){
      debounceBtn(BTN_UP_Pin);
      TIM4->ARR=200;                                      // Manage repeat acceleration
    }       
    else if(HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin)){
      debounceBtn(BTN_DOWN_Pin);
      TIM4->ARR=200;                                      // Manage repeat acceleration
    }
    else{
      TIM4->ARR=400;                                      // No repeat back to normal timer value
    }

#ifdef A2F_MODE
    if(HAL_GPIO_ReadPin(BTN_RET_GPIO_Port, BTN_RET_Pin) && // Reset on push all
       HAL_GPIO_ReadPin(BTN_ENTR_GPIO_Port, BTN_ENTR_Pin) &&
       HAL_GPIO_ReadPin(BTN_UP_GPIO_Port, BTN_UP_Pin) &&
       HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin)){
          NVIC_SystemReset();
    }
#endif

  }
  TIM4->SR = 0;
}

/**
  * @brief TIMER 3 IRQ Interrupt is handling the reading process
  * @param None
  * @retval None
  */
void TIM3_IRQHandler(void){
  //HAL_TIM_IRQHandler(&htim9);
  if (TIM3->SR & TIM_SR_UIF){
    TIM3->SR &= ~TIM_SR_UIF;                              // Clear the overflow interrupt 
    ptrSendDataIRQ();
  }else if (TIM3->SR & TIM_SR_CC1IF){                     // Pulse compare interrrupt on Channel 1
    RD_DATA_GPIO_Port->BSRR=1U <<16;                      // Reset the RD_DATA GPIO
    TIM3->SR &= ~TIM_SR_CC1IF;                            // Clear the compare interrupt flag
  }else
    TIM3->SR = 0;
}

volatile enum FS_STATUS fsState=READY;

/**
  * @brief TIMER 2 IRQ Handler is managing the WR_DATA Signal from the A2,       
  * @param (void)
  * @retval None
  */
void TIM2_IRQHandler(void){

  if (TIM2->SR & TIM_SR_UIF){ 
    TIM2->SR &= ~TIM_SR_UIF;                                                  // Reset the Interrupt
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);
    
  }else if (TIM2->SR & TIM_SR_CC2IF){                                        // The count & compare is on channel 2 to avoid issue with ETR1
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_SET);
    ptrReceiveDataIRQ();
    //HAL_GPIO_WritePin(DEBUG_GPIO_Port,DEBUG_Pin,GPIO_PIN_RESET);
    TIM2->SR &= ~TIM_SR_CC2IF;                                                // clear the count & compare interrupt
    //TIM2->SR=0;

  }else{
    TIM2->SR=0;
  }    
}


/**
  * @brief Adjust Enable IRQ for reading process to avoid jitter / delay / corrupted data 
  * @param None
  * @retval None
  */
void irqReadTrack(){

  HAL_NVIC_DisableIRQ(TIM2_IRQn);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
}
/**
  * @brief Adjust Enable IRQ for writting process to avoid jitter / delay / corrupted data
  * @param None
  * @retval None
  */
void irqWriteTrack(){

  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  
  HAL_NVIC_DisableIRQ(TIM3_IRQn);
  HAL_NVIC_DisableIRQ(TIM4_IRQn);
}

/**
  * @brief Adjust Enable IRQ for idle process to avoid jitter / delay / corrupted data
  * @param None
  * @retval None
  */
void irqWIdle(){

  HAL_NVIC_DisableIRQ(TIM2_IRQn);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
  //HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void irqEnableSDIO(){

  HAL_NVIC_EnableIRQ(SDIO_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
}

void irqDisableSDIO(){

  //HAL_NVIC_DisableIRQ(SDIO_IRQn);
  //HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
  //HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
}

void GPIOWritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState){
 
  if(PinState != GPIO_PIN_RESET)
  {
    GPIOx->BSRR = GPIO_Pin;
  }else
  {
    GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
  }
}

/**
  * @brief debug function to dump content of buffer to file
  * @param filename char * file name of the file written
  * @param buffer unsigned char * of the memory buffer
  * @param length of the buffer
  * @retval STATUS provides RET_OK / RET_ERR (1,0)
  */
enum STATUS dumpBufFile(char * filename,volatile unsigned char * buffer,int length){

  FIL fil; 		  //File handle
  FRESULT fres; //Result after operations
  while(fsState!=READY){};
  fsState=BUSY;
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if (fres != FR_OK){
	  log_error("f_open error (%i)", fres);
    fsState=READY;
    return RET_ERR;
  }
 
  UINT bytesWrote;
  UINT totalBytes=0;
  uint8_t numblock=length/512;
  for (int i=0;i<numblock;i++){
    fsState=WRITING;
    fres = f_write(&fil, (unsigned char *)buffer+i*512, 512, &bytesWrote);
    if(fres == FR_OK) {
      totalBytes+=bytesWrote;
    }else{
	    log_error("f_write error (%i)\n",fres);
      fsState=READY;
      return RET_ERR;
    }

    while(fsState!=READY){};
  }

  //log_info("Wrote %i bytes to '%s'!\n", totalBytes,filename);
  f_close(&fil);
  fsState=READY;
  
  return RET_OK;
}

/**
  * @brief write track to file using FAT_FS function
  * @param filename name of the file to write to
  * @param buffer char * containing the buffer
  * @param offset lseek offset in the file
  * @retval STATUS RET_OK / RET_ERR
  */
enum STATUS writeTrkFile(char * filename,char * buffer,uint32_t offset){
  
  FIL fil; 		  //File handle
  FRESULT fres; //Result after operations
  
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if(fres != FR_OK){
    log_error("f_open error (%i)\n",fres);
    return RET_ERR;
  }

  fres=f_lseek(&fil,offset);
  if(fres != FR_OK){
    log_error("f_lseek error (%i)\n",fres);
    return RET_ERR;
  }

  UINT bytesWrote;
  UINT totalBytes=0;

  int blk=(RAW_SD_TRACK_SIZE/512);
  int lst_blk_size=RAW_SD_TRACK_SIZE%512;

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
  * @brief dump buffer to UART in Hex with 16% value / line
  * @param buf input buffer to be displayed 
  * @param memoryAddr Not used, 
  * @param len buffer len
  * @retval None
  */
void dumpBuf(unsigned char * buf,long memoryAddr,int len){

  unsigned char * data=buf;
  int bytes=len;
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
}

/**
  * @brief create a binary representation of an INT
  * @param int char
  * @retval char *
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

/**
  * @brief callback function from FatFS SDIO DMA for write process
  * @param None
  * @retval None
  */
void Custom_SD_WriteCpltCallback(void){
  
  if (fsState==WRITING || fsState==BUSY){
    fsState=READY;                                                                                       // Reset cpu cycle counter
    //t2 = DWT->CYCCNT;
    //diff1 = t2 - t1;
    //log_info(" Custom_SD_WriteCpltCallback diff %ld",diff1);
  }
}

/**
  * @brief callback function from FatFS SDIO DMA for read process
  * @param None
  * @retval None
  */
void Custom_SD_ReadCpltCallback(void){
  
  if (fsState==READING || fsState==BUSY){
    fsState=READY;                                                                                       // Reset cpu cycle counter
    //t2 = DWT->CYCCNT;
    //diff1 = t2 - t1;
    //log_info(" Custom_SD_ReadCpltCallback diff %ld",diff1);
  }
}

/**
  * @brief read block from sdcard directly using SDIO DMA request
  * @param memoryAdr sector number of the requested block
  * @param buffer destination buffer of the read blocks
  * @param count number of block to be read
  * @retval None
  */
void getDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count){
  fsState=READING;
  DWT->CYCCNT = 0; 
  t1 = DWT->CYCCNT;
  uint8_t i=0;
  HAL_SD_CardStateTypeDef state ;
  
  while(HAL_SD_ReadBlocks_DMA(&hsd, (uint8_t *)buffer, memoryAdr, count) != HAL_OK && i<2){
    state = HAL_SD_GetCardState(&hsd);
    log_error("Error HAL_SD_ReadBlocks_DMA state:%d, memoryAdr:%ld, numBlock:%d, error:%lu, retry:%d",state,memoryAdr,count,hsd.ErrorCode,i);
    i++;
  }
}

/**
  * @brief write block from sdcard directly using SDIO DMA request
  * @param memoryAdr sector number of the requested block
  * @param buffer source buffer of the read blocks
  * @param count number of block to be written
  * @retval None
  */
void setDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count){
  fsState=WRITING;
  DWT->CYCCNT = 0; 
  t1 = DWT->CYCCNT;
  uint8_t i=0;
  while (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t *)buffer, memoryAdr, count) != HAL_OK && i<2){
    log_error("Error HAL_SD_WriteBlocks_DMA error:%d retry:%d",hsd.ErrorCode,i);
    i++;
  }
}

/**
  * @brief output binary representation of char
  * @param x char to be represented
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

  if (plst->len==0){
    list_destroy(plst);
    return sorteddirChainedList; 
  }

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
  * @brief Extract the current directory from the full path
  * @param path
  * @retval RET_OK/RET_ERR
  */
enum STATUS processPath(char *path){
  if (path==NULL)
    return -1;

  int len=strlen(path);
  sprintf(currentPath,"/");

  for (int i=len-1;i!=-1;i--){
    if (path[i]=='/'){
      snprintf(currentPath,MAX_PATH_LENGTH,"%s",path+i+1);
      break;
    }
  }
  log_debug("processPath currentPath:%s\n",currentPath);
  return RET_OK;
}



enum STATUS makeNewDisk(char * location,char * imageName,enum DISK_IMAGE di){
  log_info("location: %s",location);
  log_info("imageName: %s",imageName);
  log_info("DI: %d",di);

  switch(di){
    
    case DSK140K:
      char filename[256];
      sprintf(filename,"%s/%s.dsk",location,imageName);
      irqEnableSDIO();
      createNewDiskDSK(filename,280);
      irqDisableSDIO();
      nextAction=FSDISP;
      break;

    case NIB140K:
      sprintf(filename,"%s/%s.nic",location,imageName);
      irqEnableSDIO();
      createNewDiskNic(filename);
      irqDisableSDIO();
      break;
    
    case WOZ140K:
      
      sprintf(filename,"%s/%s.woz",location,imageName);
      irqEnableSDIO();
      createNewDiskWOZ(filename,2,1,1);
      irqDisableSDIO();
    
      break;
    
    case PO140K:
      sprintf(filename,"%s/%s.po",location,imageName);
      irqEnableSDIO();
      createNewDiskDSK(filename,280);
      irqDisableSDIO();
      break;

    case PO800K:
      
      sprintf(filename,"%s/%s.po",location,imageName);
      irqEnableSDIO();
      createNewDiskDSK(filename,1600);
      irqDisableSDIO();
      break;
    
    case PO32M:
      
      sprintf(filename,"%s/%s.po",location,imageName);
      irqEnableSDIO();
      createNewDiskDSK(filename,65536);
      irqDisableSDIO();
      break;
    
    case _2MG400K:

      break;

    case _2MG800K:
      
      break;

    default:
      log_error("not managed");

  }
  log_info("pEnd");
  return RET_OK;
}


/**
  * @brief Build & sort a new chainedlist of file/dir item based on the current path
  * @param path,
  * @param extFilter: const char * of list of file extension to filter
  * @retval RET_OK/RET_ERR
  */
enum STATUS walkDir(char * path, const  char ** extFilter){
  
  DIR dir;
  FRESULT fres;
  FILINFO fno; 

  
  log_info("walkdir() path:%s",path);
  HAL_NVIC_EnableIRQ(SDIO_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

  while(fsState!=READY){};

  fres = f_opendir(&dir, path);

  log_info("directory listing:%s",path);

  if (fres != FR_OK){
    log_error("f_opendir error (%i)\n",fres);
    return RET_ERR;
  }
    
  char * fileName;
  int len;
  
  dirChainedList=list_new();

  if (fres == FR_OK){
      
    fileName=malloc(MAX_FILENAME_LENGTH*sizeof(char));
    sprintf(fileName,"D|.");
    list_rpush(dirChainedList, list_node_new(fileName));

    if (strcmp(path,"") && strcmp(path,"/")){
      fileName=malloc(MAX_FILENAME_LENGTH*sizeof(char));
      sprintf(fileName,"D|..");
      list_rpush(dirChainedList, list_node_new(fileName));
    }
    
    while(1){
      
      uint8_t fileExtMatch=0;                                                                   // flag is directory item match the file extension filter
      fres = f_readdir(&dir, &fno);

      if (fres != FR_OK){
        log_error("Error f_readdir:%d path:%s\n", fres,path);
        return RET_ERR;
      }

      if ((fres != FR_OK) || (fno.fname[0] == 0))
        break;
                                                                                                // 256+2
      len=(int)strlen(fno.fname);                                                               // Warning strlen
      uint8_t extLen=0;
      if (!(fno.fattrib & AM_DIR)){
        //log_info("file %s",fno.fname);
        for (uint8_t i=0;i<MAX_EXTFILTER_ITEM;i++){
          //log_info("w %s",extFilter[i]);
          if (extFilter[i]==NULL || !strcmp(extFilter[i],""))                                   // End of the list
            break;
            
          extLen=strlen(extFilter[i]);
          if (!memcmp(fno.fname+(len-extLen),extFilter[i],extLen)){
            //log_info("file %s ext %s match %d %d",fno.fname,extFilter[i],extLen,i);
            fileExtMatch=1;
            break;
          } 
        }
      }

      if (((fno.fattrib & AM_DIR) && 
          !(fno.fattrib & AM_HID) && len > 0 && fno.fname[0]!='.' ) ||                          // Listing Directories & File with NIC extension
          (len>3 && fileExtMatch==1  &&                      
            !(fno.fattrib & AM_SYS) &&                                                          // Not System file
            !(fno.fattrib & AM_HID)                                                             // Not Hidden file
          )
          ){
            
            fileName=malloc(MAX_FILENAME_LENGTH*sizeof(char));
        
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
          }

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
void pSdEject(){

    if ((SD_EJECT_GPIO_Port->IDR & SD_EJECT_Pin)!=0){
      initErrorScr("SD EJECTED");                                                                 // Display the message on screen
      while((SD_EJECT_GPIO_Port->IDR & SD_EJECT_Pin)!=0){};
      NVIC_SystemReset(); 
    }                                                          
  return ;
}

/**
  * @brief  Trigger by External GPIO Interrupt 
  *         pointer to function according the page are linked to relevant function;   
  * @param GPIO_Pin
  * @retval None
  */
void processBtnInterrupt(uint16_t GPIO_Pin){     

  if (flgSoundEffect==1 && (GPIO_Pin==BTN_UP_Pin || GPIO_Pin==BTN_DOWN_Pin || GPIO_Pin==BTN_ENTR_Pin || GPIO_Pin==BTN_RET_Pin)){
      TIM1->PSC=500;
      TIM1->ARR=1000;
      TIM1->CCR2=500;
      HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
      HAL_Delay(15);
      HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
  }

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
      log_debug("BTN ENT");
      break;

    case BTN_RET_Pin:
      ptrbtnRet(NULL);
      log_debug("BTN RET");
      break;

    default:
      break;
  }       
}

/**
  * @brief  Make a new FS on the SDCARD 
  * @param  void
  * @retval STATUS RET_ERR/RET_OK
*/
enum STATUS makeSDFS(){

  FRESULT fr;
  MKFS_PARM fmt_opt = {FM_FAT32 | FM_ANY, 0, 0, 0, 32768};
  BYTE work[FF_MAX_SS];
 
  fr = f_mkfs("0:", &fmt_opt,  work, sizeof work);

  if (fr==FR_OK){
    f_setlabel("SmartDisk II");
    f_mount(&fs, "", 1);
    return RET_OK;
  }else{
    log_error("makeSDFS error %d",fr);
  }

  return RET_ERR;
}

enum STATUS unlinkImageFile(char* fullpathfilename){
    
    if (fullpathfilename==NULL){
        log_error("filename is null");
    }

    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    
    f_unlink(fullpathfilename);

    return RET_OK;
}

enum STATUS execAction(enum action *nextAction){
    switch(*nextAction){
      
      case SYSRESET:
        NVIC_SystemReset();
        *nextAction=NONE;
        break;
      
      case MKFS:
        initMakeFsConfirmedScr();
        *nextAction=NONE;
        break;

      case MKIMG:
        makeNewDisk(currentFullPath,sTmp,iTmp);
        *nextAction=FSDISP;
      
      case FSDISP:
        log_info("FSDISP fsState:%d",fsState);
        list_destroy(dirChainedList);
        log_info("FSDISP: currentFullPath:%s",currentFullPath);

        walkDir(currentFullPath,ptrFileFilter);                
        setConfigParamStr("currentPath",currentFullPath);
        saveConfigFile();
        initFsScr(currentPath);
        *nextAction=NONE;
        break;
      
      case TOGGLESCREENSAVER:
        if (flgScreenSaver==1)
          HAL_TIM_OC_Start_IT(&htim9,TIM_CHANNEL_1);
        else
          HAL_TIM_OC_Stop_IT(&htim9,TIM_CHANNEL_1); 
        break;
      
      case DISPLAY_OFF:
        setDisplayONOFF(0);
        flgDisplaySleep=1;
        HAL_TIM_OC_Stop_IT(&htim9,TIM_CHANNEL_1);
        *nextAction=NONE;
        break;
        
      case DISPLAY_WAKEUP:
        log_info("disp wakeup");
        setDisplayONOFF(1);
        flgDisplaySleep=0;
        
        if (flgScreenSaver==1){
          TIM9->CNT=0;
          HAL_TIM_OC_Start_IT(&htim9,TIM_CHANNEL_1);
        }

        *nextAction=NONE;
      break;

      default:
        log_error("execAction not handled");
        *nextAction=NONE;
        return RET_ERR;
        break;
    }
  
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

  // --------------------------------------------------------------------
  // ptr declaration needs to be before NVIC init
  // --------------------------------------------------------------------

  ptrPhaseIRQ=nothingHook;
  ptrReceiveDataIRQ=nothingHook;
  ptrSendDataIRQ=nothingHook;
  ptrWrReqIRQ=nothingHook;
  ptrSelectIRQ=nothingHook;
  ptrDeviceEnableIRQ=ui16NothingHook;
  ptrMainLoop=nothingHook;
  ptrUnmountImage=statusNothingHook;
  ptrMountImagefile=statusNothingHook;
  ptrInit=nothingHook;
  
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_FATFS_Init();
  MX_TIM4_Init();
  MX_SDIO_SD_Init();
  MX_TIM2_Init();
  MX_TIM1_Init();
  MX_TIM5_Init();
  MX_TIM9_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */

  log_set_level(LOG_INFO);
  
#ifdef A2F_MODE
  // store rotary encoder state
  re_aState = HAL_GPIO_ReadPin(RE_A_GPIO_Port, RE_A_Pin);
  re_bState = HAL_GPIO_ReadPin(RE_B_GPIO_Port, RE_B_Pin);
  re_aChanged = false;
  re_bChanged = false;
#endif

  currentFullPathImageFilename[0]=0x0;
  currentFullPath[0]=0x0;
  tmpFullPathImageFilename[0]=0x0;

  //log_info("************** BOOTING ****************");                      // Data to send
  log_info("**     This is the sound of sea !    **");
  //log_info("***************************************");
    
  fres = f_mount(&fs, "", 0);                                                 // changing to 0 to be tested                                       
  
  if (fres!=FR_OK){
    log_error("not able to mount filesystem");
  }

  /*
  FIL  fp;
  FRESULT fr;
  log_info("before f_stat");
   while(fsState!=READY){};
 fr= f_open(&fp,"toto/belle/oeil.dsk",FA_READ);
 log_info("fr stat %d",fr);
 f_close(&fp);
 while(fsState!=READY){};
 disk_initialize(0);
 *//*
  FILINFO fno;

  fr = f_stat("/test/llll/lljjj/pipo.dsk", &fno);
  
  log_info("fr stat %d",fr);

  log_info("after fstat");
  fsState=READY;
  fres = f_mount(&fs, "", 0);  
  */
  csize=fs.csize;
  database=fs.database;

  initSplash(); 
                                  
  //HAL_Delay(500);
 
  EnableTiming();                                                           // Enable WatchDog to get precise CPU Cycle counting
 
  TIM1->PSC=1000;

  int T4_DIER=0x0;
  T4_DIER|=TIM_DIER_CC2IE;
  T4_DIER|=TIM_DIER_UIE;
  TIM4->DIER|=T4_DIER;   

  int dier=0x0;
  dier|=TIM_DIER_CC1IE;
  dier|=TIM_DIER_UIE;
  TIM3->DIER=dier;
  
  pSdEject();
 
  ptrDeviceEnableIRQ(DEVICE_ENABLE_Pin);

  dirChainedList = list_new();                                              // ChainedList to manage File list in current path                                                     

  char * imgFile=NULL;

  // --------------------------------------------------------------------
  // Load configuration variable
  // --------------------------------------------------------------------

  
  if (fres == FR_OK){

    if (loadConfigFile()==RET_ERR){
      log_error("loading configFile error");
      setConfigFileDefaultValues();
      if (saveConfigFile()!=RET_OK){
        log_error("Error in saving default to configFile");
      }else{
        log_info("init default and save configFile");
      }
    }else{
      
      getFavorites();
      buildLstFromFavorites();
      
      if (getConfigParamUInt8("bootMode",&bootMode)==RET_ERR)
        log_warn("error getting bootMode from Config");
      else 
        log_info("bootMode=%d",bootMode);

      if (getConfigParamUInt8("emulationType",&emulationType)==RET_ERR)
        log_warn("error getting emulationType from Config");
      else 
        log_info("emulationType=%d",emulationType);
      
      if (getConfigParamUInt8("bootImageIndex",&bootImageIndex)==RET_ERR)
        log_warn("warning getting bootImageIndex from Config");
      else 
        log_info("bootImageIndex=%d",bootImageIndex);

      
      if (getConfigParamUInt8("soundEffect",&flgSoundEffect)==RET_ERR)
        log_warn("error getting soundEffect from Config");
      else 
        log_info("soundEffect=%d",flgSoundEffect);

      if (getConfigParamUInt8("screenSaver",&flgScreenSaver)==RET_ERR)
        log_warn("error getting screenSaver from Config");
      else 
        log_info("screenSaver=%d",flgScreenSaver);
      
      if (getConfigParamUInt8("weakBit",&flgWeakBit)==RET_ERR)
        log_warn("error getting weakBit from Config");
      else 
        log_info("weakBit=%d",flgWeakBit);

    }

    imgFile=(char*)getConfigParamStr("lastFile");
    char * tmp=(char*)getConfigParamStr("currentPath");
    
    if (tmp)
      sprintf(currentFullPath,"%s",tmp);
    else
      currentFullPath[0]=0x0;
    
    if (imgFile!=NULL){
      log_info("lastFile:%s",imgFile);
      sprintf(tmpFullPathImageFilename,"%s",imgFile);
    }
    //const char * filtr[]={"woz","WOZ"};
    
    //walkDir("/",ptrFileFilter);

  
  }else{
    log_error("Error mounting sdcard %d",fres);
  }

  // --------------------------------------------------------------------
  // Manage the case for the OLED Screen Saver
  // --------------------------------------------------------------------

  if (flgScreenSaver==1){
    log_info("Starting ScreenSaver timer");
    HAL_TIM_OC_Start_IT(&htim9,TIM_CHANNEL_1);
  }


  // --------------------------------------------------------------------
  // Prepate emulation mode
  // --------------------------------------------------------------------
  //emulationType=DISKII;
  //emulationType=SMARTPORTHD;

  switch(emulationType){
    
    case SMARTLOADER:                                         // Smartloader & DISK II shares the same function it is the driver that is changing
    case DISKII:
      log_info("loading DiskII emulation");
      ptrPhaseIRQ=DiskIIPhaseIRQ;
      ptrReceiveDataIRQ=DiskIIReceiveDataIRQ;
      ptrSendDataIRQ=DiskIISendDataIRQ;
      ptrWrReqIRQ=DiskIIWrReqIRQ;
      //ptrSelectIRQ=DiskIISelectIRQ;
      ptrDeviceEnableIRQ=DiskIIDeviceEnableIRQ;
      ptrMainLoop=DiskIIMainLoop;
      ptrUnmountImage=DiskIIUnmountImage;
      ptrMountImagefile=DiskIIMountImagefile;
      ptrInit=DiskIIInit;
      //ptrFileFilter=diskIIImageExt;

      break;

    case DISK35:
      log_info("loading Disk3.5 emulation");
      ptrPhaseIRQ=disk35PhaseIRQ;
      ptrReceiveDataIRQ=disk35ReceiveDataIRQ;
      ptrSendDataIRQ=disk35SendDataIRQ;
      ptrWrReqIRQ=disk35WrReqIRQ;
      ptrDeviceEnableIRQ=disk35DeviceEnableIRQ;
      ptrMainLoop=disk35MainLoop;
      //ptrUnmountImage=nothing;
      //ptrMountImagefile=nothing;
      ptrInit=disk35Init;
      break;

    case SMARTPORTHD:
      log_info("loading SmartPortHD emulation");
      ptrPhaseIRQ=SmartPortPhaseIRQ;
      ptrReceiveDataIRQ=SmartPortReceiveDataIRQ;
      ptrSendDataIRQ=SmartPortSendDataIRQ;
      ptrWrReqIRQ=SmartPortWrReqIRQ;
      ptrDeviceEnableIRQ=SmartPortDeviceEnableIRQ;
      ptrMainLoop=SmartPortMainLoop;
      //ptrUnmountImage=NULL;
      //ptrMountImagefile=SmartPortMountImage;
      ptrInit=SmartPortInit;
      break;

  }

  // --------------------------------------------------------------------
  // Init emulation
  // --------------------------------------------------------------------

  //switchPage(FSSELECTIMAGE,0);
  //while(1);
  ptrInit();

  // --------------------------------------------------------------------
  // Execute emulation
  // --------------------------------------------------------------------

  ptrMainLoop();

  while(1){};


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  
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
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 384;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 8;
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
  /* TIM3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);
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
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  /* EXTI15_10_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 13, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  /* TIM2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  /* TIM5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM5_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(TIM5_IRQn);
  /* TIM4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM4_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
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
  hi2c1.Init.ClockSpeed = 400000;
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
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 1;
  /* USER CODE BEGIN SDIO_Init 2 */
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;

  if (HAL_SD_Init(&hsd) != HAL_OK){
    log_error("MX_SDIO_SD_Init: error HAL_SD_Init code:%d",hsd.ErrorCode);
  }

  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK){
    log_error("MX_SDIO_SD_Init: HAL_SD_ConfigWideBusOperation error code:%d",hsd.ErrorCode);
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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 500;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1000;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  //Note on Period
      // As per Daniel to be tested //32*12-1-2;     
      // Needs to be investigate -5 otherwise does not work 

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = (32*11.5)-1-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
  sSlaveConfig.InputTrigger = TIM_TS_ETRF;
  sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
  sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
  sSlaveConfig.TriggerFilter = 0;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 2*96;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
  htim3.Init.Period = 32*12-1;
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
  sConfigOC.Pulse = 140;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_1);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
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
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 96;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 1000;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OnePulse_Init(&htim5, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief TIM9 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM9_Init(void)
{

  /* USER CODE BEGIN TIM9_Init 0 */

  /* USER CODE END TIM9_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM9_Init 1 */

  /* USER CODE END TIM9_Init 1 */
  htim9.Instance = TIM9;
  htim9.Init.Prescaler = 48000;
  htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim9.Init.Period = 65535;
  htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;
  htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim9) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 65535;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OnePulse_Init(&htim4, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_OC_ConfigChannel(&htim9, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM9_Init 2 */

  /* USER CODE END TIM9_Init 2 */

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
  huart1.Init.BaudRate = UART_BAUDRATE;
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

  /* DMA interrupt init */
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

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
  HAL_GPIO_WritePin(GPIOB, RD_DATA_Pin|WR_PROTECT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BTN_DOWN_Pin BTN_UP_Pin BTN_ENTR_Pin */
  GPIO_InitStruct.Pin = BTN_DOWN_Pin|BTN_UP_Pin|BTN_ENTR_Pin;
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
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DEVICE_ENABLE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : WR_DATA_Pin _35DSK_Pin */
  GPIO_InitStruct.Pin = WR_DATA_Pin|_35DSK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : RD_DATA_Pin */
  GPIO_InitStruct.Pin = RD_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : WR_PROTECT_Pin */
  GPIO_InitStruct.Pin = WR_PROTECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_RET_Pin */
  GPIO_InitStruct.Pin = BTN_RET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BTN_RET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_EJECT_Pin */
  GPIO_InitStruct.Pin = SD_EJECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SD_EJECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DEBUG_Pin */
  GPIO_InitStruct.Pin = DEBUG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DEBUG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SELECT_Pin */
  GPIO_InitStruct.Pin = SELECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SELECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : WR_REQ_Pin */
  GPIO_InitStruct.Pin = WR_REQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(WR_REQ_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  
  #ifdef A2F_MODE
  
/*Configure GPIO pin : AB_Pin */
  GPIO_InitStruct.Pin = AB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ENCODER_A_Pin */
  GPIO_InitStruct.Pin = RE_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(RE_A_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ENCODER_B_Pin */
  GPIO_InitStruct.Pin = RE_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(RE_B_GPIO_Port, &GPIO_InitStruct);
#endif
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Callback function for External interrupt
  * @param  GPIO_Pin
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){

  //printf("startr here 0 %d\n",GPIO_Pin);
  if( GPIO_Pin == STEP0_Pin   ||               // Step 0 PA0
      GPIO_Pin == STEP1_Pin   ||               // Step 1 PA1
      GPIO_Pin == STEP2_Pin   ||               // Step 2 PA2
      GPIO_Pin == STEP3_Pin                    // Step 3 PA3
  
  ){            

    ptrPhaseIRQ();
    
  }else if (GPIO_Pin==DEVICE_ENABLE_Pin){
    ptrDeviceEnableIRQ(DEVICE_ENABLE_Pin);

  }else if ((GPIO_Pin == BTN_RET_Pin   ||      // BTN_RETURN
            GPIO_Pin == BTN_ENTR_Pin  ||       // BTN_ENTER
            GPIO_Pin == BTN_UP_Pin    ||       // BTN_UP
            GPIO_Pin == BTN_DOWN_Pin           // BTN_DOWN
            ) && buttonDebounceState==true){

              debounceBtn(GPIO_Pin);

  }else if (GPIO_Pin == WR_REQ_Pin){
    
    ptrWrReqIRQ();
    
  }else if (GPIO_Pin == SELECT_Pin){
    
    ptrSelectIRQ();
    
  }
  
  else {
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
#ifdef USE_FULL_ASSERT
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
