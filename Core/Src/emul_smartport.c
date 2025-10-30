
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "emul_smartport.h"

#include "main.h"
#include "display.h"
#include "log.h"

#include "driver_2mg.h"

/*
    Lessons Learned: 
    
    IIe SPCard, IIc ROM4x are not managing DEVICE_ENABLE Signal on Smartport,
    Timing on IIc is very sensible and different from IIe & IIGS (need adjustement)
    IIc No packet retry on error or checksum
    IIc no drive remounting -> not working

*/




// --------------------------------------------------------------------
// Extern Declaration
// --------------------------------------------------------------------

extern TIM_HandleTypeDef htim1;                             // Timer1 is managing buzzer pwm
extern TIM_HandleTypeDef htim2;                             // Timer2 is handling WR_DATA
extern TIM_HandleTypeDef htim3;                             // Timer3 is handling RD_DATA

extern SD_HandleTypeDef hsd;
extern FATFS fs;                                            // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                       // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState;   

extern uint8_t flgSoundEffect; 
extern uint8_t bootImageIndex;
extern enum action nextAction;

extern const  char** ptrFileFilter;

extern volatile uint8_t flgBreakLoop;

prodosPartition_t devices[MAX_PARTITIONS];

char* smartPortHookFilename=NULL;
//bool is_valid_image(File imageFile);

volatile unsigned char packet_buffer[SP_PKT_SIZE];                                          // smartport packet buffer for Request / Response
int packetLen=0;
//unsigned char status/*,packet_byte*/;

int count;
int partition;
int initPartition;

static volatile unsigned char phase=0x0;
uint16_t messageId=0x0;
static unsigned long cAlive=0;
// ------------------------------------------------------
// STATIC FUNCTION DEFINITION
// ------------------------------------------------------

static void encodeDataPacket (unsigned char source);
static void encodeExtendedDataPacket (unsigned char source);
static void decodeControlExecutePacket(prodosPartition_t * d);
static int  decodeDataPacket (void);

static void encodeReplyPacket(unsigned char source,unsigned char type,unsigned char aux, unsigned char respCode);

static void encodeStatusReplyPacket (prodosPartition_t d);
static void encodeExtendedStatusReplyPacket (prodosPartition_t d);
static void encodeUnidiskStatReplyPacket(prodosPartition_t d);
static void encodeStatusDibReplyPacket (prodosPartition_t d);
static void encodeExtendedStatusDibReplyPacket (prodosPartition_t d);

static enum STATUS verifyCmdpktChecksum(void);
static void print_packet (unsigned char* data, int bytes);
//static int packet_length (void);

static enum STATUS SmartportReceivePacket();
static void pNextAction();
/**
  * @brief SmartPortReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void SmartPortPhaseIRQ(){
    phase=(GPIOA->IDR&0b0000000000001111);
    return;
}

static volatile uint8_t wrData=0;
static volatile uint8_t prevWrData=0;
static volatile uint8_t xorWrData=0;
static volatile int wrStartOffset=0;

static volatile unsigned int wrBitCounter=0;
static volatile unsigned int wrBytes=0;

static volatile unsigned char byteWindow=0x0;
static volatile uint16_t wrCycleWithNoBytes=0;
static volatile uint8_t flgPacket=2;

static volatile int flgdebug=0;

static volatile unsigned int WR_REQ_PHASE=0;
static volatile unsigned char flgDeviceEnable=0;

static uint8_t dbgbuf[512];

/*
static void sendDebugPin(uint8_t pulse){
    for (uint8_t i=0;i<pulse;i++){
        for (int j=0;j<48;j++){
            HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_SET);
        }
        for (int j=0;j<48;j++){
            HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_RESET);
        }
    }
}
*/

int SmartPortDeviceEnableIRQ(uint16_t GPIO_Pin){                                                        // DEVICE ENABLE is not used on Smartport
    __NOP();
    return 0;
}

