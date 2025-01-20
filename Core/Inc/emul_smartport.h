#ifndef emul_smartport_h
#define emul_smartport_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


typedef struct prodosPartition_s{
    FIL fil; 
    unsigned char device_id;              
    unsigned long blocks;                 
    unsigned int header_offset;           
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

void encode_data_packet (unsigned char source);
void encode_extended_data_packet (unsigned char source);
int decode_data_packet (void);

void encode_write_status_packet(unsigned char source, unsigned char status);
void encode_init_reply_packet (unsigned char source, unsigned char status);
void encode_status_reply_packet (prodosPartition_t d);

void encode_extended_status_reply_packet (prodosPartition_t d);
void encode_error_reply_packet (unsigned char source);
void encode_status_dib_reply_packet (prodosPartition_t d);
void encode_extended_status_dib_reply_packet (prodosPartition_t d);
enum STATUS verify_cmdpkt_checksum(void);
void print_packet (unsigned char* data, int bytes);
int packet_length (void);
void print_hd_info(void);
int rotate_boot (void);
bool is_ours(unsigned char source);
#endif