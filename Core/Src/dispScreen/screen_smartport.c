#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_smartport.h"
#include "display.h"
#include "configFile.h"
#include "emul_smartport.h"
#include "favorites.h"

/**
 * 
 * SMARTPORT HD EMULATION MAIN SCREEN
 * 
 */

static uint8_t harvey_ball=0;

// EXTERN DEFINITION 

extern enum page currentPage;
extern char *smartPortHookFilename;
extern enum action nextAction;
extern uint8_t bootImageIndex;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

static void pBtnRetSmartPortHD();
static void pBtnUpSmartPortHD();
static void pBtnDownSmartPortHD();
static void pBtnEntrSmartPortHD();

static void updateSelectSmartPortHD(uint8_t indx);

static void pBtnEntrSmartPortHDImageOption();

static void pBtnEntrSmartportMountImageScr();
static void pBtnRetSmartportMountImageScr();
static void pBtnDownOptionSmartPortImage();
static void pBtnUpOptionSmartPortImage();
static void toggleAddToFavorite();
static void pBtnDownOptionSmartPortImage2();
static void pBtnUpOptionSmartPortImage2();
static void pBtnEntrSmartPortHDImageOption2();
static void processSmartPortImageOptionRetScreen2();

void static showFavIcon();
static void toggleMountOption(int i);
static void pBtnToogleOption();

static uint8_t currentSmartPortImageIndex=4;                // the 4 Value enable the selection to disappear
static uint8_t previousSmartPortImageIndex=4;
char * partititionTab[MAX_PARTITIONS];

uint8_t isCurrentImageFavorite=0;

uint8_t fsHookedPartition=0x0;                           // Partition Number when navigating the FS to save the index
char * fsHookedFilename=NULL;

void initSmartPortHDScr(){

    clearScreen();
    ssd1306_SetColor(White);
    dispIcon32x32(1,1,1);
    displayStringAtPosition(35,1*SCREEN_LINE_HEIGHT,"SMARTPORT HD");
    primUpdScreen();
    previousSmartPortImageIndex=-1;
    currentSmartPortImageIndex=4;

    ptrbtnUp=pBtnUpSmartPortHD;
    ptrbtnDown=pBtnDownSmartPortHD;
    ptrbtnEntr=pBtnEntrSmartPortHD;
    ptrbtnRet=pBtnRetSmartPortHD;
    currentPage=SMARTPORT;
  
  }


void setImageTabSmartPortHD(char * fileTab[4],uint8_t bootImageIndex){

  for (uint8_t i=0;i<MAX_PARTITIONS;i++){
    partititionTab[i]=fileTab[i];
  }

  updateImageSmartPortHD();
}

void updateImageSmartPortHD(){

    // Display the 4 image Filename without with extension and only 10 char.
    
    char tmp[22]; 
    ssd1306_SetColor(White);
    char * filename;
    for (uint8_t i=0;i<4;i++){
      
      filename=partititionTab[i];

      if (filename!=NULL){
        int8_t len=strlen(filename);
        int8_t j=0;

        for (j=len-1;j>=0;j--){
          if (filename[j]=='/'){
            filename=filename+j+1;
          }
        }

        len=strlen(filename);
        // Step 1 Remove the Extension of the file
        for (j=len-1;j>0;j--){
          if (filename[j]=='.'){
            len=j+1;
          }
        }
        
        
        // Step 2 Cap the length of string to be displayed
        if (len>16)
          len=16;
  
        filename[0]=toupper(filename[0]);
    
        if ((i+1)==bootImageIndex)
          snprintf(tmp,len+2,"*:%s",filename);
        else
          snprintf(tmp,len+2,"%d:%s",i+1,filename);
      
        displayStringAtPosition(1,(3+i)*SCREEN_LINE_HEIGHT,tmp);
      }else{
        if ((i+1)==bootImageIndex)
            sprintf(tmp,"*:[SELECT]          ");
        else
            sprintf(tmp,"%d:[SELECT]         ",i+1);
        displayStringAtPosition(1,(3+i)*SCREEN_LINE_HEIGHT,tmp); 
      }
    }
    
    primUpdScreen();
  }
  
void updateCommandSmartPortHD(uint8_t imageIndex, uint8_t cmd ){

  /**
   * Change Harvey ball (toogle)
   * display cmd letter in front of the Partition image
   */
  
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
    displayStringAtPosition(128-2*7,(3+i)*SCREEN_LINE_HEIGHT,szTmp);              // 2nd half of the screen Line 3 & 4 
  }

  if (cmd == EMUL_READ){
    sprintf(szTmp,"RD");
  }else if (cmd == EMUL_WRITE){
    sprintf(szTmp,"WR");                                                          // Need to change to short instead of printing int 0-65000
  }else if (cmd== EMUL_STATUS){
    sprintf(szTmp,"I ");
  }else{
    sprintf(szTmp,"  ");
  }

  displayStringAtPosition(128-2*7,(3+imageIndex)*SCREEN_LINE_HEIGHT,szTmp);      // 2nd half of the screen Line 3 & 4 
  primUpdScreen();

}
  
