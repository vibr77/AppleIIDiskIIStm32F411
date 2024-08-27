
#ifndef disp
#define disp

void updateFSDisplay(int init);

void initScreen();
void clearScreen();
void initFSScreen(char * path);

void initIMAGEScreen(char * imageName,int type);
void updateIMAGEScreen(unsigned int status,int trk);

void initSdEjectScreen();
void mountImageScreen(char * filename);
void toggleMountOption(int i); 

void displayStringAtPosition(int x,int y,char * str);
void inverseStringAtPosition(int lineNumber,int offset);
void clearLineStringAtPosition(int lineNumber,int offset);
void displayFSItem();
void dispIcon(int x,int y,int indx);

#endif