void SmartPortWrReqIRQ(){

    if ((WR_REQ_GPIO_Port->IDR & WR_REQ_Pin)==0)
        WR_REQ_PHASE=0;
    else
        WR_REQ_PHASE=1;

    if (WR_REQ_PHASE==0){

        flgPacket=0;
        wrData=0;
        prevWrData=0;
        wrBytes=0;
        wrBitCounter=0;
        wrStartOffset=0;
        HAL_TIM_PWM_Start_IT(&htim2,TIM_CHANNEL_3);
    }else{
        
        HAL_TIM_PWM_Stop_IT(&htim2,TIM_CHANNEL_3);
        packet_buffer[wrBytes++]=0x0;
        messageId++;
        flgPacket=1;
    }
        
}
void printbits(){
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
;
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


static void pNextAction(){
    pSdEject();
    if (nextAction!=NONE){
        switch(nextAction){
            case SMARTPORT_IMGMOUNT:
                SmartPortInit();
                nextAction=NONE;
                break;
            default:
                execAction(&nextAction);
        }
    }
}

/**
  * @brief SmartPortReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void SmartPortReceiveDataIRQ(){
    
    for (int i=0;i<10;i++);                                                  // <!> Important: adding timing for IIc
    
    if ((GPIOA->IDR & WR_DATA_Pin)==0)                                       // get WR_DATA DO NOT USE THE HAL function creating an overhead
        wrData=0;
    else
        wrData=1;

    wrData^= 0x01u;                                                           // get /WR_DATA
    xorWrData=wrData ^ prevWrData;                                            // Compute Magnetic polarity inversion
    prevWrData=wrData; 

    byteWindow<<=1;
    byteWindow|=xorWrData;
    
    if (byteWindow & 0x80){                                                 // Check if ByteWindow Bit 7 is 1 meaning we have a full bytes 0b1xxxxxxx 0x80
        
        packet_buffer[wrBytes]=byteWindow;
        if (byteWindow==0xC3 && wrBitCounter>10 && wrStartOffset==0)       // Identify when the message start
            wrStartOffset=wrBitCounter;
        
        byteWindow=0x0;

        if (wrStartOffset!=0)                                               // Start writing to packet_buffer only if offset is not 0 (after sync byte)
            wrBytes++;

    }
    wrBitCounter++;
    
}

static uint8_t nextBit=0;
static volatile int bitCounter=0;
static volatile int bytePtr=0;
static volatile uint8_t bitPtr=0;
static volatile int bitSize=0;
static volatile int byteSize=0;

void SmartPortSendDataIRQ(){
    

    if (nextBit==1)                                                                 // This has to be at the beginning otherwise timing of pulse will be reduced
        RD_DATA_GPIO_Port->BSRR=RD_DATA_Pin;
    else
        RD_DATA_GPIO_Port->BSRR=RD_DATA_Pin << 16U;

    if (bitCounter>bitSize){
        nextBit=0;
        bitCounter=0;
        bitPtr=0;
        flgPacket=1;

        HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);   
    }
    
    nextBit=(packet_buffer[bytePtr]>>(7-bitPtr) ) & 1;
    
    bitPtr++;
    if (bitPtr>7){
        bitPtr=0;
        bytePtr++;
    }

    bitCounter++;

}

void setRddataPort(uint8_t direction){
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

void setWPProtectPort(uint8_t direction){
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = WR_PROTECT_Pin;
    
    if (direction==0){
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
    }else{
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    }
    HAL_GPIO_Init(WR_PROTECT_GPIO_Port, &GPIO_InitStruct);
}


char * SmartPortFindImage(char * pattern){
    DIR dir;
    FRESULT fres;  
    char path[1];
    path[0]=0x0;

    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    while(fsState!=READY){};

    fres = f_opendir(&dir, path);

    if (fres != FR_OK){
        log_error("f_opendir error (%i)\n",fres);
        return NULL;
    }
    
    char * fileName=NULL;
    int len;

    if (fres == FR_OK){
        
        while(1){
            FILINFO fno;

            fres = f_readdir(&dir, &fno);


            if (fres != FR_OK){
                log_error("Error f_readdir:%d path:%s\n", fres,path);
                return NULL;
            }

            if ((fres != FR_OK) || (fno.fname[0] == 0))
                break;

            len=(int)strlen(fno.fname);                                      
            

            if (!(fno.fattrib & AM_DIR) && 
                !(fno.fattrib & AM_HID) &&      
                len>3                   &&
                
                (!memcmp(fno.fname+(len-3),".PO",3)   ||          // .PO
                !memcmp(fno.fname+(len-3),".po",3)    ||  
                !memcmp(fno.fname+(len-4),".2mg",4)   ||
                !memcmp(fno.fname+(len-4),".2MG",4)  ) &&         
                !(fno.fattrib & AM_SYS) &&                        // Not System file
                !(fno.fattrib & AM_HID) &&                        // Not Hidden file
    
                strstr(fno.fname,pattern)
                ){
                
            fileName=malloc(MAX_FILENAME_LENGTH*sizeof(char));
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-truncation="
            snprintf(fileName,63,"%s",fno.fname);
            #pragma GCC diagnostic pop

            log_info("found %s",fileName);
            f_closedir(&dir);
            return fileName;
                
            } 
        }
    }
    log_warn("image %s not found",pattern);
    f_closedir(&dir);
    return NULL;
}
const  char * smartportImageExt[]={"PO","po","2MG","2mg","HDV","hdv",NULL};   
void SmartPortInit(){


    HAL_TIM_PWM_Stop_IT(&htim2,TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);

    TIM3->ARR=(32*12)-1;
    TIM2->ARR=(32*12)-5;
    
    ptrFileFilter=smartportImageExt;

    char * szFile;
    char key[16];
    for(uint8_t i=0; i< MAX_PARTITIONS; i++){
        sprintf(key,"smartport_vol%02d",i);
        
        szFile=(char *)getConfigParamStr(key);

        if (szFile==NULL || !strcmp(szFile,"")){
            devices[i].filename=NULL;
            devices[i].mounted=0;

            log_warn("[config] smartport_vol%02d does not exist",i);
        }else{
            devices[i].filename=szFile;
            SmartPortMountImage(&devices[i],szFile);
        
            if (devices[i].mounted!=1){
                log_error("Mount error: %s not mounted",szFile);
                sprintf(key,"smartport_vol%02d",i);
                setConfigParamStr(key,"");
                saveConfigFile();
                devices[i].filename=NULL;
            }else{
                log_info("%s mounted",szFile);
            }
        }
    }

    switchPage(SMARTPORT,NULL);                                                                 // Display the Frame of the screen
    char * fileTab[4];

    for (uint8_t i=0;i<MAX_PARTITIONS;i++){
        devices[i].dispIndex=i;
        fileTab[i]=devices[i].filename;                                                         // Display the name of the PO according to the position
    }
    setImageTabSmartPortHD(fileTab,bootImageIndex);
    SmartPortDeviceEnableIRQ(DEVICE_ENABLE_Pin);

}


void SmartPortSendPacket(volatile unsigned char* buffer){
    
                                                                                                // Reset the flag before sending
    bitCounter=0;
    bitPtr=0;
    bytePtr=0;
    bitSize=packetLen*8;

    setRddataPort(1);
    setWPProtectPort(1);                                                                        // Set ACK Port to output
    assertAck();                                                                                // Set ACK high to signal we are ready to send
    
    while (!(phase & 0x1)){
       pNextAction();
    }; 
                                                                                           // Wait Req to be HIGH, HOST is ready to receive
    HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
    flgPacket=0;                                                                                // This line need to be here <!>
    while (flgPacket!=1){
    }; 

    setRddataPort(0);
    HAL_GPIO_WritePin(RD_DATA_GPIO_Port, RD_DATA_Pin,GPIO_PIN_RESET);    
    
    deAssertAck();                                                                              // set ACK(BSY) low to signal we have sent the pkt

    while (phase & 0x01){
    
    }

    return;
}

static enum STATUS SmartportReceivePacket(){
    
    setRddataPort(1);

    setWPProtectPort(1); 
    assertAck(); 

    GPIOWritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_RESET);                                    // ACK HIGH, indicates ready to receive
    while(!(phase & 0x01)){
        pNextAction();
    };
                                                                                                // WAIT FOR REQ TO GO HIGH
    flgPacket=0;                                                                                // <!> This position is important
    while (flgPacket!=1){

    }                                                                                           // Receive finish
    packetLen=wrBytes;                                                                         // to avoid recomputation
    deAssertAck();                                                                              // ACK LOW indicates to the host we have received a packer
    
    while(phase & 0x01){
    }


    return RET_OK;                                                                              // Wait for REQ to go low
}

void assertAck(){
    WR_PROTECT_GPIO_Port->BSRR=WR_PROTECT_Pin;               
}
void deAssertAck(){
    WR_PROTECT_GPIO_Port->BSRR=WR_PROTECT_Pin << 16U;             
}

void SmartPortMainLoop(){


    /*
    Function    PH3     PH2     PH1     PH0     Binary
    Bus ENABLE   1       X       1       X        0b1010 0b1011 0b1110 Ob1111 
    Bus Reset    0       1       0       1

    Binary       8        4        2        1
    
    Bus enable can take:
    0b1010  2+8=10      0x0A
    0b1011  1+2+8=11    0x0B
    0b1110  2+4+8=13    0x0E
    0b1111  1+2+4+8=15  0x0F
    
    */

    log_info("SmartPortMainLoop entering loop");
    unsigned long blockNumber;
    uint8_t statusCode=0;
    uint8_t ctrlCode=0;

    uint8_t BN_3B_LOW,BN_3B_MID,BN_3B_HIGH;             // Block Number on 3 Bytes Low Mid High
    
    /*
    uint8_t D_LW_BUFFER_PTR_LOW;                        // Extended Call Data Pointer on 4 Bytes
    uint8_t D_LW_BUFFER_PTR_HIGH;
    uint8_t D_HW_BUFFER_PTR_LOW;
    uint8_t D_HW_BUFFER_PTR_HIGH;
    */

    uint8_t BN_LW_L;                                   // Extended Call Block Number on 4 Bytes 2x WORD
    uint8_t BN_LW_H;
    uint8_t BN_HW_L;
    uint8_t BN_HW_H;
    
    uint8_t MSB,MSB2,AUX;

    int number_partitions_initialised = 1;
    uint8_t noid = 0;

    unsigned char dest;

    HAL_GPIO_WritePin(RD_DATA_GPIO_Port, RD_DATA_Pin,GPIO_PIN_RESET);  // set RD_DATA LOW

    if (bootImageIndex==0)
        bootImageIndex=1;


    while (1) {

        initPartition=bootImageIndex-1;                                                     // Update are enable

        switch (phase) {
                                                                                            // phase lines for smartport bus reset
            case 0x00:
            case 0x01:
            case 0x04:

                setWPProtectPort(0);                                                        // Set ack (wrprot) to input to avoid clashing with other devices when sp bus is not enabled 
                break;                                                                      // ph3=0 ph2=1 ph1=0 ph0=1

            case 0x05:
                                                                                            // Monitor phase lines for reset to clear
                while (phase == 0x05);                                                      // Wait for phases to change 
                number_partitions_initialised = 1;                                          // Reset number of partitions init'd
            
                noid = 0;                                                                   // To check if needed
                for (partition = 0; partition < MAX_PARTITIONS; partition++)                // Clear device_id table
                    devices[partition].device_id = 0;
                
                log_info("Ph:0x05 Reset message");
                break;
                                                                                            // Phase lines for smartport bus enable
                                                                                            // Ph3=1 ph2=x ph1=1 ph0=x
            case 0x0a:
            case 0x0b:
            case 0x0e:
            case 0x0f:

                if (SmartportReceivePacket()==RET_ERR){                                     // Receive Packet
                    statusCode=0x06;                                                        // Generic BUS_ERR 0x06 
                    log_error("SmartportReceivePacket timeout error");
                    encodeReplyPacket(0x0,0x1 | 0x01 ,0x01,statusCode);
                    SmartPortSendPacket(packet_buffer);
                    break;
                }
                                                                                                                                                                  
                if (verifyCmdpktChecksum()==RET_ERR){                                       // Verify Packet checksum
                    statusCode=0x06;                                                        // Generic BUS_ERR 0x06 
                    log_error("Incomming command checksum error");
                    encodeReplyPacket(0x0,0x1 | 0x01 ,0x01,statusCode);
                    SmartPortSendPacket(packet_buffer);
                    break;
                }
                

                //---------------------------------------------
                // STEP 1 CHECK IF INIT PACKET 
                //---------------------------------------------
                
                //print_packet ((unsigned char*) packet_buffer, packetLen);
                
                                                                                            // lets check if the pkt is for us
                if (packet_buffer[SP_COMMAND] != 0x85){                                     // if its an init pkt, then assume its for us and continue on
                    noid = 0;
                    for  (partition = 0; partition < MAX_PARTITIONS; partition++){          // else check if its our one of our id's
                        uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                        if ( devices[dev].device_id != packet_buffer[SP_DEST]){
                            noid++;
                        } else{
                            break;
                        }
                    }

                    if (noid == MAX_PARTITIONS){  //not one of our id's
                        
                        log_info("Not our ID!");
                        
                        setWPProtectPort(0);                                                // set ack to input, so lets not interfere
                        
                        while (phase & 0x08);
                        
                                                                                            // Assume its a cmd packet, cmd code is in byte 14
                                                                                            // Now we need to work out what type of packet and stay out of the way
                        switch (packet_buffer[SP_COMMAND]) {
                            case 0x80:                                                      // is a status cmd
                            case 0x83:                                                      // is a format cmd
                            case 0x81:                                                      // is a readblock cmd
                                while (!(phase & 0x08));                                    // Wait till high
                                printf("A ");

                                while (phase & 0x08);                                       // wait till low
                                printf("a ");

                                while (!(phase & 0x08));                                     // wait till high
                                printf("A\r\n");
                                
                                break;

                            case 0x82:                                                      // is a writeblock cmd
                                while (!(phase & 0x08));                                   // wait till high
                                printf("W ");
                                
                                while (phase & 0x08);                                      // wait till low
                                printf("w ");
                                
                                while (!(phase & 0x08));                                   // wait till high
                                printf("W\r\n");
                                
                                while (phase & 0x08);                                      // wait till low
                                printf("w ");
                                
                                while (!(phase & 0x08));                                   // wait till high
                                printf("W\r\n");
                                break;
                        }
                        break;  //not one of ours
                    }
                }
                                                                                            // Not safe to assume it's a normal command packet, GSOS may throw
                                                                                            // Us several extended packets here and then crash 
                                                                                            // Refuse an extended packet
                dest =  packet_buffer[SP_DEST];
                MSB  =  packet_buffer[SP_GRP7MSB];
                AUX  =  packet_buffer[SP_AUX] & 0x7f;                                                                          
                
                switch (packet_buffer[SP_COMMAND]) {

                    case 0x80:                                                                                          //is a status cmd
                    
                        /*
                            The Status cal returns status information about a particular device or about the
                            SmartPort itself. Only Status cals that return general information are listed here.
                            Device-specific Status calls can also be implemented by a device for diagnostic or
                            other information.

                                        Standard call                       Extended call
                            CMDNUM      $00                                 $40
                            CMDLIST     Parameter count                     Parameter count
                                        Unit number                         Unit number
                                        Status list pointer (low byte)      Status list pointer (low byte, low word)        SP_G7BYTE1
                                        Status list pointer (high byte)     Status list pointer (high byte, low word)       SP_G7BYTE2
                                        Status code                         Status list pointer (low byte, high word)       SP_G7BYTE3
                                                                            Status list pointer (high byte, high word)      SP_G7BYTE4
                                                                            Status code                                     SP_G7BYTE5
                            
                            Status Code returned
                            ___________________________________________________________________________________
                            $00 Return device status
                            $01 Return device control block
                            $02 Return newline status (character devices only)
                            $03 Return device information block (DIB)
                            
                            A Status call with a unit number of $00 and a status code of $00 is a request to return the
                            status o f the SmartPort driver. This function returns the number of devices as well as
                            the current interrupt status. The format of the status list returned is as follows:
                            
                            STATLIST Byte 0. Number of devices
                            Byte 1 Reserved
                            Byte 2 Reserved
                            Byte 3 Reserved
                            Byte 4 Reserved
                            Byte 5 Reserved
                            Byte 6 Reserved
                            Byte 7 Reserved
                            
                            The number of devices field is a 1-byte field indicating the total number of devices
                            connected to the slot or port. This number wil always be in the range 0 to 127.
                            
                            Possible errors
                            ________________________________________________________________________________
                            $06 BUSERR Communications error
                            $21 BADCTL Invalid status code
                            $30-S3F / $50-$7F Device-specific error
                        
                        */
                        
                        statusCode= (packet_buffer[SP_G7BYTE3] & 0x7f) | (((unsigned short)MSB << 3) & 0x80);  ;

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                      // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest /*&& devices[dev].mounted==1 */) {                           // yes it is, and it's online, then reply
                                
                                updateCommandSmartPortHD(devices[dev].dispIndex,2);
                                                                                                                            // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                if (statusCode == 0x03) {                                                                   // if statcode=3, then status with device info block
                                    encodeStatusDibReplyPacket(devices[dev]);
                                } else if (statusCode == 0x05){                                                           
                                    
                                    //log_error("Unidisk UniDiskStat  not implemented");
                                    /* we should provide an answer like this 
                                        Msg: 0227
                                        
                                        HOST (IIGS Request) Packet size:022, src:80, dst:81, type:80, aux:80, cmd:80, paramcnt:83
                                        0000: C3 81 80 80 80 80 82 81 80 80 83 E0 C0 AB 85 84 - ..����..��......
                                        0010: 83 80 80 EA FE C8
                                        
                                        DEVICE (Unidisk Response) Packet size:021, src:81, dst:80, type:81, aux:80, cmd:80, paramcnt:81
                                        0000: C3 80 81 81 80 80 81 81 80 80 81 80 80 80 80 80 - .�..��..��.�����
                                        0010: 88 FF FF BB C8                                  - ................
                                    */

                                    //log_error("Unidisk UniDiskStat  not implemented");
                                    encodeUnidiskStatReplyPacket(devices[dev]);
                                    //print_packet((unsigned char*) packet_buffer,packetLen);

                                }else{
                                    encodeStatusReplyPacket(devices[dev]);                                              // else just return device status
                                }

                                SmartPortSendPacket(packet_buffer);  
                            }
                        }
                        break;

                    case 0xC0:                                                                                          // Extended status cmd
                        
                        /*
                                    Standard call                       Extended call
                        CMDNUM      $00                                 $40
                        CMDLIST     Parameter count                     Parameter count
                                    Unit number                         Unit number
                                    Status list pointer (low byte)      Status list pointer (low byte, low word)        SP_G7BYTE1
                                    Status list pointer (high byte)     Status list pointer (high byte, low word)       SP_G7BYTE2
                                    Status code                         Status list pointer (low byte, high word)       SP_G7BYTE3
                                                                        Status list pointer (high byte, high word)      SP_G7BYTE4
                                                                        Status code                                     SP_G7BYTE5
                        */

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                  // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest) {                                                       // yes it is, then reply
                                                                                                                        // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                updateCommandSmartPortHD(devices[dev].dispIndex,2);
                                statusCode = (packet_buffer[SP_G7BYTE5] & 0x7f);
                                
                                if (statusCode == 0x03) {                                                               // if statcode=3, then status with device info block
                                    encodeExtendedStatusDibReplyPacket(devices[dev]);
                                } else if (statusCode == 0x05){                                                         // Unidisk specific function
                                    log_error("Unidisk UniDiskStat  not implemented");
                                    encodeUnidiskStatReplyPacket(devices[dev]);
                                }else{
                                    encodeExtendedStatusReplyPacket(devices[dev]);                                      // else just return device status
                                }
                                
                                SmartPortSendPacket(packet_buffer);
                            }
                        }

                        break;

                    case 0x81:                                                                                  // is a readblock cmd
                        
                        /*

                        This call reads one 512-byte block from the block device specified by the unit number
                        passed in the parameter list. The block is read into memory starting at the address
                        specified by the data buffer pointer passed in the parameter list.

                        
                                            Standard call                                   Extended call
                        __________________________________________________________________________________________________________________________________
                        CMDNUM              $01                                             $41                                                 
                        CMDLIST             Parameter count                                 Parameter count
                                            Unit number                                     Unit number
                                            Data buffer pointer (low byte)                  Data buffer pointer (low byte, low word)            SP_G7BYTE1
                                            Data buffer pointer (high byte)                 Data buffer pointer (high byte, low word)           SP_G7BYTE2
                                            Block number (low byte)                         Data buffer pointer (low byte, high word)           SP_G7BYTE3
                                            Block number (middle byte)                      Data buffer pointer (high byte, high word)          SP_G7BYTE4
                                            Block number (high byte)                        Block number (low byte, low word)                   SP_G7BYTE5
                                                                                            Block number (high byte, low word)                  SP_G7BYTE6
                                                                                            Block number (low byte, high word)                  SP_G7BYTE7
                                                                                            Block number (high byte, high word)                 SP_G7BYTE7+2
                        The following error return values are possible.
                        _______________________________________________
                        $06 BUSERR Communications error
                        $27 IOERROR I/O error
                        $28 NODRIVE No device connected
                        $2D BADBLOCK Invalid block number
                        $2F OFFLINE Device off line or no disk in drive
                        
                        */

                        if (flgSoundEffect==1){
                            TIM1->PSC=1000;
                            HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
                            play_buzzer_ms(15);
                        }

                        BN_3B_LOW = packet_buffer[SP_G7BYTE3];                                                              // block number low
                        BN_3B_MID = packet_buffer[SP_G7BYTE4];                                                              // block number middle
                        BN_3B_HIGH = packet_buffer[SP_G7BYTE5];                                                             // block number high
                        
                        blockNumber = (BN_3B_LOW & 0x7f) | (((unsigned short)MSB << 3) & 0x80);                             // Added (unsigned short) cast to ensure calculated block is not underflowing.
                        blockNumber = blockNumber + (((BN_3B_MID & 0x7f) | (((unsigned short)MSB << 4) & 0x80)) << 8);      // block num second byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                        blockNumber = blockNumber + (((BN_3B_HIGH & 0x7f) | (((unsigned short)MSB << 5) & 0x80)) << 16);    // block num third byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                    
                        statusCode=0x28;
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                      // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;

                            if (devices[dev].device_id == dest) {                                                           // yes it is, then do the read
                                
                                if (devices[dev].mounted==0){
                                    //statusCode=0x27;                                                                        // IO ERROR
                                    statusCode=0x2F;                                                                        // No Disk in Drive 
                                }else{

                                    updateCommandSmartPortHD(devices[dev].dispIndex,0);                                     // Pass the rightImageIndex    
                                                                                                                            // block num 1st byte
                                    while(fsState!=READY){};
                                    fsState=BUSY;
                                    FRESULT fres=f_lseek(&devices[dev].fil,devices[dev].dataOffset+blockNumber*512);
                                    if (fres!=FR_OK){
                                        log_error("Read seek err!, partition:%d, block:%d res:%d",dev,blockNumber,fres);
                                        print_packet ((unsigned char*) packet_buffer,packetLen);
                                        statusCode=0x2D;                                                                    // Invalid Block Number
                                    }else{
                                        statusCode=0x0;
                                        fsState=BUSY;
                                        unsigned int pt;
                                        fres=f_read(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt);              // Reading block from SD Card
                                        
                                        if(fres != FR_OK){
                                            log_error("Read err!");
                                            statusCode=0x27;
                                        }
                                        while(fsState!=READY){};
                                    }
                                    if (statusCode==0x0){
                                        encodeDataPacket(dest);
                                        SmartPortSendPacket(packet_buffer);
                                    }else{
                                        log_error("read Error");
                                    }
                                }   
                            }
                        }
                        if (statusCode!=0x0){
                            encodeReplyPacket(dest,0x1 | AUX ,AUX,statusCode);
                            SmartPortSendPacket(packet_buffer);
                        }
                        break;

                    case 0xC1:                                                                                      // Extended Call Read
                        
                        /*
                        This call reads one 512-byte block from the block device specified by the unit number
                        passed in the parameter list. The block is read into memory starting at the address
                        specified by the data buffer pointer passed in the parameter list.

                        
                                            Standard call                                   Extended call
                        CMDNUM              $01                                             $41                                                 
                        CMDLIST             Parameter count                                 Parameter count
                                            Unit number                                     Unit number
                                            Data buffer pointer (low byte)                  Data buffer pointer (low byte, low word)            SP_G7BYTE1
                                            Data buffer pointer (high byte)                 Data buffer pointer (high byte, low word)           SP_G7BYTE2
                                            Block number (low byte)                         Data buffer pointer (low byte, high word)           SP_G7BYTE3
                                            Block number (middle byte)                      Data buffer pointer (high byte, high word)          SP_G7BYTE4
                                            Block number (high byte)                        Block number (low byte, low word)                   SP_G7BYTE5
                                                                                            Block number (high byte, low word)                  SP_G7BYTE6
                                                                                            Block number (low byte, high word)                  SP_G7BYTE7
                                                                                            Block number (high byte, high word)                 SP_G7BYTE7+2
                        */

                        if (flgSoundEffect==1){
                            TIM1->PSC=1000;
                            HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
                            play_buzzer_ms(15);
                        }
 
                        MSB2    =  packet_buffer[SP_GRP7MSB+8];

                        BN_LW_L =   packet_buffer[SP_G7BYTE5];
                        BN_LW_H =   packet_buffer[SP_G7BYTE6];
                        BN_HW_L =   packet_buffer[SP_G7BYTE7];
                        BN_HW_H =   packet_buffer[SP_G7BYTE7+2];
                        
                        blockNumber = (BN_LW_L & 0x7f) | (((unsigned short)MSB << 5) & 0x80);                             // This should be the BlockNumber                        
                        blockNumber = blockNumber + (((BN_LW_H & 0x7f) | (((unsigned short)MSB << 6) & 0x80)) << 8);    
                        blockNumber = blockNumber + (((BN_HW_L & 0x7f) | (((unsigned short)MSB << 7) & 0x80)) << 16);
                        blockNumber = blockNumber + (((BN_HW_H & 0x7f) | (((unsigned short)MSB2 << 7) & 0x80)) << 24);
        
                        statusCode=0x28;
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                  // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;

                            if (devices[dev].device_id == dest) {                                                       // yes it is, then do the read
                                
                                if (devices[dev].mounted==0){
                                    //statusCode=0x27;                                                                  // IO Error 
                                    statusCode=0x2F;                                                                    // No Disk in Drive
                                }else{

                                    updateCommandSmartPortHD(devices[dev].dispIndex,0);                               // Pass the rightImageIndex    
                                    
                                    while(fsState!=READY){};
                                    fsState=BUSY;
                                    FRESULT fres=f_lseek(&devices[dev].fil,devices[dev].dataOffset+blockNumber*512);
                                    if (fres!=FR_OK){
                                        log_error("Read seek err!, partition:%d, block:%d",dev,blockNumber);
                                        print_packet ((unsigned char*) packet_buffer,packetLen);
                                        statusCode=0x2D;                // Invalid Block Number
                                    }else{
                                        statusCode=0x0;
                                        fsState=BUSY;
                                        unsigned int pt;
                                        fres=f_read(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt); // Reading block from SD Card
                                        
                                        if(fres != FR_OK){
                                            log_error("Read err!");
                                            statusCode=0x27;
                                        }
                                        while(fsState!=READY){};
                                    }
                                    if (statusCode==0x0){
                                        encodeDataPacket(dest);
                                        SmartPortSendPacket(packet_buffer);
                                    }else{
                                        log_error("read Error");
                                    }
                                }   
                            }
                        }

                        if (statusCode!=0x0){
                            log_error("Smartport READBLOCK cmd:%02X,  dest:%002X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                            encodeReplyPacket(dest,0x1,AUX,statusCode);
                            SmartPortSendPacket(packet_buffer);
                        }

                        break;

                    case 0x82:                                                                                           // is a writeblock cmd
                        
                        /*
                        The Write call writes one 512-byte block to the block device specified by the unit 
                        number passed in the parameter list. The block is written from memory starting at the 
                        address specified by the data buffer pointer passed in the parameter list.
                        
                                            Standard call                                   Extended call
                        __________________________________________________________________________________________________________________________________
                        CMDNUM              $02                                             $42                                                 
                        CMDLIST             Parameter count                                 Parameter count
                                            Unit number                                     Unit number
                                            Data buffer pointer (low byte)                  Data buffer pointer (low byte, low word)            SP_G7BYTE1
                                            Data buffer pointer (high byte)                 Data buffer pointer (high byte, low word)           SP_G7BYTE2
                                            Block number (low byte)                         Data buffer pointer (low byte, high word)           SP_G7BYTE3
                                            Block number (middle byte)                      Data buffer pointer (high byte, high word)          SP_G7BYTE4
                                            Block number (high byte)                        Block number (low byte, low word)                   SP_G7BYTE5
                                                                                            Block number (high byte, low word)                  SP_G7BYTE6
                                                                                            Block number (low byte, high word)                  SP_G7BYTE7
                                                                                            Block number (high byte, high word)                 SP_G7BYTE7+2
                        
                        The following error return values are possible.
                        _______________________________________________
                        $06 BUSERR Communications error
                        $27 IOERROR I/O error
                        $28 NODRIVE No device connected
                        $2B NOWRITE Disk write protected
                        $2D BADBLOCK Invalid block number
                        S2F OFFLINE Device off line or no disk in drive
                        
                        */

                        if (flgSoundEffect==1){
                            TIM1->PSC=1000;
                            HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
                           play_buzzer_ms(15);
                        }

                        BN_3B_LOW = packet_buffer[SP_G7BYTE3];                                                          // block number low
                        BN_3B_MID = packet_buffer[SP_G7BYTE4];                                                          // block number middle
                        BN_3B_HIGH = packet_buffer[SP_G7BYTE5];                                                         // block number high
                        
                        statusCode=0x28;
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                              // Find the right Partition
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest) {                                                                   
                                
                                //updateSmartportHD(devices[dev].dispIndex,EMUL_WRITE);

                                blockNumber = (BN_3B_LOW & 0x7f) | (((unsigned short)MSB << 3) & 0x80);                             // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                blockNumber = blockNumber + (((BN_3B_MID & 0x7f) | (((unsigned short)MSB << 4) & 0x80)) << 8);      // block num second byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                blockNumber = blockNumber + (((BN_3B_HIGH & 0x7f) | (((unsigned short)MSB << 5) & 0x80)) << 16);    // block num third byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                                                                                                                    // get write data packet, keep trying until no timeout
                                if (SmartportReceivePacket()==RET_ERR){
                                    log_error("write command break on timeout");
                                }
                                
                                if (devices[dev].mounted==0){
                                    statusCode=0x2F;                                                                                // OffLine Device off line or no disk in drive
                                }else if (devices[dev].writeable==0)
                                    statusCode=0x2B;
                                    
                                else{
                                    statusCode = decodeDataPacket();
                                    if (statusCode == 0) {                                                                  // ok
                                        log_info("Write Bl. n.r: %d",blockNumber);
                                        
                                        while(fsState!=READY){};
                                        fsState=BUSY;
                                        
                                        FRESULT fres=f_lseek(&devices[dev].fil,devices[dev].dataOffset+blockNumber*512);
                                        if (fres!=FR_OK){
                                            log_error("Write seek err!");
                                            statusCode = 0x27;
                                        }else{
                                            fsState=BUSY;

                                            unsigned int pt;
                                            fres=f_write(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt); // Reading block from SD Card
                                            if(fres != FR_OK){
                                                log_error("Write err! Block:%d",blockNumber);
                                                statusCode = 0x27;
                                            }
                                            while(fsState!=READY){};
                                        }   
                                    }else{
                                        // return of statusCode 6
                                        log_error("Bad Checksum on write");
                                    }
                                }                                
                            }
                        }
                        
                        if (statusCode!=0x0){
                            log_error("Smartport WRITEBLOCK cmd:%02X, dest:%002X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                        }

                        encodeReplyPacket(dest,0x1,AUX,statusCode);
                        SmartPortSendPacket(packet_buffer);
                        break;
                    
                    case 0xC2:

                        /*
                        The Write call writes one 512-byte block to the block device specified by the unit 
                        number passed in the parameter list. The block is written from memory starting at the 
                        address specified by the data buffer pointer passed in the parameter list.
                        
                                            Standard call                                   Extended call
                        __________________________________________________________________________________________________________________________________
                        CMDNUM              $02                                             $42                                                 
                        CMDLIST             Parameter count                                 Parameter count
                                            Unit number                                     Unit number
                                            Data buffer pointer (low byte)                  Data buffer pointer (low byte, low word)            SP_G7BYTE1
                                            Data buffer pointer (high byte)                 Data buffer pointer (high byte, low word)           SP_G7BYTE2
                                            Block number (low byte)                         Data buffer pointer (low byte, high word)           SP_G7BYTE3
                                            Block number (middle byte)                      Data buffer pointer (high byte, high word)          SP_G7BYTE4
                                            Block number (high byte)                        Block number (low byte, low word)                   SP_G7BYTE5
                                                                                            Block number (high byte, low word)                  SP_G7BYTE6
                                                                                            Block number (low byte, high word)                  SP_G7BYTE7
                                                                                            Block number (high byte, high word)                 SP_G7BYTE7+2
                        
                        The following Error return values are possible <!> Remember to add 0x80 for Bit 7
                        __________________________________________________________________________________
                        
                        $06 BUSERR Communications error
                        $27 IOERROR I/O error
                        $28 NODRIVE No device connected
                        $2B NOWRITE Disk write protected
                        $2D BADBLOCK Invalid block number
                        $2F OFFLINE Device off line or no disk in drive
                        
                        */

                        MSB2    =  packet_buffer[SP_GRP7MSB+8];

                        BN_LW_L =   packet_buffer[SP_G7BYTE5];
                        BN_LW_H =   packet_buffer[SP_G7BYTE6];
                        BN_HW_L =   packet_buffer[SP_G7BYTE7];
                        BN_HW_H =   packet_buffer[SP_G7BYTE7+2];
                        
                        blockNumber = (BN_LW_L & 0x7f) | (((unsigned short)MSB << 5) & 0x80);                             // This should be the BlockNumber                        
                        blockNumber = blockNumber + (((BN_LW_H & 0x7f) | (((unsigned short)MSB << 6) & 0x80)) << 8);    
                        blockNumber = blockNumber + (((BN_HW_L & 0x7f) | (((unsigned short)MSB << 7) & 0x80)) << 16);
                        blockNumber = blockNumber + (((BN_HW_H & 0x7f) | (((unsigned short)MSB2 << 7) & 0x80)) << 24);
                        
                        statusCode=0x28;                                                                                            // Start with NODRIVE No device connected
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                              // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest) {                                                                   // Yes it is, then do the write
                                                                                                                                    // get write data packet, keep trying until no timeout
                                if (SmartportReceivePacket()==RET_ERR){
                                    log_error("write extended error on timeout");
                                }

                                if (devices[dev].mounted==0){
                                    statusCode=0x2F;                                                                                // OffLine Device off line or no disk in drive
                                }else if (devices[dev].writeable==0)
                                    statusCode=0x2B;
                                else{
                                    statusCode = decodeDataPacket();
                                    if (statusCode == 0) {                                                                  // ok
                                        log_info("Write Bl. n.r: %d",blockNumber);
                                        
                                        while(fsState!=READY){};
                                        fsState=BUSY;
                                        
                                        FRESULT fres=f_lseek(&devices[dev].fil,devices[dev].dataOffset+blockNumber*512);
                                        if (fres!=FR_OK){
                                            log_error("Write seek err!");
                                            statusCode = 0x27;
                                        }else{
                                            fsState=BUSY;

                                            unsigned int pt;
                                            fres=f_write(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt); // Reading block from SD Card
                                            if(fres != FR_OK){
                                                log_error("Write err! Block:%d",blockNumber);
                                                statusCode = 0x27;
                                            }
                                            while(fsState!=READY){};
                                        }
                                        
                                        
                                    
                                    }else{
                                        // return of statusCode 6
                                        log_error("Bad Checksum on write");
                                    }
                                }       
                            }
                        }
                        
                        if (statusCode!=0x0){
                            log_error("Smartport WRITEBLOCK cmd:%02X, dest:%002X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                        }

                        encodeReplyPacket(dest,0x1,AUX,statusCode);
                        SmartPortSendPacket(packet_buffer);
                        break;
                    
                    case 0xC3:
                    case 0x83:                                                                                              // STANDARD FORMAT CMD
                        /*    
                        
                        The Format call formats a block device. Note that the formatting performed by this
                        cal is not linked to any operating system; it simply prepares al blocks on the medium
                        for reading and writing. Operating-system-specific catalog information, such as bit
                        maps and catalogs, are not prepared by this call.
                                                            Standard call                   Extended call
                        __________________________________________________________________________________
                        CMDNUM                              $03                             $43
                        CMDLIST                             Parameter count                 Parameter count
                                                            Unit number                     Unit number
                        Possible errors
                        _______________________________
                        $06 BUSERR Communications error
                        $27 IOERROR IO error
                        $28 NODRIVE No device connected
                        $2B NOWRITE Disk write protected
                        $2F OFFLINE Device of line or no disk in drive
                    
                        */

                        statusCode=0x28;
                        
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;                                       // Check if its one of ours
                            if (devices[dev].device_id == dest) { 
                                if (devices[dev].mounted==0){
                                    statusCode=0x2F;
                                }else if (devices[dev].writeable==0){
                                    statusCode=0x2B;
                                }else{
                                    statusCode=0x0;
                                }                                                                                                 
                            }
                        }

                        if (statusCode!=0x0){
                            log_error("Smartport FORMAT cmd:%02X, dest:%02X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                        }
                        
                        encodeReplyPacket(dest,0x0,AUX,statusCode);                                                        // send back a sucessful response
                        SmartPortSendPacket(packet_buffer);
                        break;
                    
                    case 0xC5:                                                                                              // EXTENDED INIT
                    case 0x85:                                                                                              // INIT CMD                        
                        
                        uint8_t numMountedPartition=0;
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                                numMountedPartition++;
                        }

                        if (number_partitions_initialised >numMountedPartition){                                            // The ROM03 IIGS seems to REINIT the Smartport after Boot, we need to manage this case
                            number_partitions_initialised=1;
                            log_warn("Smartport REINIT cmd:%02X, dest:%02X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                            
                            for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                                uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;  
                                devices[dev].device_id = 0;
                            }    
                        }

                        if (number_partitions_initialised <numMountedPartition)
                            statusCode = 0x00;                          // Not the last one
                        else
                            statusCode = 0x7F;                          // the Last one

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;  
                            if (devices[dev].device_id == dest){
                                number_partitions_initialised++;
                                break;
                            }
                            else if (devices[dev].device_id == 0){
                                devices[dev].device_id=dest;
                                number_partitions_initialised++;
                                break;
                            }
                        }
                        
                        if (statusCode!=0x0){
                            log_warn("Smartport LAST INIT cmd:%02X, dest:%02X, statusCode:%02X",packet_buffer[SP_COMMAND],dest,statusCode);
                        }
                        
                        encodeReplyPacket(dest,0x0,AUX,statusCode);
                        SmartPortSendPacket(packet_buffer);
                        break;
                    
                    case 0x84:                                                                 // Normal Control

                        /* The Control call sends control information to the device. The information may be
                           either general or device specific.

                           
                                    Standard call                       Extended call
                        CMDNUM      $04                                 $44
                        GMDLIST     Parameter count                     Parameter count
                                    Unit number                         Unit number
                                    Control list pointer (low byte)     Control list pointer (low byte, low word)       SP_G7BYTE1
                                    Control list pointer (high byte)    Control list pointer (high byte, low word)      SP_G7BYTE2
                                    Control code                        Control list pointer (low byte, high word)      SP_G7BYTE3
                                                                        Control list pointer (high byte, high word)     SP_G7BYTE4
                                                                        Control code                                    SP_G7BYTE5

                            $00 Resets the device
                            $01 Sets device control block
                            $02 Sets newline status (character devices only)
                            $03 Services device interrupt

                            SmartPort calls specific to UniDisk 3.5

                            $04 Eject ejects the media from a 3.5-inch drive.
                            $05 Execute dispatches the intelligent controller in the UniDisk 3.5 device
                            $06 SetAddress
                            $07 Download

                            The folowing error return values are possible.
                            ____________________________________________________________
                            $06 BUSERR Communications error
                            $21 BADCTL Invalid control code
                            $22 BADCTLPARM Invalid parameter list
                            $30-$3F UNDEFINED Device-specific error

                            Note:   As per the firmware documentation control code should be below 0x80 and thus Bit 7 (from the MSB shoulg never be set)
                                    Thus it should not be needed to check the Bit7 from the MSB GRP 1, but to make it clean let's do it.
                        
                        0000: C3 81 80 80 80 80 82 81 80 84 83 A0 80 AA 85 80 - ..����..�...�..�
                        0010: 80 80 80 AA BF C8                               - ���.............
                        
                        */
                        
                        //unsigned char PTR_LOW,PTR_HIGH;                        
                        //PTR_LOW=    packet_buffer[SP_G7BYTE1];
                        //PTR_HIGH=   packet_buffer[SP_G7BYTE2];
                        
                        //int ctrlPtr = (PTR_LOW & 0x7f) | (((unsigned short)MSB << 1) & 0x80);                         
                        //    ctrlPtr = ctrlPtr + (((PTR_HIGH & 0x7f) | (((unsigned short)MSB << 2) & 0x80)) << 8);      
                        
                        ctrlCode= (packet_buffer[SP_G7BYTE3] & 0x7f) | (((unsigned short)MSB << 3) & 0x80);
                        
                        statusCode=0x21; 
                        
                        uint8_t respType=0x0;                                                                                          // Bad Control Code 
                        switch(ctrlCode){
                            case 0x00:                                                                                               // RESET
                                log_info("Smartport cmd:%02X, dest:%02X, control command code:%02X RESET",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                statusCode=0x0;
                                break;
                            case 0x04:                                                                                              // EJECT
                                                              
                                for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                                    uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;                                       // Check if its one of ours
                                    if (devices[dev].device_id == dest) {
                                        devices[dev].mounted=0;
                                        devices[dev].filename=NULL;
                                        
                                        char * fileTab[4];
                                        for (uint8_t i=0;i<MAX_PARTITIONS;i++){
                                            devices[i].dispIndex=i;
                                            fileTab[i]=devices[i].filename;                                                         // Display the name of the PO according to the position
                                        }
                                        setImageTabSmartPortHD(fileTab,bootImageIndex);
                                        
                                        statusCode=0x0;
                                    }
                                }  

                                log_info("Smartport cmd:%02X, dest:%02X, control command code:%02X EJECT",packet_buffer[SP_COMMAND],dest,ctrlCode);  
                                break;

                            case 0x05: // UNIDISK EXECUTE
                                SmartportReceivePacket();
                                
                                for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                                    uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;                                       // Check if its one of ours
                                    if (devices[dev].device_id == dest) {
                                        decodeControlExecutePacket(&devices[dev]);
                                    }
                                }  
                                
                                //log_info("Smartport cmd:%02X, dest:%02X, control command code:%02X EXECUTE",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                //print_packet ((unsigned char*) packet_buffer,packetLen);
                                
                                respType=0x01;
                                statusCode=0x0; 
                                break;

                            case 0x06: // UNIDISK SETADDRESS
                                
                                //
                                SmartportReceivePacket();
                                //log_info("Smartport cmd:%02X, dest:%02X, control command code:%02X SETADDRESS",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                //print_packet ((unsigned char*) packet_buffer,packetLen);
                                //log_info("Data Packet end");
                                respType=0x01;
                                statusCode=0x0; 
                                // C3 80 81 81 80 80 80 80 AA EA C8
                                break;
                            
                            case 0x07: // UNIDISK DOWNLOAD
                                //
                                SmartportReceivePacket();
                                //log_info("Smartport cmd:%02X, dest:%02X, control command code:%02X DOWNLOAD",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                //print_packet ((unsigned char*) packet_buffer,packetLen);
                                statusCode=0x0; 
                                respType=0x01;
                                break;

                            default:
                                log_warn("Smartport Unknown cmd:%02X, dest:%02X, control command code:%02X OTHER",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                
                        }
                        
                        encodeReplyPacket(dest,respType,AUX,statusCode);
                        SmartPortSendPacket(packet_buffer);
                        //log_info("Smartport Response Control Packet");
                        //print_packet ((unsigned char*) packet_buffer,packetLen);
                        break;

                    case 0xC4:                                                                                      // EXTENDED CONTROL CMD
                        
                        /* The Control call sends control information to the device. The information may be
                           either general or device specific.

                           
                                    Standard call                       Extended call
                        CMDNUM      $04                                 $44
                        GMDLIST     Parameter count                     Parameter count
                                    Unit number                         Unit number
                                    Control list pointer (low byte)     Control list pointer (low byte, low word)       SP_G7BYTE1
                                    Control list pointer (high byte)    Control list pointer (high byte, low word)      SP_G7BYTE2
                                    Control code                        Control list pointer (low byte, high word)      SP_G7BYTE3
                                                                        Control list pointer (high byte, high word)     SP_G7BYTE4
                                                                        Control code                                    SP_G7BYTE5

                            $00 Resets the device
                            $01 Sets device control block
                            $02 Sets newline status (character devices only)
                            $03 Services device interrupt

                            SmartPort calls specific to UniDisk 3.5

                            $04 Eject ejects the media from a 3.5-inch drive.
                            $05 Execute dispatches the intelligent controller in the UniDisk 3.5 device
                            $06 SetAddress
                            $07 Download

                            The folowing error return values are possible.
                            ____________________________________________________________
                            $06 BUSERR Communications error
                            $21 BADCTL Invalid control code
                            $22 BADCTLPARM Invalid parameter list
                            $30-$3F UNDEFINED Device-specific error

                            Note:   As per the firmware documentation control code should be below 0x80 and thus Bit 7 (from the MSB shoulg never be set)
                                    Thus it should not be needed to check the Bit7 from the MSB GRP 1, but to make it clean let's do it.
                        */
                         
                        ctrlCode= (packet_buffer[SP_G7BYTE5] & 0x7f) | (((unsigned short)MSB << 5) & 0x80);  ;
                        
                        statusCode=0x21; 
                        statusCode=0x00;                               // Bad Control Code 
                        switch(ctrlCode){
                            case 0x00:                                  // RESET
                                //log_info("Smartport CONTROL cmd:%02X, dest:%02X, control command code:%02X RESET",packet_buffer[SP_COMMAND],dest,ctrlCode);
                                statusCode=0x0;
                                break;
                            case 0x04:                                  // EJECT

                                for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                                    uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;                                       // Check if its one of ours
                                    if (devices[dev].device_id == dest) {
                                        devices[dev].mounted=0;
                                        statusCode=0x0;
                                    }
                                }  

                                //log_info("Smartport CONTROL cmd:%02X, dest:%02X, control command code:%02X EJECT",packet_buffer[SP_COMMAND],dest,ctrlCode);  

                                break;
                            default:
                                log_error("Smartport CONTROL cmd:%02X, dest:%02X, control command code:%02X OTHER",packet_buffer[SP_COMMAND],dest,ctrlCode);
                        }

                        //print_packet ((unsigned char*) packet_buffer,packetLen);

                        encodeReplyPacket(dest,0x0,AUX,statusCode);                                                     // just send back a successful response
                        SmartPortSendPacket(packet_buffer);
                        
                        //print_packet ((unsigned char*) packet_buffer,packetLen);
                        
                        break;
                
                    case 0x88:                                                                                      // 0x08 Standard Read
                    /*
                        The Read cal reads the number of bytes specified by the byte count into memory. The
                        starting address of memory that the data is read into is specified by the data buffer
                        pointer. The address pointer references an address within the device that the bytes are
                        to be read from. The meaning of the address parameter depends on the device
                        involved. Although this call is generally intended for use by character devices, a block
                        device might use this call to read a block of nonstandard size (a block larger than 512
                        bytes). In this latter case, the address pointer is interpreted as a block address.

                        DEST                                    01
                        ...
                        CMD                                     09
                        PARMCNT                                 10          Should Be 3
                        MSB GRP1 BIT                            11
                    
                        Data buffer pointer (low byte)          12          SP_G7BYTE1 
                        Data buffer pointer (high byte)         13          SP_G7BYTE2
                        Byte count (low byte)                   14          SP_G7BYTE3
                        Byte count (high byte)                  15          SP_G7BYTE4
                        Address pointer (low byte)              16          SP_G7BYTE5
                        Address pointer (mid byte)              17          SP_G7BYTE6
                        Address pointer (high byte)             18          SP_G7BYTE7
                    */
                        unsigned char D_PTR_LOW,D_PTR_HIGH,BC_LOW,BC_HIGH,A_PTR_L,A_PTR_M,A_PTR_H;

                        D_PTR_LOW=    packet_buffer[SP_G7BYTE1];
                        D_PTR_HIGH=   packet_buffer[SP_G7BYTE2];
                        BC_LOW=       packet_buffer[SP_G7BYTE3];
                        BC_HIGH=      packet_buffer[SP_G7BYTE4];
                        A_PTR_L=      packet_buffer[SP_G7BYTE5];
                        A_PTR_M=      packet_buffer[SP_G7BYTE6];
                        A_PTR_H=      packet_buffer[SP_G7BYTE7];

                        int dataPtr = (D_PTR_LOW & 0x7f) | (((unsigned short)MSB << 1) & 0x80);                         
                            dataPtr = dataPtr + (((D_PTR_HIGH & 0x7f) | (((unsigned short)MSB << 2) & 0x80)) << 8);

                        int ByteCount = (BC_LOW & 0x7f) | (((unsigned short)MSB << 3) & 0x80);
                            ByteCount = ByteCount + (((BC_HIGH & 0x7f) | (((unsigned short)MSB << 4) & 0x80)) << 8);
                    
                        unsigned long  addressPtr = (A_PTR_L & 0x7f) | (((unsigned short)MSB << 5) & 0x80);                             // This should be the BlockNumber                        
                                       addressPtr = addressPtr + (((A_PTR_M & 0x7f) | (((unsigned short)MSB << 6) & 0x80)) << 8);    
                                       addressPtr = addressPtr + (((A_PTR_H & 0x7f) | (((unsigned short)MSB << 7) & 0x80)) << 16);
                            
                        log_info("Smartport READ cmd:%02X, dst:%02X, dataPtr:%04x, ByteCount:%d, addrPtr:%lu",packet_buffer[SP_COMMAND],dest,dataPtr,ByteCount,addressPtr);
                        
                        //print_packet ((unsigned char*) packet_buffer,packetLen);
                        
                        encodeReplyPacket(dest,0x0,AUX,0x0);                                                                          // For the moment send a OK reply
                        SmartPortSendPacket(packet_buffer);                                                                             // We should send the data...
                        break;

                    case 0x89:                                                                 // Normal Write
                        log_info("Smartport cmd:%02X",packet_buffer[SP_COMMAND]);
                        //print_packet ((unsigned char*) packet_buffer,packetLen);
                        break;
                    
                    case 0xC8:                                                                 // Extended Read
                        log_info("Smartport cmd:%02X",packet_buffer[SP_COMMAND]);
                        //print_packet ((unsigned char*) packet_buffer,packetLen);
                        break;

                    case 0xc9:                                                                  // Extended Write
                        log_info("Smartport cmd:%02X",packet_buffer[SP_COMMAND]);
                        //print_packet ((unsigned char*) packet_buffer,packetLen);         
                        break;
                } 
               
            }
            
            packet_buffer[0]=0x0;
            //assertAck();

            cAlive++;
            if (cAlive==5000000){ 
                HAL_SD_CardStateTypeDef state;
                state = HAL_SD_GetCardState(&hsd);
                printf(".%d %lu\n",fsState,state);
                cAlive=0;
            }
            
            pSdEject();
            if (nextAction!=NONE){                                                             // Manage by pNextAction
                switch(nextAction){
                    case SMARTPORT_IMGMOUNT:
                        //TODO MANAGE THE EJECT OF THE DISK
                        //SmartPortMountImage( devices[dev]., char * filename )
                        SmartPortInit();
                        nextAction=NONE;
                        break;
                    default:
                        execAction(&nextAction);
                }
            }
            
        }            

    return;
}

