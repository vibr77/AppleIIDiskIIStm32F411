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
#include "list.h"
#include "main.h"
#include "log.h"
#include "favorites.h"
#include "configFile.h"





extern list_t * dirChainedList;
extern list_t * favoritesChainedList;
extern char currentFullPath[MAX_FULLPATH_LENGTH]; 
extern char currentPath[MAX_PATH_LENGTH];
extern char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];  // fullpath from root image filename
extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];      // fullpath from root image filename
extern unsigned char flgImageMounted;
extern uint8_t emulationType;
extern uint8_t bootImageIndex;
extern uint8_t flgSoundEffect;
extern image_info_t mountImageInfo;

extern uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

extern FATFS fs;                                            // fatfs global variable <!> do not remount witihn a function the fatfs otherwise it breaks the rest
extern long database;                                     // start of the data segment in FAT
extern int csize;
extern volatile enum FS_STATUS fsState; 

extern enum STATUS (*ptrUnmountImage)();

uint8_t scrI=0;


/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

extern enum action nextAction;

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

enum page currentPage=0;

int currentTrk=0;
int currentStatus=0;

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
    
    case MAKEFS:
      makeFsScreen();
      ptrbtnEntr=processMakeFsOption;
      ptrbtnUp=processMakeFsToggleOption;
      ptrbtnDown=processMakeFsToggleOption;
      ptrbtnRet=processBtnRet;
      currentPage=MAKEFS;
      break;

    case SMARTPORT:
      initSmartPortHD();
      ptrbtnUp=nothing;
      ptrbtnDown=nothing;
      ptrbtnEntr=nothing;
      ptrbtnRet=processSmartPortHDRetScreen;
      currentPage=SMARTPORT;
      break;

    case IMAGEMENU:
      initImageMenuScreen(0);
      ptrbtnUp=processPreviousImageMenuScreen;
      ptrbtnDown=processNextImageMenuScreen;
      ptrbtnEntr=processActiveImageMenuScreen;
      ptrbtnRet=processImageMenuScreen;
      currentPage=IMAGEMENU;
      break;

    case CONFIG:
      initConfigMenuScreen(0);
      updateConfigMenuDisplay(0);

      ptrbtnUp=processPrevConfigItem;
      ptrbtnDown=processNextConfigItem;
      ptrbtnEntr=processSelectConfigItem;
      ptrbtnRet=processReturnConfigItem;
      currentPage=CONFIG;
      break;

    case EMULATIONTYPE:
      initConfigEmulationScreen();
      updateConfigMenuDisplay(0);
      
      ptrbtnUp=processPrevConfigItem;
      ptrbtnDown=processNextConfigItem;
      ptrbtnEntr=processSelectConfigItem;
      ptrbtnRet=processReturnEmulationTypeItem;
      currentPage=EMULATIONTYPE;
      break;

    case FAVORITE:
      initFavoriteScreen();
      updateChainedListDisplay(0, favoritesChainedList);

      ptrbtnUp=processPrevFavoriteItem;
      ptrbtnDown=processNextFavoriteItem;
      ptrbtnEntr=processSelectFavoriteItem;
      ptrbtnRet=processReturnFavoriteItem;
      currentPage=FAVORITE;
      break;

    case FS:
      initFSScreen(arg);
      updateChainedListDisplay(0,dirChainedList);

      ptrbtnUp=processPrevFSItem;
      ptrbtnDown=processNextFSItem;
      ptrbtnEntr=processSelectFSItem;
      ptrbtnRet=processUpdirFSItem;
      currentPage=FS;
      break;

    case MENU:
      initMainMenuScreen(0);
      ptrbtnUp=processPreviousMainMenuScreen;
      ptrbtnDown=processNextMainMenuScreen;
      ptrbtnEntr=processActiveMainMenuScreen;
      ptrbtnRet=nothing;
      currentPage=MENU;
      break;

    case IMAGE:
      
      if (emulationType==SMARTPORTHD){
        switchPage(SMARTPORT,NULL);
        return RET_OK;
      }

      if (flgImageMounted==0){
        log_error("No image mounted");
        return RET_ERR;
      }

      initIMAGEScreen(arg,0);
      ptrbtnUp=nothing;
      ptrbtnDown=nothing;
      ptrbtnEntr=processDisplayImageMenu;
      ptrbtnRet=processBtnRet;
      currentPage=IMAGE;
      break;
    
    case MOUNT:
      mountImageScreen((char*)arg);
      ptrbtnEntr=processMountOption;
      ptrbtnUp=processToogleOption;
      ptrbtnDown=processToogleOption;
      ptrbtnRet=processBtnRet;
      currentPage=MOUNT;
      break;
    
    default:
      return RET_ERR;
      break;
  }
  return RET_OK;
}

