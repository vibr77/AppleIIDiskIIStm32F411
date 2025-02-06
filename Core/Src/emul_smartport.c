
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fatfs.h"

#include "emul_smartport.h"
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

prodosPartition_t devices[MAX_PARTITIONS];

//bool is_valid_image(File imageFile);

unsigned char packet_buffer[SP_PKT_SIZE];   //smartport packet buffer
unsigned char status, packet_byte;

int count;
int partition;
int initPartition;

static volatile unsigned char phase=0x0;

/**
  * @brief SmartPortReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void SmartPortPhaseIRQ(){
    
    phase=(GPIOA->IDR&0b0000000000001111);
    //log_info("phase:0x%02X",phase);
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
        packet_buffer[wrBytesReceived++]=0x0;
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
  * @brief SmartPortReceiveDataIRQ function is used to manage SmartPort Emulation in TIMER 
  * @param None
  * @retval None
  */
void SmartPortReceiveDataIRQ(){
        // ADD WR_REQ IRQ TO MANAGE START & STOP OF THE TIMER

        if ((GPIOA->IDR & WR_DATA_Pin)==0)                                       // get WR_DATA DO NOT USE THE HAL function creating an overhead
            wrData=0;
        else
            wrData=1;

        wrData^= 0x01u;                                                           // get /WR_DATA
        xorWrData=wrData ^ prevWrData;                                            // Compute Magnetic polarity inversion
        prevWrData=wrData; 

        byteWindow<<=1;
        byteWindow|=xorWrData;
        
        wrBytes=wrBytes%603;

        if (byteWindow & 0x80){                                                 // Check if ByteWindow Bit 7 is 1 meaning we have a full bytes 0b1xxxxxxx 0x80
            
            packet_buffer[wrBytes]=byteWindow;
            //packet_buffer[(wrBytes+1)%603]=0x0;                               // seems to be obvious but to be tested
            if (byteWindow==0xC3 && wrBitCounter>10 && wrStartOffset==0)       // Identify when the message start
                wrStartOffset=wrBitCounter;
            
            byteWindow=0x0;

            if (wrStartOffset!=0)                                               // Start writing to packet_buffer only if offset is not 0 (after sync byte)
                wrBytes++;

        }
        wrBitCounter++;                                                           // Next bit please ;)
        wrBytesReceived=wrBytes;
        
}

static uint8_t nextBit=0;
static volatile int bitCounter=0;
static volatile int bytePtr=0;
static volatile uint8_t bitPtr=0;
static volatile int bitSize=0;

void SmartPortSendDataIRQ(){
    if (nextBit==1)                                                                 // This has to be at the beginning otherwise timing of pulse will be reduced
        RD_DATA_GPIO_Port->BSRR=RD_DATA_Pin;
    else
        RD_DATA_GPIO_Port->BSRR=RD_DATA_Pin << 16U;

    bytePtr=bitCounter/8;
    bitPtr=bitCounter%8;

    if (packet_buffer[bytePtr]==0x0 || bytePtr>602){
        
        nextBit=0;
        bitCounter=0;
        flgPacket=1;

        HAL_TIM_PWM_Stop_IT(&htim3,TIM_CHANNEL_4);   
    }

    nextBit=(packet_buffer[bytePtr]>>(7-bitPtr) ) & 1;
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
                !memcmp(fno.fname+(len-3),".po",3)) &&            // .po
                !(fno.fattrib & AM_SYS) &&                        // Not System file
                !(fno.fattrib & AM_HID) &&                        // Not Hidden file
    
                strstr(fno.fname,pattern)
                ){
                
            fileName=malloc(MAX_FILENAME_LENGTH*sizeof(char));
            snprintf(fileName,63,"%s",fno.fname);
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

void SmartPortInit(){
    //log_info("SmartPort init");

    //HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);                                    // we need to set it High
    //HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_SET); 
    HAL_TIM_PWM_Stop_IT(&htim2,TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);

    TIM3->ARR=(32*12)-1;

    char sztmp[128];
    char * szfile;
    for(uint8_t i=0; i< MAX_PARTITIONS; i++){
        sprintf(sztmp,"vol%02d_",i+1);
        szfile=SmartPortFindImage(sztmp);
        devices[i].filename=szfile;
        
        
        SmartPortMountImage(&devices[i],szfile);
        
        if (devices[i].mounted!=1){
            log_error("Mount error: %s not mounted",sztmp);
        }else{
            log_info("%s mounted",sztmp);
        }
    }
    switchPage(SMARTPORT,NULL);                                                                     // Display the Frame of the screen

    for (uint8_t i=0;i<MAX_PARTITIONS;i++){
        //uint8_t indx=(i+bootImageIndex)%MAX_PARTITIONS;
        devices[i].dispIndex=i;
        updateImageSmartPortHD(devices[i].filename,i);                                        // Display the name of the PO according to the position
    }

}



void debugSend(){
    HAL_Delay(50);
    //log_info("%d",nextBit);
    SmartPortSendDataIRQ();

}
void SmartPortSendPacket(unsigned char* buffer){
    
    flgPacket=0;                                                                                    // Reset the flag before sending

                                                                                                     // Clear out the packet buffer
    setRddataPort(1);
    setWPProtectPort(1);                                                                   // Set ACK Port to output
    assertAck();                                                                                      // Set ACK high to signal we are ready to send
    
    while (!(phase & 0x1));                                                                           // Wait Req to be HIGH, HOST is ready to receive
    
    HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
    
    while (flgPacket!=1);                                                                             // Waiting for Send to finish   

    setRddataPort(0);
    HAL_GPIO_WritePin(RD_DATA_GPIO_Port, RD_DATA_Pin,GPIO_PIN_RESET);    
    
    deAssertAck();                                                                                     //set ACK(BSY) low to signal we have sent the pkt
    
    while (phase & 0x01);
    
    return;
}

void SmartportReceivePacket(){
    
    setRddataPort(1);
    flgPacket=0;
    assertAck(); 
                                                                            // ACK HIGH, indicates ready to receive
    while(!(phase & 0x01) );                                                // WAIT FOR REQ TO GO HIGH

    while (flgPacket!=1);                                                   // Receive finish

    deAssertAck();                                                          // ACK LOW indicates to the host we have received a packer
    
    while(phase & 0x01);                                                    // Wait for REQ to go low

    //printbits();

}

void assertAck(){
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port, WR_PROTECT_Pin,GPIO_PIN_SET);               
}
void deAssertAck(){
    HAL_GPIO_WritePin(WR_PROTECT_GPIO_Port, WR_PROTECT_Pin,GPIO_PIN_RESET);              
}