enum STATUS mountProdosPartition(char * filename,int partition){
    return RET_OK;
}


//*****************************************************************************
// Function: encode_data_packet
// Parameters: dest id
// Returns: none
//
// Description: encode 512 byte data packet for read block command from host
// requires the data to be in the packet buffer, and builds the smartport
// packet IN PLACE in the packet buffer
//*****************************************************************************
static void encodeDataPacket (unsigned char source){

    int grpbyte, grpcount;
    unsigned char checksum = 0, grpmsb;
    unsigned char group_buffer[7];

    for (count = 0; count < 512; count++)                                                           // Calculate checksum of sector bytes before we destroy them
        checksum = checksum ^ packet_buffer[count];                                                 // xor all the data bytes

  // Start assembling the packet at the rear and work 
  // your way to the front so we don't overwrite data
  // we haven't encoded yet

  //grps of 7
    for (grpcount = 72; grpcount >= 0; grpcount--){ //73

        memcpy(group_buffer, packet_buffer + 1 + (grpcount * 7), 7);
        grpmsb = 0;                                                                                 // add group msb byte
        for (grpbyte = 0; grpbyte < 7; grpbyte++)
            grpmsb = grpmsb | ((group_buffer[grpbyte] >> (grpbyte + 1)) & (0x80 >> (grpbyte + 1)));
    
        packet_buffer[16 + (grpcount * 8)] = grpmsb | 0x80; // set msb to one

    
        for (grpbyte = 0; grpbyte < 7; grpbyte++)                                                   // now add the group data bytes bits 6-0
            packet_buffer[17 + (grpcount * 8) + grpbyte] = group_buffer[grpbyte] | 0x80;

    }

    packet_buffer[14] = ((packet_buffer[0] >> 1) & 0x40) | 0x80;                                    //total number of packet data bytes for 512 data bytes is 584
    packet_buffer[15] = packet_buffer[0] | 0x80;                                                    //odd byte

    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        //PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        //DEST - dest id - host
    packet_buffer[8] = source;                                                                      //SRC - source id - us
    packet_buffer[9] = 0x82;                                                                        //TYPE - 0x82 = data
    packet_buffer[10] = 0x80;                                                                       //AUX
    packet_buffer[11] = 0x80;                                                                       //STAT
    packet_buffer[12] = 0x81;                                                                       //ODDCNT  - 1 odd byte for 512 byte packet
    packet_buffer[13] = 0xC9;                                                                       //GRP7CNT - 73 groups of 7 bytes for 512 byte packet

    for (count = 7; count < 14; count++)                                                            // now xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    
    packet_buffer[600] = checksum | 0xaa;                                                           // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[601] = checksum >> 1 | 0xaa;                                                      // 1 c7 1 c5 1 c3 1 c1

    //end bytes
    packet_buffer[602] = 0xc8;                                                                      //pkt end
    packet_buffer[603] = 0x00;                                                                      //mark the end of the packet_buffer
    
    packetLen=603;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
//*****************************************************************************
// Function: encodeExtendedDataPacket
// Parameters: source id
// Returns: none
//
// Description: encode 512 byte data packet for read block command from host
// requires the data to be in the packet buffer, and builds the smartport
// packet IN PLACE in the packet buffer
//*****************************************************************************
static void encodeExtendedDataPacket (unsigned char source){
    
    int grpbyte, grpcount;
    unsigned char checksum = 0, grpmsb;
    unsigned char group_buffer[7];

    for (count = 0; count < 512; count++)                                                            // Calculate checksum of sector bytes before we destroy them
        checksum = checksum ^ packet_buffer[count];                                                  // xor all the data bytes

  // Start assembling the packet at the rear and work 
  // your way to the front so we don't overwrite data
  // we haven't encoded yet

  //grps of 7
    for (grpcount = 72; grpcount >= 0; grpcount--){ //73
        memcpy(group_buffer, packet_buffer + 1 + (grpcount * 7), 7);
        grpmsb = 0;                                                                                  // add group msb byte

        for (grpbyte = 0; grpbyte < 7; grpbyte++)
            grpmsb = grpmsb | ((group_buffer[grpbyte] >> (grpbyte + 1)) & (0x80 >> (grpbyte + 1)));
        
        packet_buffer[16 + (grpcount * 8)] = grpmsb | 0x80; // set msb to one

        for (grpbyte = 0; grpbyte < 7; grpbyte++)                                                    // now add the group data bytes bits 6-0
            packet_buffer[17 + (grpcount * 8) + grpbyte] = group_buffer[grpbyte] | 0x80;

    }

    packet_buffer[14] = ((packet_buffer[0] >> 1) & 0x40) | 0x80;                                    //total number of packet data bytes for 512 data bytes is 584
    packet_buffer[15] = packet_buffer[0] | 0x80;                                                    //odd byte

    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        //PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        //DEST - dest id - host
    packet_buffer[8] = source;                                                                      //SRC - source id - us
    packet_buffer[9] = 0xC2;                                                                        //TYPE - 0xC2 = extended data
    packet_buffer[10] = 0x80;                                                                       //AUX
    packet_buffer[11] = 0x80;                                                                       //STAT
    packet_buffer[12] = 0x81;                                                                       //ODDCNT  - 1 odd byte for 512 byte packet
    packet_buffer[13] = 0xC9;                                                                       //GRP7CNT - 73 groups of 7 bytes for 512 byte packet

    for (count = 7; count < 14; count++)                                                            // now xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[600] = checksum | 0xaa;                                                           // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[601] = checksum >> 1 | 0xaa;                                                      // 1 c7 1 c5 1 c3 1 c1

    //end bytes
    packet_buffer[602] = 0xc8;                                                                      //pkt end
    packet_buffer[603] = 0x00;                                                                      //mark the end of the packet_buffer
    packetLen=603;

}
#pragma GCC diagnostic pop
//*****************************************************************************
// Function: decodeControlExecutePacket
// Parameters: prodosPartition
// Returns: error code, >0 = error encountered
//
// Description: decode incomming packet and keep track of unidisk Register
// to fulfill unidisk 
//*****************************************************************************
static void decodeControlExecutePacket(prodosPartition_t * d){

/*
HOST 2nd Message
HOST Packet size:022, src:80, dst:81, type:82, aux:80, cmd:80, paramcnt:80
0000: C3    PBEGIN
      
      81    SRC  
      80    DEST
      82    TYPE 
      80    AUX
      80    STAT
      86    MSB

      80    COUNT LOW BYTE
      80    COUNT HIGH BYTE
      80    ACCUMULATOR
      80    X REGISTER
      80    Y REGISTER
      A4    P STATE PROC VALUE
      80    LOW PC
      87    HIGH PC
      AE FB CKSUM
     
      C8    PEND
*/

    d->unidiskRegister_A=   (packet_buffer[9]  & 0x7f)  | ((((unsigned short)packet_buffer[6] << 3) & 0x80));
    d->unidiskRegister_X=   (packet_buffer[10] & 0x7f)  | ((((unsigned short)packet_buffer[6] << 4) & 0x80));
    d->unidiskRegister_Y=   (packet_buffer[11] & 0x7f)  | ((((unsigned short)packet_buffer[6] << 5) & 0x80));
    d->unidiskRegister_P=   (packet_buffer[12] & 0x7f)  | ((((unsigned short)packet_buffer[6] << 6) & 0x80));
    return;
}


//*****************************************************************************
// Function: decodeDataPacket
// Parameters: none
// Returns: error code, >0 = error encountered
//
// Description: decode 512 byte data packet for write block command from host
// decodes the data from the packet_buffer IN-PLACE!
//*****************************************************************************
static int decodeDataPacket (void){

    int grpbyte, grpcount;
    unsigned char numgrps, numodd;
    unsigned char checksum = 0, bit0to6, bit7, oddbits, evenbits;
    unsigned char group_buffer[8];

    int pl= packetLen;
    //log_info("Data Packet Len:%d",pl);
    //print_packet ((unsigned char*) packet_buffer,packetLen);

    numodd = packet_buffer[6] & 0x7f;                                                               // Handle arbitrary length packets :) 
    numgrps = packet_buffer[7] & 0x7f;

    for (count = 1; count < 8; count++)                                                             // First, checksum  packet header, because we're about to destroy it
        checksum = checksum ^ packet_buffer[count];                                                 // now xor the packet header bytes

    //log_info("PL-3:%02X PL-2:%02X",packet_buffer[pl-3],packet_buffer[pl-2]);
    
    if (packet_buffer[pl-1]==0xC8){
        evenbits = packet_buffer[pl-3] & 0x55;
        oddbits = (packet_buffer[pl-2] & 0x55 ) << 1;
    }else{
        evenbits = packet_buffer[pl-2] & 0x55;
        oddbits = (packet_buffer[pl-1] & 0x55 ) << 1;
    }

    //log_info("evBits %02X, oddBit:%02X",evenbits,oddbits);
    for(int i = 0; i < numodd; i++){                                                                 //add oddbyte(s), 1 in a 512 data packet
        packet_buffer[i] = ((packet_buffer[8] << (i+1)) & 0x80) | (packet_buffer[9+i] & 0x7f);
        checksum ^= packet_buffer[i];
    }
    
    uint8_t oddOffset=0;
    if (numodd!=0){
        oddOffset=1+numodd;
    }

    for (grpcount = 0; grpcount < numgrps; grpcount++){                                             // 73 grps of 7 in a 512 byte packet
        memcpy(group_buffer, packet_buffer + 8+oddOffset + (grpcount * 8), 8);
        for (grpbyte = 0; grpbyte < 7; grpbyte++) {
            bit7 = (group_buffer[0] << (grpbyte + 1)) & 0x80;
            bit0to6 = (group_buffer[grpbyte + 1]) & 0x7f;
            packet_buffer[1 + (grpcount * 7) + grpbyte] = bit7 | bit0to6;
            checksum ^= bit7 | bit0to6;
        }
    }

    if (checksum == (oddbits | evenbits))
        return 0;                                                                                   // NO error
    else{
        log_error("decode Data checksum %02X<>%02X",checksum,(oddbits | evenbits));
        return 6;                                                                                   // Smartport bus error code
    }
}




static void encodeUnidiskStatReplyPacket(prodosPartition_t d){
    unsigned char checksum = 0;


    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    /*

    Status Code = $05, Return UniDisk 3.5 Status This call allows the
    diagnostic program to get more detailed information about the cause of a
    read or write error, and to examine the contents of the 65C02's registers
    after a CONTROL Protocol Converter call with control code = $05

    The Retries byte returned by a STATUS call with status code = $05 (Return
    UniDisk 3.5 Status) species the number of address fields that had to be
    passed before the operation was completed. This information could be used,
    for example, to determine the number of passes necessary to read a data
    field correctly: If Retries is found to be greater than the number of sectors
    on the target track, then more than one pass was required.
    The last four bytes of the status list are set only after a CONTROL call with
    control code = $05, and are zero after any other call (STATUS calls do not
    clear the status bytes).

    Msg: 0227
    
    HOST Packet size:022, src:80, dst:81, type:80, aux:80, cmd:80, paramcnt:83
    0000: C3 81 80 80 80 80 82 81 80 80 83 E0 C0 AB 85 84 - ..����..��......
    0010: 83 80 80 EA FE C8                               - .��.............
    
    STAT_LIST   
            DFB     00      
            DFB     ERROR  
            DFB     RETRIES
            DFB     00
            DFB     REG_A   ACC Value   after a CONTROL EXECUTE CALL
            DFB     REG_X   X Value     after a CONTROL EXECUTE CALL
            DFB     REG_Y   Y Value     after a CONTROL EXECUTE CALL
            DFB     REG_P   Processor Status after a CONTROL EXECUTE CALL


    DEVICE Packet size:021, src:81, dst:80, type:81, aux:80, cmd:80, paramcnt:81
    0000:   C3          0   PBEGIN
            80          1   DEST
            81          2   SRC
            81          3   TYPE
            80          4   AUX
            80          5   STAT
            81          6   ODDCNT
            81          7   GRPCNT
            80          8   00  MSBODD
            80          9   00  ODDBYTE
            81          10  MSBGRP
            80          11  ERROR
            80          12  RETRIES
            80          13  00
            80          14  REG_A ACC
            80          15  REG_X                     
    0010:   88          16  REG_Y  
            FF          17  REG_P
            FF BB       18 19 CHECKSUM 1 2
            C8          20  PEND
    */

    unsigned char data[7];
    
    data[0]=0x0;            //  ERRROR
    data[1]=0x0;            //  RETRIES
    data[2]=0x0;            //  00
    data[3]=d.unidiskRegister_A;
    data[4]=d.unidiskRegister_X;
    data[5]=d.unidiskRegister_Y;
    data[6]=d.unidiskRegister_P;

    packet_buffer[6] = 0xC3;                                                                        // PBEGIN   - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST     - dest id - host
    packet_buffer[8] = d.device_id;                                                                 // SRC      - source id - us
    packet_buffer[9] =  0x81;                                                                       // TYPE     - status
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = 0x80;                                                                       // STAT     - data status
    packet_buffer[12] = 0x81;                                                                       // ODDCNT   - 4 data bytes
    packet_buffer[13] = 0x81;                                                                       // GRP7CNT
    packet_buffer[14] = 0x80; 
    packet_buffer[15] = 0x80; 
    
    packet_buffer[16] = 0x80 |                                                                      // GROUP MSB
                    ((data[0]>> 1) & 0x40) | 
                    ((data[1]>> 2) & 0x20) | 
                    ((data[2]>> 3) & 0x10) | 
                    ((data[3]>> 4) & 0x08 )|
                    ((data[4]>> 5) & 0x04 )|
                    ((data[5]>> 6) & 0x02 )|
                    ((data[6]>> 7) & 0x01 ); 
    
    for (count=0;count <7;count++){
        packet_buffer[17+count] = data[count] | 0x80; 
    }
   
    packet_buffer[16] = 0x81; 
    packet_buffer[17] = 0x80; 
    packet_buffer[18] = 0x80; 
    packet_buffer[19] = 0x80; 
    packet_buffer[20] = 0x80; 
    packet_buffer[21] = 0x80; 
    packet_buffer[22] = 0x88; 
    packet_buffer[23] = 0xFF;
    

    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];

    // 14 ODD MSB
    checksum = checksum ^ (packet_buffer[15] & 0x7f);

    uint8_t grpbyte=0;
    for (grpbyte = 0; grpbyte < 7; grpbyte++) {
       uint8_t bit7 = (packet_buffer[16] << (grpbyte + 1)) & 0x80;
       uint8_t bit0to6 = (packet_buffer[17 + grpbyte]) & 0x7f;
        checksum ^= bit7 | bit0to6;
        
    }    
    
    packet_buffer[24] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[25] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[26] = 0xc8;                                                                       //PEND
    packet_buffer[27] = 0x00;                                                                       //end of packet in buffer

    packetLen=28;
}



