#ifndef emul_diskii_h
#define emul_diskii_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void DiskIIPhaseIRQ();

void DiskIIReceiveDataIRQ();
void DiskIISendDataIRQ();
void DiskIIWrReqIRQ();
void DiskIISelectIRQ();
int DiskIIDeviceEnableIRQ(uint16_t GPIO_Pin);


enum STATUS DiskIIUnmountImage();
enum STATUS DiskIIMountImagefile(char * filename);
void DiskIIInit();
void DiskIIMainLoop();



#endif
