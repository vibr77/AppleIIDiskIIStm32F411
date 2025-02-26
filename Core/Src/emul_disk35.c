
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "emul_disk35.h"

#include "main.h"
#include "display.h"
#include "log.h"

// --------------------------------------------------------------------
// Extern Declaration
// --------------------------------------------------------------------

extern TIM_HandleTypeDef htim1;                                         // Timer1 is managing buzzer pwm
extern TIM_HandleTypeDef htim2;                                         // Timer2 is handling WR_DATA
extern TIM_HandleTypeDef htim3;                                         // Timer3 is handling RD_DATA

extern FATFS fs;                                                        // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                                   // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState;   

extern uint8_t flgSoundEffect; 
extern uint8_t bootImageIndex;
extern enum action nextAction;

disk35_t dsk35;
image35Info_t img35;

static volatile unsigned char phase=0x0;
static volatile uint8_t ph_track=0;                                            // Physical Track on 3.5 should not exceed 160;
static volatile uint8_t prevPh_track=160;
static unsigned long cAlive=0;

static volatile unsigned char flgDeviceEnable=0;
#pragma GCC diagnostic push                                              // TODO: To be removed when emul is finished
#pragma GCC diagnostic ignored "-Wunused-variable"
static enum STATUS (*disk35GetTrackBitStream)(int,unsigned char*);       // pointer to readBitStream function according to driver woz/nic
static enum STATUS (*disk35SetTrackBitStream)(int,unsigned char*);       // pointer to writeBitStream function according to driver woz/nic
static long (*disk35GetSDAddr)(int ,int ,int , long);                    // pointer to getSDAddr function
static int  (*disk35GetTrackFromPh)(int);                                // pointer to track calculation function
#pragma GCC diagnostic pop
static unsigned int  (*disk35GetTrackSize)(int);    

extern unsigned char read_track_data_bloc[RAW_SD_TRACK_SIZE];            // 
extern volatile unsigned char DMA_BIT_TX_BUFFER[RAW_SD_TRACK_SIZE];      // DMA Buffer for READ Track

/*
 * Emulation of the 3.5" IIgs drive (non Smartport)*
 *
 * Commands are dispatched using the in_phase and io_flags input/output "pins"
 * These commands fall into two categories "query" and "control".
 *
 * The IWM specifies the command via the PHASE0, PHASE1, PHASE2 and HEAD_SEL
 * inputs
 *
 * The PHASE3 input signal indicates a query vs control command.
 *  If LO, then a query is peformed, and its status is returned via the
 *      WRPROTECT_SENSE output.
 *  If HI, then a command is executed.
 */


uint8_t ctlSwitch;
uint8_t senseOut;

