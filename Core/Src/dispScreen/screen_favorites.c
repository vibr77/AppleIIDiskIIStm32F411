
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_favorites.h"
#include "display.h"
#include "favorites.h"


// EXTERN DEFINITION 

extern enum page currentPage;
extern list_t * favoritesChainedList;
/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);


static void pBtnUpFavoritesScr();
static void pBtnDownFavoritesScr();
static void pBtnEntrFavoritesScr();
static void pBtnRetFavoritesScr();

listWidget_t favoritesLw;

void initFavoritesScr(){

  primPrepNewScreen("Favorites");
    

  favoritesLw.lst=favoritesChainedList;

  favoritesLw.currentClistPos=0;
  favoritesLw.dispLineSelected=0;
  favoritesLw.dispMaxNumLine=4;
  favoritesLw.dispStartLine=0;
  favoritesLw.hOffset=1;
  favoritesLw.vOffset=5;

  //updateChainedListDisplay(0, favoritesChainedList);
  primUpdListWidget(&favoritesLw,-1,0);

  ptrbtnUp=pBtnUpFavoritesScr;
  ptrbtnDown=pBtnDownFavoritesScr;
  ptrbtnEntr=pBtnEntrFavoritesScr;
  ptrbtnRet=pBtnRetFavoritesScr;
  
  currentPage=FAVORITES;
  
}

static void pBtnUpFavoritesScr(){ 
  primUpdListWidget(&favoritesLw,-1,-1);
}

static void pBtnDownFavoritesScr(){ 
  primUpdListWidget(&favoritesLw,-1,1);
}

static void pBtnRetFavoritesScr(){
  switchPage(MENU,NULL);
}

static void pBtnEntrFavoritesScr(){
  // Warning Interrupt can not trigger Filesystem action otherwise deadlock can occured !!!
  // to be reworked
  /*
  listItem_t *item= settingLw.currentSelectedItem->val;
    if (item){
        log_info("selected:%s ",item->title);
        item->triggerfunction(item->arg);
    }else{
        log_error("item is null");
    }
  
  pItem=list_at(favoritesChainedList, selectedIndx);
  sprintf(tmpFullPathImageFilename,"%s",(char*)pItem->val);
  char * imageName=getImageNameFromFullPath(tmpFullPathImageFilename);
  switchPage(MOUNT,imageName);
  */
}