void SmartPortMainLoop(){

    log_info("SmartPortMainLoop entering loop");
    unsigned long int block_num;
    unsigned char LBH=0, LBL=0, LBN=0, LBT=0;

    int number_partitions_initialised = 1;
    int noid = 0;

    unsigned char dest, status,  status_code;

    HAL_GPIO_WritePin(RD_DATA_GPIO_Port, RD_DATA_Pin,GPIO_PIN_RESET);  // set RD_DATA LOW
    //HAL_TIM_PWM_Start_IT(&htim2,TIM_CHANNEL_3);
    //if (digitalRead(ejectPin) == HIGH) 
    //rotate_boot();
    if (bootImageIndex==0)
        bootImageIndex=1;

    initPartition=bootImageIndex-1;
    
    while (1) {

        //noid = 0;                                                                           // Reset noid flag
        setWPProtectPort(0);                                                                // Set ack (wrprot) to input to avoid clashing with other devices when sp bus is not enabled 
                                                                                            // read phase lines to check for smartport reset or enable

        //initPartition=bootImageIndex;

        switch (phase) {
                                                                                            // phase lines for smartport bus reset
                                                                                            // ph3=0 ph2=1 ph1=0 ph0=1
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
                //HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_SET);  // set RD_DATA LOW
                
                setWPProtectPort(1);                                                        // Set ack to output, sp bus is enabled
                assertAck();                                                                // Ready for next request                                           
                
                SmartportReceivePacket();                                                   // Receive Packet
                                                                                            // Verify Packet checksum
                if ( verify_cmdpkt_checksum()==RET_ERR  ){
                    log_error("Incomming command checksum error");
                }
                
                //---------------------------------------------
                // STEP 1 CHECK IF INIT PACKET 
                //---------------------------------------------
                
                //print_packet ((unsigned char*) packet_buffer, packet_length());
                
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
                        
                        //if ( devices[(partition + initPartition) % MAX_PARTITIONS].device_id != packet_buffer[SP_DEST])  //destination id
                        //noid++;
                    }

                    if (noid == MAX_PARTITIONS){  //not one of our id's
                        
                        log_info("Not our ID!");
                        
                        setWPProtectPort(0);                                      // set ack to input, so lets not interfere
                        
                        while (phase & 0x08);
                        
                        print_packet ((unsigned char*) packet_buffer, packet_length());

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
                dest = packet_buffer[SP_DEST];
                                                                                            // Check if its one of ours and an extended packet

                if(packet_buffer[SP_COMMAND]>=0xC0){
                    log_info("Extended packet!");
                    print_packet ((unsigned char*) packet_buffer, packet_length());
                }

                if (packet_buffer[SP_COMMAND]!=0x81)
                    log_info("cmd:0x%02X dest:0x%02X",packet_buffer[SP_COMMAND], packet_buffer[SP_DEST]);

                switch (packet_buffer[SP_COMMAND]) {

                    case 0x80:                                                                                          //is a status cmd

                        dest = packet_buffer[SP_DEST];
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                  // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest && devices[dev].mounted==1 ) {                           // yes it is, and it's online, then reply
                                
                                updateSmartportHD(devices[dev].dispIndex,EMUL_STATUS);
                                                                                                                        // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                status_code = (packet_buffer[14] & 0x7f);                                               // | (((unsigned short)packet_buffer[16] << 3) & 0x80);
                                //log_info(" Status code: %2X",status_code);
                                
                                
                                //print_packet((unsigned char*) packet_buffer, 9);                                        // Standard SmartPort command is 9 bytes
                                
                                if (status_code == 0x03) {                                                              // if statcode=3, then status with device info block
                                    log_info("******** Sending DIB! ********");
                                    encode_status_dib_reply_packet(devices[dev]);
                                    //print_packet ((unsigned char*) packet_buffer,packet_length());
                                    
                                } else {                                                                                // else just return device status
                                    /*log_info("Sending status:");
                                    log_info("  dest: %2X",dest);
                                    log_info("  Partition ID: %2X",devices[dev].device_id);
                                    log_info("  Status code:%2X",status_code);*/
                                    encode_status_reply_packet(devices[dev]);        
                                }

                                SmartPortSendPacket(packet_buffer);
                                
                            }
                        }
                        packet_buffer[0]=0x0;
                        break;

                    case 0xC1:
                        log_info("Extended read! Not implemented!");
                        break;
                    case 0xC2:
                        log_info("Extended write! Not implemented!");
                        break;
                    case 0xC3:
                        log_info("Extended format! Not implemented!");
                        break;
                    case 0xC5:
                        log_info("Extended init! Not implemented!");
                        break;
                    case 0xC0:                                                                                  // Extended status cmd
                
                        dest = packet_buffer[SP_DEST];

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                          // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest) {                                               // yes it is, then reply
                                                                                                                // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                status_code = (packet_buffer[16] & 0x7f);
                                log_info("Extended Status CMD: %2X",status_code);
                                //print_packet ((unsigned char*) packet_buffer,packet_length());
                                if (status_code == 0x03) {                                                      // if statcode=3, then status with device info block
                                    log_info("Extended status DIB!");
                                } else {                                                                        // else just return device status
                                    log_info("Extended status non-DIB:");
                                    log_info("  dest: %2X",dest);
                                    log_info("  Partition ID: %2X",devices[dev].device_id);
                                    log_info("  Status code:%2X",status_code);
                                    encode_extended_status_reply_packet(devices[dev]);        
                                }

                                SmartPortSendPacket(packet_buffer);

                            }
                        }

                        packet_buffer[0]=0x0;
                        break;

                    case 0x81:                                                                                  // is a readblock cmd
                        

                        if (flgSoundEffect==1){
                            TIM1->PSC=1000;
                            HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
                            HAL_Delay(15);
                            HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
                        }
                        //updateSmartportHD();                                                                          // Move Below to have the imageIndex

                        dest = packet_buffer[SP_DEST];
                        // CMD 9
                        // PARMCNT 10
                        // LBH  11
                        // Data Buf PTR    12
                        // Data Buf PTR    13
                        // BL 1            14
                        // BL 2            15
                        // BL 3            16
                        LBH = packet_buffer[SP_GRP7MSB];                                                                // high order bits
                        LBT = packet_buffer[16];                                                                        // block number high
                        LBL = packet_buffer[15];                                                                        // block number middle
                        LBN = packet_buffer[14];                                                                        // block number low
                        
                        //log_info("LBH:%02X, LBT:%02X, LBL:%02X, LBN:%02X",LBH,LBT,LBL,LBN);
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                                  // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;

                            if (devices[dev].device_id == dest) {                                                       // yes it is, then do the read
                                
                                updateSmartportHD(devices[dev].dispIndex,EMUL_READ);                                    // Pass the rightImageIndex    
                                                                                                                        // block num 1st byte
                                block_num = (LBN & 0x7f) | (((unsigned short)LBH << 3) & 0x80);                         // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                block_num = block_num + (((LBL & 0x7f) | (((unsigned short)LBH << 4) & 0x80)) << 8);    // block num second byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                block_num = block_num + (((LBT & 0x7f) | (((unsigned short)LBH << 5) & 0x80)) << 16);   // block num third byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                
                                //log_info("0x81,ID:%02X Read block %d",dest,block_num);
                                
                                while(fsState!=READY){};
                                fsState=BUSY;
                                FRESULT fres=f_lseek(&devices[dev].fil,block_num*512);
                                if (fres!=FR_OK){
                                    log_error("Read seek err!, partition:%d, block:%d",dev,block_num);
                                }

                                fsState=BUSY;
                                unsigned int pt;
                                fres=f_read(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt); // Reading block from SD Card
                                
                                if(fres != FR_OK){
                                    log_error("Read err!");
                                }
                                while(fsState!=READY){};
                                
                                encode_data_packet(dest);
                                SmartPortSendPacket(packet_buffer);
                                //print_packet ((unsigned char*) packet_buffer,packet_length());
                                
                            }
                        }
                        packet_buffer[0]=0x0;
                        break;

                    case 0x82:                                                                                      // is a writeblock cmd
                        dest = packet_buffer[SP_DEST];

                        if (flgSoundEffect==1){
                            TIM1->PSC=1000;
                            HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
                            HAL_Delay(15);
                            HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
                        }

                        // CMD 9
                        // PARMCNT 10
                        // LBH  11
                        // Data Buf PTR    12
                        // Data Buf PTR    13
                        // BL 1            14
                        // BL 2            15
                        // BL 3            16

                        LBH = packet_buffer[SP_GRP7MSB];                                                                // high order bits
                        LBT = packet_buffer[16];                                                                        // block number high
                        LBL = packet_buffer[15];                                                                        // block number middle
                        LBN = packet_buffer[14];                                                                        // block number low

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) {                              // Check if its one of ours
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].device_id == dest) {          // yes it is, then do the write
                                

                                //updateSmartportHD(devices[dev].dispIndex,EMUL_WRITE);

                                block_num = (LBN & 0x7f) | (((unsigned short)LBH << 3) & 0x80);                         // Added (unsigned short) cast to ensure calculated block is not underflowing.
                                block_num = block_num + (((LBL & 0x7f) | (((unsigned short)LBH << 4) & 0x80)) << 8);    // block num second byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                block_num = block_num + (((LBT & 0x7f) | (((unsigned short)LBH << 5) & 0x80)) << 16);   // block num third byte, Added (unsigned short) cast to ensure calculated block is not underflowing.
                                                                                                                        // get write data packet, keep trying until no timeout
                                SmartportReceivePacket();
                                
                                status = decode_data_packet();
                                if (status == 0) {                                                                  // ok
                                    log_info("Write Bl. n.r: %d",block_num);
                                    
                                    while(fsState!=READY){};
                                    fsState=BUSY;
                                    
                                    FRESULT fres=f_lseek(&devices[dev].fil,block_num*512);
                                    if (fres!=FR_OK){
                                        log_error("Write seek err!");
                                    }
                                    fsState=BUSY;

                                    unsigned int pt;
                                    fres=f_write(&devices[dev].fil,(unsigned char*) packet_buffer,512,&pt); // Reading block from SD Card
                                    if(fres != FR_OK){
                                        log_error("Write err! Block:%d",block_num);
                                        status = 6;
                                    }
                                    
                                    while(fsState!=READY){};
                                }
                                //now return status code to host
                                
                                encode_write_status_packet(dest, status);
                                SmartPortSendPacket(packet_buffer);
                            
                            }
                        }
                        packet_buffer[0]=0x0;
                        break;

                    case 0x83:                                                                                 // FORMAT CMD
                        
                        dest = packet_buffer[SP_DEST];
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;                                     // Check if its one of ours
                            if (devices[dev].device_id == dest) {                                                          // yes it is, then reply to the format cmd
                                encode_init_reply_packet(dest, 0x80);                                       // just send back a successful response
                                SmartPortSendPacket(packet_buffer);
                                
                                //print_packet ((unsigned char*) packet_buffer,packet_length());

                            }
                        }
                        packet_buffer[0]=0x0;
                        break;

                    case 0x85:                                                                                              // INIT CMD

                        dest = packet_buffer[SP_DEST];
                        //log_info("dbg %d %d",number_partitions_initialised,dest );
                        
                        uint numMountedPartition=0;
                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            if (devices[dev].mounted==1)
                                numMountedPartition++;
                        }

                        if (number_partitions_initialised <numMountedPartition)
                            status = 0x80;                          // Not the last one
                        else
                            status = 0xFF;                          // the Last one

                        for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;  
                            if (devices[dev].mounted==1 && devices[dev].device_id == dest){
                                //log_info("A %d %d",dev,dest);
                                number_partitions_initialised++;
                                break;
                            }
                            else if (devices[dev].mounted==1 && devices[dev].device_id == 0){
                                devices[dev].device_id=dest;
                                //log_info("B %d %d",dev,dest);
                                number_partitions_initialised++;
                                break;
                            }
                        
                        }
                        /*
                        if (number_partitions_initialised < MAX_PARTITIONS) {                                                // are all init'd yet
                            devices[(number_partitions_initialised - 1 + initPartition) % MAX_PARTITIONS].device_id = dest; //remember dest id for partition
                            number_partitions_initialised++;
                            status = 0x80;
                        }
                        else{
                            devices[(number_partitions_initialised - 1 + initPartition) % MAX_PARTITIONS].device_id = dest; //remember dest id for partition
                            number_partitions_initialised++;
                            status = 0xff;
                        }*/

                        /*for (partition = 0; partition < MAX_PARTITIONS; partition++) { 
                            uint8_t dev=(partition + initPartition) % MAX_PARTITIONS;
                            log_info("dest %d deviceid:%d",dev,devices[dev].device_id);
                        }*/
                        log_info("status dest %d:%02X",dest,status);
                        encode_init_reply_packet(dest, status);
                        SmartPortSendPacket(packet_buffer);

                        packet_buffer[0]=0x0; 

                        break;
                } 
               
            }
            HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin,GPIO_PIN_RESET);  // set RD_DATA LOW
              
            assertAck();
    
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
void encode_data_packet (unsigned char source){

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

}