void processDisplayImageMenu(){
  switchPage(IMAGEMENU,NULL);

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

  ssd1306_UpdateScreen();
  
  makeScreenShot(scrI);
  scrI++;
}


/**
 * 
 *   FILESYSTEM PAGE
 * 
 */

void processPrevFSItem(){
    
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

void processNextFSItem(){ 
    
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

void processUpdirFSItem(){
  
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

void processSelectFSItem(){
  // Warning Interrupt can not trigger Filesystem action otherwise deadlock can occured !!!
  
  if (nextAction==FSDISP)     // we need to wait for the previous action to complete (a deadlock might have happened)
    return;

  list_node_t *pItem=NULL;
  char selItem[MAX_FILENAME_LENGTH];
  pItem=list_at(dirChainedList, selectedIndx);
  sprintf(selItem,"%s",(char*)pItem->val);
  
  int len=strlen(currentFullPath);
  if (selItem[2]=='.' && selItem[3]=='.'){                // selectedItem is [UpDir];
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
    sprintf(tmpFullPathImageFilename,"%s/%s",currentFullPath,selItem+2);
    switchPage(MOUNT,selItem+2);
  }
  log_debug("result |%s|",currentFullPath);
  
}

void mountImageScreen(char * filename){
  
  char tmp[28];
  char tmp2[28];
  int i=0;

  if (filename==NULL)
    return;
  
  clearScreen();
  i=strlen(filename);
 
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(tmp,18,"%s",filename);
  if (i>20)
    snprintf(tmp2,23,"%s...",tmp);
  else
    snprintf(tmp2,23,"%s",filename);
  #pragma GCC diagnostic pop
  
  ssd1306_SetColor(White);
  displayStringAtPosition(5,1*SCREEN_LINE_HEIGHT,"Mounting:");
  displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,tmp2);
  
  displayStringAtPosition(30,4*SCREEN_LINE_HEIGHT,"YES");
  displayStringAtPosition(30,5*SCREEN_LINE_HEIGHT,"NO");
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

void toggleMountOption(int i){
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_FillRect(30-5,5*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

void initSplashScreen(){
  ssd1306_Init();
  
  ssd1306_FlipScreenVertically();
  
  ssd1306_Clear();
  ssd1306_SetColor(White);
  dispIcon32x32(1,15,0);
  displayStringAtPosition(35,3*SCREEN_LINE_HEIGHT,"SmartDisk ][");
  displayStringAtPosition(78,6*SCREEN_LINE_HEIGHT,_VERSION);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

/*
 * 
 *  IMAGES SCREEN
 * 
 */

enum STATUS initIMAGEScreen(char * imageName,int type){
  

  char tmp[32];
  clearScreen();
  ssd1306_SetColor(White);
  
  char CL,WP,SYN;
  displayStringAtPosition(5,1*SCREEN_LINE_HEIGHT,mountImageInfo.title);
  if (mountImageInfo.type==0)
    displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: NIC");
  else if (mountImageInfo.type==1)
    displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: WOZ");
  else if (mountImageInfo.type==2)
    displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: DSK");
  else if (mountImageInfo.type==3)
    displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: PO ");
  else
    displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: ERR ");
  
  displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT,"Track: 0");

  if (mountImageInfo.cleaned==1)
    CL='Y';
  else
    CL='N';
  
  if (mountImageInfo.writeProtected==1)
    WP='Y';
  else
    WP='N';
  
  if (mountImageInfo.synced==1)
    SYN='Y';
  else 
    SYN='N';

  sprintf(tmp,"CL:%c OT:%d",CL,mountImageInfo.optimalBitTiming);
  displayStringAtPosition(5,5*SCREEN_LINE_HEIGHT,tmp);

  sprintf(tmp,"WP:%c SYN:%c V:%d",WP,SYN,mountImageInfo.version);
  displayStringAtPosition(5,6*SCREEN_LINE_HEIGHT,tmp);
  if (mountImageInfo.favorite==1)
    dispIcon12x12(115,18,0);
  else
    dispIcon12x12(115,18,1);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

  return RET_OK;
}

uint8_t harvey_ball=0;
void updateIMAGEScreen(uint8_t status,uint8_t trk){

  if (currentPage!=IMAGE)
    return;

  if (currentTrk!=trk || status!=currentStatus){

    char tmp[32];

    if (currentTrk!=trk){
      sprintf(tmp,"TRACK: %02d",trk);
      displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT,tmp);
      currentTrk=trk;
    }
    
    if (status==0){
      sprintf(tmp,"RD");
      displayStringAtPosition(72,3*SCREEN_LINE_HEIGHT,tmp);
    }else if(status==1){
      sprintf(tmp,"WR");
      displayStringAtPosition(72,3*SCREEN_LINE_HEIGHT,tmp);
    }
    
    ssd1306_SetColor(Black);
    ssd1306_FillRect(118,30,8,8);
    ssd1306_SetColor(White);
    
    if (harvey_ball==0){
      harvey_ball=1;
      dispIcon(118,30,12);
    }else{
      harvey_ball=0;
      dispIcon(118,30,13);
    }

    ssd1306_UpdateScreen();

  }
  return;
}

void toggleAddToFavorite(){
  if (isFavorite(currentFullPathImageFilename)==0){
    log_info("add from Favorite:%s",currentFullPathImageFilename);
    if (addToFavorites(currentFullPathImageFilename)==RET_OK)
      mountImageInfo.favorite=1;
  }
  else{
    log_info("remove from Favorite:%s",currentFullPathImageFilename);
    if (removeFromFavorites(currentFullPathImageFilename)==RET_OK)
      mountImageInfo.favorite=0;
  }

  buildLstFromFavorites();
  saveConfigFile();
  initIMAGEScreen(NULL,0);
}

/*
 * 
 *  TOGGLE SCREEN
 * 
 */

int toggle=1;
void processToogleOption(){
  
  if (toggle==1){
    toggle=0;
    toggleMountOption(0);
  }else{
    toggle=1;
    toggleMountOption(1);
  }
}

void processMountOption(){
  if (toggle==0){
    switchPage(FS,currentFullPath);
    toggle=1;                               // rearm toggle switch
  }else{
    
    nextAction=IMG_MOUNT;                       // Mounting can not be done via Interrupt, must be done via the main thread
  }
}

void nothing(){
  return;
  //__NOP();
}

void processBtnRet(){
  if (currentPage==MOUNT || currentPage==IMAGE){
    switchPage(FS,currentFullPath);
  }else if (currentPage==FS){

  }
}



/*
*
* CONFIG MENU SCREEN
*
*/

typedef struct MNUDISPITEM{
  uint8_t displayPos;
  uint8_t update;
  uint8_t selected;
  uint8_t mnuItemIndx;
  uint8_t status;
}MNUDISPITEM_t;

MNUDISPITEM_t mnuDispItem[SCREEN_MAX_LINE_ITEM]; // LINE OF MENU ITEM DISP ON SCREEN

typedef struct MNUITEM{
  uint8_t type;             // 0 -> simpleLabel; 1-> boolean; 2-> value
  uint8_t icon;
  char title[32];
  
  void (* triggerfunction)();
  uint8_t arg;
  uint8_t ival;
}MNUITEM_t;

MNUITEM_t mnuItem[16]; // MENU ITEM DEFINITION
uint8_t mnuItemCount=0;

void processBootOption(int arg){
  if (arg==0){
    mnuItem[0].ival=1;
    mnuItem[1].ival=0;
    mnuItem[2].ival=0;
  }else if (arg==1){
    mnuItem[0].ival=0;
    mnuItem[1].ival=1;
    mnuItem[2].ival=0;
  }else if (arg==2){
    mnuItem[0].ival=0;
    mnuItem[1].ival=0;
    mnuItem[2].ival=1;
  }
  
  updateConfigMenuDisplay(-1);
  setConfigParamInt("bootMode",arg);
  saveConfigFile();
return;
}

void processSoundEffect(){
  if (mnuItem[6].ival==1){
    setConfigParamInt("soundEffect",0);
    flgSoundEffect=0;
    mnuItem[6].ival=0;
  }else{
    setConfigParamInt("soundEffect",1);
    flgSoundEffect=1;
    mnuItem[6].ival=1;
  }
    
  saveConfigFile();
  updateConfigMenuDisplay(-1);
}

void processClearprefs(){
  deleteConfigFile();
  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"Prefs cleared");
  ssd1306_UpdateScreen();
}

void processClearFavorites(){
  wipeFavorites();
  saveConfigFile();
  buildLstFromFavorites();
  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"Favorites cleared");
  ssd1306_UpdateScreen();
}


/*
*     MAKE FS SCREEN
*/

uint8_t toggleMakeFs=1;

void makeFsScreen(){
    
  clearScreen();
  ssd1306_SetColor(White);

  displayStringAtPosition(0,0,"Make FileSystem");
  ssd1306_DrawLine(0,8,127,8);
  
  displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"This will format");
  displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT,"the SDCARD:");
  displayStringAtPosition(30,5*SCREEN_LINE_HEIGHT,"YES");
  displayStringAtPosition(30,6*SCREEN_LINE_HEIGHT,"NO");
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,5*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

void processMakeFsToggleOption(){
  
  if (toggleMakeFs==1){
    toggleMakeFs=0;
    toggleMakeFsOption(0);
  }else{
    toggleMakeFs=1;
    toggleMakeFsOption(1);
  }
}

void toggleMakeFsOption(int i){
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,5*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_FillRect(30-5,6*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

void processMakeFsOption(){
  if (toggleMakeFs==0){
    switchPage(MENU,NULL);
    toggleMakeFs=1;                               // rearm toggle switch
  }else{
    nextAction=MAKEFS;                            // Very important this has to be managed by the main thread and not by interrupt TODO PUT ALL ACTION IN MAIN with trigger from emulator

    //processMakeFsConfirmed();
  }
}


void processMakeFsConfirmed(){
 
  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Make Filesystem");
  ssd1306_DrawLine(0,8,127,8);

  ptrbtnUp=nothing;
  ptrbtnDown=nothing;
  ptrbtnEntr=processMakeFsSysReset;
  ptrbtnRet=nothing;
 
  displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Please wait...");
  displayStringAtPosition(0,4*SCREEN_LINE_HEIGHT,"Formatting SDCARD");
  ssd1306_UpdateScreen();

  if (makeSDFS()==RET_OK){
    displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Result: OK      ");
    log_info("makeSDFS success");
  }else{
    displayStringAtPosition(0,3*SCREEN_LINE_HEIGHT,"Result: ERROR   ");
   log_error("makeSDFS error");
  }
  displayStringAtPosition(0,4*SCREEN_LINE_HEIGHT,"                   ");
  displayStringAtPosition(0,5*SCREEN_LINE_HEIGHT,"Press [ENTER] to ");
  displayStringAtPosition(0,6*SCREEN_LINE_HEIGHT,"reboot");
  ssd1306_UpdateScreen();
  makeScreenShot(scrI);
  scrI++;
  while(1){};
}

 

void processMakeFsSysReset(){

  HAL_Delay(200);                     // Important need a delay otherwise Reset does not work.
  NVIC_SystemReset();
  return;
}


void processMakeFsBtnRet(){
  mnuItem[6].triggerfunction=processMakeFs;
  ptrbtnRet=processBtnRet;
  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"         ");
  ssd1306_UpdateScreen();
}


void processPrevConfigItem(){
  //uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);
  uint8_t itemCount=mnuItemCount;
    
  if (itemCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==0){
    dispSelectedIndx=itemCount-1;
  }else if (dispSelectedIndx==0)
    if (currentClistPos==0)
      currentClistPos=itemCount-1;
    else
      currentClistPos=(currentClistPos-1)%itemCount;
  else{
    dispSelectedIndx--;
  }
  log_debug("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
  
  updateConfigMenuDisplay(-1);
}

void processDispEmulationScreen(){
  switchPage(EMULATIONTYPE,0);
  return;
}

void processNextConfigItem(){
  //uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);
  uint8_t itemCount=mnuItemCount;
  log_info("item cnt %d dispSelectedIndx:%d",itemCount,dispSelectedIndx);
  if (itemCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==itemCount-1){
    dispSelectedIndx=0;
  }else if (dispSelectedIndx==(SCREEN_MAX_LINE_ITEM-1))
    currentClistPos=(currentClistPos+1)%itemCount;
  else{
    dispSelectedIndx++;
  }
  log_debug("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
  updateConfigMenuDisplay(-1);
}

void processSelectConfigItem(){
  MNUITEM_t * cMnuItem;

  cMnuItem=&mnuItem[selectedIndx];
  log_info("selected:%s ",cMnuItem->title);
  cMnuItem->triggerfunction(cMnuItem->arg);

}

void processReturnConfigItem(){
  switchPage(MENU,NULL);
}

void updateConfigMenuDisplay(int init){
  int offset=5;
  int h_offset=10;

  MNUITEM_t * cMnuItem;
  uint8_t mnuIndx=0;
  uint8_t itemCount=mnuItemCount;

  log_info("lst_count:%d init:%d",itemCount,init);

  if (init!=-1){
    currentClistPos=(init-dispSelectedIndx)%mnuItemCount;
    //dispSelectedIndx=0;
  }

  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){
    
    mnuDispItem[i].status=1;                   // starting with item status =0, if an error then status = -1;
    mnuDispItem[i].displayPos=i;               // corresponding line on the screen
    mnuDispItem[i].update=1;
    
    if (i>itemCount-1){                          // End of the list before the MAX_LINE_ITEM
      mnuDispItem[i].status=0;
      continue;
    }

    mnuIndx=(currentClistPos+i)%itemCount;
    cMnuItem=&mnuItem[mnuIndx];
    if (mnuDispItem[i].selected==1 && i!=dispSelectedIndx){    // It was selected (inversed, thus we need to reinverse)
        mnuDispItem[i].selected=0;
    }else if (i==dispSelectedIndx){
      mnuDispItem[i].selected=1;                // first item of the list is selected
      selectedIndx=mnuIndx;
    }  
      
    if (cMnuItem!=NULL)
      mnuDispItem[i].mnuItemIndx=mnuIndx;
    else{
      mnuDispItem[i].status=0;
      continue;
    }

  }

  // Render Part
  for (int i=0;i<SCREEN_MAX_LINE_ITEM;i++){

    clearLineStringAtPosition(1+i,offset);
    if (mnuDispItem[i].status!=0){
      uint8_t cMnuIndx=mnuDispItem[i].mnuItemIndx;
      ssd1306_SetColor(White);
      dispIcon(1,(1+i)*SCREEN_LINE_HEIGHT+offset,mnuItem[cMnuIndx].icon);

      ssd1306_SetColor(White);
      displayStringAtPosition(1+h_offset,(1+i)*SCREEN_LINE_HEIGHT+offset,mnuItem[cMnuIndx].title);
      
      if (mnuItem[cMnuIndx].type==1){
        if (mnuItem[cMnuIndx].ival==1)
          dispIcon(118,(1+i)*SCREEN_LINE_HEIGHT+offset,7); // 7 FULL
        else
          dispIcon(118,(1+i)*SCREEN_LINE_HEIGHT+offset,6); // 7 FULL
      }else if (mnuItem[cMnuIndx].type==2){
          char sztmp[5];
          sprintf(sztmp,"%d",mnuItem[cMnuIndx].ival);
          displayStringAtPosition(118,(1+i)*SCREEN_LINE_HEIGHT+offset,sztmp); 
      }

      if (mnuDispItem[i].selected==1){
        inverseStringAtPosition(1+i,offset);
      }
    }
  }

  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

}

void processDispMakeFsScreen(){
   switchPage(MAKEFS,NULL);
   return;
}

void processBootImageIndex(){
  bootImageIndex++;

  if (bootImageIndex==5)
    bootImageIndex=1;
  //bootImageIndex=(bootImageIndex+1)%(MAX_PARTITIONS+1);
  
  mnuItem[4].ival=bootImageIndex;

  setConfigParamInt("bootImageIndex",bootImageIndex);
  saveConfigFile();
 
  updateConfigMenuDisplay(4);
  return;
}

void initConfigMenuScreen(int i){
  
  mnuItemCount=10;

  if (i>=mnuItemCount)
    i=0;

  if (i<0)
    i=mnuItemCount-1;

  int bootMode=0;

  if (getConfigParamInt("bootMode",&bootMode)==RET_ERR)
    log_warn("Warning: getting bootMode from Config failed");
  else 
    log_info("bootMode=%d",bootMode);

  sprintf(mnuItem[0].title,"Boot last image");
  mnuItem[0].type=1;
  mnuItem[0].icon=10;
  mnuItem[0].triggerfunction=processBootOption;
  mnuItem[0].arg=0;
  mnuItem[0].ival=0;

  sprintf(mnuItem[1].title,"Boot last dir");
  mnuItem[1].type=1;
  mnuItem[1].icon=10;
  mnuItem[1].triggerfunction=processBootOption;
  mnuItem[1].arg=1;
  mnuItem[1].ival=0;

  sprintf(mnuItem[2].title,"Boot favorites");
  mnuItem[2].type=1;
  mnuItem[2].icon=10;
  mnuItem[2].triggerfunction=processBootOption;
  mnuItem[2].arg=2;
  mnuItem[2].ival=0;

  mnuItem[bootMode].ival=1;

  sprintf(mnuItem[3].title,"Emulation type");
  mnuItem[3].type=0;
  mnuItem[3].icon=10;
  mnuItem[3].triggerfunction=processDispEmulationScreen;
  mnuItem[3].arg=2;
  mnuItem[3].ival=0;

  sprintf(mnuItem[4].title,"Boot Img index");
  mnuItem[4].type=2;
  mnuItem[4].icon=10;
  mnuItem[4].triggerfunction=processBootImageIndex;
  mnuItem[4].arg=0;
  mnuItem[4].ival=bootImageIndex;

  sprintf(mnuItem[5].title,"Boot folder");
  mnuItem[5].type=0;
  mnuItem[5].icon=10;
  mnuItem[5].triggerfunction=nothing;
  mnuItem[5].arg=0;
  mnuItem[5].ival=0;

  sprintf(mnuItem[6].title,"Sound effect");
  mnuItem[6].type=1;
  mnuItem[6].icon=9;
  mnuItem[6].triggerfunction=processSoundEffect;
  mnuItem[6].arg=0;
  mnuItem[6].ival=flgSoundEffect;

  sprintf(mnuItem[7].title,"Clear prefs");
  mnuItem[7].type=0;
  mnuItem[7].icon=8;
  mnuItem[7].triggerfunction=processClearprefs;
  mnuItem[7].arg=0;

  sprintf(mnuItem[8].title,"Clear favorites");
  mnuItem[8].type=0;
  mnuItem[8].icon=8;
  mnuItem[8].triggerfunction=processClearFavorites;
  mnuItem[8].arg=0;

  sprintf(mnuItem[9].title,"Make filesystem");
  mnuItem[9].type=0;
  mnuItem[9].icon=11;
  mnuItem[9].triggerfunction=processDispMakeFsScreen;
  mnuItem[9].arg=0;

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Config Menu");
  ssd1306_DrawLine(0,8,127,8);

  ssd1306_SetColor(White);
  ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);

  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"");
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

  return;

}

/**
 * 
 * 
 * EMULATION TYPE SCREEN
 * 
 */

void initConfigEmulationScreen(){

  mnuItemCount=3;
  dispSelectedIndx=0;
  u_int8_t i=0;
  if (i>=mnuItemCount)
    i=0;

  if (i<0)
    i=mnuItemCount-1;

  int emulationType=0;
  if (getConfigParamInt("emulationType",&emulationType)==RET_ERR)
    log_warn("Warning: getting emulationType from Config failed");
  else 
    log_info("emulationType=%d",emulationType);

  sprintf(mnuItem[0].title,"Disk II");
  mnuItem[0].type=1;
  mnuItem[0].icon=10;
  mnuItem[0].triggerfunction=processEmulationTypeOption;
  mnuItem[0].arg=0;
  mnuItem[0].ival=0;

  sprintf(mnuItem[1].title,"Smartport HD");
  mnuItem[1].type=1;
  mnuItem[1].icon=10;
  mnuItem[1].triggerfunction=processEmulationTypeOption;
  mnuItem[1].arg=1;
  mnuItem[1].ival=0;

  sprintf(mnuItem[2].title,"DISK 3.5");
  mnuItem[2].type=1;
  mnuItem[2].icon=10;
  mnuItem[2].triggerfunction=processEmulationTypeOption;
  mnuItem[2].arg=2;
  mnuItem[2].ival=0;

  mnuItem[emulationType].ival=1;

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Emulation type");
  ssd1306_DrawLine(0,8,127,8);

  ssd1306_SetColor(White);
  ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);

  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"");
  ssd1306_UpdateScreen();

  makeScreenShot(scrI);
  scrI++;

  return;
}

