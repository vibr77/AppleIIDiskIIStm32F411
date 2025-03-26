
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
extern uint8_t emulationType;
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

void initFavoritesScr(const  char ** extFilter){

  primPrepNewScreen("Favorites");
  
  favoritesLw.lst=list_new();

  uint8_t lstCount=favoritesChainedList->len;
  listItem_t *item;
  uint8_t fileExtMatch=0;
  
  for (uint8_t i=0;i<lstCount;i++){
    fileExtMatch=0;
    item=list_at(favoritesChainedList,i)->val;
    
    int len=(int)strlen(item->cval);                                                         // Warning strlen
    uint8_t extLen=0;
    
    for (uint8_t j=0;j<MAX_EXTFILTER_ITEM;j++){
          
      if (extFilter[j]==NULL || !strcmp(extFilter[j],""))                                   // End of the list
        break;
            
      extLen=strlen(extFilter[j]);
      if (!memcmp(item->cval+(len-extLen),extFilter[j],extLen)){
        fileExtMatch=1;
        break;
      } 
    }
    if (fileExtMatch==1){
      list_rpush(favoritesLw.lst, list_node_new(item));
    } 
  }

  favoritesLw.currentClistPos=0;
  favoritesLw.dispLineSelected=0;
  favoritesLw.dispMaxNumLine=4;
  favoritesLw.dispStartLine=0;
  favoritesLw.hOffset=1;
  favoritesLw.vOffset=5;

  primUpdListWidget(&favoritesLw,0,0);

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

  extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH]; 
  listItem_t *item= favoritesLw.currentSelectedItem->val;
  if (item){
    if (emulationType==DISKII){
      sprintf(tmpFullPathImageFilename,"%s",(char*)item->cval);
      switchPage(MOUNT,item->title);
    }else if (emulationType==SMARTPORTHD){
      sprintf(tmpFullPathImageFilename,"%s",item->cval);
      switchPage(SMARTPORT_MOUNT,tmpFullPathImageFilename);
    }
  }else{
    log_error("item is null");
  }

}