/**
  * @brief SmartPortReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void disk35PhaseIRQ(){
    
    // CA0 -> PA0
    // CA1 -> PA1
    // CA2 -> PA2
    // CA3 -> PA3
    // SEL -> PB8

/*
    CA1 CA0 SEL CA2  CONT35   Function
	--- --- --- ---  -------  --------
	off off off off   $00	  Set step direction inward (toward higher-numbered tracks.)
	off off off on    $01	  Set step direction outward (toward lower-numbered tracks.
	off off on  on    $03     *Reset disk-switched flag?  (The firmware uses this to clear disk-switched errors.)
	off on  off off   $04	  Step one track in current direction (takes about 12 msec).
	on  off off off   $08	  Turn spindle motor on.
	on  off off on    $09	  Turn spindle motor off.
	on  on  off on    $0D	  Eject the disk.  This takes about 1/2 sec to complete.  The drive may not recognize further control commands until this operation is complete.
*/
    
    phase |=(GPIOA->IDR & 0b0000000000000100) >> 2;  //Phase 0000X
    phase |=(GPIOB->IDR & 0b0000000100000000) >> 7;  //Phase 000X0 // SELECT LINE IS PB8 >> 8 << 1 == >> 7
    phase |=(GPIOA->IDR & 0b0000000000000001) << 2;  //Phase 00X00
    phase |=(GPIOA->IDR & 0b0000000000000010) << 2;  //Phase 0X000
    phase |=(GPIOA->IDR & 0b0000000000001000) << 1;  //Phase X0000 LSTRB Line Strobe


    if (phase & 0x10){                                                                          // Strobe Line is High set the Flag and                                                                                 
            dsk35.status_mask_35 |= IWM_DISK35_STATUS_STROBE;
    }else {
        
        ctlSwitch=phase & 0x0F /* 0b00001111 */;                                               // Remove the Strobe bit (5th LSB)
        
        if ( dsk35.status_mask_35 & IWM_DISK35_STATUS_STROBE) {                                 // Strober is back LOW => Action are triggered
                dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_STROBE;                              // Remove the Flag                                                                                      
        
            switch (ctlSwitch) {                                                                // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_STEP_IN:                                                    //  0   0   0   0  $00     Set step direction inward (toward higher-numbered tracks.)                                                  
                    dsk35.status_mask_35 |= IWM_DISK35_STATUS_STEP_IN;
                    log_info("dsk35: step to inward tracks");
                    break;                                                                      // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_STEP_OUT:                                                   //  0   0   0   1  $01     Set step direction outward (toward lower-numbered tracks.                                                  
                    dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_STEP_IN;
                    log_info("dsk35: step to outward tracks");
                    break;
                case IWM_DISK35_CTL_EJECTED_RESET:                                              // CA1 CA0 SEL CA2 CONT35 Function
                    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTED) {                     //  0   0   1   1   $03     *Reset disk-switched flag? (The firmwareuses this to clear disk-switched errors.) 
                        log_info("dsk35: clearing eject status");
                    }
                    dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_EJECTED;
                    break;                                                                      // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_STEP_ONE:                                                   //  0   1   0   0   $04     *Reset disk-switched flag? (The firmwareuses this to clear disk-switched errors.)                                                 
                    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTING) {
                        dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_EJECTING;
                        dsk35.status_mask_35 |= IWM_DISK35_STATUS_EJECTED;
                        log_info("dsk35: ejected disk");
                    } else {
                        if (dsk35.status_mask_35 & IWM_DISK35_STATUS_STEP_IN) {
                            if (ph_track < 158) {
                                ph_track += 2;
                                log_info("dsk35: stepped in track = %u", ph_track);
                            }
                        } else {
                            if (ph_track >= 2) {
                                ph_track -= 2;
                                log_info("dsk35: stepped out track = %u", ph_track);
                            }
                        }
                    }
                    break;                                                                      // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_MOTOR_ON:                                                   //  1   0   0   0   $08     Turn spindle motor on.
                    if (!dsk35.isSpindleOn) {
                        dsk35.isSpindleOn = 0;
                        HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);                             // Starting the READ timer
                    }
                    log_info("dsk35: drive motor on");
                    break;                                                                      // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_MOTOR_OFF:                                                  //  1   0   0   1   $09     Turn spindle motor off.                                             
                    dsk35.isSpindleOn = 0;
                    HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);                                  // Stopping the READ Timer 
                    log_info("dsk35: drive motor off");                                         
                    break;                                                                      // CA1 CA0 SEL CA2 CONT35 Function
                case IWM_DISK35_CTL_EJECT:                                                      //  1   1   0   1   $0D     Eject the disk. This takes about 1/2 sec to complete. The drive may not recognize further control commands until this operation is complete.
                    disk35StartEjecting();
                    break;
                default:
                    log_info("dsk35: ctl %02X not supported?", ctlSwitch);
                    break;
            }
        }else{                                                                                  // IWM QUERY TO FLOPPY WR_PROTECT LINE TO PROVIDE RESULT
            switch (ctlSwitch) {                                                                // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_STEP_DIR:                                                 //  0   0   0   0   $00     Step direction: 0 = head set to step inward (toward higher-numbered tracks) / 1 = head set to step outward (toward lower-numbered tracks)
                    senseOut = (dsk35.status_mask_35 & IWM_DISK35_STATUS_STEP_IN) == 0;
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_DISK_IN_DRIVE:                                            //  0   0   0   1   $02     Disk in place: 0 = disk in drive / 1 = drive is empty.
                    senseOut = !dsk35.hasDisk;
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_IS_STEPPING:                                              //  0   0   1   0   $04     Disk is stepping: 0 = head is stepping between tracks / 1 = head is not stepping.
                    senseOut=dsk35.isReady;
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_WRITE_PROTECT:                                            //  0   0   1   1   $06     Disk locked: 0 = disk is write protected / 1 = disk is write-enabled.
                    if (dsk35.hasDisk) {
                        senseOut = !img35.isWriteprotect;
                    } else {
                        senseOut = 0;
                    }
                    break;
                case IWM_DISK35_QUERY_MOTOR_ON:                                                 // CA2 CA1 CA0 SEL STAT35 Function
                    senseOut = !dsk35.isSpindleOn;                                              //  0   1   0   0   $08     Motor on: 0 = spindle motor is spinning / 1 = motor is off
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_TRACK_0:                                                  //  0   1   0   1   $0A     Track 0: 0 = head is at track 0 / 1 = head is at some other track                                                
                    senseOut = (ph_track != 0);
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_EJECTED:                                                  //  0   1   1   0   $0C     *Disk switched?:0 = user ejected disk by pressing the eject button / 1 = disk not ejected.                                                
                    //  it appears the Neil Parker docs on this are confusing, or
                    //  incorrect.   ejected == 1, not ejected == 0
                    senseOut = (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTED) != 0;
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function                                      
                case IWM_DISK35_QUERY_60HZ_ROTATION:                                            //  0   1   1   1   $0E     Tachometer. 60 pulses per disk revolution
                    log_info("dsk35:IWM_DISK35_QUERY_60HZ_ROTATION");
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_IO_HEAD_LOWER:                                            //  1   0   0   0   $01     Instantaneous data from lower head. Reading this bit configures the drive to do I/O with the lower head.
                    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_IO_HEAD_HI) {
                        ph_track--;
                        dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_IO_HEAD_HI;
                        log_info("dsk35:switching to lower head, track = %u",ph_track);
                    }
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function
                case IWM_DISK35_QUERY_IO_HEAD_UPPER:                                            //  1   0   0   1   $03     Instantaneous data from upper head. Reading this bit configures the drive to do I/O with the upper head.
                    if (!(dsk35.status_mask_35 & IWM_DISK35_STATUS_IO_HEAD_HI)) {
                        ph_track++;
                        dsk35.status_mask_35 |= IWM_DISK35_STATUS_IO_HEAD_HI;
                        log_info("dsk35: switching to upper head, track = %u",ph_track);
                    }
                    break;
                case IWM_DISK35_QUERY_DOUBLE_SIDED:                                             // CA2 CA1 CA0 SEL STAT35 Function 
                    senseOut=img35.isDoubleSided;                                               //  1   1   0   0   $09    Number of sides: 0 = single-sided drive / 1 = double-sided drive
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function 
                case IWM_DISK35_QUERY_READ_READY:                                               //  1   1   0   1   $0B    *Disk ready for reading: 0 = ready / 1 = not ready I am not too sure about this bit. The firmware waits for this bit to go low before trying to read a sector address field.
                    senseOut=dsk35.isReady;
                    break;                                                                      // CA2 CA1 CA0 SEL STAT35 Function 
                case IWM_DISK35_QUERY_ENABLED:                                                  //  1   1   0   1   $0B    Drive installed. 0 = drive is connected / 1 = no drive is connected
                    senseOut = 0;
                    break;
                default:
                        log_info("dsk35: query %02X not supported?", ctlSwitch);
                    break;
            }
        }
        dsk35.ctlSwitch = ctlSwitch;

        //  Apple 3.5" drives reuse the write protect as a "sense" flag

        if (senseOut) {
            HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);                                                 // TODO MOVE AWAY FROM HAL FUNCTION WHICH CREATE OVERHEAD
        } else {
            HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port,WR_PROTECT_Pin,GPIO_PIN_SET);
        }
    }

    return;
}