uint8_t emulationTypeChanged=0;
void processEmulationTypeOption(int arg){
  
  if (arg==0){
    mnuItem[0].ival=1;
    mnuItem[1].ival=0;
    mnuItem[2].ival=0;
  }else if (arg==1){
    mnuItem[0].ival=0;
    mnuItem[1].ival=1;
    mnuItem[2].ival=0;
  }else if (arg==2){
    mnuItem[0].ival=0;
    mnuItem[1].ival=0;
    mnuItem[2].ival=1;
  }
  
  updateConfigMenuDisplay(-1);
  setConfigParamInt("emulationType",arg);
  saveConfigFile();
  emulationTypeChanged=1;
  
return;
}

void processReturnEmulationTypeItem(){
  if (emulationTypeChanged==1){
    HAL_Delay(300);
    NVIC_SystemReset();

    return;
  }
  switchPage(CONFIG,NULL);
}

/*
 * 
 *  MENU IMAGE OPTION 
 * 
 */

int8_t currentImageMenuItem=0;

void processNextImageMenuScreen(){
  currentImageMenuItem++;
  initImageMenuScreen(currentImageMenuItem);
}

void processPreviousImageMenuScreen(){
  currentImageMenuItem--;
  initImageMenuScreen(currentImageMenuItem);
}

