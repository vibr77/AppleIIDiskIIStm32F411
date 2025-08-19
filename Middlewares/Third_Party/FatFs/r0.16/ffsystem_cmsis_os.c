/**
  ******************************************************************************
  * @file    ffsystem_cmsis_os.c
  * @author  MCD Application Team
  * @brief   ffsystem cmsis_os functions implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (C) 2018, ChaN, all right reserved
  * Copyright (c) 2023 STMicroelectronics
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes -------------------------------------------------------------*/
#include "ff.h"

#if FF_USE_LFN == 3
#include "FreeRTOS.h"
#endif

#if FF_FS_REENTRANT
#include "cmsis_os2.h"
#endif

/* Dynamic memory allocation */
#if FF_USE_LFN == 3

/*------------------------------------------------------------------------*/
/* Allocate/Free a Memory Block                                           */
/*------------------------------------------------------------------------*/

void* ff_memalloc ( /* Returns pointer to the allocated memory block (null if not enough core) */
    UINT msize      /* Number of bytes to allocate */
)
{
    return (void *) pvPortMalloc((size_t)msize);    /* Allocate a new memory block */
}


/*------------------------------------------------------------------------*/
/* Free a memory block                                                    */
/*------------------------------------------------------------------------*/

void ff_memfree (
    void* mblock    /* Pointer to the memory block to free (no effect if null) */
)
{
    vPortFree(mblock);  /* Free the memory block */
}

#endif


/* Mutal exclusion */
#if FF_FS_REENTRANT

/* Definition table of Mutex */
static osMutexId_t Mutex[FF_VOLUMES + 1];  /* Table of mutex ID */

/*------------------------------------------------------------------------*/
/* Create a Mutex                                                         */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount function to create a new mutex
/  or semaphore for the volume. When a 0 is returned, the f_mount function
/  fails with FR_INT_ERR.
*/

int ff_mutex_create (   /* Returns 1:Function succeeded or 0:Could not create the mutex */
    int vol             /* Mutex ID: Volume mutex (0 to FF_VOLUMES - 1) or system mutex (FF_VOLUMES) */
)
{
  int ret;
  const osMutexAttr_t attr = {
    "FatFsMutex",
    osMutexRecursive,
    NULL,
    0U
  };

  Mutex[vol] = osMutexNew(&attr);
  if (Mutex[vol] != NULL)
  {
    ret = 1;
  }
  else
  {
    ret = 0;
  }

  return ret;
}


/*------------------------------------------------------------------------*/
/* Delete a Mutex                                                         */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount function to delete a mutex or
/  semaphore of the volume created with ff_mutex_create function.
*/

void ff_mutex_delete (  /* Returns 1:Function succeeded or 0:Could not delete due to an error */
    int vol             /* Mutex ID: Volume mutex (0 to FF_VOLUMES - 1) or system mutex (FF_VOLUMES) */
)
{
    osMutexDelete(Mutex[vol]);
}


/*------------------------------------------------------------------------*/
/* Request a Grant to Access the Volume                                   */
/*------------------------------------------------------------------------*/
/* This function is called on enter file functions to lock the volume.
/  When a 0 is returned, the file function fails with FR_TIMEOUT.
*/

int ff_mutex_take ( /* Returns 1:Succeeded or 0:Timeout */
    int vol         /* Mutex ID: Volume mutex (0 to FF_VOLUMES - 1) or system mutex (FF_VOLUMES) */
)
{
  int ret;

  if(osMutexAcquire(Mutex[vol], FF_FS_TIMEOUT) == osOK)
  {
    ret = 1;
  }
  else
  {
    ret = 0;
  }

  return ret;
}

/*------------------------------------------------------------------------*/
/* Release a Grant to Access the Volume                                   */
/*------------------------------------------------------------------------*/
/* This function is called on leave file functions to unlock the volume.
*/

void ff_mutex_give (
    int vol         /* Mutex ID: Volume mutex (0 to FF_VOLUMES - 1) or system mutex (FF_VOLUMES) */
)
{
  osMutexRelease(Mutex[vol]);
}
#endif
