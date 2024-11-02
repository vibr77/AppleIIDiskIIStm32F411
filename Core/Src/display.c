
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
extern char currentPath[128];
extern int currentClistPos;
extern image_info_t mountImageInfo;

uint8_t selectedFsIndx=0;
uint8_t selectedIndx=0;

#define MAX_LINE_ITEM 4
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

FSDISPITEM_t fsDispItem[MAX_LINE_ITEM];

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
void updateChainedListDisplay(int init, list_t * lst ){
  int offset=5;
  int h_offset=10;

  char tmp[32];
  char * value;

  list_node_t *fsItem; 
  uint8_t fsIndx=0;
  uint8_t lstCount=lst->len;

  log_info("lst_count:%d init:%d",lstCount,init);

  if (init!=-1){
    currentClistPos=init;
    dispSelectedIndx=0;
  }

  for (int i=0;i<MAX_LINE_ITEM;i++){
    
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
      selectedFsIndx=fsIndx;
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
        fsDispItem[i].type=0;                 // 0 -> Directory
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
  for (int i=0;i<MAX_LINE_ITEM;i++){

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
  sprintf(tmp,"%02d/%02d",selectedFsIndx+1,lstCount);
  displayStringAtPosition(96,6*9+1,tmp);

  ssd1306_UpdateScreen();
}

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

void clearScreen(){
  ssd1306_SetColor(Black);
  ssd1306_FillRect(0,0,127,63);
}

void initScreen(){
  ssd1306_Init();
  ssd1306_FlipScreenVertically();
  
  ssd1306_Clear();
  ssd1306_SetColor(White);
  displayStringAtPosition(30,3*9,"SmartDisk ][");
  ssd1306_UpdateScreen();
}


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

MNUDISPITEM_t mnuDispItem[MAX_LINE_ITEM]; // LINE OF MENU ITEM DISP ON SCREEN

typedef struct MNUITEM{
  uint8_t type;             // 0 -> simpleLabel; 1-> boolean; 2-> value
  uint8_t icon;
  char title[32];
  
  void (* triggerfunction)();
  uint8_t arg;
  uint8_t ival;
}MNUITEM_t;

MNUITEM_t mnuItem[6]; // MENU ITEM DEFINITION

enum STATUS processBootOption(int arg){
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
return RET_OK;
}

void processClearprefs(){
  f_unlink("/sdiskConfig.json"); 
  displayStringAtPosition(1,6*9+1,"Prefs cleared");
  ssd1306_UpdateScreen();
}

void processClearFavorites(){
  wipeFavorites();
  saveConfigFile();
  displayStringAtPosition(1,6*9+1,"Favorites cleared");
  ssd1306_UpdateScreen();
}

void processMakeFs(){

}

void processPrevConfigItem(){
  uint8_t itemCount=sizeof(mnuItem)/sizeof(MNUITEM_t);
    
  if (itemCount<=MAX_LINE_ITEM && dispSelectedIndx==0){
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
    
  if (itemCount<=MAX_LINE_ITEM && dispSelectedIndx==itemCount-1){
    dispSelectedIndx=0;
  }else if (dispSelectedIndx==(MAX_LINE_ITEM-1))
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

void processUpdirConfigItem(){
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

  for (int i=0;i<MAX_LINE_ITEM;i++){
    
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
  for (int i=0;i<MAX_LINE_ITEM;i++){

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
  uint8_t h_offset=10;

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


/**
 * 
 * MAIN MENU SCREEN
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

  displayStringAtPosition(1,6*9+1,VERSION);
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