void processImageMenuScreen(){
  switchPage(IMAGE,NULL);
}

void processActiveImageMenuScreen(){
  switch(currentImageMenuItem){
    
    case 0:                                                   // Add / Remove from Favorite
      toggleAddToFavorite();
      switchPage(IMAGE,NULL);
      break;

    case 1:                                                  // unmount();
      ptrUnmountImage();
      switchPage(FS,0x0);
      break;

    case 2:                                                 // unlink();
      ptrUnmountImage();
      unlinkImageFile(currentFullPathImageFilename);
      nextAction=FSDISP;
      break;

    default:
      break;
  }
  return;
}

void initImageMenuScreen(int i){
  
  char * menuItem[3];
  u_int8_t numItems=3;
  uint8_t h_offset=10;

  if (i>=numItems)
    i=0;

  if (i<0)
    i=numItems-1;

  menuItem[0]="Toggle favorite";
  menuItem[1]="Unmount image";
  menuItem[2]="Delete image";

  uint8_t menuIcon[3];            // TODO Change ICO
  menuIcon[0]=5;
  menuIcon[1]=0;
  menuIcon[2]=4;

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Image Menu");
  ssd1306_DrawLine(0,8,127,8);

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
  ssd1306_UpdateScreen();
  currentImageMenuItem=i;

  makeScreenShot(scrI);
  scrI++;

  return;

}

