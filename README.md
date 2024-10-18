# Apple II STM32 based Hardware DISKII EMULATOR

This project is about an Apple II Disk II hardware emulator capable of reading / writing disk image from/to SDCARD.

This hardware emulator tries to replicate the behaviour of a real DISK II and thus should pass the copy protection on guenine disk images.

The project is still in beta mode, progress thread is on [AppleFritter Apple II Disk emulator using STM32]( https://www.applefritter.com/content/apple-ii-disk-emulator-using-stm32).

<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_FILELST.jpeg?raw=true" width="400px" />


** If you decide to build one, please note that hardware design might evolve and new software releases might not work with the current hardware design. ** 
 
This project relies on a STM32F411(BlackPill) with SDIO Port.

## Project structre ##

| Directory | Description  |
|:----------|:----------|
|  ./gerber  | gerber release to produce PCB    |
| ./hardware    | kicad project   |
| ./core   | firmware source code    |
| ./Middleware    | libraries used   |
| ./FATFS   | fatfs wrapper  |
| ./doc   | documentation used as reference  |


## <!> SDCARD <!> ##

The SDCard must use FAT32 file system must use 64 Sectors of 512 Byte each per cluster.

to format the SDCard under linux use the following command: 

`mkfs.fat -F 32 -s 64 `

Please be careful, Window 10/11 is not formatting the SDCard the right way. 

## Main Features ##

The list of features currently supported in this project:
- Read the content of the SDCard and display the list of images on a 0.96 OLED display based of the ss1306.
- Mount / unmount disk image
- Read mounted image file on Apple II
- Write (experimental)


### Supported disk image format:

| format  | read | write |
|:------  |:-----|:------|
| NIC| YES   | NO    | 
| WOZ 1.0| YES    | NO    |
| WOZ 2.0| YES | Experimental|
| DSK       | NO  | NO    |
| PO    | NO    | NO     |

### Copy protection status

| Copy Protection | Status  | 
|:----------|:----------|
| FAT Track | PASSED   |
| Weak Bit  | PASSED    | 
| Cross track sync  | PASSED    | 
| Half track | PASSED    | 
| Data Latch    | PASSED    | 
| Timing Bits   | PASSED    |
| E7   | PASSED    | 
| Optimal Bit Timing <4uS   | PASSED    |
| Various Bit Counter    | PASSED    | 
| SpiraDisc    | PASSED    | 


## Status of the project ##

The first PCB and software are available for testing purpose only. 


## What is coming next ## 

- Creation of a PCB with the STM32 chip directly,
- Addition of protection to 20 pin connector inversion,
- Support of dsk,po image format for reading,
- Support of nic,woz 1.x -> 2.1 for writing,
- configuration screen,
- adding USB & UF2 bootlooder firmware update support,
- Remove STM HAL (High Level) driver and move to low level,
- Finalize a stable software.

## Hardware design main principles: ##

- The STM32F4x is preferred compared to the STM32F1 due to CPU Freq, available SRAM >60 kB and for the STM32F411 the use of SDIO. On this project timing is really critical, especially to pass some copy protection.

A DISK II floppy track is about 50.000 Bits, and to be able to read some of the floppy shifting from one track to another should respect some very specific rules.

- After many, many, many iterations and design tests, I decided to use a single track load in memory and to have SDCARD Data Read/Write using a 4bit SDIO port for speed. This way, there is no adjacent track management and complex buffer copy with internal index along with DMA interrupt. The constraints is to use very fast loading capability of the SDIO port (SPI works but with no containgency). 


- The Apple II is expecting data at a very precise pace 1 bit every 4 uS (32*125ns). Multiple options can be considered to perform this:
	- 1/ Using SPI with DMA
	- 2/ Assembly code with CPU cycle calculation & GPIO Bitbanging
	- 3/ Timer interrupt trigger with GPIO Bitbanging (preferred option)


The first option was my initial choice, very simple straight forward, and the CPU using DMA was completly free to do something else while sending data. One of the aspect of the SPI is to send 8 bits (1 Bytes) at a time. When using WOZ 1.0 and WOZ 2.0 the number of bits per track are not aligned with 8 and thus using SPI may (every time on a 36 track disk) introduce bit misalignement and corrupted data. There is no way to have WOZ copy protected image file to work using 8 Bits aligned data stream.

The second option was also tested and gives very poor reliability and was very quickly put aside.

The last one, using TIMER interrupt seems by far to be the best option and enable to free up CPU time to manage the OLED display updates and to other button interrupt. 
Each interrupt, the Read GPIO is bitbang according to the track stream position. The real advantage is to be able to address very easily some specific protection mecanism using fake bit tank. Using timer is also very easy to increase or reduce the space between 2 bit because some games use 3.8 uS instead of 4 uS. The trick is to manage other interrupt priority not to disturb the READ/WRITE Timer process.


- The approach for the writing process is pretty much the same as for the reading process, using a dedicated timer with an overflow every 4us. As writing uses polarity inversion, an internal software XOR is done instead of using external circuitery. Using the SDIO makes it also very easy timing wise to write to SDCard.  

Using this approach with Timers, there is no need to external circuitery and almost everything can be manage from a software standpoint (in the future I might add a 74LS125 to protect the STM32).

Main design on STM32:

Head positionning stepper, managed by external interrupts: 
  - PA0 STEP0, ExtI0
  - PA1 STEP1, ExtI1
  - PA2 STEP2, ExtI2
  - PA3 STEP3, ExtI3


Button: External Interrupt 9-15 with Timer4 as a debouncer
  - PC13 BTN_DOWN
  - PC14 BTN_UP
  - PC15 BTN_ENTR
  - PB12 BTN_RET

SDCARD: SDIO Port with a clock divider by 2 (to keep most of the SDCard working)
  - PB4 DO
  - PA8 D1
  - PA9 D2
  - PB5 D3
  - PA6 CMD
  - PB15 CK

I2C Oled Screen SSD1306
  - PB06 SCL
  - PB07 SDA

UART:
  - PA15 TX
  - PB3 RX

TIMERS:
- TIM2 Timer 2 : Use to Manage the WR_DATA, ETR1 Slave Reset mode to resync with the Apple II  Write Pulse that is 3.958 uS instead of 4uS. Every Rising Edge resync
- TIM3 Timer 3 : Use to Manage the RD_DATA, 
- TIM4 Timer 4 : Internal no PWM, debouncer for the control button


## BOM ## 


| Ref.      | Type.     |  Ref Supplier  |
|:----------|:----------|:----------|
| Uc1        | STM32F411 (BlackPill)   | Aliexpress WeAct Studio 3.0 STM32F411CEU6  |
| U2        | SDCard PushPush GCT MEM2055 / GCT MEM2075  | Mouser 640-MEM20750014001A / LCSC C381084  |
| U8        | SSD1315 I2C 128x64 0.96 OLED   |  LCSC C5248080   |
| R1,R2,R3  | Resistor 1K (Optional)  1/4W L6.3mm_D2.5mm_P10.16mm  | Mouser 708-CFF14JT1K00 |
|J1|PWR|Pin Header_1x02 P2.54mm  |
|J2|Apple IDC 2x10P P2.54| LCSC C115249|
|J3|DBG|Pin Header_1x05 P2.54mm  |
|J4|UART|Pin Header_1x03 P2.54mm  |
|J5|TIM|Pin Header_1x03 P2.54mm  |
|J6|STLINK|Pin Header_1x04 P2.54mm  |
|DOWN1,ENTR1,RET1,UP1|6mm 5mm Round Button 50mA Direct Insert 6mm SPST 12V| LCSC C393938


## Software design main principles: ##

The current software version rely on :
- STM32 HAL Driver from STMicro
- FATFS 0.15 (becareful STCUBEMX32 override to version 0.11)
- CJOSN for configuration file (with some tweaks to make FATFS working)
- SSD1306 Lib with DMA

Software Architecture: 

the maximum track size is 13*512*8= 53 248 Bits or 6656 Bytes

2 Unsigned char Buffers are used :
- DMA_BIT_TX_BUFFER[6656];   	// For reading part     
- DMA_BIT_RX_BUFFER[6656];		// For writing part

weakBitTank uint_8 array & fakeBitTank char array are used to manage some copy protection mecanism

### Important Functions ### 

- void TIM3_IRQHandler: Manage the Bitbang of the GPIO port to send track data every 4us (or less depending on the WOZ file configuration) to the Apple II

- void void TIM2_IRQHandler: Manage the data sent from the Apple II for the writing process. It use a software XOR to detect polarity inversion and write 1 or 0 to the DMA_BIT_RX_BUFFER.

- void irqReadTrack: Configure interrupt for Reading 
- void irqWriteTrack: Configure interrupt for Writing

- void getDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count): Instead of using FATFS function such as F_OPEN, using direct access to the SDCard using the SDIO port to receive data is faster.

- void setDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count): same for the writing process, faster than using FATFS

<!> keep in mind that even if FATFS is not used to read and write to the SDCARD data from/to the Apple II, FATFS is needed to build up the File Allocation Table with the right file address block.

- void processDeviceEnableInterrupt: Interrrupt function used when Apple II change signal on DEVICE_ENABLE PIN to activate or deactivate the DISKII Drive. This signal is active LOW

- void processBtnInterrupt: Manage the 4 BTN press

- void processDiskHeadMoveInterrupt(uint16_t GPIO_Pin): Interrupt function link to the 4 GPIO Stepper motor of the DISKII.


- Main () is orchestring the program execution. 

Please note that you will need to have a STLINK32 to upload the firmware to the STM32F411


## Recommanded reading 

- Woz 2.1 Image file reference
- Woz 1.0 Image file reference
- Tome of copy protection
- Beneath Apple DOS
- Assembly lines
- Understanding the Apple II 
- PoC|GFTO_issue10
- PoC|GFTO_issue11

 