//*****************************************************************************
// Function: encode_data_packet
// Parameters: source id
// Returns: none
//
// Description: encode 512 byte data packet for read block command from host
// requires the data to be in the packet buffer, and builds the smartport
// packet IN PLACE in the packet buffer
//*****************************************************************************
void encode_extended_data_packet (unsigned char source){
    
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

}


//*****************************************************************************
// Function: decode_data_packet
// Parameters: none
// Returns: error code, >0 = error encountered
//
// Description: decode 512 byte data packet for write block command from host
// decodes the data from the packet_buffer IN-PLACE!
//*****************************************************************************
int decode_data_packet (void){

    int grpbyte, grpcount;
    unsigned char numgrps, numodd;
    unsigned char checksum = 0, bit0to6, bit7, oddbits, evenbits;
    unsigned char group_buffer[8];

    numodd = packet_buffer[6] & 0x7f;                                                              // Handle arbitrary length packets :) 
    numgrps = packet_buffer[7] & 0x7f;

    for (count = 1; count < 8; count++)                                                            // First, checksum  packet header, because we're about to destroy it
        checksum = checksum ^ packet_buffer[count];                                                 // now xor the packet header bytes

    evenbits = packet_buffer[594] & 0x55;
    oddbits = (packet_buffer[595] & 0x55 ) << 1;

    for(int i = 0; i < numodd; i++){                                                                 //add oddbyte(s), 1 in a 512 data packet
        packet_buffer[i] = ((packet_buffer[8] << (i+1)) & 0x80) | (packet_buffer[9+i] & 0x7f);
    }

    for (grpcount = 0; grpcount < numgrps; grpcount++){                                             // 73 grps of 7 in a 512 byte packet
        memcpy(group_buffer, packet_buffer + 10 + (grpcount * 8), 8);
        for (grpbyte = 0; grpbyte < 7; grpbyte++) {
            bit7 = (group_buffer[0] << (grpbyte + 1)) & 0x80;
            bit0to6 = (group_buffer[grpbyte + 1]) & 0x7f;
            packet_buffer[1 + (grpcount * 7) + grpbyte] = bit7 | bit0to6;
        }
    }

    for (count = 0; count < 512; count++)                                                           // Verify checksum
        checksum = checksum ^ packet_buffer[count];                                                 // XOR all the data bytes

    log_info("write checksum %02X<>%02X",checksum,(oddbits | evenbits));
    //print_packet ((unsigned char*) packet_buffer,packet_length());
    
    if (checksum == (oddbits | evenbits))
        return 0;                                                                                   // NO error
    else
        return 6;                                                                                   // Smartport bus error code

}