/*
 * 
 *  MAIN MENU SCREEN
 * 
 */


int8_t currentMainMenuItem=0;

void processNextMainMenuScreen(){
  currentMainMenuItem++;
  initMainMenuScreen(currentMainMenuItem);
}

void processPreviousMainMenuScreen(){
  currentMainMenuItem--;
  initMainMenuScreen(currentMainMenuItem);
}

void processActiveMainMenuScreen(){
  switch(currentMainMenuItem){
    
    case 0:
      switchPage(FAVORITE,NULL);
      break;

    case 1:
      switchPage(FS,0x0);
      break;

    case 2:
      switchPage(IMAGE,NULL);
      break;

    case 3:
      switchPage(CONFIG,NULL);
      break;

    default:
      break;
  }
  return;
}

void initMainMenuScreen(int i){
  
  char * menuItem[5];
  u_int8_t numItems=4;
  uint8_t h_offset=10;

  if (i>=numItems)
    i=0;

  if (i<0)
    i=numItems-1;

  menuItem[0]="Favorites";
  menuItem[1]="File manager";
  menuItem[2]="Mounted image";
  menuItem[3]="Config";
  
uint8_t menuIcon[5];
menuIcon[0]=5;
menuIcon[1]=0;
menuIcon[2]=4;
menuIcon[3]=3;

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Main Menu");
  ssd1306_DrawLine(0,8,127,8);


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
  ssd1306_UpdateScreen();
  currentMainMenuItem=i;

  makeScreenShot(scrI);
  scrI++;
  return;

}

