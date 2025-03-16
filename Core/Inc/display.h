
#include <stdint.h>
#include "defines.h"
#include "list.h"
#include "log.h"
#include "../Src/dispScreen/screen_smartport.h"
#include "../Src/dispScreen/screen_mainmenu.h"
#include "../Src/dispScreen/screen_splash.h"
#include "../Src/dispScreen/screen_makefs.h"
#include "../Src/dispScreen/screen_favorites.h"
#include "../Src/dispScreen/screen_settings.h"
#include "../Src/dispScreen/screen_fs.h"
#include "../Src/dispScreen/screen_diroption.h"
#include "../Src/dispScreen/screen_diskii.h"
#include "../Src/dispScreen/screen_error.h"
#ifndef disp
#define disp

enum EMUL_CMD{EMUL_READ,EMUL_WRITE,EMUL_STATUS};
enum page{FS,MOUNT,MENU,DISKIIIMAGE,FAVORITES,SETTINGS,EMULATIONTYPE,IMAGEMENU,SMARTPORT,SMARTPORT_IMAGEOPTION,SMARTPORT_MOUNT,MAKEFS,DIROPTION,NEWIMAGE};


typedef struct lstItem_s{
    uint8_t type;             // 0 -> simpleLabel; 1-> boolean; 2-> value
    uint8_t icon;
    char title[24];
    void (* triggerfunction)();
    uint8_t arg;
    uint8_t ival;
}listItem_t;

typedef struct dispItem_s{
    uint8_t displayPos;                     // Line on the screen of the item
    uint8_t update;                         // updated 
    uint8_t selected;                       // is this line selected
    uint8_t lstItemIndx;                    // item index in the list                      
    uint8_t status;                         // simple status
    list_node_t *lstItem; 
}dispItem_t;

typedef struct listWidget_s{
    uint8_t dispStartLine;                                           // Line number of the first item to be displayed
    uint8_t hOffset;                        
    uint8_t vOffset;                                        
    uint8_t dispMaxNumLine;                                         // Number of line to be displayed on the screen
    uint8_t dispLineSelected;
    uint8_t currentClistPos;
    uint8_t lstItemCount;
    list_t * lst;
    list_node_t * currentSelectedItem;

    dispItem_t dispItem[SCREEN_MAX_LINE_ITEM];
} listWidget_t;

char * getImageNameFromFullPath(char * fullPathImageName);
enum STATUS switchPage(enum page newPage,void * arg);
void updateChainedListDisplay(int init, list_t * lst );
void nothing();

/*      DISPLAY PRIMITIVES              */
void primUpdListWidget(listWidget_t *lw,int8_t init, int8_t direction);
void primPrepNewScreen(char * szTitle);
void primUpdScreen();

void clearScreen();
void dispIcon32x32(int x,int y,uint8_t indx);
void dispIcon24x24(int x,int y,uint8_t indx);
void dispIcon12x12(int x,int y,int indx);
void dispIcon(int x,int y,int indx);

void displayStringAtPosition(int x,int y,char * str);
void inverseStringAtPosition(int lineNumber,int offset);
void clearLineStringAtPosition(int lineNumber,int offset);

enum STATUS makeScreenShot(uint8_t screenShotIndex);

#endif
