
#include "main.h"
#ifndef CONFIG_FI

#define CONFIG_FI

enum STATUS loadConfigFile();
enum STATUS saveConfigFile();
enum STATUS deleteConfigFile();

void cleanJsonMem();
void dumpConfigParams();

void setConfigFileDefaultValues();

const char * getConfigParamStr(char * key);
enum STATUS getConfigParamUInt8(char * key,uint8_t * value);
enum STATUS getConfigParamInt(char * key,int * value);

enum STATUS setConfigParamStr(char * key,char * value);
enum STATUS setConfigParamInt(char * key,int value);

#endif