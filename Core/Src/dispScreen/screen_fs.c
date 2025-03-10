#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_fs.h"
#include "display.h"

/**
 * 
 * FILESYSTEM SCREEN
 * 
 */


// EXTERN DEFINITION 

extern enum page currentPage;
extern list_t * dirChainedList;

extern char currentFullPath[MAX_FULLPATH_LENGTH]; 
extern char currentPath[MAX_PATH_LENGTH];
extern char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];  // fullpath from root image filename
extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];      // fullpath from root image filename

extern enum action nextAction;
extern uint8_t emulationType;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);



static void pBtnUpFsScr();
static void pBtnDownFsScr();
static void pBtnEntrFsScr();
static void pBtnRetFsScr();


uint8_t selectedIndx=0;
uint8_t currentClistPos=0;

uint8_t dispSelectedIndx;                        // Which line is selected

typedef struct FSDISPITEM{
  uint8_t displayPos;
  uint8_t update;
  uint8_t selected;
  uint8_t type;
  uint8_t icon;
  uint8_t status;
  char title[32];
  uint8_t chainedListPosition;
}FSDISPITEM_t;

FSDISPITEM_t fsDispItem[SCREEN_MAX_LINE_ITEM];




void initFsScr(char * path){
  
    primPrepNewScreen("File Listing");

    char tmp[24];
   
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(tmp,18,"%s",currentPath);
  #pragma GCC diagnostic pop
    displayStringAtPosition(0,6*SCREEN_LINE_HEIGHT+1,tmp);
  
    updateChainedListDisplay(0,dirChainedList);

    ptrbtnUp=pBtnUpFsScr;
    ptrbtnDown=pBtnDownFsScr;
    ptrbtnEntr=pBtnEntrFsScr;
    ptrbtnRet=pBtnRetFsScr;
    currentPage=FS;

  }

void updateChainedListDisplay(int init, list_t * lst ){
  int offset=5;
  int h_offset=10;

  char tmp[32];
  char * value;

  list_node_t *fsItem; 
  uint8_t fsIndx=0;
  uint8_t lstCount=lst->len;

  log_debug("lst_count:%d init:%d",lstCount,init);

  if (init!=-1){
    currentClistPos=init;
    dispSelectedIndx=0;
  }

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){
    
    fsDispItem[i].status=1;                   // starting with item status =0, if an error then status = -1;
    fsDispItem[i].displayPos=i;               // corresponding line on the screen
    fsDispItem[i].update=1;
    
    if (i>lstCount-1){                          // End of the list before the MAX_LINE_ITEM
      fsDispItem[i].status=0;
      continue;
    }

    fsIndx=(currentClistPos+i)%lstCount;
    fsItem=list_at(lst, fsIndx);
    if (fsDispItem[i].selected==1 && i!=dispSelectedIndx){    // It was selected (inversed, thus we need to reinverse)
        fsDispItem[i].selected=0;
    }else if (i==dispSelectedIndx){
      fsDispItem[i].selected=1;                // first item of the list is selected
      selectedIndx=fsIndx;
    }  
      
    if (fsItem!=NULL)
      fsDispItem[i].chainedListPosition=fsIndx;
    else{
      fsDispItem[i].status=0;
      continue;
    }

    value=fsItem->val;
    if (value!=NULL){
      if (value[0]=='D' && value[1]=='|'){
        fsDispItem[i].icon=0;   
        fsDispItem[i].type=0;                // 0 -> Directory
        snprintf(fsDispItem[i].title,24,"%s",value+2); 
      }else if (value[0]=='F' && value[1]=='|'){
        fsDispItem[i].icon=1; 
        fsDispItem[i].type=1;                // 1 -> file
        snprintf(fsDispItem[i].title,24,"%s",value+2);  
      }else{
        fsDispItem[i].icon=1; 
        fsDispItem[i].type=1;                // 1 -> file
        char * title=getImageNameFromFullPath(value);
        snprintf(fsDispItem[i].title,24,"%s",title);  
      }
                  
    }else{
      fsDispItem[i].status=-1;
    }
  }

  // Render Part

#ifdef A2F_MODE
  ssd1306_SetColor(Black);
  ssd1306_DrawLine(0,13,127,13);
