#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "screen_splash.h"
#include "display.h"

#ifdef A2F_MODE
#include "a2f.h"
#endif

/**
 * 
 * SPLASH SCREEN
 * 
 */


// EXTERN DEFINITION 

extern enum page currentPage;

/* BTN FUNCTION POINTER CALLED FROM IRQ*/
extern void (*ptrbtnUp)(void *);                                 
extern void (*ptrbtnDown)(void *);
extern void (*ptrbtnEntr)(void *);
extern void (*ptrbtnRet)(void *);



#ifdef A2F_MODE
void initSplash(){
  ssd1306_Init();
//  ssd1306_FlipScreenVertically(); 
  ssd1306_Clear();
  ssd1306_SetColor(White);
  ssd1306_UpdateScreen();
  ssd1306_DrawBitmap(1,1,128,64,a2fSplash);
  for (int i=65; i>1; i-=4){
    ssd1306_Clear();
    ssd1306_DrawBitmap(1,i,128,65-i,a2fSplash);
    ssd1306_UpdateScreen();
    HAL_Delay(50);
  }  
  displayStringAtPosition(1,1*SCREEN_LINE_HEIGHT,"VIBR SmartDisk][");
  displayStringAtPosition(1,6*SCREEN_LINE_HEIGHT,_VERSION);
 
  primUpdScreen();
  HAL_Delay(500);
  ssd1306_Clear();
}

#else
void initSplash(){
  ssd1306_Init(); 
  ssd1306_FlipScreenVertically();
  ssd1306_Clear();
  ssd1306_SetColor(White);
  dispIcon32x32(1,15,0);
  displayStringAtPosition(35,3*SCREEN_LINE_HEIGHT,"SmartDisk ][");
  displayStringAtPosition(78,6*SCREEN_LINE_HEIGHT,_VERSION);

  primUpdScreen();
}
#endif