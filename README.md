# SmartDisk II — Apple II Floppy Emulator

**The Apple II floppy emulator that even copy protection can't tell apart.**

Swap a shelf of aging 5.25″ disks for a single SD card — without giving up an ounce of authenticity. SmartDisk II reproduces the exact timing and behaviour of a real DISK II and UNIDISK 3.5 drive, so your games load and run exactly as they did back in the day — protection schemes and all.

<p align="center">
  <img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/readmeMainPhoto.png?raw=true" width="420px" />
</p>

<p align="center">
  <a href="https://www.ebay.fr/itm/306883712221"><b>🛒 Buy a prebuilt board</b></a> ·
  <a href="https://github.com/vibr77/AppleIIDiskIIStm32F411"><b>🔧 Build it yourself</b></a> ·
  <a href="https://discord.gg/ZnzDqC2k"><b>💬 Join the Discord</b></a> ·
  <a href="https://www.r3tr0.net/index.php/smartdisk-ii-documentation/"><b>📖 Documentation</b></a>
</p>

---

## What is it

SmartDisk II is an **open-source hardware floppy-disk emulator for the Apple II**. It plugs into your machine like a real drive, reads disk images straight from an SD card, and lets you browse and mount them from a built-in OLED menu — no PC, no cables, no fuss.

Under the hood it's built on the **STM32F411 ("BlackPill")** and emulates the disk hardware *at the bit level*. That's the difference between *reading a disk image* and *being a disk drive* — and it's why SmartDisk II runs the titles other emulators choke on.

## Why SmartDisk II

- **Bit-perfect accuracy.** Timing is reproduced down to the microsecond, so the Apple II can't tell it apart from a mechanical drive.
- **It beats copy protection.** Fake bits, weak bits, half-tracks, SpiraDisc and more — it passes every scheme tested so far.
- **Your whole collection on one card.** Thousands of disks on a single SD card, browsable on the on-board OLED.
- **Works across the Apple II line.** From the II+ to the IIGS, Disk II and SmartPort — one device covers your fleet.
- **Read *and* write.** Save your progress and modify disks, not just load them.
- **Open source, or ready to run.** Build it from the published files, or order a prebuilt, flashed board and plug in today.

## Browse and boot — right from the Apple II

With **SmartLoader**, you don't even need the SmartDisk II's own buttons for everyday loading. It puts a fast green-screen browser on the Apple II itself: SmartLoader reads the disk images on your SD card and lists them right on screen, so you can scroll through your whole collection and boot any title straight from the Apple II — highlight a disk, press **[B]** to boot, **[R]** to refresh the listing, or **[S]** for settings.

## Copy protection that actually works

Most emulators fall over the moment a disk uses copy protection. SmartDisk II doesn't. By reproducing the original drive's bit-level timing, it satisfies the protection checks that real software relied on — so your original protected images run untouched.

| Copy protection             | Status     |
|:----------------------------|:-----------|
| Fat Track                   | ✅ Passed |
| Weak Bit                    | ✅ Passed |
| Cross-track synchronisation | ✅ Passed |
| Half Track                  | ✅ Passed |
| Data Latch                  | ✅ Passed |
| Timing Bits                 | ✅ Passed |
| E7                          | ✅ Passed |
| Optimal Bit Timing (<4µs)   | ✅ Passed |
| Various Bit Counter         | ✅ Passed |
| SpiraDisc                   | ✅ Passed |

## Compatibility

| Apple II model | Emulation             |
|:---------------|:----------------------|
| II+            | Disk II               |
| II Europlus    | Disk II               |
| IIe            | Disk II               |
| IIc (ROM 4)    | Disk II + SmartPort   |
| IIGS           | Disk II + SmartPort   |

SmartPort emulation is also available on the IIc (ROM 4.x), IIGS, and on the II+/IIe with a Liron or SoftSP6 card.

## Supported disk formats

| Format  | Read | Write |
|:--------|:-----|:------|
| WOZ 2.0 | ✅   | ✅    |
| DSK     | ✅   | ✅    |
| PO      | ✅   | ✅    |
| 2MG     | ✅   | ✅    |
| WOZ 1.0 | ✅   | —     |
| NIC     | ✅   | —     |

## Features at a glance

- **Three drives in one** — DISK II 5.25″, SmartPort HD, and UNIDISK 3.5 emulation
- **On-board navigation** — browse, mount and unmount images from the 0.96″ OLED with four buttons
- **Big images, no sweat** — 32 MB images and full Total Replay collections run smoothly
- **Modern software too** — loads GS/OS on the IIGS; runs titles like Arkanoid from 2MG
- **SmartLoader** built in — browse and boot from the Apple II screen itself
- **Actively developed** — frequent firmware releases with the latest improvements