void toggleMainMenuOption(int i){         // Todo checked if used ?
  

  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_FillRect(30-5,5*SCREEN_LINE_HEIGHT-1,50,9);
  ssd1306_UpdateScreen();
}
/*
 * 
 *  SD EJECT SCREEN
 * 
 */

void initErrorScreen(char * msg){

  clearScreen();
  ssd1306_SetColor(White);
  displayStringAtPosition(30,3*SCREEN_LINE_HEIGHT, "ERROR:");
  displayStringAtPosition(30,4*SCREEN_LINE_HEIGHT, msg);
  dispIcon24x24(5,22,1);
  ssd1306_UpdateScreen();
}

void initFSScreen(char * path){

  clearScreen();
  
  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"File listing");
  ssd1306_DrawLine(0,8,127,8);
  ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);
  
  char tmp[32];
  sprintf(tmp,"xx/xx");
  displayStringAtPosition(96,6*SCREEN_LINE_HEIGHT+1,tmp);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(tmp,18,"%s",currentPath);
#pragma GCC diagnostic pop
  displayStringAtPosition(0,6*SCREEN_LINE_HEIGHT+1,tmp);

  ssd1306_UpdateScreen();
}

/**
 * 
 *    FAVORITE SCREEN
 * 
 */

void processPrevFavoriteItem(){
    
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
    log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
    updateChainedListDisplay(-1,favoritesChainedList);
}