//*****************************************************************************
// Function: encode_write_status_packet
// Parameters: source,status
// Returns: none
//
// Description: this is the reply to the write block data packet. The reply
// indicates the status of the write block cmd.
//*****************************************************************************
void encode_write_status_packet(unsigned char source, unsigned char status){

    unsigned char checksum = 0;

    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    //int i;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = source;                                                                      // SRC - source id - us
    packet_buffer[9] = 0x81;                                                                        // TYPE
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = status | 0x80;                                                              // STAT
    packet_buffer[12] = 0x80;                                                                       // ODDCNT
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT

    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[14] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[15] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[16] = 0xc8;                                                                       // pkt end
    packet_buffer[17] = 0x00;                                                                       // mark the end of the packet_buffer

}

//*****************************************************************************
// Function: encode_init_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the init command packet. A reply indicates
// the original dest id has a device on the bus. If the STAT byte is 0, (0x80)
// then this is not the last device in the chain. This is written to support up
// to 4 partions, i.e. devices, so we need to specify when we are doing the last
// init reply.
//*****************************************************************************
void encode_init_reply_packet (unsigned char source, unsigned char status){

    unsigned char checksum = 0;

    packet_buffer[0] = 0xff;                                                                        // sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = source;                                                                      // SRC - source id - us
    packet_buffer[9] = 0x80;                                                                        // TYPE
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = status;                                                                     // STAT - data status

    packet_buffer[12] = 0x80;                                                                       // ODDCNT
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT

    for (count = 7; count < 14; count++)                                                            // XOR the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[14] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[15] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[16] = 0xc8;                                                                       // PEND
    packet_buffer[17] = 0x00;                                                                       // end of packet in buffer

}

