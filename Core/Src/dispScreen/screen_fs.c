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
extern char sTmp[256];
extern uint8_t iTmp;
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

static void pBtnUpLabelInputScr();
static void pBtnDownLabelInputScr();
static void pBtnRetLabelInputScr();
static void pBtnEntrLabelInputScr();

static void pBtnUpSelectDiskImageFormatScr();
static void pBtnDownSelectDiskImageFormatScr();
static void pBtnEntrSelectDiskImageFormatScr();
static void pBtnRetSelectDiskImageFormatScr();


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
  char title[MAX_FILENAME_LENGTH];
  uint8_t chainedListPosition;
}FSDISPITEM_t;

FSDISPITEM_t fsDispItem[SCREEN_MAX_LINE_ITEM];


uint8_t selectedDiskImageFormat=0;

extern SSD1306_MARQUEE_t marqueeObj;
 
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

    

    //marqueeObj.text=(char *)&demoText;
log_info("initFsScr - selectedIndx:%d",selectedIndx);

marqueeObj.x=0;
marqueeObj.y=0;
marqueeObj.visibleWidth=19*6;
marqueeObj.Color=White;
marqueeObj.inverted=1;

ssd1306_marqueeInit(&marqueeObj,Font_6x8);
memset(marqueeObj.renderBuffer,0x00,128);

//ssd1306_marquee_build_text_bitmap(&marqueeObj);
//ssd1306_marquee_display(&marqueeObj);
//ssd1306_UpdateScreen();
/*while(1){
  ssd1306_marquee_display(&marqueeObj);
  ssd1306_UpdateScreen();
  HAL_Delay(15);
}; */

  }

void updateChainedListDisplay(int init, list_t * lst ){
  int offset=5;
  int h_offset=10;

  char tmp[32];
  char * value;

  list_node_t *fsItem; 
  uint8_t fsIndx=0;
  uint8_t lstCount=lst->len;

  
  if (init!=-1){
    currentClistPos=init;
    dispSelectedIndx=-1;
  }

  log_debug("lst_count:%d init:%d dispSelected:%d",lstCount,init,dispSelectedIndx);

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){
    
    fsDispItem[i].status=1;                                   // starting with item status =0, if an error then status = -1;
    fsDispItem[i].displayPos=i;                               // corresponding line on the screen
    fsDispItem[i].update=1;
    
    if (i>lstCount-1){                                        // End of the list before the MAX_LINE_ITEM
      fsDispItem[i].status=0;
      continue;
    }

    fsIndx=(currentClistPos+i)%lstCount;
    fsItem=list_at(lst, fsIndx);
    if (fsDispItem[i].selected==1 && i!=dispSelectedIndx){    // It was selected (inversed, thus we need to reinverse)
        fsDispItem[i].selected=0;
    }else if (i==dispSelectedIndx){
      fsDispItem[i].selected=1;                               // first item of the list is selected
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
        snprintf(fsDispItem[i].title,MAX_FILENAME_LENGTH,"%s",value+2); 
      }else if (value[0]=='F' && value[1]=='|'){
        fsDispItem[i].icon=1; 
        fsDispItem[i].type=1;                // 1 -> file
        snprintf(fsDispItem[i].title,MAX_FILENAME_LENGTH,"%s",value+2);  
      }else{
        fsDispItem[i].icon=1; 
        fsDispItem[i].type=1;                // 1 -> file
        char * title=getImageNameFromFullPath(value);
        snprintf(fsDispItem[i].title,MAX_FILENAME_LENGTH,"%s",title);  
      }
                  
    }else{
      fsDispItem[i].status=-1;
    }
  }

  // Render Part

#ifdef A2F_MODE
  ssd1306_SetColor(Black); 
  ssd1306_DrawLine(0,13,127,13); // Delete remaining top line when scrolling up and down again
#endif

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){

    clearLineStringAtPosition(1+i,offset);
    if (fsDispItem[i].status!=0){
      
      ssd1306_SetColor(White);
      dispIcon(1,(1+i)*SCREEN_LINE_HEIGHT+offset,fsDispItem[i].icon);

      ssd1306_SetColor(White);
      displayStringAtPosition(1+h_offset,(1+i)*SCREEN_LINE_HEIGHT+offset,fsDispItem[i].title);
      if (fsDispItem[i].selected==1){
        marqueeObj.x=1+h_offset;
        marqueeObj.y=(1+i)*SCREEN_LINE_HEIGHT+offset;
        //marqueeObj.inverted=1;
        marqueeObj.fullLineInverted=1;
        marqueeObj.inverted=0;
        marqueeObj.Color=White;
        marqueeObj.scrollType=1;
        marqueeObj.scrollMaxBF=1;
        marqueeObj.text=fsDispItem[i].title;
        memset(marqueeObj.renderBuffer,0x00,128);
        if (!marqueeObj.initialized){
          ssd1306_marqueeInit(&marqueeObj,Font_6x8);
        }
        ssd1306_marquee_build_text_bitmap(&marqueeObj);
        ssd1306_marquee_display(&marqueeObj);
        marquee_refresh_ms(500);
        //inverseStringAtPosition(1+i,offset);
        
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
  marquee_stop();
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
  marquee_stop();
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
    if (emulationType==DISKII ||emulationType==SMARTLOADER){
      sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,selItem+2);
      switchPage(MOUNT,selItem+2);
    }else if (emulationType==SMARTPORTHD){
      sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,selItem+2);
      switchPage(SMARTPORT_MOUNT,tmpFullPathImageFilename);
    }
  }
  log_debug("result |%s|",currentFullPath);
  
}