void processNextFavoriteItem(){ 

    uint8_t lstCount=dirChainedList->len;
    
    if (lstCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==lstCount-1){
      dispSelectedIndx=0;
    }else if (dispSelectedIndx==(SCREEN_MAX_LINE_ITEM-1))
      currentClistPos=(currentClistPos+1)%lstCount;
    else{
      dispSelectedIndx++;
    }
    log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
    updateChainedListDisplay(-1,favoritesChainedList);
}

void processReturnFavoriteItem(){
  switchPage(MENU,NULL);
}

void processSelectFavoriteItem(){
  // Warning Interrupt can not trigger Filesystem action otherwise deadlock can occured !!!
  
  list_node_t *pItem=NULL;
  
  pItem=list_at(favoritesChainedList, selectedIndx);
  sprintf(tmpFullPathImageFilename,"%s",(char*)pItem->val);
  char * imageName=getImageNameFromFullPath(tmpFullPathImageFilename);
  switchPage(MOUNT,imageName);

}

void initFavoriteScreen(){

  clearScreen();
  
  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Favorites");
  ssd1306_DrawLine(0,8,127,8);
  ssd1306_DrawLine(0,6*SCREEN_LINE_HEIGHT-1,127,6*SCREEN_LINE_HEIGHT-1);
  
  char tmp[32];
  sprintf(tmp,"xx/10");
  displayStringAtPosition(96,6*SCREEN_LINE_HEIGHT+1,tmp);

  ssd1306_UpdateScreen();

}

