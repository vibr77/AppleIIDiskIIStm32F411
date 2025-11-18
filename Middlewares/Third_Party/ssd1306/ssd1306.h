/*
 * ssd1306.h
 *
 *  Created on: 14/04/2018
 *  Update on: 10/04/2019
 *      Author: Andriy Honcharenko
 *      version: 2
 *
 *  Modify on: 22/10/2021
 *      Author: Roberto Benjami
 *  Added features in DMA mode:
 *  - ssd1306_UpdateScreen works without blocking
 *  - you can query that UpdateScreen is complete (if ssd1306_UpdateScreenCompleted() == 1)
 *  - callback function if UpdateScreen is complete (ssd1306_UpdateCompletedCallback)
 *  Added features in DMA mode with continuous display update:
 *  - enable continuous display update
 *  - disable continuous display update
 *  - enable raster interrupt(s) of PAGEx (you can set which PAGE(s) )
 */

#ifndef SSD1306_H_
#define SSD1306_H_

#include "ssd1306_defines.h"
#include "fonts.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>

// I2c address
#define SSD1306_I2C_ADDR       SSD1306_ADDRESS << 1 // 0x3C << 1 = 0x78

#ifdef  SSD1306_128X64
#define SSD1306_GEOMETRY       GEOMETRY_128_64
// SSD1306 width in pixels
#define SSD1306_WIDTH          128
// SSD1306 LCD height in pixels
#define SSD1306_HEIGHT         64
#define SSD1306_PAGES          8
#define SSD1306_MARQUEE_MAXCHAR 64
#elif defined(SSD1306_128X32)
#define SSD1306_GEOMETRY       GEOMETRY_128_32
// SSD1306 width in pixels
#define SSD1306_WIDTH          128
// SSD1306 LCD height in pixels
#define SSD1306_HEIGHT         32
#endif

// SSD1306 LCD Buffer Size
#define SSD1306_BUFFER_SIZE   (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

// Display commands
#define CHARGEPUMP            0x8D
#define COLUMNADDR            0x21
#define COMSCANDEC            0xC8
#define COMSCANINC            0xC0
#define DISPLAYALLON          0xA5
#define DISPLAYALLON_RESUME   0xA4
#define DISPLAYOFF            0xAE
#define DISPLAYON             0xAF
#define EXTERNALVCC           0x01
#define INVERTDISPLAY         0xA7
#define MEMORYMODE            0x20
#define NORMALDISPLAY         0xA6
#define PAGEADDR              0x22
#define SEGREMAP              0xA0
#define SETCOMPINS            0xDA
#define SETCONTRAST           0x81
#define SETDISPLAYCLOCKDIV    0xD5
#define SETDISPLAYOFFSET      0xD3
#define SETHIGHCOLUMN         0x10
#define SETLOWCOLUMN          0x00
#define SETMULTIPLEX          0xA8
#define SETPRECHARGE          0xD9
#define SETSEGMENTREMAP       0xA1
#define SETSTARTLINE          0x40
#define SETVCOMDETECT         0xDB
#define SWITCHCAPVCC          0x02

#define SWAP_INT16_T(a, b) { int16_t t = a; a = b; b = t; }
//
//  Enumeration for screen colors
//
typedef enum {
  Black = 0x00,  // Black color, no pixel
  White = 0x01,  // Pixel is set. Color depends on LCD
  Inverse = 0x02
} SSD1306_COLOR;

typedef enum {
  GEOMETRY_128_64 = 0,
  GEOMETRY_128_32 = 1
} SSD1306_Geometry;
//
//  Struct to store transformations
//

typedef struct {
  uint8_t x;                                 // X position of the marquee on the screen          
  uint8_t y;                                 // Y position of the marquee on the screen
  int8_t width;                              // Width of the marquee area in pixels visible+hidden
  uint8_t visibleWidth;                      // Width of the visible area in pixels    
  int8_t scrollPos;                          // Current scroll position (active column)
  uint8_t scrollSpeed;                       // Scroll speed (number of updates before moving)
  uint8_t scrollDirection;                   // Scroll direction: 0 = left to right, 1 = right to left
  uint8_t scrollCounter;                     // Scroll counter to manage back and forth scrolling
  uint8_t scrollMaxBF;                       // Maximum Back & Forth rotations
  uint8_t scrollType;                        // Scroll type: 0 = continuous, 1 back and forth
 
  uint8_t inverted;                          // Inversion flag 
  uint8_t fullLineInverted;
  
  char *text;                                // Text to be displayed in the marquee    
  uint8_t *renderBuffer;                     // Buffer to hold the rendered text bitmap
  int renderTextWidth;                   // Width of the rendered text bitmap in pixels 
  uint8_t noUpdateDisp;
  uint8_t dispUpdateReady;
  SSD1306_COLOR Color;
  FontDef *Font;
  uint8_t initialized;
  uint8_t txtrendered;

} SSD1306_MARQUEE_t;


