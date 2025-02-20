/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "list.h"
#include "defines.h"



/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

#ifdef A2F_MODE
// rotary encoder state
extern uint8_t rLastState;
extern uint8_t rEncoder;
#endif

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
enum action{NONE,IMG_MOUNT,FSDISP,FSMOUNT,SYSRESET,DUMP_TX,WRITE_TRK,UPDIMGDISP,PROCESS_FS_CHANGEDIR};

enum FS_STATUS{READY,READING,WRITING,BUSY,DBG};
enum EMULATION_TYPE{DISKII,  SMARTPORTHD,DISK35 };

enum STATUS{RET_OK,RET_ERR};

void EnableTiming(void);
void dumpBuf(unsigned char * buf,long memoryAddr,int len);
enum STATUS dumpBufFile(char * filename,volatile unsigned char * buffer,int length);
enum STATUS writeTrkFile(char * filename,char * buffer,uint32_t offset);

char *byte_to_binary(int x);

list_t * sortLinkedList(list_t * plst);                             // Sort the chainedList
enum STATUS walkDir(char * path);                                   // Build chainedList of Directories/Files Items

enum STATUS mountImagefile(char * filename);
enum STATUS unmountImage();
enum STATUS unlinkImageFile(char* fullpathfilename);

void processBtnInterrupt(uint16_t GPIO_Pin);



void debounceBtn(int GPIO);

void irqReadTrack();
void irqWriteTrack();
void irqWIdle();

void irqEnableSDIO();
void irqDisableSDIO();

void Custom_SD_WriteCpltCallback(void);
void Custom_SD_ReadCpltCallback(void);

void getDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count);
void setDataBlocksBareMetal(long memoryAdr,volatile unsigned char * buffer,int count);

void processDiskHeadMoveInterrupt(uint16_t GPIO_Pin);
char processDeviceEnableInterrupt(uint16_t GPIO_Pin);
char processSdEject(uint16_t GPIO_PIN);

enum STATUS makeSDFS();

enum STATUS mountImagefile(char * filename);
enum STATUS initeBeaming();

typedef struct image_info_s {
  char title[32];
  uint8_t favorite;
  uint8_t type;
  uint8_t version;
  uint8_t writeProtected;
  uint8_t synced;
  uint8_t cleaned;
  uint8_t optimalBitTiming;
} image_info_t;


/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_DOWN_Pin GPIO_PIN_13
#define BTN_DOWN_GPIO_Port GPIOC
#define BTN_DOWN_EXTI_IRQn EXTI15_10_IRQn
#define BTN_UP_Pin GPIO_PIN_14
#define BTN_UP_GPIO_Port GPIOC
#define BTN_UP_EXTI_IRQn EXTI15_10_IRQn
#define BTN_ENTR_Pin GPIO_PIN_15
#define BTN_ENTR_GPIO_Port GPIOC
#define BTN_ENTR_EXTI_IRQn EXTI15_10_IRQn
#define STEP0_Pin GPIO_PIN_0
#define STEP0_GPIO_Port GPIOA
#define STEP0_EXTI_IRQn EXTI0_IRQn
#define STEP1_Pin GPIO_PIN_1
#define STEP1_GPIO_Port GPIOA
#define STEP1_EXTI_IRQn EXTI1_IRQn
#define STEP2_Pin GPIO_PIN_2
#define STEP2_GPIO_Port GPIOA
#define STEP2_EXTI_IRQn EXTI2_IRQn
#define STEP3_Pin GPIO_PIN_3
#define STEP3_GPIO_Port GPIOA
#define STEP3_EXTI_IRQn EXTI3_IRQn
#define DEVICE_ENABLE_Pin GPIO_PIN_4
#define DEVICE_ENABLE_GPIO_Port GPIOA
#define DEVICE_ENABLE_EXTI_IRQn EXTI4_IRQn
#define WR_DATA_Pin GPIO_PIN_7
#define WR_DATA_GPIO_Port GPIOA
#define RD_DATA_Pin GPIO_PIN_0
#define RD_DATA_GPIO_Port GPIOB
#define WR_PROTECT_Pin GPIO_PIN_2
#define WR_PROTECT_GPIO_Port GPIOB
#define BTN_RET_Pin GPIO_PIN_12
#define BTN_RET_GPIO_Port GPIOB
#define BTN_RET_EXTI_IRQn EXTI15_10_IRQn
#define SD_EJECT_Pin GPIO_PIN_13
#define SD_EJECT_GPIO_Port GPIOB
#define DEBUG_Pin GPIO_PIN_10
#define DEBUG_GPIO_Port GPIOA
#define WR_REQ_Pin GPIO_PIN_9
#define WR_REQ_GPIO_Port GPIOB
#define WR_REQ_EXTI_IRQn EXTI9_5_IRQn

#ifdef A2F_MODE
#define AB_GPIO_Port GPIOB
#define AB_Pin GPIO_PIN_8
#define RE_A_Pin GPIO_PIN_13
#define RE_A_GPIO_Port GPIOA
#define RE_A_EXTI_IRQn EXTI15_10_IRQn
#define RE_B_Pin GPIO_PIN_14
#define RE_B_GPIO_Port GPIOA
#define RE_B_EXTI_IRQn EXTI15_10_IRQn
#endif

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
