
#include "stdio.h"
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
extern image_info_t mountImageInfo;


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
    
    case CONFIG:
      initConfigMenuScreen(0);
      updateConfigMenuDisplay(0);

      ptrbtnUp=processPrevConfigItem;
      ptrbtnDown=processNextConfigItem;
      ptrbtnEntr=processSelectConfigItem;
      ptrbtnRet=processReturnConfigItem;
      currentPage=CONFIG;
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
      if (flgImageMounted==0){
        log_error("No image mounted");
        return RET_ERR;
      }

      initIMAGEScreen(arg,0);
      ptrbtnUp=nothing;
      ptrbtnDown=nothing;
      ptrbtnEntr=toggleAddToFavorite;
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
      dispIcon(1,(1+i)*9+offset,fsDispItem[i].icon);

      ssd1306_SetColor(White);
      displayStringAtPosition(1+h_offset,(1+i)*9+offset,fsDispItem[i].title);
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
  displayStringAtPosition(96,6*9+1,tmp);

  ssd1306_UpdateScreen();
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
    log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
    
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
  displayStringAtPosition(5,1*9,"Mounting:");
  displayStringAtPosition(5,2*9,tmp2);
  
  displayStringAtPosition(30,4*9,"YES");
  displayStringAtPosition(30,5*9,"NO");
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*9-1,50,9);
  ssd1306_UpdateScreen();
}

void toggleMountOption(int i){
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*9-1,50,9);
  ssd1306_FillRect(30-5,5*9-1,50,9);
  ssd1306_UpdateScreen();
}

void initSplashScreen(){
  ssd1306_Init();
  ssd1306_FlipScreenVertically();
  
  ssd1306_Clear();
  ssd1306_SetColor(White);
  displayStringAtPosition(30,3*9,"SmartDisk ][");
  displayStringAtPosition(90,6*9,_VERSION);
  ssd1306_UpdateScreen();
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
  displayStringAtPosition(5,1*9,mountImageInfo.title);
  if (mountImageInfo.type==0)
    displayStringAtPosition(5,2*9,"type: NIC");
  else if (mountImageInfo.type==1)
    displayStringAtPosition(5,2*9,"type: WOZ");
  else if (mountImageInfo.type==2)
    displayStringAtPosition(5,2*9,"type: DSK");
  else if (mountImageInfo.type==3)
    displayStringAtPosition(5,2*9,"type: PO ");
  else
    displayStringAtPosition(5,2*9,"type: ERR ");
  
  displayStringAtPosition(5,3*9,"Track: 0");

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
  displayStringAtPosition(5,5*9,tmp);

  sprintf(tmp,"WP:%c SYN:%c V:%d",WP,SYN,mountImageInfo.version);
  displayStringAtPosition(5,6*9,tmp);
  if (mountImageInfo.favorite==1)
    dispIcon12x12(115,18,0);
  else
    dispIcon12x12(115,18,1);
  ssd1306_UpdateScreen();
  return RET_OK;
}

void updateIMAGEScreen(uint8_t status,uint8_t trk){

  if (currentPage!=IMAGE)
    return;

  if (currentTrk!=trk || status!=currentStatus){

    char tmp[32];

    if (currentTrk!=trk){
      sprintf(tmp,"TRACK: %02d",trk);
      displayStringAtPosition(5,3*9,tmp);
      currentTrk=trk;
    }
    
    if (status==0){
      sprintf(tmp,"RD");
      displayStringAtPosition(72,3*9,tmp);
    }else if(status==1){
      sprintf(tmp,"WR");
      displayStringAtPosition(72,3*9,tmp);
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
  __NOP();
}

void processBtnRet(){
  if (currentPage==MOUNT || currentPage==IMAGE){
    switchPage(FS,currentFullPath);
    printf("here .\n");
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

MNUITEM_t mnuItem[6]; // MENU ITEM DEFINITION

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

void processClearprefs(){
  deleteConfigFile();
  displayStringAtPosition(1,6*9+1,"Prefs cleared");
  ssd1306_UpdateScreen();
}

void processClearFavorites(){
  wipeFavorites();
  saveConfigFile();
  buildLstFromFavorites();
  displayStringAtPosition(1,6*9+1,"Favorites cleared");
  ssd1306_UpdateScreen();
}

void processMakeFs(){

}

void processPrevConfigItem(){
  uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);
    
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
  log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
  updateConfigMenuDisplay(-1);
}

void processNextConfigItem(){
  uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);
    
  if (itemCount<=SCREEN_MAX_LINE_ITEM && dispSelectedIndx==itemCount-1){
    dispSelectedIndx=0;
  }else if (dispSelectedIndx==(SCREEN_MAX_LINE_ITEM-1))
    currentClistPos=(currentClistPos+1)%itemCount;
  else{
    dispSelectedIndx++;
  }
  log_info("currentClistPos:%d, dispSelectedIndx:%d",currentClistPos,dispSelectedIndx);
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
  uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);

  log_info("lst_count:%d init:%d",itemCount,init);

  if (init!=-1){
    currentClistPos=init;
    dispSelectedIndx=0;
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
      dispIcon(1,(1+i)*9+offset,mnuItem[cMnuIndx].icon);

      ssd1306_SetColor(White);
      displayStringAtPosition(1+h_offset,(1+i)*9+offset,mnuItem[cMnuIndx].title);
      
      if (mnuItem[cMnuIndx].type==1){
        if (mnuItem[cMnuIndx].ival==1)
          dispIcon(118,(1+i)*9+offset,7); // 7 FULL
        else
          dispIcon(118,(1+i)*9+offset,6); // 7 FULL
      }
      
      if (mnuDispItem[i].selected==1){
        inverseStringAtPosition(1+i,offset);
      }
    }
  }

  ssd1306_UpdateScreen();
}