//*****************************************************************************
// Function: encode_status_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command packet. The reply
// includes following:
// data byte 1 is general info.
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. 
// Size determined from image file.
//*****************************************************************************
void encode_status_reply_packet (prodosPartition_t d){

    unsigned char checksum = 0;
    unsigned char data[4];

    //Build the contents of the packet
    //Info byte
    //Bit 7: Block  device
    //Bit 6: Write allowed
    //Bit 5: Read allowed
    //Bit 4: Device online or disk in drive
    //Bit 3: Format allowed
    //Bit 2: Media write protected
    //Bit 1: Currently interrupting (//c only)
    //Bit 0: Currently open (char devices only) 
    data[0] = 0b11111000;
    //Disk size
    data[1] = d.blocks & 0xff;
    data[2] = (d.blocks >> 8 ) & 0xff;
    data[3] = (d.blocks >> 16 ) & 0xff;

    
    packet_buffer[0] = 0xff;                                                                        //sync bytes
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
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT
    //4 odd bytes
    packet_buffer[14] = 0x80 | 
                    ((data[0]>> 1) & 0x40) | 
                    ((data[1]>> 2) & 0x20) | 
                    ((data[2]>> 3) & 0x10) | 
                    ((data[3]>> 4) & 0x08 );                                                        //odd msb

    packet_buffer[15] = data[0] | 0x80;                                                             // data 1
    packet_buffer[16] = data[1] | 0x80;                                                             // data 2 
    packet_buffer[17] = data[2] | 0x80;                                                             // data 3 
    packet_buffer[18] = data[3] | 0x80;                                                             // data 4 
    
    for(int i = 0; i < 4; i++){                                                                     //calc the data bytes checksum
        checksum ^= data[i];
    }

    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    
    packet_buffer[19] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[20] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[21] = 0xc8;                                                                       //PEND
    packet_buffer[22] = 0x00;                                                                       //end of packet in buffer

}


