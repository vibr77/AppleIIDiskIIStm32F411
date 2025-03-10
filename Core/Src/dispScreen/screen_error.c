#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_error.h"
#include "display.h"

/**
 * 
 * DIRECTORY SCREEN ERROR 
 * 
 */

// EXTERN DEFINITION 

extern enum page currentPage;
/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);

void initErrorScr(char * msg){

    primPrepNewScreen("Warning");
    displayStringAtPosition(30,3*SCREEN_LINE_HEIGHT, "ERROR:");
    displayStringAtPosition(30,4*SCREEN_LINE_HEIGHT, msg);
    dispIcon24x24(5,22,1);
    primUpdScreen();

}