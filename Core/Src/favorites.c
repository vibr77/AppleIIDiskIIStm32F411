#include "stdio.h"
#include "list.h"
#include "main.h"
#include "log.h"
#include "cJSON.h"
#include <string.h>
#include "configFile.h"
#include "favorites.h"

extern cJSON *json;
cJSON *favorites = NULL;
list_t * favoritesChainedList=NULL;

enum STATUS wipeFavorites(){
    if (favorites==NULL && getFavorites()==RET_ERR)
        return RET_ERR;
    
    cJSON_DeleteItemFromObject(json, "favorites");
    
    return RET_OK;
}

enum STATUS addToFavorites(char * fullpathImageName){
    
    cJSON *item = NULL;
    cJSON *filename = NULL;

    if (fullpathImageName==NULL){
        log_error("fullpathImageName is null");
        return RET_ERR;
    }

    if (favorites==NULL && getFavorites()==RET_ERR)
        return RET_ERR;

    cJSON_ArrayForEach(item, favorites){
        filename = cJSON_GetObjectItemCaseSensitive(item, "filename");
        if (cJSON_IsString(filename) && (filename->valuestring != NULL && !strcmp(filename->valuestring,fullpathImageName))){
            log_error("favorite item already exist");
            return RET_ERR;
        }
    }

    item = cJSON_CreateObject();
    if (item == NULL){
        log_error("not able to create item");
        return RET_ERR;
    }
    filename = cJSON_CreateString(fullpathImageName);
    cJSON_AddItemToArray(favorites, item);
    cJSON_AddItemToObject(item, "filename", filename);

    return RET_OK;
}

enum STATUS removeFromFavorites(char * fullpathImageName){
    
    cJSON *item = NULL;
    cJSON *filename = NULL;

    if (fullpathImageName==NULL){
        log_error("fullpathImageName is null");
        return RET_ERR;
    }
    if (favorites==NULL && getFavorites()==RET_ERR)
        return RET_ERR;
    int i=0;

    cJSON_ArrayForEach(item, favorites){
        filename = cJSON_GetObjectItemCaseSensitive(item, "filename");
        if (cJSON_IsString(filename) && (filename->valuestring != NULL && !strcmp(filename->valuestring,fullpathImageName))){
            log_info("removing item i:%d",i);
            cJSON_DeleteItemFromArray(favorites,i);
   
            return  RET_OK;
        }
        i++;
    }
    log_error("item not found");
    return RET_ERR;
}

enum STATUS getFavorites(){

    /*
    // For testing only to be removed in production
    cJSON_DeleteItemFromObject(json, "favorites");
    saveConfigFile();
    return RET_OK;
    */

    favorites = cJSON_GetObjectItemCaseSensitive(json, "favorites");
    if (favorites==NULL){
        log_info("favorites JSON item is null -> creating");
        favorites = cJSON_AddArrayToObject(json, "favorites");
        if (favorites==NULL){
            log_error("unable to create favorites in JSON");
            return RET_ERR;
        }
    }

    return RET_OK;
}

uint8_t isFavorite(char * fullpathImageName){
    
    cJSON *item = NULL;
    cJSON *filename = NULL;

    if (fullpathImageName==NULL){
        log_error("fullpathImageName is null");
        return RET_ERR;
    }
    if (favorites==NULL && getFavorites()==RET_ERR)
        return RET_ERR;
    
    cJSON_ArrayForEach(item, favorites){
        filename = cJSON_GetObjectItemCaseSensitive(item, "filename");
        if (cJSON_IsString(filename) && (filename->valuestring != NULL && !strcmp(filename->valuestring,fullpathImageName))){
            return  1;
        }

    }
    
    return 0;
}

enum STATUS buildLstFromFavorites(){
    
    cJSON *item = NULL;
    cJSON *filename = NULL;

    if (favorites==NULL && getFavorites()==RET_ERR)
        return RET_ERR;
    
    if (favoritesChainedList!=NULL)
        list_destroy(favoritesChainedList);

    favoritesChainedList=list_new();
    cJSON_ArrayForEach(item, favorites){
        filename = cJSON_GetObjectItemCaseSensitive(item, "filename");
        list_rpush(favoritesChainedList, list_node_new(filename->valuestring));
    }

    return RET_OK;
}

void printChainedList(){
    if (favoritesChainedList==NULL)
        return;
    uint8_t len=favoritesChainedList->len;
    list_node_t *cItem;
    for (uint8_t i=0;i<len;i++){
        cItem=list_at(favoritesChainedList,i);
        log_info("item:%d %s",i,cItem->val);
    }
    return;
}