//*****************************************************************************
// Function: encode_long_status_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the extended status command packet. The reply
// includes following:
// data byte 1
// data byte 2-5 number of blocks. 2 is the LSB and 5 the MSB. 
// Size determined from image file.
//*****************************************************************************
void encode_extended_status_reply_packet (prodosPartition_t d){
    unsigned char checksum = 0;

    unsigned char data[5];

    //Build the contents of the packet
    //Info byte
    //Bit 7: Block  device
    //Bit 6: Write allowed
    //Bit 5: Read allowed
    //Bit 4: Device online or disk in drive
    //Bit 3: Format allowed
    //Bit 2: Media write protected
    //Bit 1: Currently interrupting (//c only)
    //Bit 0: Currently open (char devices only) 
    data[0] = 0b11111000;
    //Disk size
    data[1] = d.blocks & 0xff;
    data[2] = (d.blocks >> 8 ) & 0xff;
    data[3] = (d.blocks >> 16 ) & 0xff;
    data[4] = (d.blocks >> 24 ) & 0xff;

    packet_buffer[0] = 0xff;                                                                        //sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = d.device_id;                                                                 // SRC - source id - us
    packet_buffer[9] = 0xC1;                                                                        // TYPE - extended status
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = 0x80;                                                                       // STAT - data status
    packet_buffer[12] = 0x85;                                                                       // ODDCNT - 5 data bytes
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT
    //5 odd bytes
    packet_buffer[14] = 0x80 | 
                        ((data[0]>> 1) & 0x40) | 
                        ((data[1]>> 2) & 0x20) | 
                        ((data[2]>> 3) & 0x10) |
                        ((data[3]>> 4) & 0x08) | 
                        ((data[4]>> 5) & 0x04) ;                                                    //odd msb
    packet_buffer[15] = data[0] | 0x80;                                                             //data 1
    packet_buffer[16] = data[1] | 0x80;                                                             //data 2 
    packet_buffer[17] = data[2] | 0x80;                                                             //data 3 
    packet_buffer[18] = data[3] | 0x80;                                                             //data 4 
    packet_buffer[19] = data[4] | 0x80;                                                             //data 5
    
    for(int i = 0; i < 5; i++){                                                                     //calc the data bytes checksum
        checksum ^= data[i];
    }
    
    for (count = 7; count < 14; count++)                                                            //calc the data bytes checksum                                                           
        checksum = checksum ^ packet_buffer[count];                                                 // xor the packet header bytes
    packet_buffer[20] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[21] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[22] = 0xc8;                                                                       //PEND
    packet_buffer[23] = 0x00;                                                                       //end of packet in buffer

}
void encode_error_reply_packet (unsigned char source){
    unsigned char checksum = 0;

    packet_buffer[0] = 0xff;                                                                        // sync bytes
    packet_buffer[1] = 0x3f;
    packet_buffer[2] = 0xcf;
    packet_buffer[3] = 0xf3;
    packet_buffer[4] = 0xfc;
    packet_buffer[5] = 0xff;

    packet_buffer[6] = 0xc3;                                                                        // PBEGIN - start byte
    packet_buffer[7] = 0x80;                                                                        // DEST - dest id - host
    packet_buffer[8] = source;                                                                      // SRC - source id - us
    packet_buffer[9] = 0x80;                                                                        // TYPE -status
    packet_buffer[10] = 0x80;                                                                       // AUX
    packet_buffer[11] = 0xA1;                                                                       // STAT - data status - error
    packet_buffer[12] = 0x80;                                                                       // ODDCNT - 0 data bytes
    packet_buffer[13] = 0x80;                                                                       // GRP7CNT

    for (count = 7; count < 14; count++)                                                            // xor the packet header bytes
        checksum = checksum ^ packet_buffer[count];
    packet_buffer[14] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[15] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[16] = 0xc8;                                                                       //PEND
    packet_buffer[17] = 0x00;                                                                       //end of packet in buffer

}

