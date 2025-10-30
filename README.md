# SmartDisk II for Apple II

The SmartDisk II is an Apple II floppy disk emulator based on the STM32F411 (known as the blackPill)


<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/readmeMainPhoto.png?raw=true" width="400px" />

The following Apple II are supported:

| Apple II | Emulation  |
|:----------|:----------|
| II+       | Disk II    |
| II Europlus       | Disk II    |
| IIe 	    | Disk II    |
| IIc      |  Disk II  & Smartport (Rom 4)   |
| IIGS	    | Disk II & Smartport    |


In this repository, all information are available to enable DIY build (Kicad PCB, Gerber, 3D case, file, firmware release and source code). Please pay attention to the licence of this project.

If you do not want to build yourself a board, you can order direcly a [ready to use prebuild and flashed board on eBay](
https://www.ebay.fr/itm/306152710617)

The current production board revision is 4, 
the beta revision is 8, It has not yet been fully tested. If you decide to build it yourself, it is higly recommanded to use production revision.

A discord server is available if you need support and discuss the product [discords server](
https://discord.gg/6y2Zdazy)

The current firmware revision supports the followwing 

### Supported Emulation:
| Emulation  | Status
|:------  |:-----|
| DISK II 5.25| YES   |
| SMARTPORT HD| YES   |
| UNIDISK IIGS| YES   |
| SMARTLOADER | YES   |


### Supported disk image format:

| Format  | Read | Write |
|:------  |:-----|:------|
| NIC| YES   | NO    | 
| WOZ 1.0| YES    | NO    |
| WOZ 2.0| YES | YES|
| DSK       | YES  | YES    |
| PO    | YES    | YES     |
| 2MG    | YES    | YES     |

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

## Licence

license attached to this project enables users to distribute, remix, adapt, and build upon the material in any medium or format for noncommercial purposes only, and only so long as attribution is given to the creator. Commercial rights are reserved to the Author of this project.

## Project

This hardware emulator replicates the behaviour of a real DISK II and thus should pass the copy protection on guenine disk images.

The project is still in beta mode, progress thread is on [AppleFritter Apple II Disk emulator using STM32]( https://www.applefritter.com/content/apple-ii-disk-emulator-using-stm32).

<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_FILELST.jpeg?raw=true" width="600px" />


** If you decide to build one, please note that hardware design might evolve and new software releases might not work with the current hardware design. ** 
 
This project relies on a STM32F411(BlackPill) with SDIO Port.

## Project structre ##

| Directory | Description  |
|:----------|:----------|
| ./gerber  | gerber release to produce PCB    |
| ./hardware    | kicad project   |
| ./core   | firmware source code    |
| ./Middleware    | libraries used   |
| ./FATFS   | fatfs wrapper  |
| ./doc   | documentation used as reference  |
| ./3DPrintCase   | 3D print file & Fusion 360 source to print case  |


## IMPORTANT NOTE ##

The ST-Link programmer should never be connected at the same time with either USB or the Apple II. The ST-link has some very fragile voltage regulator. 

The J1 PWR located at top right of the PCB enable +5V from the Apple II to the STM32.

Regarding the OLED Screen: becareful of the Power pins order, some version have pin inversed

R1,R2,R3 are not needed.

Before hardware Rev 3 the board is not compatible with IIGS & IIC, trace between IDC pin 5 & 7 needs to be cut

## <!> SDCARD <!> ##

The SDCard must use FAT32 file system must use 64 Sectors of 512 Byte each per cluster.

to format the SDCard under linux use the following command: 

`mkfs.fat -F 32 -s 64 `

Please be careful, Window 10/11 is not formatting the SDCard the right way. 

An config menu option is provided directly in the SmartDisk II to format the SDCard

<!> Please note as well that some SDCard has a very poor read / write rate, I have been testing dozen of sdcard, I really suggest to use known brand to avoid wasting your time.

## Main Features ##

The list of features currently supported in this project:
- Read the content of the SDCard and display the list of images on a 0.96 OLED display based of the ss1306.
- Mount / unmount disk image
- Read mounted image file on Apple II
- Write (experimental)

<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_MONTING.jpeg?raw=true" width="400px" />

<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_IMAGE.jpeg?raw=true" width="400px" />

## Release

There are 2 releases type :
- BIN : classic binary to be uploaded via stlink and starting at 0x08000000
- UF2 : first you need to upload via stlink the custom bootloader, and then using USB and double click on NRST button, you can easily drag and drop UF2 release file to the stm32. WARNING if using the bootloader, the flash memory from 0x0800000 to 0x80100000 will become read-only (you need to use st-tool to change the stm32 register to reverse).

## Hardware design main principles: ##

- The STM32F4x is preferred compared to the STM32F1 due to CPU Freq, available SRAM >60 kB and for the STM32F411 the use of SDIO. On this project timing is really critical, especially to pass some copy protection.

A DISK II floppy track is about 50.000 Bits, and to be able to read some of the floppy shifting from one track to another should respect some very specific rules.

- After many, many, many iterations and design tests, I decided to use a single track load in memory and to have SDCARD Data Read/Write using a 4bit SDIO port for speed. This way, there is no adjacent track management and complex buffer copy with internal index along with DMA interrupt. The constraints is to use very fast loading capability of the SDIO port (SPI works but with no containgency). 


- The Apple II is expecting data at a very precise pace 1 bit every 4 uS (32*125ns). Multiple options can be considered to perform this:
	- 1/ Using SDIO
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

Other GPIO:
  - PB8 SELECT, ExtI8 HIGH on Disk II controller (used to define when the A2 is POWERED ON)
  - PB9 WR Request, ExtI9 Active LOW when writing 
  - PA4 Device Enable, ExtI4 Active LOW when enable
  - PB2 WR Protect 
  - PA7 WR Data 
  - PB0 RD Data
  - PA11 _DISK3.5

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


| Board Revision | Ref.      | Mandatory |Type.     |  Ref Supplier  |
|:---------------|:----------|:----------|:---------------|:---------|
| All            | Uc1       | YES        | STM32F411 (BlackPill)   | Aliexpress WeAct Studio 3.0 STM32F411CEU6  |
| All            | U2        | YES        | SDCard PushPush GCT MEM2055 / GCT MEM2075  | Mouser 640-MEM20750014001A / LCSC C381084  |
| All            | U8        | YES        | SSD1315 I2C 128x64 0.96 OLED   |  LCSC C5248080   |
| All            | J1        | NO         | PWR Pin Header_1x02 P2.54mm  |
| All            | J2        | YES        | Apple IDC 2x10P P2.54| LCSC C115249|
| All            | J3        | NO         | DEBUG Pin Header_1x05 P2.54mm  |
| All            | J4        | NO         | UART Pin Header_1x03 P2.54mm  |
| All            | J5        | NO         | TIM Pin Header_1x03 P2.54mm  |
| All            | J6        | NO         | STLINK|Pin Header_1x04 P2.54mm  |
| All            | DOWN1, ENTR1, RET1, UP1| YES | 6mm 5mm Round Button 50mA Direct Insert 6mm SPST 12V| LCSC C393938
| All            | BZ1       | NO         | Passive buzzer 12 x 6.0MM P6.5| LCSC C252917
| Rev.7           | R1,R2,R3  | NO        | Resistor 330R (Optional) SMD footprint 805 | LCSC C17630|
| Rev.7           | R4,R5     | NO        | Resistor 1K5 (Optional) SMD footprint 805 | LCSC C114555|
| Rev.7           | U1        | NO        | LM1117MPX-3.3/NOPB| LCSC C9662
| Rev.7			      | C1, C2		| NO		    | Ceramic / Tantalum Capacitor 10uf SMD footprint 805 | LCSC C15850



## Software design main principles: ##

The current software version rely on :
- STM32 HAL Driver from STMicro
- FATFS 0.15 (becareful STCUBEMX32 overrides to version 0.11)
- CJSON for configuration file (with some tweaks to make FATFS working)
- SSD1306 Lib with DMA

Software Architecture: 

the maximum track size is 13*512*8= 53 248 Bits or 6656 Bytes

- DMA_BIT_TX_BUFFER[6656];   	// For reading part     

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

<img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/STLINK.jpg?raw=true" width="400px" />


## Recommanded reading 

- Woz 2.1 Image file reference
- Woz 1.0 Image file reference
- Tome of copy protection
- Beneath Apple DOS
- Assembly lines
- Understanding the Apple II 
- PoC|GFTO_issue10
- PoC|GFTO_issue11
- IIGS Firmware reference
- IIc Programmer Guide to 3.5 ROM part 2

 




