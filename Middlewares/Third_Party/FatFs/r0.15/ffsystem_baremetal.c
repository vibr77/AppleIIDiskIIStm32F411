/**
  ******************************************************************************
  * @file    ffsystem_baremetal.c
  * @author  MCD Application Team
  * @brief   ffsystem baremetal functions implementation
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
#include <stdlib.h>

#if FF_FS_REENTRANT
#error "The flag FF_FS_REENTRANT should be set to 0"
#endif

/* Dynamic memory allocation */
#if FF_USE_LFN == 3

/*------------------------------------------------------------------------*/
/* Allocate a memory block                                                */
/*------------------------------------------------------------------------*/

void* ff_memalloc ( /* Returns pointer to the allocated memory block (null if not enough core) */
    UINT msize      /* Number of bytes to allocate */
)
{
    return malloc((size_t)msize);   /* Allocate a new memory block */
}


void ff_memfree (
    void* mblock    /* Pointer to the memory block to free (no effect if null) */
)
{
    free(mblock);   /* Free the memory block */
}

#endif