/**
 * 
 * SMARTPORT HD EMULATION
 * 
 */

void initSmartPortHD(){

  clearScreen();
  
  ssd1306_SetColor(White);
  dispIcon32x32(1,1,1);
  displayStringAtPosition(35,1*SCREEN_LINE_HEIGHT,"SMARTPORT HD");
  //displayStringAtPosition(35,2*9,"HD");
  ssd1306_UpdateScreen();

}

void updateImageSmartPortHD(char * filename,uint8_t i){
  // Display the 4 image Filename without with extension and only 10 char.

  char tmp[22]; 
  ssd1306_SetColor(White);

  if (filename!=NULL){
    uint8_t len=strlen(filename);
    len=(len-3-5);                                  // Remove 3 char (.PO) and limit to 10 char
    if (len>16)
      len=16;

    filename[6]=toupper(filename[6]);
  
    if ((i+1)==bootImageIndex)
      snprintf(tmp,len+2,"*:%s",filename+6);
    else
      snprintf(tmp,len+2,"%d:%s",i+1,filename+6);
    
    displayStringAtPosition(1,(3+i)*SCREEN_LINE_HEIGHT,tmp); 
  }else{
    if ((i+1)==bootImageIndex)
      sprintf(tmp,"*:Empty");
    else
      sprintf(tmp,"%d:Empty",i+1);
    displayStringAtPosition(1,(3+i)*SCREEN_LINE_HEIGHT,tmp); 
  }
  ssd1306_UpdateScreen();
  makeScreenShot(scrI);
  scrI++;
}

void updateSmartportHD(uint8_t imageIndex, enum EMUL_CMD cmd ){
  
  if (currentPage!=SMARTPORT){
    return;
  }

  uint8_t icoVOffset=8;
  uint8_t icoHOffset=118;

  ssd1306_SetColor(Black);
  ssd1306_FillRect(icoHOffset,icoVOffset,8,8);
  ssd1306_SetColor(White);
    
  if (harvey_ball==0){
      harvey_ball=1;
      dispIcon(icoHOffset,icoVOffset,12);
  }else{
      harvey_ball=0;
      dispIcon(icoHOffset,icoVOffset,13);
  }
  char szTmp[5];

  sprintf(szTmp,"  ");
  for (int i=0;i<4;i++){
    displayStringAtPosition(128-2*7,(3+i)*SCREEN_LINE_HEIGHT,szTmp);      // 2nd half of the screen Line 3 & 4 
  }

  if (cmd == EMUL_READ){
    sprintf(szTmp,"RD");
  }else if (cmd == EMUL_WRITE){
    sprintf(szTmp,"WR");                            // Need to change to short instead of printing int 0-65000
  }else if (cmd== EMUL_STATUS){
    sprintf(szTmp,"I ");
  }else{
    sprintf(szTmp,"  ");
  }

  displayStringAtPosition(128-2*7,(3+imageIndex)*SCREEN_LINE_HEIGHT,szTmp);      // 2nd half of the screen Line 3 & 4 
  ssd1306_UpdateScreen();  

}

void processSmartPortHDRetScreen(){
  switchPage(MENU,NULL);
}

/**
 * 
 * DISPLAY PRIMITIVE
 * 
 */

// Icon converter BMP ->  https://mischianti.org/ssd1306-oled-display-draw-images-splash-and-animations-2/
// <!> Generate Vertical 1 bit per pixel

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
  ssd1306_FillRect(0,0,127,63);
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
