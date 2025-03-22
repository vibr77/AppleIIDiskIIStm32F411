#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "list.h"
#include "screen_settings.h"
#include "display.h"
#include "configFile.h"
#include "log.h"
#include "favorites.h"


// EXTERN DEFINITION 

extern enum page currentPage;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

extern uint8_t emulationType;
extern uint8_t bootImageIndex;
extern uint8_t flgSoundEffect;
extern uint8_t flgWeakBit;

/*
*
* CONFIG MENU SCREEN
*
*/

static void pBtnUpSettingsScr();
static void pBtnDownSettingsScr();
static void pBtnEntrSettingsScr();
static void pBtnRetSettingsScr();

static void pBtnRetSettingEmulationSrc();
static void pBtnEntrSettingEmulationSrc(int arg);

static void pBootOption(int arg);
static void pWeakBit();
static void pSoundEffect();
static void pClearprefs();
static void pClearFavorites();
static void pDispEmulationScreen();
static void pDispMakeFsScr();
static void pBootImageIndex();


listWidget_t settingLw;

void initSettingsScr(uint8_t i){
    
    listItem_t * settingItem;
    uint8_t bootMode=0;

    settingLw.lst=list_new();

    settingLw.currentClistPos=0;
    settingLw.dispLineSelected=0;
    settingLw.dispMaxNumLine=4;
    settingLw.dispStartLine=0;
    settingLw.hOffset=1;
    settingLw.vOffset=5;
    
  
    if (getConfigParamInt("bootMode",(int*)&bootMode)==RET_ERR)
      log_warn("Warning: getting bootMode from Config failed");
    else 
      log_info("bootMode=%d",bootMode);
  

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }

    sprintf(settingItem->title,"Boot last image");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBootOption;
    settingItem->ival=0;
    settingItem->arg=0;
    if (bootMode==settingItem->arg)
        settingItem->ival=1;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }

    sprintf(settingItem->title,"Boot last dir");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBootOption;
    settingItem->ival=0;
    settingItem->arg=1;
    if (bootMode==settingItem->arg)
        settingItem->ival=1;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }

    sprintf(settingItem->title,"Boot favorites");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBootOption;
    settingItem->ival=0;
    settingItem->arg=2;
    if (bootMode==settingItem->arg)
        settingItem->ival=1;

    list_rpush(settingLw.lst, list_node_new(settingItem));
  
    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Emulation type");
    settingItem->type=0;
    settingItem->icon=10;
    settingItem->triggerfunction=pDispEmulationScreen;
    settingItem->ival=0;
    settingItem->arg=2;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Boot Img Index");
    settingItem->type=2;
    settingItem->icon=10;
    settingItem->triggerfunction=pBootImageIndex;
    settingItem->ival=bootImageIndex;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));
 
    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Sound effect");
    settingItem->type=1;
    settingItem->icon=9;
    settingItem->triggerfunction=pSoundEffect;
    settingItem->ival=flgSoundEffect;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));


    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"WeakBit");
    settingItem->type=1;
    settingItem->icon=9;
    settingItem->triggerfunction=pWeakBit;
    settingItem->ival=flgWeakBit;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));
  
    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Clear prefs");
    settingItem->type=0;
    settingItem->icon=8;
    settingItem->triggerfunction=pClearprefs;
    settingItem->ival=0;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Clear favorites");
    settingItem->type=0;
    settingItem->icon=8;
    settingItem->triggerfunction=pClearFavorites;
    settingItem->ival=0;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));
  
    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Make filesystem");
    settingItem->type=0;
    settingItem->icon=11;
    settingItem->triggerfunction=pDispMakeFsScr;
    settingItem->ival=0;
    settingItem->arg=0;
    list_rpush(settingLw.lst, list_node_new(settingItem));
  
    primPrepNewScreen("Settings");    
    
    primUpdListWidget(&settingLw,-1,0);

    ptrbtnUp=pBtnUpSettingsScr;
    ptrbtnDown=pBtnDownSettingsScr;
    ptrbtnEntr=pBtnEntrSettingsScr;
    ptrbtnRet=pBtnRetSettingsScr;
    currentPage=SETTINGS;

    return;
  
}

