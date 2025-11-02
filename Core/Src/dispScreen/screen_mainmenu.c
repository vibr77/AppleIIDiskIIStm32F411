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
extern uint8_t emulationType;
extern uint8_t smartloaderEmulationType;

/*
 * 
 *  MAIN MENU SCREEN
 * 
 */


static void pBtnUpMainMenu();
static void pBtnDownMainMenu();
static void pBtnEntrMainMenu();

static void pFavorites();
static void pFS();
static void pSettings();
static void pDiskIIImage();
static void pSmartloader();
static void pSmartport();

int8_t currentMainMenuItem=0;

listWidget_t menuLw;

 void initMainMenuScr(uint8_t i){
     
    listItem_t * manuItem;
    
    menuLw.lst=list_new();
    menuLw.currentClistPos=0;
    menuLw.dispLineSelected=0;
    menuLw.dispMaxNumLine=4;
    menuLw.dispStartLine=0;
    menuLw.hOffset=1;
    menuLw.vOffset=5;
    
   if (emulationType==DISKII){
      manuItem=(listItem_t *)malloc(sizeof(listItem_t));
      if (manuItem==NULL){
          log_error("malloc error listItem_t");
          return;
      }

      sprintf(manuItem->title,"Favorites");
      manuItem->type=0;
      manuItem->icon=5;
      manuItem->triggerfunction=pFavorites;
      manuItem->ival=0;
      manuItem->arg=0;
      
      list_rpush(menuLw.lst, list_node_new(manuItem));
    }

    if (emulationType==DISKII || (emulationType==SMARTLOADER && smartloaderEmulationType==DISKII)){                                   // TODO to be tested
      manuItem=(listItem_t *)malloc(sizeof(listItem_t));
      if (manuItem==NULL){
          log_error("malloc error listItem_t");
          return;
      }
      sprintf(manuItem->title,"File manager");
      manuItem->type=0;
      manuItem->icon=0;
      manuItem->triggerfunction=pFS;
      manuItem->ival=0;
      manuItem->arg=0;
      
      list_rpush(menuLw.lst, list_node_new(manuItem));
    }

    if (emulationType==DISKII){
      manuItem=(listItem_t *)malloc(sizeof(listItem_t));
      if (manuItem==NULL){
          log_error("malloc error listItem_t");
          return;
      }
      
      sprintf(manuItem->title,"DISK II");
      manuItem->type=0;
      manuItem->icon=4;
      manuItem->triggerfunction=pDiskIIImage;
      manuItem->ival=0;
      manuItem->arg=0;
      
      list_rpush(menuLw.lst, list_node_new(manuItem));
    }

    if (emulationType==SMARTLOADER){                                                            // Callback Menu for Smartloader after browsing to file
      manuItem=(listItem_t *)malloc(sizeof(listItem_t));
      if (manuItem==NULL){
          log_error("malloc error listItem_t");
          return;
      }
      
      sprintf(manuItem->title,"SMARTLOADER");
      manuItem->type=0;
      manuItem->icon=4;
      manuItem->triggerfunction=pSmartloader;
      manuItem->ival=0;
      manuItem->arg=0;
      
      list_rpush(menuLw.lst, list_node_new(manuItem));
    }



    if (emulationType==SMARTPORTHD){
      manuItem=(listItem_t *)malloc(sizeof(listItem_t));
      if (manuItem==NULL){
          log_error("malloc error listItem_t");
          return;
      }
      
      sprintf(manuItem->title,"SMARTPORT");
      manuItem->type=0;
      manuItem->icon=4;
      manuItem->triggerfunction=pSmartport;
      manuItem->ival=0;
      manuItem->arg=0;
      
      list_rpush(menuLw.lst, list_node_new(manuItem));
    }

    manuItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (manuItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(manuItem->title,"Settings");
    manuItem->type=0;
    manuItem->icon=3;
    manuItem->triggerfunction=pSettings;
    manuItem->ival=0;
    manuItem->arg=0;
    
    list_rpush(menuLw.lst, list_node_new(manuItem));


    primPrepNewScreen("Main menu");   
    primUpdListWidget(&menuLw,0,0);

  
    primUpdScreen();
 
    ptrbtnUp=pBtnUpMainMenu;
    ptrbtnDown=pBtnDownMainMenu;
    ptrbtnEntr=pBtnEntrMainMenu;
    ptrbtnRet=nothing;
    currentPage=MENU;
 
    return;
  
  }

static void pBtnDownMainMenu(){
  primUpdListWidget(&menuLw,-1,1);
 }
 
 static void pBtnUpMainMenu(){
  primUpdListWidget(&menuLw,-1,-1);
 }
 
 static void pBtnEntrMainMenu(){
   
  listItem_t *item= menuLw.currentSelectedItem->val;
  if (item){
      log_info("selected:%s ",item->title);
      item->triggerfunction(item->arg);
  }else{
      log_error("item is null");
  }
}
 

static void pFavorites(){
  switchPage(FAVORITES,NULL);
}

static void pFS(){
  nextAction=FSDISP;
  return;
}

static void pSettings(){
  switchPage(SETTINGS,NULL);
  return;
}

static void pDiskIIImage(){
  switchPage(DISKIIIMAGE,NULL);
  return;
}

static void pSmartloader(){
  switchPage(DISKIIIMAGE,NULL);                                       // Smartloader and DiskIIImage share the same main screen
  return;
}

static void pSmartport(){
  switchPage(SMARTPORT,NULL); 
  return;
}
