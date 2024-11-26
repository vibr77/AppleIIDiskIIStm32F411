
#include <stdint.h>
#include "list.h"
#ifndef disp
#define disp


enum page{FS,MOUNT,MENU,IMAGE,FAVORITE,CONFIG,IMAGEMENU};
char * getImageNameFromFullPath(char * fullPathImageName);
enum STATUS switchPage(enum page newPage,void * arg);
void updateChainedListDisplay(int init, list_t * lst );



/*      SPLASH SCREEN                    */
void initSplashScreen();

/*      FILESYSTEM SCREEN               */
void processPrevFSItem();
void processNextFSItem();
void processUpdirFSItem();
void processSelectFSItem();

void processToogleOption();
void processMountOption();
void nothing();
void processBtnRet();

/*      MOUNT IMAGE SCREEN              */
void mountImageScreen(char * filename);
void toggleMountOption(int i); 

/*      IMAGE SCREEN                    */
enum STATUS initIMAGEScreen(char * imageName,int type);
void updateIMAGEScreen(uint8_t status,uint8_t trk);
void toggleAddToFavorite();

/*     IMAGE MENU SCREEN                */
void processPreviousImageMenuScreen();
void processNextImageMenuScreen();
void processActiveImageMenuScreen();
void processImageMenuScreen();
void initImageMenuScreen(int i);
void processDisplayImageMenu();

/*      MAIN MENU SCREEN                */
void processPreviousMainMenuScreen();
void processNextMainMenuScreen();
void processActiveMainMenuScreen();
void initMainMenuScreen(int i);

/*      CONFIG MENU SCREEN              */

void processPrevConfigItem();
void processNextConfigItem();
void processSelectConfigItem();
void processReturnConfigItem();

void processBootOption(int arg);
void processSoundEffect();
void processClearprefs();
void processClearFavorites();

void processMakeFs();
void processMakeFsConfirmed();
void processMakeFsBtnRet();

void initConfigMenuScreen(int i);
void updateConfigMenuDisplay(int init);

void initErrorScreen(char * msg);
void initFSScreen(char * path);


/*      FAVORITE SCREEN                 */
void initFavoriteScreen();
void processPrevFavoriteItem();
void processNextFavoriteItem();
void processReturnFavoriteItem();
void processSelectFavoriteItem();

/*      DISPLAY PRIMITIVES              */
void clearScreen();
void dispIcon32x32(int x,int y,uint8_t indx);
void dispIcon24x24(int x,int y,uint8_t indx);
void dispIcon12x12(int x,int y,int indx);
void dispIcon(int x,int y,int indx);

void displayStringAtPosition(int x,int y,char * str);
void inverseStringAtPosition(int lineNumber,int offset);
void clearLineStringAtPosition(int lineNumber,int offset);

#endif