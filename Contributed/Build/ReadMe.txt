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

