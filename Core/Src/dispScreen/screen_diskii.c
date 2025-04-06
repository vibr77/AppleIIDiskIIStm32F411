
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_fs.h"
#include "display.h"
#include "favorites.h"
#include "configFile.h"

// EXTERN DEFINITION 

extern enum page currentPage;
extern image_info_t mountImageInfo;
extern enum STATUS (*ptrUnmountImage)();

extern char currentFullPath[MAX_FULLPATH_LENGTH]; 
extern char currentPath[MAX_PATH_LENGTH];
extern char currentFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];  // fullpath from root image filename
extern char tmpFullPathImageFilename[MAX_FULLPATHIMAGE_LENGTH];      // fullpath from root image filename

extern enum action nextAction;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

static void pBtnRetDiskIIImageScr();
static void pBtnEntrDiskIIImageScr();

static void pBtnEntrDiskIIImageMenuScr();
static void pBtnUpDiskIIImageMenuScr();
static void pBtnDownDiskIIImageMenuScr();
static void pBtnRetDiskIIImageMenuScr();

static void toggleAddToFavorite();
static void toggleMountOption(int i);

static void pBtnEntrMountImageScr();
static void pBtnRetMountImageScr();
static void pBtnToogleOption();

uint8_t currentTrk=0;
uint8_t currentStatus=0;

/*
 * 
 *  DISKII IMG
 * 
 */

void initDiskIIImageScr(char * imageName,int type){
  
    char tmp[32];
    clearScreen();
    ssd1306_SetColor(White);
    
    char CL,WP,SYN;
    displayStringAtPosition(5,1*SCREEN_LINE_HEIGHT,mountImageInfo.title);
  
    #ifdef A2F_MODE
        inverseStringAtPosition(1,0);
        ssd1306_SetColor(White);
        displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT,"Track:");
        if (mountImageInfo.type > 3)
        displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,"type: ERR ");
        dispIcon32x32(96,37,1);
    #endif
  
    #ifndef A2F_MODE
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
    #endif
  
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
   
    primUpdScreen();

    ptrbtnUp=nothing;
    ptrbtnDown=nothing;
    ptrbtnEntr=pBtnEntrDiskIIImageScr;
    ptrbtnRet=pBtnRetDiskIIImageScr;
    currentPage=DISKIIIMAGE;
  
    return ;
}
  
static uint8_t harvey_ball=0;

void updateDiskIIImageScr(uint8_t status,uint8_t trk){
  
    if (currentPage!=DISKIIIMAGE)
      return;
  
    if (currentTrk!=trk || status!=currentStatus){
  
      char tmp[32];
  
      if (currentTrk!=trk){
  
    #ifdef A2F_MODE
            sprintf(tmp,"%02d",trk);
            ssd1306_SetCursor(44,22);
            ssd1306_WriteString(tmp,Font_11x18);
    #else
            sprintf(tmp,"TRACK: %02d",trk);
            displayStringAtPosition(5,3*SCREEN_LINE_HEIGHT,tmp);
    #endif
  
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
        dispIcon(117,30,12);
      }else{
        harvey_ball=0;
        dispIcon(117,30,13);
      }
  
      ssd1306_UpdateScreen();
  
    }
    return;
  }
  

static void pBtnRetDiskIIImageScr(){
    switchPage(MENU,0);
}

static void pBtnEntrDiskIIImageScr(){
    switchPage(IMAGEMENU,NULL);
}


/*
 * 
 *  MENU IMAGE OPTION 
 * 
 */

int8_t currentImageMenuItem=0;

void initDiskIIImageMenuScr(int i){
   
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
 
    primPrepNewScreen("Image menu");
 
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
   
    currentImageMenuItem=i;
 
    primUpdScreen();

    ptrbtnUp=pBtnUpDiskIIImageMenuScr;
    ptrbtnDown=pBtnDownDiskIIImageMenuScr;
    ptrbtnEntr=pBtnEntrDiskIIImageMenuScr;
    ptrbtnRet=pBtnRetDiskIIImageMenuScr;

   currentPage=IMAGEMENU;
 
   return;
 
 }

static void pBtnUpDiskIIImageMenuScr(){
    currentImageMenuItem--;
    initDiskIIImageMenuScr(currentImageMenuItem);
}

static void pBtnDownDiskIIImageMenuScr(){
    currentImageMenuItem++;
    initDiskIIImageMenuScr(currentImageMenuItem);
}

static void pBtnRetDiskIIImageMenuScr(){
    switchPage(DISKIIIMAGE,NULL);
}

static void pBtnEntrDiskIIImageMenuScr(){
    
    switch(currentImageMenuItem){
        
        case 0:                                                   // Add / Remove from Favorite
            toggleAddToFavorite();
            switchPage(DISKIIIMAGE,NULL);
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


static void toggleAddToFavorite(){
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

    initDiskIIImageScr(NULL,0);
}


void initMountImageScr(char * filename){
  
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
    
    primUpdScreen();

    ptrbtnEntr=pBtnEntrMountImageScr;
    ptrbtnUp=pBtnToogleOption;
    ptrbtnDown=pBtnToogleOption;
    ptrbtnRet=pBtnRetMountImageScr;
    currentPage=MOUNT;
  
  }
  
static void toggleMountOption(int i){
    
    ssd1306_SetColor(Inverse);
    ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
    ssd1306_FillRect(30-5,5*SCREEN_LINE_HEIGHT-1,50,9);
    primUpdScreen();
}
  

static uint8_t toggle=1;

static void pBtnToogleOption(){

    if (toggle==1){
        toggle=0;
        toggleMountOption(0);
    }else{
        toggle=1;
        toggleMountOption(1);
    }
}

static void pBtnEntrMountImageScr(){
    if (toggle==0){
      switchPage(FS,currentFullPath);
      toggle=1;                               // rearm toggle switch
    }else{
      log_info("getting here");
      nextAction=IMG_MOUNT;                       // Mounting can not be done via Interrupt, must be done via the main thread
    }
  }

static void pBtnRetMountImageScr(){
    switchPage(DISKIIIMAGE,0);
}
  