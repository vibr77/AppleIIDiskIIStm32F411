
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "emul_disk35.h"
#include "utils.h"
#include "main.h"
#include "display.h"
#include "log.h"

// --------------------------------------------------------------------
// Extern Declaration
// --------------------------------------------------------------------

extern TIM_HandleTypeDef htim1;                             // Timer1 is managing buzzer pwm
extern TIM_HandleTypeDef htim2;                             // Timer2 is handling WR_DATA
extern TIM_HandleTypeDef htim3;                             // Timer3 is handling RD_DATA

extern FATFS fs;                                            // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                       // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState;   

extern uint8_t flgSoundEffect; 
extern uint8_t bootImageIndex;

disk35_t dsk35;

static volatile unsigned char phase=0x0;


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
	off off on  on    $03    *Reset disk-switched flag?  (The firmware uses this to clear disk-switched errors.)
	off on  off off   $04	  Step one track in current direction (takes about 12 msec).
	on  off off off   $08	  Turn spindle motor on.
	on  off off on    $09	  Turn spindle motor off.
	on  on  off on    $0D	  Eject the disk.  This takes about 1/2 sec to complete.  The drive may not recognize further control commands until this operation is complete.
*/
    
    phase |=(GPIOA->IDR & 0b0000000000000100) >> 2;  //Phase 000X
    phase |=(GPIOB->IDR & 0b0000000100000000) >> 7;  //Phase 00X0 // SELECT LINE IS PB8 >> 8 << 1 == >> 7
    phase |=(GPIOA->IDR & 0b0000000000000001) << 2;  //Phase 0X00
    phase |=(GPIOA->IDR & 0b0000000000000010) << 2;  //Phase X000

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

static u_int8_t dbgbuf[512];

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
        printf("%d",dbgbuf[i]);
    }
    printf("\r\n");
    for (int i=0;i<wrBitCounter;i++){
        bitw<<=1;
        bitw|=dbgbuf[i];
        printf("%d",dbgbuf[i]);
        if (bitw & 0x80){
            printf(" %02X\r\n",bitw);
            bitw=0;
        }
    }
    int j=0;
    printf("\r\n");
    for (int i=0;i<wrBitCounter;i++){
        bitw<<=1;
        bitw|=dbgbuf[i];
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
static volatile int bitCounter=0;
static volatile int bytePtr=0;
static volatile uint8_t bitPtr=0;
static volatile int bitSize=0;



void disk35StartEjecting(){
    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTING)
        return;
    dsk35.is_spindle_on = false;
    dsk35.status_mask_35 |= IWM_DISK35_STATUS_EJECTING;
    //drive->step_timer_35_dt = CLEM_IWM_DISK35_EJECT_TIME_CLOCKS;
    log_info("drive35 ejecting");
    

}

