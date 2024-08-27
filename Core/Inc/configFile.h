
#include "main.h"
#ifndef CONFIG_FI

#define CONFIG_FI

#define maxLength 512


enum STATUS loadConfigFile();
enum STATUS saveConfigFile();

void cleanJsonMem();
void dumpConfigParams();

void setConfigFileDefaultValues();

const char * getConfigParamStr(char * key);
enum STATUS getConfigParamInt(char * key,int value);

enum STATUS setConfigParamStr(char * key,char * value);
enum STATUS setConfigParamInt(char * key,int value);

#endif