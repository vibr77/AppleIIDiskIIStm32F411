
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_makefs.h"
#include "display.h"

// EXTERN DEFINITION 

extern enum page currentPage;
extern enum action nextAction;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

static void pBtnRetMakeFsScr();
static void updateToggleMakeFsScr(uint8_t i);
static void pBtnToggleOptionMakeFsScr();
static void pBtnEntrMakeFsScr();


/*
*     MAKE FS SCREEN
*/

static uint8_t toggleMakeFs=1;

void initMakeFsScr(){
  uint8_t vOffset=5;
  primPrepNewScreen("Make filesystem");
  
  displayStringAtPosition(5,1*SCREEN_LINE_HEIGHT+vOffset,"Erase the SDCARD?");
 
  displayStringAtPosition(30,3*SCREEN_LINE_HEIGHT,"YES");
  displayStringAtPosition(30,4*SCREEN_LINE_HEIGHT,"NO");
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,3*SCREEN_LINE_HEIGHT-1,50,9);
  primUpdScreen();

  ptrbtnUp=pBtnToggleOptionMakeFsScr;
  ptrbtnDown=pBtnToggleOptionMakeFsScr;
  ptrbtnEntr=pBtnEntrMakeFsScr;
  ptrbtnRet=pBtnRetMakeFsScr;

}

static void updateToggleMakeFsScr(uint8_t i){
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,3*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
  primUpdScreen();
}

static void pBtnToggleOptionMakeFsScr(){
  if (toggleMakeFs==1){
    toggleMakeFs=0;
    updateToggleMakeFsScr(0);
  }else{
    toggleMakeFs=1;
    updateToggleMakeFsScr(1);
  }
}

static void pBtnEntrMakeFsScr(){
  if (toggleMakeFs==0){
    switchPage(MENU,NULL);
    toggleMakeFs=1;                               // rearm toggle switch
  }else{
    //log_info("here");
    nextAction=MKFS;                            // Very important this has to be managed by the main thread and not by interrupt TODO PUT ALL ACTION IN MAIN with trigger from emulator
  }
}

static void pBtnRetMakeFsScr(){  
  switchPage(SETTINGS,NULL);
}


void initMakeFsConfirmedScr(){
 
  primPrepNewScreen("Make filesystem");

  ptrbtnUp=nothing;
  ptrbtnDown=nothing;
  ptrbtnEntr=nothing;
  ptrbtnRet=nothing;
 
  displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Please wait...");
  displayStringAtPosition(0,4*SCREEN_LINE_HEIGHT,"Formatting SDCARD");
  primUpdScreen();

  if (makeSDFS()==RET_OK){
    displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Result: OK      ");
    log_info("makeSDFS success");
  }else{
    displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Result: ERROR   ");
    log_error("makeSDFS error");
  }
  displayStringAtPosition(0,4*SCREEN_LINE_HEIGHT,"                   ");
  displayStringAtPosition(0,5*SCREEN_LINE_HEIGHT,"[ENTER] to reboot");

  
  primUpdScreen();
  ptrbtnEntr=pMakeFsSysReset;
  while(1){};
}

void pMakeFsSysReset(){
  HAL_Delay(200);                     // Important need a delay otherwise Reset does not work.
  NVIC_SystemReset();
  return;
}