//*****************************************************************************
// Function: encodeStatusReplyPacket
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command packet. The reply
// includes following:
// data byte 1 is general info.
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. 
// Size determined from image file.
//*****************************************************************************
static void encodeStatusReplyPacket (prodosPartition_t d){

    unsigned char checksum = 0;
    unsigned char data[4];

    /* Statcode = $00: 
     The device status consists o f 4 bytes. 
     
     The first is the general status byte

     General Status Byte
     _________________________________________________________________________
     Bit 7: Block  device              1 = Block device; 0 - Character device
     Bit 6: Write allowed
     Bit 5: Read allowed
     Bit 4: Device online or disk in drive
     Bit 3: Format allowed
     Bit 2: Media write protected      (block devices only)
     Bit 1: Currently interrupting     (//c only)
     Bit 0: Currently open             (char devices only) 

     If the device is a block device, the next field indicates the number of blocks in the
     device. This is a 3-byte field for standard cals or a 4-byte field for extended cals. The
     least significant byte is first. If the device is a character device, these bytes are set to zero.
    
     */

    data[0] = 0b11111000;

    if (d.mounted==0){
        data[0]=data[0] & 0b11101111;
    }
    
    //Disk size Bytes [1-3]:
    data[1] = d.blocks & 0xff;
    data[2] = (d.blocks >> 8 ) & 0xff;
    data[3] = (d.blocks >> 16 ) & 0xff;

    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                    // PBEGIN   - start byte
    packet_buffer[7] = 0x80;                                                                    // DEST     - dest id - host
    packet_buffer[8] = d.device_id;                                                             // SRC      - source id - us
    packet_buffer[9] = 0x81;                                                                    // TYPE     - status
    packet_buffer[10] = 0x80;                                                                   // AUX
    packet_buffer[11] = 0x80;                                                                   // STAT     - data status
    packet_buffer[12] = 0x84;                                                                   // ODDCNT   - 4 data bytes
    packet_buffer[13] = 0x80;                                                                   // GRP7CNT
    //4 odd bytes
    packet_buffer[14] = 0x80 | 
                    ((data[0]>> 1) & 0x40) | 
                    ((data[1]>> 2) & 0x20) | 
                    ((data[2]>> 3) & 0x10) | 
                    ((data[3]>> 4) & 0x08 );                                                    // odd msb

    packet_buffer[15] = data[0] | 0x80;                                                         // data 1
    packet_buffer[16] = data[1] | 0x80;                                                         // data 2 
    packet_buffer[17] = data[2] | 0x80;                                                         // data 3 
    packet_buffer[18] = data[3] | 0x80;                                                         // data 4 
    
    for(int i = 0; i < 4; i++){                                                                 //calc the data bytes checksum
        checksum ^= data[i];
    }

    for (count = 7; count < 14; count++)                                                        // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    
    packet_buffer[19] = checksum | 0xaa;                                                        // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[20] = checksum >> 1 | 0xaa;                                                   // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[21] = 0xc8;                                                                   // PEND
    packet_buffer[22] = 0x00;                                                                   // end of packet in buffer

    packetLen=22;

}


