#ifndef emul_smartport_h
#define emul_smartport_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "main.h"
#include "configFile.h"

typedef struct prodosPartition_s{
    FIL fil; 
    enum DISK_FORMAT diskFormat;
    unsigned char device_id;              
    unsigned long blocks;                 
    unsigned int dataOffset;           
    uint8_t mounted;
    uint8_t writeable;
    char * filename;
    uint8_t dispIndex;
    unsigned char unidiskRegister_A;
    unsigned char unidiskRegister_X;
    unsigned char unidiskRegister_Y;
    unsigned char unidiskRegister_P;

} prodosPartition_t;

void SmartPortWrReqIRQ();
void SmartPortPhaseIRQ();
void SmartPortReceiveDataIRQ();
void SmartPortSendDataIRQ();
void SmartPortMainLoop();
void SmartPortInit();

void assertAck();
void deAssertAck();

char * SmartPortFindImage(char * pattern);
enum STATUS SmartPortMountImage( prodosPartition_t *d, char * filename );

void setWPProtectPort(uint8_t direction);

enum STATUS mountProdosPartition(char * filename,int partition);



#endif