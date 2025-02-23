### VIBR SmartDisk II - Apple II forever modification ###

https://youtube.com/shorts/TgZ5UqMbQsM


https://github.com/joergschne/AppleIIDiskIIStm32F411

https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

export PATH="/Applications/ArmGNUToolchain/13.3.rel1/arm-none-eabi/bin:$PATH"

Download mandatory STM32F4xx driver at: https://www.st.com/en/development-tools/stm32cubemx.html

# Driver/BSP
# Driver/CMSIS
# Driver/STM32F4xx_HAL_Driver


Build with Apple II forever mode: make -f Makefile USE_BOOTLOADER=1 A2F_MODE=1 

# python uf2conv/uf2conv.py -b 0x08010000 -f STM32F4 -o build/AppleIISDiskII_stm32f411_sdio.uf2 build/AppleIISDiskII_stm32f411_sdio.bin


BOM:
https://shop.lcd-module.de/128x64-OLED-YELLOW/OLEDM128-6LGA
or
https://shop.lcd-module.de/OLED128x64green-COG/W128064-XALE

1	311-1344-1-ND			CC0603KRX7R9BB104	CAP CER 0.1UF 50V X7R 0603		1
2	478-1655-1-ND			TAJA106K016RNJ		CAP TANT 10UF 10% 16V 1206		3
3	732-2096-ND			61202021621		CONN HEADER VERT 20POS 2.54MM		2
4	296-SN74LS257BDRCT-ND		SN74LS257BDR		IC MULTIPLEXER 4 X 2:1 16SOIC		1
5	4809-RKJXT1F42001-ND		RKJXT1F42001		SWITCH JOYSTICK ANALOG 10MA 5V		1
6	664-GMC020080HRCT-ND		GMC020080HR		CONN SD/MMC/MS CARD R/A SMD		1
7	102-1266-1-ND	CT-1205H-SMT-TR	BUZZER MAGNETIC 5V 12.8X12.8 SMD				1
8	311-10KLGCT-ND	AC0805FR-0710KL	RES SMD 10K OHM 1% 1/8W 0805					12
9	13-AC0805FR-07910KLCT-ND	AC0805FR-07910KL	RES SMD 910K OHM 1% 1/8W 0805		1
10	P12937SCT-ND			EVQ-Q2F03W		SWITCH TACTILE SPST-NO 0.02A 15V	4
11	MICRO SD-CARD SMD		GCT-MEM2055-00-190-01-A_VIB					1
12	LED 5 mm
13	Buchsenleisten 2,54 mm
14	Flachbandkabel und 20P Pfosten-Steckverbinder