//*****************************************************************************
// Function: encodeExtendedStatusReplyPacket
// Parameters: proDosPartitiion
// Returns: none
//
// Description: this is the reply to the extended status command packet. The reply
// includes following:
// data byte 1
// data byte 2-5 number of blocks. 2 is the LSB and 5 the MSB. 
// Size determined from image file.
//*****************************************************************************
static void encodeExtendedStatusReplyPacket (prodosPartition_t d){
    unsigned char checksum = 0;

    unsigned char data[5];

    /* Statcode = $00: 
     The device status consists o f 5 bytes. 
     
     The first is the general status byte

     General Status Byte
     _________________________________________________________________________
     Bit 7: Block  device              1 = Block device; 0 - Character device
     Bit 6: Write allowed
     Bit 5: Read allowed
     Bit 4: Device online or disk in drive
     Bit 3: Format allowed
     Bit 2: Media write protected      (block devices only)
     Bit 1: Currently interrupting     (//c only)
     Bit 0: Currently open             (char devices only) 

     If the device is a block device, the next field indicates the number of blocks in the
     device. This is a 3-byte field for standard cals or a 4-byte field for extended cals. The
     least significant byte is first. If the device is a character device, these bytes are set to zero.
    
     */

    data[0] = 0b11111000;                                                                       // Status Bytes
    if (d.mounted==0){
        data[0]=data[0] & 0b11101111;
    }
    data[1] = d.blocks & 0xff;                                                                  // Disk size Bytes
    data[2] = (d.blocks >> 8 ) & 0xff;
    data[3] = (d.blocks >> 16 ) & 0xff;
    data[4] = (d.blocks >> 24 ) & 0xff;

    packet_buffer[0] = 0xff;                                                                    // SYNC bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                    // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                    // DEST - dest id - host
    packet_buffer[8] = d.device_id;                                                             // SRC - source id - us
    packet_buffer[9] = 0xC1;                                                                    // TYPE - extended status
    packet_buffer[10] = 0x80;                                                                   // AUX
    packet_buffer[11] = 0x80;                                                                   // STAT - data status
    packet_buffer[12] = 0x85;                                                                   // ODDCNT - 5 data bytes
    packet_buffer[13] = 0x80;                                                                   // GRP7CNT
    //5 odd bytes
    packet_buffer[14] = 0x80 | 
                        ((data[0]>> 1) & 0x40) | 
                        ((data[1]>> 2) & 0x20) | 
                        ((data[2]>> 3) & 0x10) |
                        ((data[3]>> 4) & 0x08) | 
                        ((data[4]>> 5) & 0x04) ;                                                // odd msb
    packet_buffer[15] = data[0] | 0x80;                                                         // data 1
    packet_buffer[16] = data[1] | 0x80;                                                         // data 2 
    packet_buffer[17] = data[2] | 0x80;                                                         // data 3 
    packet_buffer[18] = data[3] | 0x80;                                                         // data 4 
    packet_buffer[19] = data[4] | 0x80;                                                         // data 5
    
    for(int i = 0; i < 5; i++){                                                                 // calc the data bytes checksum
        checksum ^= data[i];
    }
    
    for (count = 7; count < 14; count++)                                                        // calc the data bytes checksum                                                           
        checksum = checksum ^ packet_buffer[count];                                             // xor the packet header bytes
    packet_buffer[20] = checksum | 0xaa;                                                        // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[21] = checksum >> 1 | 0xaa;                                                   // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[22] = 0xc8;                                                                   // PEND
    packet_buffer[23] = 0x00;                                                                   // end of packet in buffer

    packetLen=23;

}