rollingWidget_t labelRw;

void initLabelInputScr(){
  primPrepNewScreen("Image Name");
    
  listItem_t * lblItem;
  
  labelRw.lst=list_new();

  labelRw.currentClistPos=0;
  labelRw.dispLine=3;
  labelRw.hOffset=1;
  labelRw.vOffset=1;
  labelRw.labelMaxLen=10;

  sprintf(labelRw.label,"NewImage");

  const char * lstValues[]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","R","S","T","U","V","W","X","Y","Z","0","1","2","3","4","5","6","7","8","9","[OK]",NULL};
  
  uint8_t i=0;
  while(lstValues[i]!=NULL){
    
    lblItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (lblItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }

    sprintf(lblItem->title,"%s",lstValues[i]);
    
    lblItem->type=0;
    lblItem->triggerfunction=NULL;
    lblItem->ival=lstValues[i][0];

    list_rpush(labelRw.lst, list_node_new(lblItem));
    i++;
  }

  ptrbtnUp=pBtnUpLabelInputScr;
  ptrbtnDown=pBtnDownLabelInputScr;
  ptrbtnEntr=pBtnEntrLabelInputScr;
  ptrbtnRet=pBtnRetLabelInputScr;
  currentPage=FSLABEL;
  primUpdRollingLabelListWidget(&labelRw,0,0);
  primUpdScreen();

}

static void pBtnUpLabelInputScr(){
  primUpdRollingLabelListWidget(&labelRw,-1,-1);
}

static void pBtnDownLabelInputScr(){
  primUpdRollingLabelListWidget(&labelRw,-1,1);
}

static void pBtnRetLabelInputScr(){
  uint8_t len=strlen(labelRw.label);
  if (len!=0){
    labelRw.label[len-1]=0x0;
    primUpdRollingLabelListWidget(&labelRw,-1,0); 
  }else{
    switchPage(FS,0);
  }
}

static void pBtnEntrLabelInputScr(){
  
  listItem_t *itm;
  itm=labelRw.currentSelectedItem->val;
  
  if (!strcmp(itm->title,"[OK]")){
    
    sprintf(sTmp,"%s",labelRw.label);
    iTmp=selectedDiskImageFormat;
    nextAction=MKIMG;

    primPrepNewScreen("Create disk"); 
    displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT+1,"Work in progress");
    primUpdScreen();
    return; 
  }

  uint8_t len=strlen(labelRw.label);
  if (len<32-2){
    sprintf(labelRw.label+len,"%s",itm->title);
  }
  primUpdRollingLabelListWidget(&labelRw,-1,0);

}



listWidget_t diskImageLw;

void initSelectDiskImageFormatScr(){                                                            // TODO MEMORY MGT ON LEAVING SCREEN TO CLEAN UP LST (FOR ALL SCREEN)

  const char * diskImageLabel[]={"DSK 140k","NIB 140k","WOZ 140k","PRODOS 140k","PRODOS 800k","PRODOS 32M","2MG 400k","2MG 800k",NULL};
  
  listItem_t * diskItem;
  
  diskImageLw.currentClistPos=0;
  diskImageLw.dispLineSelected=0;
  diskImageLw.dispMaxNumLine=4;
  diskImageLw.dispStartLine=0;
  diskImageLw.hOffset=1;
  diskImageLw.vOffset=5;

  diskImageLw.lst=list_new();
  uint8_t i=0;
  while(diskImageLabel[i]!=NULL){
    diskItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (diskItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    
    sprintf(diskItem->title,"%s",diskImageLabel[i]);
    diskItem->type=0;
    diskItem->triggerfunction=NULL;
    diskItem->ival=i;
    diskItem->icon=0;

    list_rpush(diskImageLw.lst, list_node_new(diskItem));
    i++;
  
  }
  
  primPrepNewScreen("Select format");    
  primUpdListWidget(&diskImageLw,0,0);
  
  primUpdScreen();

  ptrbtnUp=pBtnUpSelectDiskImageFormatScr;
  ptrbtnDown=pBtnDownSelectDiskImageFormatScr;
  ptrbtnEntr=pBtnEntrSelectDiskImageFormatScr;
  ptrbtnRet=pBtnRetSelectDiskImageFormatScr;
  currentPage=FSSELECTIMAGE;
  return;
}

static void pBtnUpSelectDiskImageFormatScr(){
  primUpdListWidget(&diskImageLw,-1,-1);
}

static void pBtnDownSelectDiskImageFormatScr(){
  primUpdListWidget(&diskImageLw,-1,1);
}

static void pBtnRetSelectDiskImageFormatScr(){
  switchPage(FS,0);
}

static void pBtnEntrSelectDiskImageFormatScr(){
  selectedDiskImageFormat=diskImageLw.lstSelectIndx;
  switchPage(FSLABEL,0);
}