void initConfigMenuScreen(int i){
  
  u_int8_t numItems=6;

  if (i>=numItems)
    i=0;

  if (i<0)
    i=numItems-1;

  int bootMode=0;
  if (getConfigParamInt("bootMode",&bootMode)==RET_ERR)
    log_error("error getting bootMode from Config");
  else 
    log_info("bootMode=%d",bootMode);

  sprintf(mnuItem[0].title,"Boot last image");
  mnuItem[0].type=1;
  mnuItem[1].icon=0;
  mnuItem[0].triggerfunction=processBootOption;
  mnuItem[0].arg=0;
  mnuItem[0].ival=0;


  sprintf(mnuItem[1].title,"Boot last dir");
  mnuItem[1].type=1;
  mnuItem[1].icon=0;
  mnuItem[1].triggerfunction=processBootOption;
  mnuItem[1].arg=1;
  mnuItem[1].ival=0;

  sprintf(mnuItem[2].title,"Boot favorites");
  mnuItem[2].type=1;
  mnuItem[2].icon=1;
  mnuItem[2].triggerfunction=processBootOption;
  mnuItem[2].arg=2;
  mnuItem[2].ival=0;


  mnuItem[bootMode].ival=1;


  sprintf(mnuItem[3].title,"Clear prefs");
  mnuItem[3].type=0;
  mnuItem[3].icon=0;
  mnuItem[3].triggerfunction=processClearprefs;
  mnuItem[3].arg=0;

  sprintf(mnuItem[4].title,"Clear favorites");
  mnuItem[4].type=0;
  mnuItem[4].icon=0;
  mnuItem[4].triggerfunction=processClearFavorites;
  mnuItem[4].arg=0;

  sprintf(mnuItem[5].title,"Make filesystem");
  mnuItem[5].type=0;
  mnuItem[5].icon=0;
  mnuItem[5].triggerfunction=processMakeFs;
  mnuItem[5].arg=0;

  clearScreen();

  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"Config Menu");
  ssd1306_DrawLine(0,8,127,8);

  ssd1306_SetColor(White);
  ssd1306_DrawLine(0,6*9-1,127,6*9-1);

  displayStringAtPosition(1,6*9+1,"");
  ssd1306_UpdateScreen();


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
    displayStringAtPosition(1+h_offset,(1+j)*9+5,menuItem[j]);

    dispIcon(1,(1+j)*9+5,menuIcon[j]);
  }

  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(1,(1+i)*9-1+5,126,9);
  
  ssd1306_SetColor(White);
  ssd1306_DrawLine(0,6*9-1,127,6*9-1);

  displayStringAtPosition(1,6*9+1,_VERSION);
  ssd1306_UpdateScreen();
  currentMainMenuItem=i;
  return;

}

void toggleMainMenuOption(int i){
  

  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*9-1,50,9);
  ssd1306_FillRect(30-5,5*9-1,50,9);
  ssd1306_UpdateScreen();
}
/*
 * 
 *  SD EJECT SCREEN
 * 
 */
void initSdEjectScreen(){

  clearScreen();
  ssd1306_SetColor(White);
  displayStringAtPosition(5,3*9, "ERROR:");
  displayStringAtPosition(5,4*9, "SD CARD IS EJECTED");
  ssd1306_UpdateScreen();
}

void initFSScreen(char * path){

  clearScreen();
  
  ssd1306_SetColor(White);
  displayStringAtPosition(0,0,"File listing");
  ssd1306_DrawLine(0,8,127,8);
  ssd1306_DrawLine(0,6*9-1,127,6*9-1);
  
  char tmp[32];
  sprintf(tmp,"xx/xx");
  displayStringAtPosition(96,6*9+1,tmp);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(tmp,18,"%s",currentPath);
#pragma GCC diagnostic pop
  displayStringAtPosition(0,6*9+1,tmp);

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
  ssd1306_DrawLine(0,6*9-1,127,6*9-1);
  
  char tmp[32];
  sprintf(tmp,"xx/10");
  displayStringAtPosition(96,6*9+1,tmp);

  ssd1306_UpdateScreen();

}

/**
 * 
 * DISPLAY PRIMITIVE
 * 
 */

// Icon converter BMP ->  https://mischianti.org/ssd1306-oled-display-draw-images-splash-and-animations-2/
// <!> Generate Vertical 1 bit per pixel

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
    0x00, 0x7e, 0x7e, 0x7e, 0x7c, 0x7c, 0x7c, 0x00,   // indx=0 'folderb', 8x8px 
    0x00, 0x7e, 0x42, 0x46, 0x4a, 0x7e, 0x00, 0x00,   // indx=1 'file2', 8x8px
    0x08, 0xd8, 0x7c, 0x3f, 0x3f, 0x7c, 0xd8, 0x08,   // indx=2 'star', 8x8px
    0x00, 0x6c, 0x7c, 0x3e, 0x7c, 0x7c, 0x10, 0x00,   // indx=3 'config', 8x8px
    0x00, 0x60, 0x68, 0x1c, 0x3e, 0x1e, 0x0e, 0x00,   // indx=4 'launch', 8x8px
    0x00, 0x1c, 0x3e, 0x7c, 0x7c, 0x3e, 0x1c, 0x00,   // indx=5 'favorite', 8x8px
    0x00, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x00,   // indx=6 'chkbox_empty", 8x8px
    0x00, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x00    // indx=7 'chkbox_full", 8x8px
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
  ssd1306_FillRect(0,lineNumber*9-1+offset,127,9);
}

void clearLineStringAtPosition(int lineNumber,int offset){
  ssd1306_SetColor(Black);
  ssd1306_FillRect(0,lineNumber*9+offset,127,9);
}