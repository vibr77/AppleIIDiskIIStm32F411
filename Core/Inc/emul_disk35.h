
#ifndef emul_disk35_h
#define emul_disk35_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#define IWM_DISK35_STATUS_STEP_IN       0x0001
#define IWM_DISK35_STATUS_IO_HEAD_HI    0x0002
#define IWM_DISK35_STATUS_EJECTED       0x0008
#define IWM_DISK35_STATUS_EJECTING      0x0010
#define IWM_DISK35_STATUS_STROBE        0x8000


#define IWM_DISK35_QUERY_STEP_DIR       0x00
#define IWM_DISK35_QUERY_IO_HEAD_LOWER  0x01
#define IWM_DISK35_QUERY_DISK_IN_DRIVE  0x02
#define IWM_DISK35_QUERY_IO_HEAD_UPPER  0x03
#define IWM_DISK35_QUERY_IS_STEPPING    0x04
#define IWM_DISK35_QUERY_WRITE_PROTECT  0x06
#define IWM_DISK35_QUERY_MOTOR_ON       0x08
#define IWM_DISK35_QUERY_DOUBLE_SIDED   0x09
#define IWM_DISK35_QUERY_TRACK_0        0x0A
#define IWM_DISK35_QUERY_READ_READY     0x0B
#define IWM_DISK35_QUERY_EJECTED        0x0C
#define IWM_DISK35_QUERY_60HZ_ROTATION  0x0E
#define IWM_DISK35_QUERY_ENABLED        0x0F

#define IWM_DISK35_CTL_STEP_IN          0x00
#define IWM_DISK35_CTL_STEP_OUT         0x01
#define IWM_DISK35_CTL_EJECTED_RESET    0x03
#define IWM_DISK35_CTL_STEP_ONE         0x04
#define IWM_DISK35_CTL_MOTOR_ON         0x08
#define IWM_DISK35_CTL_MOTOR_OFF        0x09
#define IWM_DISK35_CTL_EJECT            0x0D

typedef struct disk35_s{
    uint8_t is_spindle_on;                      /**< Drive spindle running */
    uint8_t has_disk;                           /**< Has a disk in the drive */
    uint16_t status_mask_35;
} disk35_t;


void disk35StartEjecting();
#endif 