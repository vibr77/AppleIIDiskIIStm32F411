# Apple II STM32 DISK EMULATOR

This project is about creating an Apple II Disk emulator capable of reading / writing disk image from/to SDCARD.

This hardware emulator tries to replicate the behaviour of a real DISK II and thus should pass the copy protection on guenine disk images. 

The project is still in beta mode, progress thread is on [AppleFritter Apple II Disk emulator using STM32]( https://www.applefritter.com/content/apple-ii-disk-emulator-using-stm32).
 
This project relies on a STM32F411 (BlackPill) with SDIO Port



 

## Features ##

The list of features currently supported in this project:
- Read the content of the SDCard and display the list of images on a 0.96 OLED display based of the ss1306.
- Mount / unmount disk image
- Read mounted image file on Apple II
- Write (experimental)
- supported disk image format:

	| format  | read  | write  |
	|:----------|:----------|:----------|
	| NIC   	 | YES   | NO    |      |
	| WOZ 1.0   | YES    | NO    |
	| WOZ 2.0   | YES    | Experimental    |
	| DSK    | NO    | NO    |
	| PO    | NO    | NO    |



## Status of the project ##

This is currently a DIY project made on plug board. 


## What is coming next ## 

- Creation of a PCB with the blackpill
- Creation of a PCB with the STM32 chip directly
- Addition of protection to 20 pin connector inversion
- Support of dsk,po image format for reading
- Support of nic,woz 1.x -> 2.1 for writing
- configuration screen
- adding USB & UF2 bootlooder firmware update support
- Remove STM HAL (High Level) driver and move to low level 

## Design main principles: 

- The STM32F4x is preferred due to the size of the SRAM > 60 kB. a DISK II floppy track is about 50.000 Bits, and to be able to read some of the floppy protection mecanism 3 adjacents track need to be loaded in memory at the same time (a woz track maximum size is 13 blocs of 512 Bytes = 6656 Bytes). On top of this 2 DMA Buffer are needed : 1 for reading, 1 for writing.

- After many iterations and testing, it appears that the ARM Cortex Mx are not able to fully respect CPU cycle on bitbanging & baremetal GPIO output, thus the approach is sending read buffer via ASM code was not giving great result. Instead the approach of sending read buffer via circular DMA gives pretty good results.

- It is the same for the writing process, the preferred approach was to use SPI with DMA.
- Thus 3 SPI are needed:
	- SPI 2 For reading / writing to the SDCard
	- SPI 1 for sending read buffer to the Apple II Disk Controller
	- SPI 3 for receiving write buffer from the Apple II Disk Controller

- Some floppy disk protections use the track density to increase the number of bits on a track, decreasing the classic period of 4Us per bit. To address this, the read SPI is set a slave to be able to control the clock based on PWM Timer. Using this timer, the SPI frequency can be adjusted moving with a 0.125 uS step. Since woz 2.0, in the info chunk Byte at offset 59 provide with the Optimal Bit timing to manage disk bit density. The SPI clock is manage by TIM3 on the STM32

- The Read pulse width, The Disk II is generating a pulse of 1 us within a period of 4 us, the SPI is not able to generate by itself such a signal, the 74LS123 U1A will do the job of generating a signal of 1.125 us on the failing edge of the SPI MISO (MISO because of the Slave mode of the SPI). The B Pin (schmitt trigger is used), PIN A has to be ground.

- The write process is much more complex and need more circuitery.

- Head move: the head move is managed via STEP0, STEP1, STEP2, STEP3 GPIO_PIN and Rising/Falling Edge interrupt.

- Drive Enable signal from the DISK Controller is active Low, Thus from a software standpoint we need to have an active High signal, thus the LS14 is used.

- The screen is based on a very classic 0.96 Oled display connected via I2C.

- FatFs 0.15 is used to read / write from/to the SDCard, due to overhead on using f_read, HAL SPI function

## BOM ## 


| Ref.      | Type.     | Mouser    |
|:----------|:----------|:----------|
| U1        | 74LS123   | Cell 3    |
| U2        | LM1117-3.3    | Cell 3    |
| U3        | TTL 74LS125   | Cell 3    |
| U4        | TTL 74LS14    | Cell 3    |
| U5        | 74LS74    | Cell 3    |
| U6        | 74LS393   | Cell 3    |
| U7        | 74LS86    | Cell 3    |
| C1        | 220pF    	 | Cell 3    |
| C2        | 47pF    	 | Cell 3    |
| C3,C4     | 10uF    	 | Cell 3    |
| C5,C6,C7,C8,C9     | 100nF    	 | Cell 3    |
| R1,R2,R3,R4,R5,R6  | 10K    	 | Cell 3    |
| R7,R8,R9  | 1K    	 | Cell 3    |
| J1		| Micro SDCard    	 | Cell 3    |
| J2		| 2x10 Conn    	 | Cell 3    |










Reference reading