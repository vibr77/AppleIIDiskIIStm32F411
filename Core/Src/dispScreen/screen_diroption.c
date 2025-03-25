#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_diroption.h"
#include "display.h"

/**
 * 
 * DIRECTORY OPTION SCREEN 
 * 
 */

// EXTERN DEFINITION 

extern enum page currentPage;
extern enum action nextAction;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

static void pBtnUpDirOptionScr();
static void pBtnDownDirOptionScr();
static void pBtnEntrDirOptionScr();
static void pBtnRetDirOptionScr();

static void pBtnUpDirOptionNewImageScr();
static void pBtnDownDirOptionNewImageScr();
static void pBtnRetDirOptionNewImageScr();
static void pBtnEntrDirOptionNewImageScr();

static uint8_t currentDirOptionItem=0;
static uint8_t mnuItemCount=0;
static uint8_t dispSelectedIndx=0;
static listItem_t mnuItem[3];

void initDirOptionScr(uint8_t i){
  
    int h_offset=10;

    mnuItemCount=3;
    dispSelectedIndx=0;
    
    if (i>=mnuItemCount)
        i=0;

    if (i<0)
        i=mnuItemCount-1;

    sprintf(mnuItem[0].title,"Refresh");
    mnuItem[0].type=1;
    mnuItem[0].icon=10;
    mnuItem[0].triggerfunction=pBtnEntrDirOptionScr;
    mnuItem[0].arg=0;
    mnuItem[0].ival=0;

    sprintf(mnuItem[1].title,"Create disk");
    mnuItem[1].type=1;
    mnuItem[1].icon=10;
    mnuItem[1].triggerfunction=pBtnEntrDirOptionScr;
    mnuItem[1].arg=1;
    mnuItem[1].ival=0;

    sprintf(mnuItem[2].title,"Info");
    mnuItem[2].type=1;
    mnuItem[2].icon=10;
    mnuItem[2].triggerfunction=pBtnEntrDirOptionScr;
    mnuItem[2].arg=2;
    mnuItem[2].ival=0;

    primPrepNewScreen("Directory options");

    for (int j=0;j<mnuItemCount;j++){
        displayStringAtPosition(1+h_offset,(1+j)*SCREEN_LINE_HEIGHT+5,mnuItem[j].title);
        dispIcon(1,(1+j)*SCREEN_LINE_HEIGHT+5,mnuItem[j].icon);
    }

    ssd1306_SetColor(Inverse);
    ssd1306_FillRect(1,(1+i)*SCREEN_LINE_HEIGHT-1+5,126,9);
    
    ssd1306_SetColor(White);
    ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);

    displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,_VERSION);
    
    currentDirOptionItem=i;

    primUpdScreen();

    ptrbtnUp=pBtnUpDirOptionScr;
    ptrbtnDown=pBtnDownDirOptionScr;
    ptrbtnEntr=pBtnEntrDirOptionScr;
    ptrbtnRet=pBtnRetDirOptionScr;
    currentPage=DIROPTION;

    return;

}

static void pBtnUpDirOptionScr(){
    currentDirOptionItem--;
    initDirOptionScr(currentDirOptionItem);
}

static void pBtnDownDirOptionScr(){
    currentDirOptionItem++;
    initDirOptionScr(currentDirOptionItem);
}

static void pBtnRetDirOptionScr(){
    switchPage(FS,NULL);
}

static void pBtnEntrDirOptionScr(){
  switch(currentDirOptionItem){
    
    case 0:                                                  
      log_info("pBtnEntrDirOptionScr option 0:Refresh");
      nextAction=FSDISP;
      break;

    case 1:                                             
        switchPage(FSSELECTIMAGE,0);
        break;

    case 2:                                                 
      log_info("option 2");
      break;

    default:
      break;
  }
  return;
}


