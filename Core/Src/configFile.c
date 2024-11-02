#include "configFile.h"
#include "fatfs.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "main.h"

#include <string.h>
#include "cJSON.h"

extern FATFS fs;
const char configFilename[]="sdiskConfig.json";
extern volatile enum FS_STATUS fsState;
cJSON *json;

enum STATUS loadConfigFile(){

    FIL fil; 		                            //File handle
    FRESULT fres;                               //Result after operations
    
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    
    while(fsState!=READY){};
    fsState=BUSY;
    fres = f_open(&fil, configFilename, FA_READ );
    if(fres != FR_OK) {
        log_error("f_open error (%i)\n", fres);
        fsState=READY;
        return RET_ERR;
    }
    
    char * jsonBuffer=(char *)malloc(JSON_BUFFER_SIZE*sizeof(char));
    unsigned int pt;
    
    
    fres = f_read(&fil,jsonBuffer,JSON_BUFFER_SIZE,&pt);
    if(fres != FR_OK){
        log_error("File read Error: (%i)",fres);
        return RET_ERR;
    }
    
    f_close(&fil);
    
    fsState=READY;
    
    json = cJSON_Parse(jsonBuffer);
    if (json==NULL){
        log_error("json==null,creating empty json, saveconfig");
        
        json = cJSON_Parse("{}");
        saveConfigFile();
    }
    log_debug("Config JSON:%s\n",jsonBuffer);
    
    free(jsonBuffer);
    
    if (json == NULL){
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL){
            fprintf(stderr, "Error before: %s\n", error_ptr);
        } 
    }

    return RET_OK;
}


void setConfigFileDefaultValues(){
    json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "currentPath", "");   
}

enum STATUS saveConfigFile(){
    
    FIL fil; 		                            //File handle
    FRESULT fres;                               //Result after operations

    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    while(fsState!=READY){};

    fsState=BUSY;
    fres = f_open(&fil, configFilename, FA_WRITE | FA_CREATE_ALWAYS);
    if(fres != FR_OK) {
        log_error("f_open error (%i)\n", fres);
        fsState=READY;
        return RET_ERR;
    }
    
    char * jsonBuffer=cJSON_PrintUnformatted(json);
    unsigned int pt;
    fres = f_write(&fil,jsonBuffer,1024,&pt);
    if(fres != FR_OK){
        log_error("File read Error: (%i)",fres);
        return RET_ERR;
    }
    f_close(&fil);
    
    fsState=READY;

    printf("json:%s\n",jsonBuffer);
    return RET_OK;
}

void cleanJsonMem(){
    // json_value_free(root_value);
}

const char * getConfigParamStr(char * key){
    
    const cJSON *value = NULL;
    if (key==NULL){
        return NULL;
    }
    value = cJSON_GetObjectItemCaseSensitive(json, key);
    if (cJSON_IsString(value) && (value->valuestring != NULL)){
        
        printf("getParam:%s %s\n",key,value->valuestring);
        return value->valuestring;
    }else
        return NULL;
    
}

enum STATUS getConfigParamInt(char * key,int * value){
    
    const cJSON *obj = NULL;
    if (key==NULL){
        return RET_ERR;
    }
    obj = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsNumber(obj)){
        printf("getParam:%s %d\n",key,obj->valueint);
        *value= obj->valueint;
        return RET_OK;
    }else
        return RET_ERR; 
}

enum STATUS setConfigParamStr(char * key,char * value){
    
    cJSON *obj = NULL;

    obj = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsString(obj))
        cJSON_SetValuestring(obj, value);
    else
        cJSON_AddStringToObject(json, key, value);

    return RET_OK;
   
}

enum STATUS setConfigParamInt(char * key,int value){
    if (key==NULL){
        return RET_ERR;
    }

    cJSON *obj = NULL;

    obj = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsNumber(obj))
        cJSON_SetIntValue(obj, value);
    else
        cJSON_AddNumberToObject(json, key, value);

    return RET_OK;
}



