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

void encodeDataPacket (unsigned char source);
void encodeExtendedDataPacket (unsigned char source);
int decodeDataPacket (void);

void encodeReplyPacket(unsigned char source,unsigned char type,unsigned char aux, unsigned char respCode);

void encodeStatusReplyPacket (prodosPartition_t d);
void encodeExtendedStatusReplyPacket (prodosPartition_t d);

void encodeStatusDibReplyPacket (prodosPartition_t d);
void encodeExtendedStatusDibReplyPacket (prodosPartition_t d);

enum STATUS verifyCmdpktChecksum(void);
void print_packet (unsigned char* data, int bytes);
int packet_length (void);

#endif