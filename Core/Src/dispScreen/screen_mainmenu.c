#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_mainmenu.h"
#include "display.h"


/**
 * 
 * MAIN MENU SCREEN
 * 
 */

// EXTERN DEFINITION 

extern enum page currentPage;
/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

extern enum action nextAction;

/*
 * 
 *  MAIN MENU SCREEN
 * 
 */


static void pBtnUpMainMenu();
static void pBtnDownMainMenu();
static void pBtnEntrMainMenu();

int8_t currentMainMenuItem=0;


 void initMainMenuScr(uint8_t i){
   
    char * menuItem[5];
    uint8_t numItems=4;
    uint8_t h_offset=10;
  
    if (i>=numItems)
      i=0;
  
    if (i<0)
      i=numItems-1;
  
    menuItem[0]="Favorites";
    menuItem[1]="File manager";
    menuItem[2]="Mounted image";
    menuItem[3]="Settings";
    
    uint8_t menuIcon[5];
    menuIcon[0]=5;
    menuIcon[1]=0;
    menuIcon[2]=4;
    menuIcon[3]=3;
  
    primPrepNewScreen("Main menu");
  
    ssd1306_SetColor(White);
    for (int j=0;j<numItems;j++){
      displayStringAtPosition(1+h_offset,(1+j)*SCREEN_LINE_HEIGHT+5,menuItem[j]);
  
      dispIcon(1,(1+j)*SCREEN_LINE_HEIGHT+5,menuIcon[j]);
    }
  
    ssd1306_SetColor(Inverse);
    ssd1306_FillRect(1,(1+i)*SCREEN_LINE_HEIGHT-1+5,126,9);
    
    ssd1306_SetColor(White);
    ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);
  
    displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,_VERSION);
  
    currentMainMenuItem=i;
  
    primUpdScreen();
 
    ptrbtnUp=pBtnUpMainMenu;
    ptrbtnDown=pBtnDownMainMenu;
    ptrbtnEntr=pBtnEntrMainMenu;
    ptrbtnRet=nothing;
    currentPage=MENU;
 
    return;
  
  }

static void pBtnDownMainMenu(){
   currentMainMenuItem++;
   initMainMenuScr(currentMainMenuItem);
 }
 
 static void pBtnUpMainMenu(){
   currentMainMenuItem--;
   initMainMenuScr(currentMainMenuItem);
 }
 
 static void pBtnEntrMainMenu(){
   switch(currentMainMenuItem){
     
     case 0:
       switchPage(FAVORITES,NULL);
       break;
 
     case 1:
      nextAction=FSDISP;
       //switchPage(FS,0x0);
       break;
 
     case 2:
       switchPage(DISKIIIMAGE,NULL);
       break;
 
     case 3:
       switchPage(SETTINGS,NULL);
       break;
 
     default:
       break;
   }
   return;
 }
 