void disk35SendDataIRQ(){
    

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



void disk35Init(){


}

void  disk35MainLoop(){

    log_info("disk35MainLoop entering loop");
   // int qtr_track_index = dsk35.qtr_track_index;
    unsigned ctl_switch;
    bool sense_out = false;
    bool ctl_strobe = (phase & 0x8) != 0;

    if (ctl_strobe) {
        dsk35.status_mask_35 |= IWM_DISK35_STATUS_STROBE;
    }else {
        if ( dsk35.status_mask_35 & IWM_DISK35_STATUS_STROBE) {
             dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_STROBE;
            /* control strobe = perform command now */
            switch (ctl_switch) {
                case IWM_DISK35_CTL_STEP_IN:
                    dsk35.status_mask_35 |= IWM_DISK35_STATUS_STEP_IN;
                    log_info("dsk35: step to inward tracks");
                    break;
                case IWM_DISK35_CTL_STEP_OUT:
                    dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_STEP_IN;
                    log_info("dsk35: step to outward tracks");
                    break;
                case IWM_DISK35_CTL_EJECTED_RESET:
                    if (dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTED) {
                        log_info("dsk35: clearing eject status");
                    }
                    dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_EJECTED;
                    break;
                case IWM_DISK35_CTL_STEP_ONE:
                    if (!(dsk35.status_mask_35 & IWM_DISK35_STATUS_EJECTING)) {
                        //dsk35.step_timer_35_dt = IWM_DISK35_STEP_TIME_CLOCKS;
                        //log_info("dsk35: step from track %u", qtr_track_index);
                    } else {
                        log_info("dsk35: attempt to step while ejecting");
                    }
                    break;
                case IWM_DISK35_CTL_MOTOR_ON:
                    if (!dsk35.is_spindle_on) {
                        dsk35.is_spindle_on = true;
                        //dsk35.read_buffer = 0;
                    }
                    log_info("dsk35: drive motor on");
                    break;
                case IWM_DISK35_CTL_MOTOR_OFF:
                    dsk35.is_spindle_on = false;
                    log_info("dsk35: drive motor off");
                    break;
                case IWM_DISK35_CTL_EJECT:
                    disk35StartEjecting();
                    break;
                default:
                    log_info("dsk35: ctl %02X not supported?", ctl_switch);
                    break;
            }
            /*
            CLEM_LOG("clem_drive35: control switch %02X <= %02X",
                        ctl_switch, drive->ctl_switch);
            */
        } else {
            /* control query */
            switch (ctl_switch) {
            case IWM_DISK35_QUERY_STEP_DIR:
                sense_out = (dsk35.status_mask_35 & IWM_DISK35_STATUS_STEP_IN) == 0;
                break;
            case IWM_DISK35_QUERY_DISK_IN_DRIVE:
                sense_out = !dsk35.has_disk;
                break;
            case IWM_DISK35_QUERY_IS_STEPPING:
                //sense_out = (dsk35.step_timer_35_dt == 0);
                break;
            case IWM_DISK35_QUERY_WRITE_PROTECT:
                if (dsk35.has_disk) {
                    //sense_out = !drive->disk.is_write_protected;
                } else {
                    sense_out = false;
                }
                break;
            case IWM_DISK35_QUERY_MOTOR_ON:
                //sense_out = !drive->is_spindle_on;
                break;
            case IWM_DISK35_QUERY_TRACK_0:
                //sense_out = (drive->qtr_track_index != 0);
                break;
            case IWM_DISK35_QUERY_EJECTED:
                //  it appears the Neil Parker docs on this are confusing, or
                //  incorrect.   ejected == 1, not ejected == 0
                //sense_out = (drive->status_mask_35 & IWM_DISK35_STATUS_EJECTED) != 0;
                break;
            case IWM_DISK35_QUERY_60HZ_ROTATION:
                //assert(true);
                break;
            case IWM_DISK35_QUERY_IO_HEAD_LOWER:
                if (dsk35.status_mask_35 & IWM_DISK35_STATUS_IO_HEAD_HI) {
                    //qtr_track_index -= 1;
                    dsk35.status_mask_35 &= ~IWM_DISK35_STATUS_IO_HEAD_HI;
                    // CLEM_LOG("clem_drive35: switching to lower head, track = %u",
                    // qtr_track_index);
                }
                break;
            case IWM_DISK35_QUERY_IO_HEAD_UPPER:
                if (!(dsk35.status_mask_35 & IWM_DISK35_STATUS_IO_HEAD_HI)) {
                    //qtr_track_index += 1;
                   dsk35.status_mask_35 |= IWM_DISK35_STATUS_IO_HEAD_HI;
                    // CLEM_LOG("clem_drive35: switching to upper head, track = %u",
                    // qtr_track_index);
                }
                break;
            case IWM_DISK35_QUERY_DOUBLE_SIDED:
                //sense_out = drive->disk.is_double_sided;
                break;
            case IWM_DISK35_QUERY_READ_READY:
                //sense_out = (drive->step_timer_35_dt > 0);
                break;
            case IWM_DISK35_QUERY_ENABLED:
                /* TODO, can this drive be disabled? */
                sense_out = false;
                break;
            default:
                // CLEM_LOG("clem_drive35: query %02X not supported?", ctl_switch);
                break;
            }
            /*
            if (ctl_switch != drive->ctl_switch || ctl_strobe) {
                CLEM_LOG("clem_drive35: query switch %02X <= %02X",
                         ctl_switch, drive->ctl_switch);
            }
            */
        }
    }
    //drive->ctl_switch = ctl_switch;

    //  Apple 3.5" drives reuse the write protect as a "sense" flag
    /*if (sense_out) {
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
    } else {
        *io_flags &= ~CLEM_IWM_FLAG_WRPROTECT_SENSE;
    }*/

    //return qtr_track_index;

}