## See it in action

- ▶ [Total Replay & 32 MB images, running smoothly](https://www.youtube.com/watch?v=Sws6IjnWDGQ)
- ▶ [Arkanoid, loaded from a 2MG image](https://www.youtube.com/watch?v=U3N-gej0NLk)
- ▶ [Loading GS/OS on the IIGS](https://www.youtube.com/watch?v=gZH2njO4CEQ)

## Get your SmartDisk II

**Buy a prebuilt board.** Don't want to solder? Order a ready-to-use, fully flashed and tested SmartDisk II and plug in today → [Order on eBay](https://www.ebay.fr/itm/306883712221)

**Build it yourself.** Everything you need is open source — KiCad PCB, Gerber files, bill of materials, 3D-printable case, firmware releases and full source code. **Current production board: Revision 8**, recommended for self-builds.

**Get help & follow along.** Join the community on [Discord](https://discord.gg/6y2Zdazy), or read the full development story on [AppleFritter](https://www.applefritter.com/content/apple-ii-disk-emulator-using-stm32).

## Tech specs

- **MCU:** STM32F411 ("BlackPill") — chosen for CPU speed, >60 kB SRAM and 4-bit SDIO
- **Storage:** SD card, FAT32 (4-bit SDIO for speed) — a quality, known-brand card is recommended
- **Display:** 0.96″ OLED (SSD1306) with four-button navigation
- **Board:** Production Revision 8, fully tested
- **Licence:** open source for non-commercial use with attribution

<p align="center">
  <img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_MONTING.jpeg?raw=true" width="380px" />
  <img src="https://github.com/vibr77/AppleIIDiskIIStm32F411/blob/main/resources/PCB_REV_1_IMAGE.jpeg?raw=true" width="380px" />
</p>

---

# Build & developer guide

Everything below is for people building, flashing, or hacking on the SmartDisk II. The full end-user manual (wiring, SD-card prep, menu walkthrough) lives in the [online documentation](https://www.r3tr0.net/index.php/smartdisk-ii-documentation/).

## Firmware

The latest firmware (see the releases on the right-hand side of the GitHub page) requires **bootloader v0.14 or v0.16**. Version 0.15 is **not** compatible and causes issues with the IIGS SmartPort. It is recommended to update the bootloader to the latest version.

There are two release types:

- **BIN** — classic binary, uploaded via ST-Link, starting at `0x08000000`.
- **UF2** — first upload the custom bootloader via ST-Link, then connect USB and double-click the NRST button to drag-and-drop the UF2 release onto the STM32. **Warning:** with the bootloader, flash from `0x08000000` to `0x08100000` becomes read-only (use ST-Link tooling to change the STM32 register to reverse this).

## Project structure

| Directory            | Description                                          |
|:---------------------|:-----------------------------------------------------|
| `./hardware`         | KiCad project                                        |
| `./hardware/gerber`  | Gerber release to produce the PCB                    |
| `./hardware/bom`     | Bill of materials                                    |
| `./core`             | Firmware source code                                 |
| `./bootloader`       | Bootloader                                           |
| `./Middleware`       | Libraries used                                       |
| `./FATFS`            | FATFS wrapper                                         |
| `./doc`              | Documentation used as reference                      |
| `./3DPrintCase`      | 3D-print files & Fusion 360 source for the case      |

## SD card

The SD card must use a **FAT32 file system with 64 sectors of 512 bytes each per cluster (32 KB clusters)**.

To format the card under Linux:

```bash
mkfs.fat -F 32 -s 64 /dev/sdX
```

Windows 10/11 does **not** format the card the right way. A config-menu option is provided directly on the SmartDisk II to format the SD card correctly. Note that some SD cards have very poor read/write rates — use a known brand to avoid wasting your time.

## Important notes

- The ST-Link programmer should **never** be connected at the same time as either USB or the Apple II — its voltage regulator is fragile.
- The **J1 PWR** jumper (top-right of the PCB) enables +5 V from the Apple II to the STM32.
- OLED screen: be careful of the power-pin order; some versions have the pins inverted.
- R1, R2, R3 are not needed.
- Before hardware Rev 3, the board is not compatible with the IIGS & IIc — the trace between IDC pins 5 & 7 needs to be cut.
- Hardware design may evolve; new software releases might not work with older hardware revisions.

## Hardware design principles

The STM32F4x is preferred over the STM32F1 for CPU frequency, available SRAM (>60 kB), and (on the F411) SDIO. Timing is critical on this project, especially to pass copy protection.

A DISK II floppy track is about 50,000 bits, and shifting from one track to another must respect specific timing rules. After many design iterations, the chosen approach loads a **single track in memory** and uses the **4-bit SDIO port** for SD card read/write speed — avoiding adjacent-track management and complex buffer copies.

The Apple II expects data at a precise pace of **1 bit every 4 µs (32 × 125 ns)**. Three options were considered:

1. SDIO with DMA — frees the CPU, but SPI sends 8 bits at a time; WOZ tracks aren't 8-bit aligned, which introduces bit misalignment and corrupted data on protected images.
2. Assembly with CPU-cycle counting & GPIO bit-banging — poor reliability, discarded.
3. **Timer-interrupt-triggered GPIO bit-banging (chosen)** — frees CPU time for OLED updates and button handling, makes fake-bit/protection mechanisms easy to address, and allows fine timing control (e.g. 3.8 µs vs 4 µs that some games use).

The write process mirrors the read process with a dedicated timer overflowing every 4 µs; polarity inversion is handled in software with an XOR rather than external circuitry.

### STM32 pin mapping

**Head-positioning stepper (external interrupts):**
`PA0` STEP0 / ExtI0 · `PA1` STEP1 / ExtI1 · `PA2` STEP2 / ExtI2 · `PA3` STEP3 / ExtI3

**Other GPIO:**
`PB8` SELECT (ExtI8, HIGH on Disk II controller — A2 powered on) · `PB9` WR Request (ExtI9, active LOW when writing) · `PA4` Device Enable (ExtI4, active LOW) · `PB2` WR Protect · `PA7` WR Data · `PB0` RD Data · `PA11` _DISK3.5

**Buttons (ExtI 9-15, TIM4 debounce):**
`PC13` BTN_DOWN · `PC14` BTN_UP · `PC15` BTN_ENTR · `PB12` BTN_RET

**SD card (SDIO, clock ÷2):**
`PB4` D0 · `PA8` D1 · `PA9` D2 · `PB5` D3 · `PA6` CMD · `PB15` CK

**OLED SSD1306 (I²C):** `PB6` SCL · `PB7` SDA

**UART:** `PA15` TX · `PB3` RX

**Timers:**
- **TIM2** — manages WR_DATA; ETR1 slave-reset mode resyncs with the Apple II write pulse (3.958 µs instead of 4 µs) on every rising edge.
- **TIM3** — manages RD_DATA.
- **TIM4** — internal (no PWM), debouncer for the control buttons.

## Software design principles

Built on:

- STM32 HAL drivers from STMicro
- FATFS 0.15 (note: STM32CubeMX overrides to 0.11)
- cJSON for the configuration file (tweaked to work with FATFS)
- SSD1306 library with DMA

Maximum track size is `13 × 512 × 8 = 53,248` bits (6,656 bytes), held in `DMA_BIT_TX_BUFFER[6656]`. `weakBitTank` (uint8 array) and `fakeBitTank` (char array) manage copy-protection mechanisms.

**Key functions:**

- `TIM3_IRQHandler` — bit-bangs the GPIO to send track data every 4 µs (or less, per WOZ config) to the Apple II.
- `TIM2_IRQHandler` — handles data sent from the Apple II for writing; uses a software XOR to detect polarity inversion and writes 1/0 to `DMA_BIT_RX_BUFFER`.
- `irqReadTrack` / `irqWriteTrack` — configure read/write interrupts.
- `getDataBlocksBareMetal(...)` / `setDataBlocksBareMetal(...)` — direct SDIO access to the SD card, faster than FATFS `f_open`. (FATFS is still required to build the file allocation table with correct block addresses.)
- `processDeviceEnableInterrupt` — activates/deactivates the Disk II drive on the DEVICE_ENABLE pin (active LOW).
- `processBtnInterrupt` — handles the four buttons.
- `processDiskHeadMoveInterrupt(...)` — handles the four stepper-motor GPIOs.
- `main()` — orchestrates program execution.

A **ST-Link** programmer is required to upload firmware to the STM32F411.

## Bill of materials

Interactive BOM: [SmartDisk II Bill of Material](https://vibr77.github.io/AppleIIDiskIIStm32F411/hardware/bom/ibom.html)

## Recommended reading

WOZ 2.1 & 1.0 image file references · Tome of Copy Protection · Beneath Apple DOS · Assembly Lines · Understanding the Apple II · PoC‖GTFO issues 10 & 11 · IIGS Firmware Reference · IIc Programmer's Guide to the 3.5 ROM (part 2)

## Licence

This project may be distributed, remixed, adapted and built upon, in any medium or format, **for non-commercial purposes only**, and only with attribution to the creator. Commercial rights are reserved to the author.