/**
 * 
 * 
 * EMULATION TYPE SCREEN
 * 
 */

 void initSettingsEmulationScr(uint8_t i){
 
    settingLw.lst=list_new();
    listItem_t * settingItem;
    
    int emulationType=0;
    if (getConfigParamInt("emulationType",&emulationType)==RET_ERR)
      log_warn("Warning: getting emulationType from Config failed");
    else 
      log_info("emulationType=%d",emulationType);
    
    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Disk II");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBtnEntrSettingEmulationSrc;
    settingItem->ival=0;
    settingItem->arg=0;
    if (emulationType==settingItem->arg)
        settingItem->ival=1;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Smartport HD");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBtnEntrSettingEmulationSrc;
    settingItem->ival=0;
    settingItem->arg=1;
    if (emulationType==settingItem->arg)
        settingItem->ival=1;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    settingItem=(listItem_t *)malloc(sizeof(listItem_t));
    if (settingItem==NULL){
        log_error("malloc error listItem_t");
        return;
    }
    sprintf(settingItem->title,"Disk 3.5");
    settingItem->type=1;
    settingItem->icon=10;
    settingItem->triggerfunction=pBtnEntrSettingEmulationSrc;
    settingItem->ival=0;
    settingItem->arg=2;
    if (emulationType==settingItem->arg)
        settingItem->ival=1;
    list_rpush(settingLw.lst, list_node_new(settingItem));

    primPrepNewScreen("Emulation type");
    settingLw.dispLineSelected=0;
    primUpdListWidget(&settingLw,1,0);
    
    ptrbtnRet=pBtnRetSettingEmulationSrc;
    currentPage=EMULATIONTYPE;
}

static uint8_t emulationTypeChanged=0;

static void pBtnRetSettingEmulationSrc(){
    // Cleanup the chainlist
    if (emulationTypeChanged==0){
        list_destroy(settingLw.lst);
        initSettingsScr(4);
    }else{
        HAL_Delay(300);
        NVIC_SystemReset();
    }
}

static void pBtnEntrSettingEmulationSrc(int arg){
  
  for (uint8_t i=0;i<3;i++){
    list_node_t * lsi=list_at(settingLw.lst,i);
    if (lsi==NULL){
        log_error("list item is null");
    }

    listItem_t *item=lsi->val; 
    if (arg==item->arg)
        item->ival=1;
    else
        item->ival=0;
    }

    setConfigParamInt("emulationType",arg);
    saveConfigFile();
    emulationTypeChanged=1;

    primUpdListWidget(&settingLw,-1,0);
    return;

}


static void pBtnUpSettingsScr(){
    primUpdListWidget(&settingLw,-1,-1);
}
  
static void pBtnDownSettingsScr(){
    primUpdListWidget(&settingLw,-1,1);
}

static void pBtnEntrSettingsScr(){
    listItem_t *item= settingLw.currentSelectedItem->val;
    if (item){
        log_info("selected:%s ",item->title);
        item->triggerfunction(item->arg);
    }else{
        log_error("item is null");
    }
  
}
  
static void pBtnRetSettingsScr(){
    list_destroy(settingLw.lst);
    switchPage(MENU,NULL);
}

  
static void pBootOption(int arg){
    
    for (uint8_t i=0;i<3;i++){
        list_node_t * lsi=list_at(settingLw.lst,i);
        if (lsi==NULL){
            log_error("list item is null");
        }

        listItem_t *item=lsi->val; 
        if (arg==item->arg)
            item->ival=1;
        else
            item->ival=0;
    }

    setConfigParamInt("bootMode",arg);
    saveConfigFile();
    
    primUpdListWidget(&settingLw,-1,0);
    return;
}

static void pWeakBit(){
    
    listItem_t *item= settingLw.currentSelectedItem->val;
    if (item==NULL){
        log_error("item is null");
        return;
    } 
    
    if (item->ival==1){
      
      flgWeakBit=0;
    }else{
      flgWeakBit=1;
    }
    setConfigParamInt("weakBit",flgWeakBit);
    saveConfigFile();
    item->ival=flgWeakBit;
    
    primUpdListWidget(&settingLw,-1,0);
}

static void pSoundEffect(){
    
    listItem_t *item= settingLw.currentSelectedItem->val;
    if (item==NULL){
        log_error("item is null");
        return;
    } 
    
    if (item->ival==1){
      
      flgSoundEffect=0;
    }else{
      flgSoundEffect=1;
    }
    setConfigParamInt("soundEffect",flgSoundEffect);
    saveConfigFile();
    item->ival=flgSoundEffect;
    
    primUpdListWidget(&settingLw,-1,0);
}
  
static void pClearprefs(){
    deleteConfigFile();
    displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"Prefs cleared");
    ssd1306_UpdateScreen();
}
  
static void pClearFavorites(){
    wipeFavorites();
    saveConfigFile();
    buildLstFromFavorites();
    displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT+1,"Favorites cleared");
    ssd1306_UpdateScreen();
}
  
static void pDispEmulationScreen(){
    // First Cleanup the ChainList 
    list_destroy(settingLw.lst);
    switchPage(EMULATIONTYPE,0);
    return;
}

static void pDispMakeFsScr(){
    switchPage(MAKEFS,NULL);
    return;
}
 
static void pBootImageIndex(){
   bootImageIndex++;
 
   if (bootImageIndex==5)
     bootImageIndex=1;
   
   listItem_t *item= settingLw.currentSelectedItem->val;
   if (item){
       item->ival=bootImageIndex;
   }    
  
   setConfigParamInt("bootImageIndex",bootImageIndex);
   saveConfigFile();
  
   primUpdListWidget(&settingLw,4,0);
   return;
 }