void ssd1306_marqueeInit( SSD1306_MARQUEE_t* marquee, FontDef Font);
void ssd1306_marqueeDeInit( SSD1306_MARQUEE_t* marquee);
void ssd1306_marquee_build_text_bitmap(SSD1306_MARQUEE_t* marquee);
void ssd1306_marquee_display(SSD1306_MARQUEE_t* marquee);


typedef struct {
  uint16_t      CurrentX;
  uint16_t      CurrentY;
  uint8_t       Inverted;
  SSD1306_COLOR Color;
  uint8_t       Initialized;
} SSD1306_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} SSD1306_VERTEX;

//  Definition of the i2c port in main
extern I2C_HandleTypeDef SSD1306_I2C_PORT;

/* Private function prototypes -----------------------------------------------*/
uint16_t ssd1306_GetWidth(void);
uint16_t ssd1306_GetHeight(void);
SSD1306_COLOR ssd1306_GetColor(void);
void ssd1306_SetColor(SSD1306_COLOR color);
uint8_t ssd1306_Init(void);
void ssd1306_Fill(void);
void ssd1306_DrawPixel(uint8_t x, uint8_t y);
void ssd1306_DrawBitmap(uint8_t X, uint8_t Y, uint8_t W, uint8_t H, const uint8_t* pBMP);
void ssd1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void ssd1306_DrawVerticalLine(int16_t x, int16_t y, int16_t length);
void ssd1306_DrawHorizontalLine(int16_t x, int16_t y, int16_t length);
void ssd1306_DrawRect(int16_t x, int16_t y, int16_t width, int16_t height);
void ssd1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3);
void ssd1306_FillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height);
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep);
void ssd1306_DrawCircle(int16_t x0, int16_t y0, int16_t radius);
void ssd1306_FillCircle(int16_t x0, int16_t y0, int16_t radius);
void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size);
void ssd1306_DrawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads);
void ssd1306_DrawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress);
char ssd1306_WriteChar(char ch, FontDef Font);
char ssd1306_WriteString(char* str, FontDef Font);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_Clear(void);



void ssd1306_WriteCommand(uint8_t command);

#define ssd1306_DisplayOn()             ssd1306_WriteCommand(DISPLAYON)
#define ssd1306_DisplayOff()            ssd1306_WriteCommand(DISPLAYOFF)
#define ssd1306_InvertDisplay()         ssd1306_WriteCommand(INVERTDISPLAY)
#define ssd1306_NormalDisplay()         ssd1306_WriteCommand(NORMALDISPLAY)
#define ssd1306_ResetOrientation()      { ssd1306_WriteCommand(SEGREMAP); ssd1306_WriteCommand(COMSCANINC); }
#define ssd1306_FlipScreenVertically()  { ssd1306_WriteCommand(SEGREMAP | 0x01); ssd1306_WriteCommand(COMSCANDEC); }
#define ssd1306_MirrorScreen()          { ssd1306_WriteCommand(SEGREMAP | 0x01); ssd1306_WriteCommand(COMSCANINC); }
#define ssd1306_MirrorFlipScreen()      { ssd1306_WriteCommand(SEGREMAP); ssd1306_WriteCommand(COMSCANDEC); }

#if  SSD1306_USE_DMA == 0
void ssd1306_UpdateScreen(void);      /* copy the contents of the Screenbuffer (SSD1306_Buffer) to the display */
#define ssd1306_UpdateScreenCompleted() 1
#define ssd1306_ContUpdateEnable()
#define ssd1306_ContUpdateDisable()
#define ssd1306_SetRasterInt(r)
#elif SSD1306_USE_DMA == 1
#if   SSD1306_CONTUPDATE == 0
void ssd1306_UpdateScreen(void);      /* copy the contents of the Screenbuffer (SSD1306_Buffer) to the display */
char ssd1306_UpdateScreenCompleted(void); /* asks if the update is already complete (0:not completed, 1:completed) */
__weak void ssd1306_UpdateCompletedCallback(void); /* you can create a function for the end of the update (attention!: interrupt function) */
#define ssd1306_ContUpdateEnable()
#define ssd1306_ContUpdateDisable()
#define ssd1306_SetRasterInt(r)
#elif SSD1306_CONTUPDATE == 1
#define ssd1306_UpdateScreen()
#define ssd1306_UpdateScreenCompleted() 1
void ssd1306_ContUpdateEnable(void);  /* enable the continuous dsplay update in background (use DMA and interrupt) */
void ssd1306_ContUpdateDisable(void); /* disable the continuous dsplay update in background */
void ssd1306_SetRasterInt(uint8_t r); /* enable raster interrupt(s) of PAGEx (0:NONE, 1:PAGE0, 2:PAGE1, 4:PAGE2 ... 128:PAGE7, 255:All_PAGES) */
__weak void ssd1306_RasterIntCallback(uint8_t r); /* 0:At the beginning of PAGE0, 1:PAGE1, 2:PAGE2 ... 7:PAGE7 (attention!: interrupt function) */
#endif
#endif

#endif /* SSD1306_H_ */
