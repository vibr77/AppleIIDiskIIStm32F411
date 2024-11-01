
#include <stdint.h>
#include "list.h"
#ifndef disp
#define disp

void updateFSDisplay(int init);
void updateChainedListDisplay(int init, list_t * lst );
void dispIcon(int x,int y,int indx);
void mountImageScreen(char * filename);
void toggleMountOption(int i); 
void clearScreen();
void initScreen();

enum STATUS initIMAGEScreen(char * imageName,int type);

void updateIMAGEScreen(uint8_t status,uint8_t trk);

void processPreviousMainMenuScreen();
void processNextMainMenuScreen();
void processActiveMainMenuScreen();
void initMainMenuScreen(int i);

void initConfigMenuScreen(int i);

void initSdEjectScreen();
void initFSScreen(char * path);
void initFavoriteScreen();

void displayStringAtPosition(int x,int y,char * str);
void inverseStringAtPosition(int lineNumber,int offset);
void clearLineStringAtPosition(int lineNumber,int offset);
void displayFSItem();

#endif