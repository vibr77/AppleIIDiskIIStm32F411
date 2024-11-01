

#include <stdint.h>
#ifndef fav
#define fav

enum STATUS wipeFavorites();
enum STATUS addToFavorites(char * fullpathImageName);
enum STATUS removeFromFavorites(char * fullpathImageName);
enum STATUS getFavorites();
uint8_t isFavorite(char * fullpathImageName);
enum STATUS buildLstFromFavorites();
void printChainedList();

#endif