//*****************************************************************************
// Function: encode_status_dib_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB.
// Calculated from actual image file size.
//*****************************************************************************
void encode_status_dib_reply_packet (prodosPartition_t d){
    
    int grpbyte, grpcount, i;
    int grpnum, oddnum; 
    unsigned char checksum = 0, grpmsb;
    unsigned char group_buffer[7];
    unsigned char data[25];

    grpnum=3;
    oddnum=4;
    
    //* write data buffer first (25 bytes) 3 grp7 + 4 odds
    data[0] = 0xf8;                                                                                 // general status - f8 
                                                                                                    // number of blocks =0x00ffff = 65525 or 32mb
    data[1] = d.blocks & 0xff;                                                                      // block size 1 
    data[2] = (d.blocks >> 8 ) & 0xff;                                                              // block size 2 
    data[3] = (d.blocks >> 16 ) & 0xff ;                                                            // block size 3 
    data[4] = 0x0b;                                                                                 // ID string length - 11 chars
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
    data[16] = ' ';
    data[17] = ' ';
    data[18] = ' ';
    data[19] = ' ';
    data[20] = ' ';                                                                                 // ID string (16 chars total)
    data[21] = 0x02;                                                                                // Device type    - 0x02  harddisk
    data[22] = 0x0a;                                                                                // Device Subtype - 0x0a
    data[23] = 0x01;                                                                                // Firmware version 2 bytes
    data[24] = 0x0f;                                                                                
    
    for (count = 0; count < 25; count++)                                                            // Calculate checksum of sector bytes before we destroy them
        checksum = checksum ^ data[count];                                                          // xor all the data bytes

    // Start assembling the packet at the rear and work 
    // your way to the front so we don't overwrite data
    // we haven't encoded yet
    for (grpcount = grpnum-1; grpcount >= 0; grpcount--){                                            //grps of 7// 3
    
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
}


//*****************************************************************************
// Function: encode_long_status_dib_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-5 number of blocks. 2 is the LSB and 5 the MSB.
// Calculated from actual image file size.
//*****************************************************************************
void encode_extended_status_dib_reply_packet (prodosPartition_t d){

    unsigned char checksum = 0;

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
    packet_buffer[11] = 0x83;                                                                       // STAT - data status
    packet_buffer[12] = 0x80;                                                                       // ODDCNT - 4 data bytes
    packet_buffer[13] = 0x83;                                                                       // GRP7CNT - 3 grps of 7
    packet_buffer[14] = 0xf0;                                                                       // grp1 msb
    packet_buffer[15] = 0xf8;                                                                       // general status - f8

    //number of blocks =0x00ffff = 65525 or 32mb
    packet_buffer[16] = d.blocks & 0xff;                                                            // block size 1 
    packet_buffer[17] = (d.blocks >> 8 ) & 0xff;                                                    // block size 2 
    packet_buffer[18] = ((d.blocks >> 16 ) & 0xff) | 0x80 ;                                           // block size 3 - why is the high bit set?
    packet_buffer[19] = ((d.blocks >> 24 ) & 0xff) | 0x80 ;                                           // block size 4 - why is the high bit set?  
    packet_buffer[20] = 0x8d;                                                                       // ID string length - 13 chars
    packet_buffer[21] = 'S';
    packet_buffer[22]= 'm';                                                                       // ID string (16 chars total)
    packet_buffer[23] = 0x80;                                                                       // grp2 msb
    packet_buffer[24] = 'a';
    packet_buffer[25]= 'r';  
    packet_buffer[26]= 't';  
    packet_buffer[27]= 'p';  
    packet_buffer[28]= 'o';  
    packet_buffer[29]= 'r';  
    packet_buffer[30]= 't';  
    

    packet_buffer[31] = 0x80;                                                                       // grp3 msb
    packet_buffer[32] = ' ';
    packet_buffer[33]= 'S';  
    packet_buffer[34]= 'D';  
    packet_buffer[35]= ' ';  
    packet_buffer[36]= ' ';  
    packet_buffer[37]= ' ';  
    packet_buffer[38]= ' ';  
    packet_buffer[39] = 0x80;                                                                       // odd msb
    packet_buffer[40] = 0x02;                                                                       // Device type    - 0x02  harddisk
    packet_buffer[41] = 0x00;                                                                       // Device Subtype - 0x20
    packet_buffer[42] = 0x01;                                                                       // Firmware version 2 bytes
    packet_buffer[43]=  0x0f;
    packet_buffer[44] = 0x90;                                                                       //

    for (count = 7; count < 45; count++) 
        checksum = checksum ^ packet_buffer[count];                                                 // xor the packet bytes
    packet_buffer[45] = checksum | 0xaa;                                                            // 1 c6 1 c4 1 c2 1 c0
    packet_buffer[46] = checksum >> 1 | 0xaa;                                                       // 1 c7 1 c5 1 c3 1 c1

    packet_buffer[47] = 0xc8;                                                                        // PEND
    packet_buffer[48] = 0x00;                                                                        // end of packet in buffer

}

//*****************************************************************************
// Function: verify_cmdpkt_checksum
// Parameters: none
// Returns: RET_OK, RET_ERR enum
//
// Description: verify the checksum for command packets
//
// 
//*****************************************************************************
enum STATUS verify_cmdpkt_checksum(void){
    int count = 0, length;
    unsigned char evenbits, oddbits, bit7, bit0to6, grpbyte;
    unsigned char calc_checksum = 0; //initial value is 0
    unsigned char pkt_checksum;

    length = packet_length();
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

    oddbits = (packet_buffer[length - 2] & 0x55 )<< 1 ;
    evenbits = packet_buffer[length - 3] & 0x55;
    
    pkt_checksum = oddbits | evenbits;

    // calculate checksum for overhead bytes
    

    if ( pkt_checksum == calc_checksum )
        return RET_OK;
    else{
        print_packet ((unsigned char*) packet_buffer,packet_length());
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
void print_packet (unsigned char* data, int bytes){
    int count, row;
    char xx;

    
    log_info("Dump packet src:%02X,dst:%02X,type:%02X,aux:%02x,cmd:%02X,paramcnt:%02X",data[SP_SRC],data[SP_DEST],data[SP_TYPE],data[SP_AUX],data[SP_COMMAND],data[SP_PARMCNT]);
    printf("\r\n");
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

//*****************************************************************************
// Function: packet_length
// Parameters: none
// Returns: length
//
// Description: Calculates the length of the packet in the packet_buffer.
// A zero marks the end of the packet data.
//*****************************************************************************
int packet_length (void){
    int x = 0;

    while (packet_buffer[x++]);

    return x - 1; // point to last packet byte = C8
}

//*****************************************************************************
// Function: print_hd_info
// Parameters: none
// Returns: none
//
// Description: print informations about the ATA dispositive and the FAT File System
//*****************************************************************************
void print_hd_info(void){
    //int i = 0;
    /* if(!sdcard.begin(chipSelect, SPI_HALF_SPEED)){
        log_info("Error init card");
        
    } else {
    
        digitalWrite(statusledPin, HIGH);
        delay(100);
        digitalWrite(statusledPin, LOW);
        delay(100);
    */
    //}
}

//*****************************************************************************
// Function: rotate_boot
// Parameters: none
// Returns: none
//
// Description: Cycle by the 4 partition for selecting boot ones, choosing next
// and save it to EEPROM.  Needs REBOOT to get new partition
//*****************************************************************************
int rotate_boot (void){
    int i;

    for(i = 0; i < MAX_PARTITIONS; i++){
        log_info("Init partition was: %d",initPartition);
        
        initPartition++;
        initPartition = initPartition % 4;
        //Find the next partition that's available 
        //and set it to be the boot partition
        if(devices[initPartition].mounted==1){
            log_info("Selecting boot partition number %d",initPartition);
            break;
        }
    }

    if(i == MAX_PARTITIONS){
        log_error("No online partitions found. Check that you have a file called PARTx.PO and try again, where x is from 1 to %d",MAX_PARTITIONS);
        initPartition = 0;
    }
    // change with config lib
    /*eeprom_write_byte(0, initPartition);
    digitalWrite(statusledPin, HIGH);
    log_info("Changing boot partition to: "));
    Serial.print(initPartition, DEC);
    while (1){
    for (i=0;i<(initPartition+1);i++) {
        digitalWrite(statusledPin,HIGH);
        digitalWrite(partition_led_pins[initPartition], HIGH);
        delay(200);   
        digitalWrite(statusledPin,LOW);
        digitalWrite(partition_led_pins[initPartition], LOW);
        delay(100);   
    }
    delay(600);
    }
    */
    // stop programs
    return 0;
}

enum STATUS SmartPortMountImage( prodosPartition_t *d, char * filename ){
    FRESULT fres; 
    if (filename==NULL){
        log_error("filename is null");
        return RET_ERR;
    }
        

    while(fsState!=READY){};
    fsState=BUSY;
    fres = f_open(&d->fil,filename , FA_READ | FA_WRITE | FA_OPEN_EXISTING);    

    log_info("Mouting image file:%s",filename);
    fsState=READY;

    if(fres!=FR_OK){
        log_error("f_open error:%s",filename);
        d->mounted=0;
        return RET_ERR;
    }
    
    if (f_size(&d->fil) != (f_size(&d->fil)>>9)<<9 || (f_size(&d->fil)==0 )){
        log_error("     File must be an unadorned ProDOS order image with no header!");
        log_error("     This means its size must be an exact multiple of 512!");
        d->mounted=0;
        return RET_ERR;
    }
    d->mounted=1;
    log_info("Mounted: %s",filename);
    d->blocks = f_size(&d->fil) >> 9;

    return RET_OK;
}


bool is_ours(unsigned char source){
    for (unsigned char partition = 0; partition < MAX_PARTITIONS; partition++) { //Check if its one of ours
        if (devices[(partition + initPartition) % MAX_PARTITIONS].device_id == source) {  //yes it is
        return true;
        }
    }
    return false;
}