#endif

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){

    clearLineStringAtPosition(1+i,offset);
    if (fsDispItem[i].status!=0){
      
      ssd1306_SetColor(White);
      dispIcon(1,(1+i)*SCREEN_LINE_HEIGHT+offset,fsDispItem[i].icon);

      ssd1306_SetColor(White);
      displayStringAtPosition(1+h_offset,(1+i)*SCREEN_LINE_HEIGHT+offset,fsDispItem[i].title);
      if (fsDispItem[i].selected==1){
        inverseStringAtPosition(1+i,offset);
      }
    }
  }

  ssd1306_SetColor(White);
  if (lstCount>0)
    sprintf(tmp,"%02d/%02d",selectedIndx+1,lstCount);
  else
    sprintf(tmp,"Empty");
  displayStringAtPosition(96,6*SCREEN_LINE_HEIGHT+1,tmp);

  primUpdScreen();
}
  
  
  /**
   * 
   *   FILESYSTEM PAGE
   * 
   */
  
  static void pBtnUpFsScr(){
  
  uint8_t lstCount=dirChainedList->len;
  
  if (lstCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==0){
    dispSelectedIndx=lstCount-1;
  }else if (dispSelectedIndx==0)
    if (currentClistPos==0)
      currentClistPos=lstCount-1;
    else
      currentClistPos=(currentClistPos-1)%lstCount;
  else{
    dispSelectedIndx--;
  }
  log_debug("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
  
  updateChainedListDisplay(-1,dirChainedList);
}
  
static void pBtnDownFsScr(){ 
  
  uint8_t lstCount=dirChainedList->len;
  
  if (lstCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==lstCount-1){
    dispSelectedIndx=0;
  }else if (dispSelectedIndx==(SCREEN_MAX_LINE_ITEM-1))
    currentClistPos=(currentClistPos+1)%lstCount;
  else{
    dispSelectedIndx++;
  }
  log_debug("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
  
  updateChainedListDisplay(-1,dirChainedList);
}

static void pBtnRetFsScr(){
  
  if (currentFullPath[0]==0x0){
    // we are at the ROOT -> Disp General Menu
    switchPage(MENU,NULL);
    return;
  }

  int len=strlen(currentFullPath);
  for (int i=len-1;i!=-1;i--){
    if (currentFullPath[i]=='/'){
      snprintf(currentPath,MAX_PATH_LENGTH,"%s",currentFullPath+i);
      currentFullPath[i]=0x0;
      dispSelectedIndx=0;
      currentClistPos=0;
      nextAction=FSDISP;
      break;
    }
    if (i==0)
      currentFullPath[0]=0x0;
  }

}
  
static void pBtnEntrFsScr(){
  // Warning Interrupt can not trigger Filesystem action otherwise deadlock can occured !!!
  
  if (nextAction==FSDISP)     // we need to wait for the previous action to complete (a deadlock might have happened)
    return;

  list_node_t *pItem=NULL;
  char selItem[MAX_FILENAME_LENGTH];
  pItem=list_at(dirChainedList, selectedIndx);
  sprintf(selItem,"%s",(char*)pItem->val);
  
  int len=strlen(currentFullPath);
  
  if (selItem[2]=='.' && selItem[3]==0x0){                      // selectedItem is [CurrentDir] // DO NOTHING FUTURE USE
    log_info("currentDir selItem");
    switchPage(DIROPTION,NULL);
  }else if (selItem[2]=='.' && selItem[3]=='.'){                // selectedItem is [UpDir];
    for (int i=len-1;i!=-1;i--){
      if (currentFullPath[i]=='/'){
        snprintf(currentPath,MAX_PATH_LENGTH,"%s",currentFullPath+i);
        currentFullPath[i]=0x0;
        dispSelectedIndx=0;
        currentClistPos=0;
        nextAction=FSDISP;
        break;
      }
      if (i==0)
        currentFullPath[0]=0x0;
    }
  }else if (selItem[0]=='D' && selItem[1]=='|'){        // selectedItem is a directory
    sprintf(currentFullPath+len,"/%s",selItem+2);
    dispSelectedIndx=0;
    currentClistPos=0;
    nextAction=FSDISP;
    
  }else{
    if (emulationType==DISKII){
      sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,selItem+2);
      switchPage(MOUNT,selItem+2);
    }else if (emulationType==SMARTPORTHD){
      sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,selItem+2);
      switchPage(MOUNT,selItem+2);
    }
  }
  log_debug("result |%s|",currentFullPath);
  
}
