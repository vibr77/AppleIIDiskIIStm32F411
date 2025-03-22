#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fatfs.h"

#include "display.h"
#include "ssd1306.h"
#include "defines.h"
#include "fonts.h"
#include "main.h"
#include "log.h"

#ifdef A2F_MODE
#include "a2f.h"
#endif

extern uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

extern FATFS fs;                                            // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern volatile enum FS_STATUS fsState; 

enum page currentPage;

uint8_t scrI=0;



char * getImageNameFromFullPath(char * fullPathImageName){
  
  if (fullPathImageName==NULL)
    return NULL;

  int len=strlen(fullPathImageName);
  int i=0;

  for (i=len-1;i!=0;i--){
    if (fullPathImageName[i]=='/')
      break;
  }

  return fullPathImageName+i+1;

}

/*
*     PAGE SWITCHER MAIN FUNCTION
*/

enum STATUS switchPage(enum page newPage,void * arg){

// Manage with page to display and the attach function to button Interrupt  
  
  switch(newPage){
    
    case DIROPTION:
      initDirOptionScr(0);
      break;
    
    case MAKEFS:
      initMakeFsScr();
      break;

    

    case SMARTPORT:
      initSmartPortHDScr();
      updateImageSmartPortHD();
      break;

    case SMARTPORT_MOUNT:
      initSmartportMountImageScr((char *)arg);
      break;

    case SMARTPORT_IMAGEOPTION:
      initSmartPortHDImageOptionScr();
      break;
    
    case IMAGEMENU:
      initDiskIIImageMenuScr(0);
      break;

    case SETTINGS:
      initSettingsScr(0);
      break;

    case EMULATIONTYPE:
      initSettingsEmulationScr(0);
      break;

    case FAVORITES:
      initFavoritesScr();
      break;

    case FS:
      initFsScr(arg);
      break;

    case MENU:
      initMainMenuScr(0);
      break;

    case DISKIIIMAGE:
      initDiskIIImageScr(arg,0);
      break;
    
    case MOUNT:
      initMountImageScr((char*)arg);
      break;
    
    default:
      return RET_ERR;
      break;
  }
  return RET_OK;
}


void nothing(){
  return;
  //__NOP();
}

void primUpdListWidget(listWidget_t * lw,int8_t init, int8_t direction){

  /**
   * primUpdListWidget Manage the update of a list of item on the screen
   * lw contains the properties of the listWidget
   * init !=-1 enable to jump start to an item
   * direction: indicate the direction to go: -1 Backward in the list, 0 Nothing, +1 Forward
   */
 
  uint8_t lstIndx=0;
  if (lw){
    lw->lstItemCount=lw->lst->len;
  }
  // Step 1 Process the direction 

  switch(direction){
    
    case 1:
      log_info("listWidget going forward item cnt %d dispLineSelected:%d",lw->lstItemCount,lw->dispLineSelected);
      if (lw->lstItemCount<=lw->dispMaxNumLine && lw->dispLineSelected==lw->lstItemCount-1){
        lw->dispLineSelected=0;
      }else if (lw->dispLineSelected==(lw->dispMaxNumLine-1))
        lw->currentClistPos=(lw->currentClistPos+1)%lw->lstItemCount;
      else{
        lw->dispLineSelected++;
      }
      break;
    
    case -1:
      log_info("listWidget going backward item cnt %d dispLineSelected:%d",lw->lstItemCount,lw->dispLineSelected);
      if (lw->lstItemCount<=lw->dispMaxNumLine && lw->dispLineSelected==0){
        lw->dispLineSelected=lw->lstItemCount-1;
      }else if (lw->dispLineSelected==0)
        if (lw->currentClistPos==0)
          lw->currentClistPos=lw->lstItemCount-1;
        else
          lw->currentClistPos=(lw->currentClistPos-1)%lw->lstItemCount;
      else{
        lw->dispLineSelected--;
      }
      break;

    case 0:
      log_info("listwidget do nothing direction==0");
      break;

    default:
      log_warn("listwidget unsupported direction");
      break;
  }

  log_info("new currentClistPos:%d, dispLineSelected:%d",lw->currentClistPos,lw->dispLineSelected);
  log_info("lst_count:%d init:%d",lw->lstItemCount,init);

  if (init!=-1){
    lw->currentClistPos=(init-lw->dispLineSelected)%lw->lstItemCount;
  }

  // STEP 2 BUILD THE dispItem Array 

  for (int i=0;i<lw->dispMaxNumLine;i++){
    lw->dispItem[i].status=1;                   // starting with item status =0, if an error then status = -1;
    lw->dispItem[i].displayPos=i;               // corresponding line on the screen
    lw->dispItem[i].update=1;
    
    if (i>lw->lstItemCount-1){                          // End of the list before the MAX_LINE_ITEM
      lw->dispItem[i].status=0;
      continue;
    }

    lstIndx=(lw->currentClistPos+i)%lw->lstItemCount;
    lw->dispItem[i].lstItem=list_at(lw->lst, lstIndx);
    
    if (lw->dispItem[i].lstItem!=NULL){
      if (lw->dispItem[i].selected==1 && i!=lw->dispLineSelected){    // It was selected (inversed, thus we need to reinverse)
        lw->dispItem[i].selected=0;
      }else if (i==lw->dispLineSelected){
        lw->dispItem[i].selected=1;                                  // first item of the list is selected
        lw->currentSelectedItem=lw->dispItem[i].lstItem;
      }  
    }else{
      lw->dispItem[i].status=0;
      continue;
    }
  }

  // Step 3 Render the list on the screen 

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){

    clearLineStringAtPosition(lw->dispStartLine+1+i,lw->vOffset);
    if (lw->dispItem[i].status!=0){
      listItem_t *itm=lw->dispItem[i].lstItem->val;
      
      ssd1306_SetColor(White);
      dispIcon(1,lw->dispStartLine+(1+i)*SCREEN_LINE_HEIGHT+lw->vOffset,itm->icon);
      displayStringAtPosition(1+lw->hOffset,lw->dispStartLine+(1+i)*SCREEN_LINE_HEIGHT+lw->vOffset,itm->title);
      
      if (itm->type==1){
        if (itm->ival==1)
          dispIcon(118,lw->dispStartLine+(1+i)*SCREEN_LINE_HEIGHT+lw->vOffset,7); // 7 FULL
        else
          dispIcon(118,lw->dispStartLine+(1+i)*SCREEN_LINE_HEIGHT+lw->vOffset,6); // 6 EMPTY
      }else if (itm->type==2){
        char sztmp[5];
        snprintf(sztmp,4,"%d",itm->ival);
        displayStringAtPosition(118,lw->dispStartLine+(1+i)*SCREEN_LINE_HEIGHT+lw->vOffset,sztmp); 
      }

      if (lw->dispItem[i].selected==1){
        inverseStringAtPosition(lw->dispStartLine+1+i,lw->vOffset);
      }
    }
  }

  // Step 4 FInally display the screen
  primUpdScreen();
}