static void pBtnRetSmartPortHD(){
  switchPage(MENU,NULL);
}
  


void pBtnUpSmartPortHD(){
  previousSmartPortImageIndex=currentSmartPortImageIndex;
  if (currentSmartPortImageIndex==0)
    currentSmartPortImageIndex=MAX_PARTITIONS;
  else
    currentSmartPortImageIndex--;
  
  updateSelectSmartPortHD(currentSmartPortImageIndex);
}

static void pBtnDownSmartPortHD(){
  previousSmartPortImageIndex=currentSmartPortImageIndex;
  if (currentSmartPortImageIndex==MAX_PARTITIONS)
    currentSmartPortImageIndex=0;
  else
    currentSmartPortImageIndex++;
  
  updateSelectSmartPortHD(currentSmartPortImageIndex);
}

static void updateSelectSmartPortHD(uint8_t indx){
  log_info("prev:%d current:%d",previousSmartPortImageIndex,currentSmartPortImageIndex);
  
  ssd1306_SetColor(Inverse);
  if (previousSmartPortImageIndex!=4)
    ssd1306_FillRect(1,(3+previousSmartPortImageIndex)*SCREEN_LINE_HEIGHT-1,126,9);
  if (currentSmartPortImageIndex!=4)
    ssd1306_FillRect(1,(3+currentSmartPortImageIndex)*SCREEN_LINE_HEIGHT-1,126,9);
  primUpdScreen();
}

static  void pBtnEntrSmartPortHD(){
  if (currentSmartPortImageIndex!=4){
    fsHookedPartition=currentSmartPortImageIndex;
    if (partititionTab[currentSmartPortImageIndex]!=NULL){                          // If an image exists then display the option menu
      log_info("selected %d",currentSmartPortImageIndex);
      switchPage(SMARTPORT_IMAGEOPTION,0);
    }else{                                                                          // Otherwise display directly the FS screen to select an image
      initSmartPortHDImageOption2Scr();
    }
  }
}
  
/* SMARTPORTHD IMAGE OPTION  */

listWidget_t imageOptionLw;

void initSmartPortHDImageOptionScr(){

  primPrepNewScreen("Smartport Image");


  listItem_t * optionItem;
   
  imageOptionLw.lst=list_new();

  imageOptionLw.currentClistPos=0;
  imageOptionLw.dispLineSelected=0;
  imageOptionLw.dispMaxNumLine=4;
  imageOptionLw.dispStartLine=0;
  imageOptionLw.hOffset=1;
  imageOptionLw.vOffset=5;

  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"Unmount image");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=0;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));

  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"Select image");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=1;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));

  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"First to boot");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=2;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));

  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"Toggle favorite");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=3;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));

  /*
  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"New image");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=4;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));
  */
 
  primUpdListWidget(&imageOptionLw,0,0);
  
  char * fn=partititionTab[fsHookedPartition];
  if (isFavorite(fn)==0)
    isCurrentImageFavorite=0;
  else
    isCurrentImageFavorite=1;

  showFavIcon();

  ptrbtnUp=pBtnUpOptionSmartPortImage;
  ptrbtnDown=pBtnDownOptionSmartPortImage;
  ptrbtnEntr=pBtnEntrSmartPortHDImageOption;
  ptrbtnRet=pBtnRetSmartPortHD;
  currentPage=SMARTPORT_IMAGEOPTION;

}
  
static void pBtnUpOptionSmartPortImage(){
  primUpdListWidget(&imageOptionLw,-1,-1);
  
  showFavIcon();
}

static void pBtnDownOptionSmartPortImage(){
  primUpdListWidget(&imageOptionLw,-1,1);
  showFavIcon();
}

void static showFavIcon(){
  if (isCurrentImageFavorite==1)
    dispIcon12x12(115,18,0);
  else
    dispIcon12x12(115,18,1);
  primUpdScreen();
}

void processSmartPortImageOptionRetScreen(){
  switchPage(SMARTPORT,NULL); 
}

static void pBtnEntrSmartPortHDImageOption(){
  listItem_t * optionItem=imageOptionLw.currentSelectedItem->val;
  if (optionItem){
    switch(optionItem->ival){
      case 0:
        SmartPortUnMountImageFromIndex(fsHookedPartition);
        char key[17];
        sprintf(key,"smartport_vol%02d",fsHookedPartition);
        setConfigParamStr(key,"");
        saveConfigFile();
        switchPage(SMARTPORT,NULL); 
    
        partititionTab[fsHookedPartition]=NULL;
        updateImageSmartPortHD();
        break;
      case 1:
        initSmartPortHDImageOption2Scr();
        break;
      break;
      case 2:
        setConfigParamInt("bootImageIndex",fsHookedPartition+1);
        bootImageIndex=fsHookedPartition+1;
        saveConfigFile();
        initSmartPortHDScr();
        updateImageSmartPortHD();
      break;
      case 3:
        toggleAddToFavorite();
        break;
      case 4:
        toggleAddToFavorite();
        break;
      default:
        log_error("not managed");
    }
  }
}