static volatile uint8_t wrData=0;
static volatile uint8_t prevWrData=0;
static volatile uint8_t xorWrData=0;
static volatile int wrStartOffset=0;

static volatile unsigned int wrBitCounter=0;
static volatile unsigned int wrBytes=0;
static volatile unsigned int wrBytesReceived=0;

static volatile unsigned char byteWindow=0x0;
static volatile uint16_t wrCycleWithNoBytes=0;
static volatile uint8_t flgPacket=2;

static volatile int flgdebug=0;

static volatile unsigned int WR_REQ_PHASE=0;

unsigned max_sectors_per_region_35[DISK_35_NUM_REGIONS] = {12, 11, 10, 9, 8};
unsigned track_start_per_region_35[DISK_35_NUM_REGIONS + 1] = {0, 32, 64, 96, 128, 160};

void disk35WrReqIRQ(){
    
}


void disk35printbits(){
    unsigned char bitw=0;
    printf("wrBitcounter=%d\r\n",wrBitCounter);
    printf("wrStartOffset=%d\r\n",wrStartOffset);
    for (int i=0;i<wrBitCounter;i++){
        if (i%64==0)
            printf("\n");

        if (i%8==0)
            printf(" ");
        //printf("%d",dbgbuf[i]);
    }
    printf("\r\n");
    for (int i=0;i<wrBitCounter;i++){
        bitw<<=1;
        //bitw|=dbgbuf[i];
        //printf("%d",dbgbuf[i]);
        if (bitw & 0x80){
            printf(" %02X\r\n",bitw);
            bitw=0;
        }
    }
    int j=0;
    printf("\r\n");
    for (int i=0;i<wrBitCounter;i++){
        bitw<<=1;
       // bitw|=dbgbuf[i];
        //printf("%d",dbgbuf[i]);
        if (bitw & 0x80){
            j++;
            if (j%16==0)
                printf("\r\n");
            printf(" %02X",bitw);
            bitw=0;
        }
    }
    printf("\r\n");
}
/**
  * @brief disk35ReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void disk35ReceiveDataIRQ(){
        
        
}

static uint8_t nextBit=0;
static volatile uint8_t *bbPtr=0x0;

static volatile int bitCounter=0;
static volatile int bytePtr=0;
static volatile uint8_t bitPtr=0;
static volatile int bitSize=0;

static volatile int ByteSize=0;

void disk35StartEjecting(){
    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTING)
        return;
    dsk35.isSpindleOn = false;
    dsk35.status_mask_35 |= IWM_DISK35_STATUS_EJECTING;
    //drive->step_timer_35_dt = CLEM_IWM_DISK35_EJECT_TIME_CLOCKS;
    log_info("drive35 ejecting");
    
}

void disk35SendDataIRQ(){
    // If we go full speed 2uS by BitCell instead of 4uS it means that the following code need to be nibble (ultra nibble < 2*96 CPU Cycle)
    RD_DATA_GPIO_Port->BSRR=nextBit;                          // start by outputing the nextBit and then due the internal cooking for the next one

    nextBit=(*(bbPtr+bytePtr)>>(7-bitCounter) ) & 1;          // Assuming it is on GPIO PORT B and Pin 0 (0x1 Set and 0x01 << Reset)

    bitCounter++;

    if (bitPtr==8){
        bitPtr=0;
        bytePtr++;
    }

    if (bitCounter>=bitSize){
        bitCounter=0;
        bytePtr=0;
        bitPtr=0;
    }

}

void disk35SetRddataPort(uint8_t direction){
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = RD_DATA_Pin;
    
    if (direction==0){
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    }else{
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    }
    HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);
}

int disk35DeviceEnableIRQ(uint16_t GPIO_Pin){
    // The DEVICE_ENABLE signal from the Disk controller is activeLow
    
    uint8_t  a=0;
    if ((GPIOA->IDR & GPIO_Pin)==0)
        a=0;
    else
        a=1;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (a==0 && dsk35.isSpindleOn==1){ 
        flgDeviceEnable=1;

        GPIO_InitStruct.Pin   = RD_DATA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
        HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);

    }else if (flgDeviceEnable==1 && a==1 ){

        flgDeviceEnable=0;

        GPIO_InitStruct.Pin   = RD_DATA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(RD_DATA_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);
        
    }

    log_info("flgDeviceEnable==%d",flgDeviceEnable);
    return flgDeviceEnable;
}


void disk35Init(){


}

void  disk35MainLoop(){

    log_info("disk35MainLoop entering loop");

    while(1){
            
        if (flgDeviceEnable==1 && ph_track!=prevPh_track && dsk35.isSpindleOn){              
                                                                                        // Track has changed, but avoid new change during the process
            DWT->CYCCNT = 0;                                                            // Reset cpu cycle counter
            //t1 = DWT->CYCCNT;                                       
            cAlive=0;
            
            // --------------------------------------------------------------------
            // PART 1 MAIN TRACK & RESTORE AS QUICKLY AS POSSIBLE THE DMA
            // --------------------------------------------------------------------
            
            if (flgSoundEffect==1){
                TIM1->PSC=1000;
                HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
            }

            irqEnableSDIO();
            disk35GetTrackBitStream(ph_track,read_track_data_bloc);
            while(fsState!=READY){}
            irqDisableSDIO();

            memcpy((unsigned char *)&DMA_BIT_TX_BUFFER,read_track_data_bloc,RAW_SD_TRACK_SIZE);
            
            bitSize=disk35GetTrackSize(ph_track);
            ByteSize=bitSize/8; 
            
            prevPh_track=ph_track;
        
        }else{

            if (nextAction!=NONE){
                execAction(nextAction);
            }
            
            cAlive++;
            if (flgSoundEffect==1){
                HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
            }
            
            if (cAlive==5000000){
                printf(".\n");
                cAlive=0;
                
            }
        }  
    }
}