/**
 * 
 * DISPLAY PRIMITIVE
 * 
 */

// Icon converter BMP ->  https://mischianti.org/ssd1306-oled-display-draw-images-splash-and-animations-2/
// <!> Generate Vertical 1 bit per pixel


void primPrepNewScreen(char * szTitle){
  
  if (szTitle==NULL){
    log_error("Title should not be NULL");
  }

  char szTmp[24];
  snprintf(szTmp,24,"%s",szTitle);

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,szTmp);
  ssd1306_DrawLine(0,8,127,8);
  ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);
  
  return;
}

void primUpdScreen(){
  ssd1306_UpdateScreen();
#if SCREENSHOT==1
  makeScreenShot(scrI);
  scrI++;
#endif
}


void dispIcon32x32(int x,int y,uint8_t indx){
  const unsigned char icon32x32[]={
    // 'floppy', 32x32px
    0x00, 0x00, 0xfc, 0x04, 0xf4, 0x54, 0x14, 0xf4, 0x14, 0x54, 0xf4, 0xf4, 0xf4, 0x04, 0x04, 0x04, 
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x84, 0x84, 0xfc, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xc0, 0x20, 0x10, 
    0x10, 0x10, 0x20, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x04, 0xfc, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0xc4, 
    0x24, 0xc4, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0x3f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x27, 
    0x28, 0x27, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3f, 0x00, 0x00,
    // 'smartport', 32x32px,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0x01, 0x01, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 
    0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x59, 0x59, 0x01, 0x01, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0x80, 0x80, 0xa4, 0xbc, 0x80, 0xbc, 0xa4, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x98, 0x98, 0x98, 0x98, 0x80, 0x80, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  ssd1306_DrawBitmap(x,y,32,32,icon32x32+128*indx);
}

void dispIcon24x24(int x,int y,uint8_t indx){
  const unsigned char icon24x24[]={
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xf0, 0x3c, 0x3c, 0xf0, 0xc0, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xf0, 0x3c, 
    0x0f, 0x01, 0x00, 0xfc, 0xfc, 0x00, 0x03, 0x0f, 0x3c, 0xf0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x1c, 0x3f, 0x33, 0x30, 0x30, 0x30, 0x30, 0x30, 0x36, 0x36, 0x30, 0x30, 0x30, 
    0x30, 0x30, 0x33, 0x3f, 0x1c, 0x00, 0x00, 0x00,
    // 'warning_v2', 24x24px
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0x30, 0x18, 0x18, 0x30, 0xe0, 0x80, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0x38, 0x0e, 
    0x03, 0x00, 0x00, 0x7e, 0x7e, 0x00, 0x00, 0x03, 0x0e, 0x38, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x18, 0x1e, 0x33, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x33, 0x30, 0x30, 0x30, 
    0x30, 0x30, 0x30, 0x33, 0x1e, 0x18, 0x00, 0x00
  };

  ssd1306_DrawBitmap(x,y,24,24,icon24x24+72*indx);
}

void dispIcon12x12(int x,int y,int indx){
  const unsigned char icon12x12[]={
    // 'fav_full_12x12', 12x12px
    0x00, 0x70, 0xf8, 0xfc, 0xfc, 0xf8, 0xf8, 0xfc, 0xfc, 0xf8, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 
    0x03, 0x07, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00,
    // 'fav_empty_12x12', 12x12px
    0x00, 0x70, 0xf8, 0x9c, 0x0c, 0x08, 0x08, 0x0c, 0x9c, 0xf8, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 
    0x03, 0x06, 0x06, 0x03, 0x01, 0x00, 0x00, 0x00
  };

  ssd1306_DrawBitmap(x,y,12,12,icon12x12+24*indx);
}

void dispIcon(int x,int y,int indx){
  const unsigned char icon_set[]  = {
    0x00, 0x7e, 0x7e, 0x7e, 0x7c, 0x7c, 0x7c, 0x00,   // indx=0   'folderb',      8x8px 
    0x00, 0x7e, 0x42, 0x46, 0x4a, 0x7e, 0x00, 0x00,   // indx=1   'file2',        8x8px
    0x08, 0xd8, 0x7c, 0x3f, 0x3f, 0x7c, 0xd8, 0x08,   // indx=2   'star',         8x8px
    0x00, 0x6c, 0x7c, 0x3e, 0x7c, 0x7c, 0x10, 0x00,   // indx=3   'config',       8x8px
    0x00, 0x60, 0x68, 0x1c, 0x3e, 0x1e, 0x0e, 0x00,   // indx=4   'launch',       8x8px
    0x00, 0x1c, 0x3e, 0x7c, 0x7c, 0x3e, 0x1c, 0x00,   // indx=5   'favorite',     8x8px
    0x00, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x00,   // indx=6   'chkbox_empty", 8x8px
    0x00, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x00,   // indx=7   'chkbox_full",  8x8px
    0x00, 0x04, 0x7c, 0x46, 0x46, 0x7c, 0x04, 0x00,   // indx=8   'trash',        8x8px
    0x18, 0x18, 0x24, 0x42, 0x7e, 0x00, 0x24, 0x18,   // indx=9   'sound',        8x8px
    0x00, 0x66, 0x42, 0x18, 0x18, 0x42, 0x66, 0x00,   // indx=10  'boot',         8x8px
    0xaa, 0xee, 0xea, 0xba, 0xba, 0xea, 0xee, 0xaa,   // indx=11  'makefs',       8x8px
    0x3c, 0x4e, 0x8f, 0x8f, 0xf1, 0xf1, 0x72, 0x3c,   // indx=12  'Harvey1',      8x8px
    0x3c, 0x72, 0xf1, 0xf1, 0x8f, 0x8f, 0x4e, 0x3c    // indx=13  'Harvey2',      8x8px
  };

  ssd1306_DrawBitmap(x,y,8,8,icon_set+8*indx);
}

void clearScreen(){
  ssd1306_SetColor(Black);

#ifdef A2F_MODE
  ssd1306_FillRect(0,0,128,64);
#else
  ssd1306_FillRect(0,0,127,63);
#endif

}

void displayStringAtPosition(int x,int y,char * str){
  ssd1306_SetCursor(x,y);
  ssd1306_WriteString(str,Font_6x8);
}

void inverseStringAtPosition(int lineNumber,int offset){
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(0,lineNumber*SCREEN_LINE_HEIGHT-1+offset,127,9);
}

void clearLineStringAtPosition(int lineNumber,int offset){
  ssd1306_SetColor(Black);
  ssd1306_FillRect(0,lineNumber*SCREEN_LINE_HEIGHT+offset,127,9);
}

enum STATUS makeScreenShot(uint8_t screenShotIndex){

#if SCREENSHOT==0
  return RET_OK;
#endif

  char filename[32];
  FIL fil; 		                                                    //File handle
  FRESULT fres;                                                   //Result after operations

  sprintf(filename,"scr_%03d.scr",screenShotIndex);
                 
  HAL_NVIC_EnableIRQ(SDIO_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

  while(fsState!=READY){};
  fsState=BUSY;
  
  fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  
  if (fres != FR_OK){
	  log_error("f_open error (%i)", fres);
    fsState=READY;
    HAL_NVIC_DisableIRQ(SDIO_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
    return RET_ERR;
  }
 
  UINT bytesWrote;
  UINT totalBytes=0;

  for (int i=0;i<2;i++){
    fsState=WRITING;
    fres = f_write(&fil, (unsigned char *)SSD1306_Buffer+i*512, 512, &bytesWrote);
    
    if(fres == FR_OK) {
      totalBytes+=bytesWrote;
    }else{
	    log_error("f_write error (%i)\n",fres);
      fsState=READY;
      HAL_NVIC_DisableIRQ(SDIO_IRQn);
      HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
      HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
      return RET_ERR;
    }

    while(fsState!=READY){};
  }

  log_info("screenShot: Wrote %i bytes to '%s'!\n", totalBytes,filename);
  f_close(&fil);
  fsState=READY;
  
  HAL_NVIC_DisableIRQ(SDIO_IRQn);
  HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
  HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);

  return RET_OK;
}
