
#include "stdio.h"
#include "display.h"
#include "ssd1306.h"
#include "fonts.h"
#include "list.h"
#include "main.h"
#include "log.h"

extern list_t * dirChainedList;
extern char currentFullPath[1024]; 
extern char currentPath[128];
extern int currentClistPos;
extern image_info_t mountImageInfo;

uint8_t selectedFsIndx=0;

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

void updateFSDisplay(int init){
  
  int offset=5;
  int h_offset=10;

  char tmp[32];
  char * value;

  list_node_t *fsItem; 
  uint8_t fsIndx=0;
  uint8_t lstCount=dirChainedList->len;

  log_info("lst_count:%d init:%d",lstCount,init);

  if (init!=-1)
    currentClistPos=init;

  for (int i=0;i<MAX_LINE_ITEM;i++){
    
    fsDispItem[i].status=1;                   // starting with item status =0, if an error then status = -1;
    fsDispItem[i].displayPos=i;               // corresponding line on the screen
    fsDispItem[i].update=1;
    
    if (i>lstCount-1){                          // End of the list before the MAX_LINE_ITEM
      fsDispItem[i].status=0;
      continue;
    }

    fsIndx=(currentClistPos+i)%lstCount;
    fsItem=list_at(dirChainedList, fsIndx);
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
      }else{
        fsDispItem[i].icon=1; 
        fsDispItem[i].type=1;                // 1 -> file 
      }
      snprintf(fsDispItem[i].title,24,"%s",value+2);              
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

void dispIcon(int x,int y,int indx){
  const unsigned char icon_set[]  = {
    // 'folder, 8x8px indx=0
  0x00, 0x70, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x00,
  //  File, 8x8px indx=1
  0x00, 0x7c, 0x54, 0x4c, 0x44, 0x44, 0x7c, 0x00
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
  displayStringAtPosition(30,3*9,"SmartDiskII");
  ssd1306_UpdateScreen();
}


enum STATUS initIMAGEScreen(char * imageName,int type){
  
  if (imageName==NULL){
    log_error("imageName is null");
    return RET_ERR;
  }
    

  char tmp[32];
  clearScreen();
  ssd1306_SetColor(White);
  
  int len=strlen(imageName);
  int i=0;

  for (i=len-1;i!=0;i--){
    if (imageName[i]=='/')
      break;
  }

  snprintf(tmp,20,"%s",imageName+i+1);
  len=strlen(tmp);

  if (len<20){
    for (i=len-1;i!=0;i--){
      if (tmp[i]=='.')
        break;
    }
    if (i!=0)
      tmp[i]=0x0;
  }
  char CL,WP,SYN;
  displayStringAtPosition(5,1*9,tmp);
  if (mountImageInfo.type==0)
    displayStringAtPosition(5,2*9,"type: NIC");
  else if (mountImageInfo.type==1)
    displayStringAtPosition(5,2*9,"type: WOZ");
  
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
  ssd1306_UpdateScreen();
  return RET_OK;
}

void updateIMAGEScreen(uint8_t status,uint8_t trk){
  unsigned long t1,t2,diff1;  
   DWT->CYCCNT = 0;
   t1 = DWT->CYCCNT; 
  if (currentTrk!=trk || status!=currentStatus){

    char tmp[32];

    if (currentTrk!=trk){
      //sprintf(tmp,"Track: %02d ",trk);
      sprintf(tmp,"TRACK: %02d",trk);
      displayStringAtPosition(5,3*9,tmp);
      currentTrk=trk;
    }
        t2 = DWT->CYCCNT;
    diff1 = t2 - t1;
    //log_info("diff Str: %d",diff1);
    
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