static void encodeReplyPacket(unsigned char source,unsigned char type,unsigned char aux, unsigned char respCode){

    /*

    The contents type consists of a type and aux type byte. Three contents types are
    currently defined: Type = $80 is a command packet, type = $81 is a status packet, and
    type = $82 is a data packet. Bit-6 is the command byte, and the aux type byte defines
    the packet as either extended or standard. Aux type = $80 indicates a standard packet,
    and $CO indicates an extended packet. Command = $8X indicates a standard packet,
    and SCX indicates a n extended packet.


    C3 80 81 81 80 80 80 80 AA EA C8
    */

    unsigned char checksum = 0;

    packet_buffer[0] = 0xff;                                                                        // sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xC3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = source;                                                                      // SRC - source id - us
    packet_buffer[9] = type | 0x80;                                                                 // TYPE -status
    packet_buffer[10] = aux | 0x80;                                                                 // AUX
    packet_buffer[11] = respCode | 0x80;                                                             // STAT - data status - error
    packet_buffer[12] = 0x80;                                                                       // ODDCNT - 0 data bytes
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT

    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[14] = checksum | 0xAA;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[15] = checksum >> 1 | 0xAA;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[16] = 0xC8;                                                                       //PEND
    packet_buffer[17] = 0x00;                                                                       //end of packet in buffer
    
    packetLen=18;
    
}