static void toggleAddToFavorite(){
  char * fn=partititionTab[fsHookedPartition];
  if (isFavorite(fn)==0){
      log_info("add from Favorite:%s",fn);
      if (addToFavorites(fn)==RET_OK)
      isCurrentImageFavorite=1;
  }
  else{
      log_info("remove from Favorite:%s",fn);
      if (removeFromFavorites(fn)==RET_OK)
        isCurrentImageFavorite=0;
  }
 
  if (isCurrentImageFavorite==1)
      dispIcon12x12(115,18,0);
    else
      dispIcon12x12(115,18,1);
  
  primUpdScreen();

  buildLstFromFavorites();
  saveConfigFile();

  
}

void initSmartPortHDImageOption2Scr(){

  primPrepNewScreen("Select image from");

  listItem_t * optionItem;
   
  imageOptionLw.lst=list_new();

  imageOptionLw.currentClistPos=0;
  imageOptionLw.dispLineSelected=0;
  imageOptionLw.dispMaxNumLine=4;
  imageOptionLw.dispStartLine=0;
  imageOptionLw.hOffset=1;
  imageOptionLw.vOffset=5;

  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"Favorites");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=0;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));
  
  optionItem=(listItem_t *)malloc(sizeof(listItem_t));
  if (optionItem==NULL){
      log_error("malloc error listItem_t");
      return;
  }

  sprintf(optionItem->title,"File manager");
  optionItem->type=0;
  optionItem->icon=0;
  optionItem->triggerfunction=NULL;
  optionItem->ival=1;
  optionItem->arg=0;
  
  list_rpush(imageOptionLw.lst, list_node_new(optionItem));
  primUpdListWidget(&imageOptionLw,0,0);
  
  primUpdScreen();

  ptrbtnUp=pBtnUpOptionSmartPortImage2;
  ptrbtnDown=pBtnDownOptionSmartPortImage2;
  ptrbtnEntr=pBtnEntrSmartPortHDImageOption2;
  ptrbtnRet=processSmartPortImageOptionRetScreen2;
    ;
  currentPage=SMARTPORT_IMAGEOPTIONLV2;
}

static void pBtnUpOptionSmartPortImage2(){
  primUpdListWidget(&imageOptionLw,-1,-1);
}

static void pBtnDownOptionSmartPortImage2(){
  primUpdListWidget(&imageOptionLw,-1,1);
}
  

void processSmartPortImageOptionRetScreen2(){
  switchPage(SMARTPORT,NULL); 
}

static void pBtnEntrSmartPortHDImageOption2(){
  listItem_t * optionItem=imageOptionLw.currentSelectedItem->val;
  if (optionItem){
    switch(optionItem->ival){
      case 0:
        switchPage(FAVORITES,NULL);
        break;
      case 1:
        log_info("h1");
        nextAction=FSDISP;
        break;

      default:
        log_error("not managed");
    }
  }
  

}



void initSmartportMountImageScr(char * filename){
  
  char tmp[28];
  char tmp2[28];
  int i=0;

  if (filename==NULL)
    return;
  
  clearScreen();
  i=strlen(filename);

  fsHookedFilename=filename;
 
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-truncation"
  
  char * fn=getImageNameFromFullPath(filename);
  
  snprintf(tmp,18,"%s",fn);
  if (i>20)
    snprintf(tmp2,23,"%s...",fn);
  else
    snprintf(tmp2,23,"%s",fn);
  #pragma GCC diagnostic pop
  
  ssd1306_SetColor(White);
  displayStringAtPosition(5,1*SCREEN_LINE_HEIGHT,"Mounting:");
  displayStringAtPosition(5,2*SCREEN_LINE_HEIGHT,tmp2);
  
  displayStringAtPosition(30,4*SCREEN_LINE_HEIGHT,"YES");
  displayStringAtPosition(30,5*SCREEN_LINE_HEIGHT,"NO");
  
  ssd1306_SetColor(Inverse);
  ssd1306_FillRect(30-5,4*SCREEN_LINE_HEIGHT-1,50,9);
  
  primUpdScreen();

  ptrbtnEntr=pBtnEntrSmartportMountImageScr;
  ptrbtnUp=pBtnToogleOption;
  ptrbtnDown=pBtnToogleOption;
  ptrbtnRet=pBtnRetSmartportMountImageScr;
  currentPage=SMARTPORT_MOUNT;

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

static void pBtnEntrSmartportMountImageScr(){
  if (toggle==0){
    switchPage(FS,0x0);
    toggle=1;                               // rearm toggle switch
  }else{
    char key[17];
    sprintf(key,"smartport_vol%02d",fsHookedPartition);
    if (fsHookedFilename==NULL){
      log_error("fs returning filename is null");
      return;
    }

    setConfigParamStr(key,fsHookedFilename);
    saveConfigFile();
    nextAction = SMARTPORT_IMGMOUNT;

  }
}

static void pBtnRetSmartportMountImageScr(){
  switchPage(SMARTPORT,0);
}
  