//*****************************************************************************
// Function: encodeStatusDibReplyPacket
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB.
// Calculated from actual image file size.
//*****************************************************************************
void encodeStatusDibReplyPacket (prodosPartition_t d){
    
    /*
    Standard call                                   Extended call
    __________________________________________________________________________________
    Device status byte                              Device status byte
    Block size (low byte)                           Block size (low byte, low word)
    Block size (mid byte)                           Block size (high byte, low word)
    Block size (high byte)                          Block size (low byte, high word)
    ID string length                                Block size (high byte, high word)
    ID string (16 bytes)                            ID string length
    Device type byte                                ID string (16 bytes)
    Device subtype byte                             Device type byte
    Version word                                    Device subtype byte
    Version word
    
    Type            Subtype             Device
    __________________________________________________________________________________
    $00             $00                 Apple Il memory expansion card
    $00             $CO                 Apple IGS Memory Expansion Card configured as a RAM disk
    $01             $OO                 UniDisk 3.5
    $01             $CO                 Apple 3.5 drive
    $03             $EO                 Apple I SCSI with nonremovable media

    Undefined SmartPort devices may implement the following types and subtypes:

    Type            Subtype             Device
    __________________________________________________________________________________
    $02             $20                 Hard disk
    $02             $00                 Removable hard disk
    $02             $40                 Removable hard disk supporting disk-switched errors
    $02             $AO                 Hard disk supporting extended calls
    $02             $CO                 Removable hard disk supporting extended calls and disk-switched errors
    $02             SAO                 Hard disk supporting extended calls
    $03             $CO                 SCSI with removable media

    The firmware version field is a 2-byte field consisting of a number indicating the
    */

    int grpbyte, grpcount, i;
    int grpnum, oddnum; 
   
    uint8_t checksum = 0, grpmsb;
    uint8_t group_buffer[7];
    uint8_t data[25];
   
    uint8_t devicetype=0;
    uint8_t deviceSubType=0;

    if (d.diskFormat==_2MG){
        //devicetype=0x02;                                                                      // Hard Disk
        devicetype=0x01;                                                                        // Unidisk
        deviceSubType=0x0;                                                                      // Removable hard disk
    }else if (d.diskFormat==PO){
        
        devicetype=0x02;
        
        //deviceSubType=0x0;                                                                    // Removable hard disk
        //deviceSubType=$20;                                                                    // Hard disk
        deviceSubType=0x40;                              // Removable hard disk supporting disk-switched errors
        //deviceSubType=0xA0;                            // Hard disk supporting extended calls
        //deviceSubType=0xC0;                            // Removable hard disk supporting extended calls and disk-switched errors
    }else{
        devicetype=0x01;                                 // Unidisk
        deviceSubType=0x0;                               // Removable hard disk
    }
    
    grpnum=3;
    oddnum=4;

    // TODO MANAGE WRITE PROTECTED MEDIA

    //* write data buffer first (25 bytes) 3 grp7 + 4 odds
    data[0] = 0xF8;                                                                                 // 0b11111000; general status - f8 
    if (d.mounted==0){
        data[0]=data[0] & 0b11101111;
    }                                                                                               // number of blocks =0x00ffff = 65525 or 32mb
    data[1] = d.blocks & 0xff;                                                                      // block size 1 
    data[2] = (d.blocks >> 8 ) & 0xff;                                                              // block size 2 
    data[3] = (d.blocks >> 16 ) & 0xff ;                                                            // block size 3 
    
    data[4] = 0x0c;                                                                                 // ID string length - 11 chars
    data[5] = 'S';
    data[6] = 'M';
    data[7] = 'A';
    data[8] = 'R';
    data[9] = 'T';
    data[10] = 'D';
    data[11] = 'I';
    data[12] = 'S';
    data[13] = 'K';
    data[14] = 'H';
    data[15] = 'D';
    data[16] = d.device_id+0x30;                                                                    // 
    data[17] = ' ';
    data[18] = ' ';
    data[19] = ' ';
    data[20] = ' ';                                                                                 // ID string (16 chars total)
    data[21] = devicetype;                                                                          // Device type    - 0x02  harddisk
    data[22] = deviceSubType;                                                                       // Change by VIBR 24.02.25 0x0A -> 0x20   Or 0x00 Removable Harddisk
    data[23] = 0x01;                                                                                // Firmware version 2 bytes
    data[24] = 0x0f;                                                                                
    
    for (count = 0; count < 25; count++)                                                            // Calculate checksum of sector bytes before we destroy them
        checksum = checksum ^ data[count];                                                          // xor all the data bytes

    // Start assembling the packet at the rear and work 
    // your way to the front so we don't overwrite data
    // we haven't encoded yet
    for (grpcount = grpnum-1; grpcount >= 0; grpcount--){                                            //grps of 7 // 3
    
        for (i=0;i<8;i++) {
            group_buffer[i]=data[i + oddnum + (grpcount * 7)];
        }
        
        grpmsb = 0;                                                                                 // add group msb byte
        for (grpbyte = 0; grpbyte < 7; grpbyte++)
            grpmsb = grpmsb | ((group_buffer[grpbyte] >> (grpbyte + 1)) & (0x80 >> (grpbyte + 1)));
        packet_buffer[(14 + oddnum + 1) + (grpcount * 8)] = grpmsb | 0x80; // set msb to one

        for (grpbyte = 0; grpbyte < 7; grpbyte++)                                                   // now add the group data bytes bits 6-0
            packet_buffer[(14 + oddnum + 2) + (grpcount * 8) + grpbyte] = group_buffer[grpbyte] | 0x80;
    }
           
    //odd byte
    packet_buffer[14] = 0x80 | 
                    ((data[0]>> 1) & 0x40) | 
                    ((data[1]>> 2) & 0x20) | 
                    ((data[2]>> 3) & 0x10) | 
                    ((data[3]>> 4) & 0x08 );                                                        // odd msb

    packet_buffer[15] = data[0] | 0x80;
    packet_buffer[16] = data[1] | 0x80;
    packet_buffer[17] = data[2] | 0x80;
    packet_buffer[18] = data[3] | 0x80;;

    packet_buffer[0] = 0xff;                                                                        // sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;
    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = d.device_id;                                                                 // SRC - source id - us
    packet_buffer[9] = 0x81;                                                                        // TYPE -status
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = 0x80;                                                                       // STAT - data status
    packet_buffer[12] = 0x84;                                                                       // ODDCNT - 4 data bytes
    packet_buffer[13] = 0x83;                                                                       // GRP7CNT - 3 grps of 7
    
    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[43] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[44] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[45] = 0xc8;                                                                       // PEND
    packet_buffer[46] = 0x00;                                                                       // end of packet in buffer

    packetLen=46;
}


//*****************************************************************************
// Function: encodeExtendedStatusDibReplyPacket
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-5 number of blocks. 2 is the LSB and 5 the MSB.
// Calculated from actual image file size.
//*****************************************************************************
void encodeExtendedStatusDibReplyPacket (prodosPartition_t d){
    
    /*
    Standard call                                   Extended call
    __________________________________________________________________________________
    Device status byte                              Device status byte
    Block size (low byte)                           Block size (low byte, low word)
    Block size (mid byte)                           Block size (high byte, low word)
    Block size (high byte)                          Block size (low byte, high word)
    ID string length                                Block size (high byte, high word)
    ID string (16 bytes)                            ID string length
    Device type byte                                ID string (16 bytes)
    Device subtype byte                             Device type byte
    Version word                                    Device subtype byte
    Version word
    
    Type            Subtype             Device
    __________________________________________________________________________________
    $00             $00                 Apple Il memory expansion card
    $00             $CO                 Apple IGS Memory Expansion Card configured as a RAM disk
    $01             $OO                 UniDisk 3.5
    $01             $CO                 Apple 3.5 drive
    $03             $EO                 Apple I SCSI with nonremovable media

    Undefined SmartPort devices may implement the following types and subtypes:

    Type            Subtype             Device
    __________________________________________________________________________________
    $02             $20                 Hard disk
    $02             $00                 Removable hard disk
    $02             $40                 Removable hard disk supporting disk-switched errors
    $02             $AO                 Hard disk supporting extended calls                                             TO BE TESTED
    $02             $CO                 Removable hard disk supporting extended calls and disk-switched errors
    $02             SAO                 Hard disk supporting extended calls
    $03             $CO                 SCSI with removable media

    The firmware version field is a 2-byte field consisting of a number indicating the
    */

    uint8_t devicetype=0;
    uint8_t deviceSubType=0;

    if (d.diskFormat==_2MG){
        devicetype=0x02;
        deviceSubType=0x00;
    }else if (d.diskFormat==PO){
        devicetype=0x02;
        deviceSubType=0x00;
    }else{
        devicetype=0x01;                                 // Unidisk
        deviceSubType=0x0;                               // Removable hard disk
    }

    unsigned char checksum = 0;

    packet_buffer[0] = 0xff;                                                                        // SYNC BYTES
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = d.device_id;                                                                 // SRC - source id - us
    packet_buffer[9] = 0x81;                                                                        // TYPE -status
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = 0x83;                                                                       // STAT - data status
    packet_buffer[12] = 0x80;                                                                       // ODDCNT - 4 data bytes
    packet_buffer[13] = 0x83;                                                                       // GRP7CNT - 3 grps of 7
    packet_buffer[14] = 0xf0;                                                                       // grp1 msb
    packet_buffer[15] = 0xf8;                                                                       // general status - f8
    
    if (d.mounted==0){
        packet_buffer[15]=packet_buffer[15] & 0b11101111;
    }    

    //number of blocks =0x00ffff = 65525 or 32mb
    packet_buffer[16] = d.blocks & 0xff;                                                            // block size 1 
    packet_buffer[17] = (d.blocks >> 8 ) & 0xff;                                                    // block size 2 
    packet_buffer[18] = ((d.blocks >> 16 ) & 0xff) | 0x80 ;                                         // block size 3 - why is the high bit set?
    packet_buffer[19] = ((d.blocks >> 24 ) & 0xff) | 0x80 ;                                         // block size 4 - why is the high bit set?  
    packet_buffer[20] = 0x8C;                                                                       // ID string length - 11 chars
    packet_buffer[21] = 'S';
    packet_buffer[22] = 'M';                                                                         // ID string (16 chars total)
    packet_buffer[23] = 0x80;                                                                       // grp2 msb
    packet_buffer[24] = 'A';
    packet_buffer[25] = 'R';  
    packet_buffer[26] = 'T';  
    packet_buffer[27] = 'D';  
    packet_buffer[28] = 'I';  
    packet_buffer[29] = 'S';  
    packet_buffer[30] = 'K';  
    

    packet_buffer[31] = 0x80;                                                                       // grp3 msb
    packet_buffer[32] = 'H';
    packet_buffer[33] = 'D';  
    packet_buffer[34] = d.device_id+0x30; 
    packet_buffer[35] = ' ';  
    packet_buffer[36] = ' ';  
    packet_buffer[37] = ' ';  
    packet_buffer[38] = ' ';  
    packet_buffer[39] = 0x80;                                                                       // odd msb
    packet_buffer[40] = devicetype;                                                                 // Device type    - 0x02  harddisk
    packet_buffer[41] = deviceSubType;       // 0x00 -> 0x20 CORRECTED BY VIBR 24.02.25             // Device Subtype - 0x20
    packet_buffer[42] = 0x01;                                                                       // Firmware version 2 bytes
    packet_buffer[43] = 0x0f;
    packet_buffer[44] = 0x90;                                                                       // ??? THIS SHOULD NOT EXIST !! NOT IN THE SPEC

    for (count = 7; count < 45; count++) 
        checksum = checksum ^ packet_buffer[count];                                                 // xor the packet bytes
    
    packet_buffer[45] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[46] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[47] = 0xc8;                                                                        // PEND
    packet_buffer[48] = 0x00;                                                                        // end of packet in buffer

    packetLen=48;
}

//*****************************************************************************
// Function: verifyCmdpktChecksum
// Parameters: none
// Returns: RET_OK, RET_ERR enum
//
// Description: verify the checksum for command packets
//
// 
//*****************************************************************************
static enum STATUS verifyCmdpktChecksum(void){
    int count = 0, length;
    unsigned char evenbits, oddbits, bit7, bit0to6, grpbyte;
    unsigned char calc_checksum = 0; //initial value is 0
    unsigned char pkt_checksum;

    length = packetLen;
    uint8_t offset=0;
    if(packet_buffer[length-1]!=0xC8){                                          // This is ultra weired but on the IIc & IIe 
                                                                                // There is no trailing C8 (as per the spec)
                                                                                // We need to adjust in that case
        //printf("Oups :%02X!\n",packet_buffer[length-1]);
        offset=1;
    }
/*
C3              PBEGIN    MARKS BEGINNING OF PACKET             32 micro Sec.       6   0
81              DEST      DESTINATION UNIT NUMBER               32 micro Sec.       7   1
80              SRC       SOURCE UNIT NUMBER                    32 micro Sec.       8   2
80              TYPE      PACKET TYPE FIELD                     32 micro Sec.       9   3
80              AUX       PACKET AUXILLIARY TYPE FIELD          32 micro Sec.      10   4
80              STAT      DATA STATUS FIELD                     32 micro Sec.      11   5
82              ODDCNT    ODD BYTES COUNT                       32 micro Sec.      12   6
81              GRP7CNT   GROUP OF 7 BYTES COUNT                32 micro Sec.      13   7
80              ODDMSB    ODD BYTES MSB's                       32 micro Sec.      14   8
81              COMMAND   1ST ODD BYTE = Command Byte           32 micro Sec.      15   9
83              PARMCNT   2ND ODD BYTE = Parameter Count        32 micro Sec.      16  10
80              GRP7MSB   MSB's FOR 1ST GROUP OF 7              32 micro Sec.      17  11
80              G7BYTE1   BYTE 1 FOR 1ST GROUP OF 7             32 micro Sec.      18  12

0000: C3 81 80 80 80 80 82 81 80 81 83 82 80 88 80 80 - ..80808080..80...80.8080
0010: 80 FF 80 FF BB C8


0000: C3 81 80 80 80 80 82 81 80 81 83 80 80 BE B4 A1 - ..����..�..��...
0010: 80 88 80 AB FB                                  - �.�.............
.
23:59:59 INFO  Core/Src/emul_smartport.c:2606: packet_buffer[length - 2]:AB
23:59:59 INFO  Core/Src/emul_smartport.c:2607: packet_buffer[length - 3]:80
23:59:59 INFO  Core/Src/emul_smartport.c:2608: pkt_chksum:02!=calc_chksum:A3
23:59:59 ERROR Core/Src/emul_smartport.c:633: Incomming command checksum error

*/

    //unsigned char oddcnt=packet_buffer[SP_ODDCNT] & 0x80;
    //unsigned char grpcnt=packet_buffer[SP_GRP7CNT] & 0x80;

    //2 oddbytes in cmd packet
    calc_checksum ^= ((packet_buffer[SP_ODDMSB] << 1) & 0x80) | (packet_buffer[SP_COMMAND] & 0x7f);
    calc_checksum ^= ((packet_buffer[SP_ODDMSB] << 2) & 0x80) | (packet_buffer[SP_PARMCNT] & 0x7f);

    // 1 group of 7 in a cmd packet
    
    for (grpbyte = 0; grpbyte < 7; grpbyte++) {
        bit7 = (packet_buffer[SP_GRP7MSB] << (grpbyte + 1)) & 0x80;
        bit0to6 = (packet_buffer[SP_G7BYTE1 + grpbyte]) & 0x7f;
        calc_checksum ^= bit7 | bit0to6;
    }

    // calculate checksum for overhead bytes
    for (count = 1; count < 8; count++) // start from first id byte
        calc_checksum ^= packet_buffer[count];

    oddbits = (packet_buffer[length - 2+offset] & 0x55 )<< 1 ;
    evenbits = packet_buffer[length - 3+offset] & 0x55;
    
    pkt_checksum = oddbits | evenbits;

    if ( pkt_checksum == calc_checksum )
        return RET_OK;
    else{
        print_packet ((unsigned char*) packet_buffer,packetLen);
        log_info("packet_buffer[length - 2]:%02X",packet_buffer[length - 2]);
        log_info("packet_buffer[length - 3]:%02X",packet_buffer[length - 3]);
        log_info("pkt_chksum:%02X!=calc_chksum:%02X",pkt_checksum,calc_checksum);
        return RET_ERR;
    }

}

//*****************************************************************************
// Function: print_packet
// Parameters: pointer to data, number of bytes to be printed
// Returns: none
//
// Description: prints packet data for debug purposes to the serial port
//*****************************************************************************
static void print_packet (unsigned char* data, int bytes){
    int count, row;
    char xx;

    
    log_info("Packet id:%04d, size:%03d, src:%02X, dst:%02X, type:%02X, aux:%02x, cmd:%02X, paramcnt:%02X",messageId,bytes,data[SP_SRC],data[SP_DEST],data[SP_TYPE],data[SP_AUX],data[SP_COMMAND],data[SP_PARMCNT]);
    
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
    printf(".\r\n");
    
}

enum STATUS SmartPortUnMountImageFromIndex(uint8_t i){
    return SmartPortUnMountImage(&devices[i]);
}

enum STATUS SmartPortUnMountImage(prodosPartition_t *d){
    d->mounted=0;
    free(d->filename);
    d->filename=NULL;
    return RET_OK;

}

enum STATUS SmartPortMountImage( prodosPartition_t *d, char * filename ){
                                                                                    // Note: Image can be PO or 2MG
                                                                                    // If 2MG then we need to manage the OFFSET of the Data block
    FRESULT fres; 
    d->mounted=0;
    if (filename==NULL){
        log_error("filename is null");
        return RET_ERR;
    }
                                                                                    // Assuming Filename is not null and file exist
                                                                                    // Check if it is a PO or 2MG Disk Format based on the extension
    uint8_t len=(uint8_t)strlen(filename);
    
    if (len<4){
        log_error("bad filename");
        return RET_ERR;
    }else if (!memcmp(filename+(len-4),".2mg",4) ||                                // Check if 2MG or 2mg
              !memcmp(filename+(len-4),".2MG",4)){
        
        d->diskFormat=_2MG;
        
        _2mg_t * st2mg=NULL;
        if(mount2mgFile(st2mg,filename)==RET_OK){
            d->blocks=st2mg->blockCount;
            d->dataOffset=64;
            d->writeable=1;
        }else{
            log_error("mount2mgFile error");  
            return RET_ERR;
        }

        while(fsState!=READY){};
        fsState=BUSY;
        fres = f_open(&d->fil,filename , FA_READ | FA_WRITE | FA_OPEN_EXISTING);    

        log_info("Mouting image file:%s",filename);
        fsState=READY;

        if(fres!=FR_OK){
            log_error("f_open error:%s",filename);    
            return RET_ERR;
        }

    }else if (!memcmp(filename+(len-3),".po",3)  ||                                
              !memcmp(filename+(len-3),".PO",3)  || 
              !memcmp(filename+(len-4),".hdv",4) ||                                
              !memcmp(filename+(len-4),".HDV",4) 
            ){                                 
        
        while(fsState!=READY){};
        fsState=BUSY;
        fres = f_open(&d->fil,filename , FA_READ | FA_WRITE | FA_OPEN_EXISTING);    
    
        log_info("Mouting image file:%s",filename);
        fsState=READY;
    
        if(fres!=FR_OK){
            log_error("f_open error:%s",filename);    
            return RET_ERR;
        } 
            
        d->diskFormat=PO;
        d->dataOffset=0;
        d->writeable=1;                                                                     // TODO: TO BE CHANGED

        if (f_size(&d->fil) != (f_size(&d->fil)>>9)<<9 || (f_size(&d->fil)==0 )){
            log_error("     File must be an unadorned ProDOS order image with no header!");
            log_error("     This means its size must be an exact multiple of 512!");
            
            return RET_ERR;
        }
        d->blocks = f_size(&d->fil) >> 9;
    }else{
        log_error("unknown file format:%s",filename);
        return RET_ERR;
    }                                   
     
    
    d->mounted=1;
    log_info("Mounted: %s",filename);
    log_info("blockCount %d",d->blocks);

#ifdef A2F_MODE    
    HAL_GPIO_WritePin(AB_GPIO_Port,AB_Pin,GPIO_PIN_SET);
#endif

    return RET_OK;
}
