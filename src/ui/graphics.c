/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>
#include <stdlib.h>

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/graphics.h"
#include "ui/inputbox.h"
#include "misc.h"

#include "fonts/NumbersStepanv3.h"
#include "fonts/NumbersStepanv4.h"
#include "fonts/TomThumb.h"
#include "fonts/muHeavy8ptBold.h"
#include "fonts/muMatrix8ptRegular.h"
#include "fonts/symbols.h"


#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

void UI_GenerateChannelString(char *pString, const uint8_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint8_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 3; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[3] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%03u", ChannelNumber + 1);
    } else if (ChannelNumber == 0xFF) {
        strcpy(pString, "NULL");
    } else {
        sprintf(pString, "%03u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            memcpy(buffer + offset, font + index * char_width, char_width);
        }
    }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            memcpy(gFrameBuffer[Line + 0] + ofs, &gFontBig[index][0], 7);
            memcpy(gFrameBuffer[Line + 1] + ofs, &gFontBig[index][7], 7);
        }
    }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
}

void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    const uint8_t pattern = 1 << (y % 8);
    if(black)
        buffer[y/8][x] |= pattern;
    else
        buffer[y/8][x] &= ~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

void PutPixel(uint8_t x, uint8_t y, bool fill) {
    UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
}

void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
    UI_DrawPixelBuffer(&gFrameBuffer[0], x, y, fill);
}

void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                bool statusbar, bool fill) {
    uint8_t c;
    uint8_t pixels;
    const uint8_t *p = (const uint8_t *)pString;

    while ((c = *p++) && c != '\0') {
    c -= 0x20;
    for (int i = 0; i < 3; ++i) {
        pixels = gFont3x5[c][i];
        for (int j = 0; j < 6; ++j) {
        if (pixels & 1) {
            if (statusbar)
            PutPixelStatus(x + i, y + j, fill);
            else
            PutPixel(x + i, y + j, fill);
        }
        pixels >>= 1;
        }
    }
    x += 4;
    }
}
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}


void UI_DisplayPopup(const char *string)
{
    UI_DisplayClear();
    UI_PrintString(string, 9, 118, 2, 8);
    UI_PrintStringSmallNormal("Press EXIT", 9, 118, FRAME_LINES-1);
}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}

// --------------------------------------------------------------------------------------------
#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

static Cursor cursor = {0, 0};

static const GFXfont *fontSmall = &TomThumb;
static const GFXfont *fontMedium = &MuMatrix8ptRegular;
static const GFXfont *fontMediumBold = &muHeavy8ptBold;
static const GFXfont *fontBig = &dig_11;
static const GFXfont *fontBiggest = &dig_14;

void UI_ClearStatus(void) {
  memset(gFrameBuffer[0], 0, sizeof(gFrameBuffer[0]));
}

void UI_ClearScreen(void) {
  for (uint8_t i = 1; i < 8; ++i) {
    memset(gFrameBuffer[i], 0, sizeof(gFrameBuffer[i]));
  }
}

void NPutPixel(uint8_t x, uint8_t y, uint8_t fill) {
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
    return;
  }
  if (fill == C_FILL) {
    gFrameBuffer[y >> 3][x] |= 1 << (y & 7);
  } else if (fill == C_INVERT) {
    gFrameBuffer[y >> 3][x] ^= 1 << (y & 7);
  } else {
    gFrameBuffer[y >> 3][x] &= ~(1 << (y & 7));
  }
}

static void DrawALine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      NPutPixel((uint8_t)y0, (uint8_t)x0, color);
    } else {
      NPutPixel((uint8_t)x0, (uint8_t)y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void NDrawVLine(int16_t x, int16_t y, int16_t h, Color color) {
  DrawALine(x, y, x, y + h - 1, color);
}

void DrawHLine(int16_t x, int16_t y, int16_t w, Color color) {
  DrawALine(x, y, x + w - 1, y, color);
}

void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
  if (x0 == x1) {
    if (y0 > y1)
      _swap_int16_t(y0, y1);
    NDrawVLine(x0, y0, y1 - y0 + 1, color);
  } else if (y0 == y1) {
    if (x0 > x1)
      _swap_int16_t(x0, x1);
    DrawHLine(x0, y0, x1 - x0 + 1, color);
  } else {
    DrawALine(x0, y0, x1, y1, color);
  }
}

void DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color) {
  DrawHLine(x, y, w, color);
  DrawHLine(x, y + h - 1, w, color);
  NDrawVLine(x, y, h, color);
  NDrawVLine(x + w - 1, y, h, color);
}

void FillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color) {
  for (int16_t i = x; i < x + w; i++) {
    NDrawVLine(i, y, h, color);
  }
}

static void m_putchar(int16_t x, int16_t y, unsigned char c, Color color,
                      uint8_t size_x, uint8_t size_y,
                      const GFXfont *gfxFont) {
  c -= gfxFont->first;
  GFXglyph *glyph = &gfxFont->glyph[c];
  const uint8_t *bitmap = gfxFont->bitmap;

  uint16_t bo = glyph->bitmapOffset;
  uint8_t w = glyph->width, h = glyph->height;
  int8_t xo = glyph->xOffset, yo = glyph->yOffset;
  uint8_t xx, yy, bits = 0, bit = 0;
  int16_t xo16 = 0, yo16 = 0;

  if (size_x > 1 || size_y > 1) {
    xo16 = xo;
    yo16 = yo;
  }

  for (yy = 0; yy < h; yy++) {
    for (xx = 0; xx < w; xx++) {
      if (!(bit++ & 7)) {
        bits = bitmap[bo++];
      }
      if (bits & 0x80) {
        if (size_x == 1 && size_y == 1) {
          NPutPixel(x + xo + xx, y + yo + yy, color);
        } else {
          FillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y, size_x,
                   size_y, color);
        }
      }
      bits <<= 1;
    }
  }
}

void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx,
                int16_t *miny, int16_t *maxx, int16_t *maxy, uint8_t textsize_x,
                uint8_t textsize_y, bool wrap, const GFXfont *gfxFont) {

  if (c == '\n') { // Newline?
    *x = 0;        // Reset x to zero, advance y by one line
    *y += textsize_y * gfxFont->yAdvance;
  } else if (c != '\r') { // Not a carriage return; is normal char
    uint8_t first = gfxFont->first, last = gfxFont->last;
    if ((c >= first) && (c <= last)) { // Char present in this font?
      GFXglyph *glyph = &gfxFont->glyph[c - first];
      uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
      int8_t xo = glyph->xOffset, yo = glyph->yOffset;
      if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > LCD_WIDTH)) {
        *x = 0; // Reset x to zero, advance y by one line
        *y += textsize_y * gfxFont->yAdvance;
      }
      int16_t tsx = (int16_t)textsize_x, tsy = (int16_t)textsize_y,
              x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1,
              y2 = y1 + gh * tsy - 1;
      if (x1 < *minx)
        *minx = x1;
      if (y1 < *miny)
        *miny = y1;
      if (x2 > *maxx)
        *maxx = x2;
      if (y2 > *maxy)
        *maxy = y2;
      *x += xa * tsx;
    }
  }
}

static void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1,
                          int16_t *y1, uint16_t *w, uint16_t *h, bool wrap,
                          const GFXfont *gfxFont) {

  uint8_t c; // Current character
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1; // Bound rect
  // Bound rect is intentionally initialized inverted, so 1st char sets it

  *x1 = x; // Initial position is value passed in
  *y1 = y;
  *w = *h = 0; // Initial size is zero

  while ((c = *str++)) {
    // charBounds() modifies x/y to advance for each character,
    // and min/max x/y are updated to incrementally build bounding rect.
    charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy, 1, 1, wrap, gfxFont);
  }

  if (maxx >= minx) {     // If legit string bounds were found...
    *x1 = minx;           // Update x1 to least X coord,
    *w = maxx - minx + 1; // And w to bound rect width
  }
  if (maxy >= miny) { // Same for height
    *y1 = miny;
    *h = maxy - miny + 1;
  }
}

void write(uint8_t c, uint8_t textsize_x, uint8_t textsize_y, bool wrap,
           Color color, const GFXfont *gfxFont) {
  if (c == '\n') {
    cursor.x = 0;
    cursor.y += (int16_t)textsize_y * gfxFont->yAdvance;
  } else if (c != '\r') {
    uint8_t first = gfxFont->first;
    if ((c >= first) && (c <= gfxFont->last)) {
      GFXglyph *glyph = &gfxFont->glyph[c - first];
      uint8_t w = glyph->width, h = glyph->height;
      if ((w > 0) && (h > 0)) { // Is there an associated bitmap?
        int16_t xo = glyph->xOffset;
        if (wrap && ((cursor.x + textsize_x * (xo + w)) > LCD_WIDTH)) {
          cursor.x = 0;
          cursor.y += (int16_t)textsize_y * gfxFont->yAdvance;
        }
        m_putchar(cursor.x, cursor.y, c, color, textsize_x, textsize_y,
                  gfxFont);
      }
      cursor.x += glyph->xAdvance * (int16_t)textsize_x;
    }
  }
}

static void printString(const GFXfont *gfxFont, uint8_t x, uint8_t y,
                        Color color, TextPos posLCR, const char *pattern,
                        va_list args) {
  char String[64] = {'\0'};
  vsnprintf(String, 63, pattern, args);

  int16_t x1, y1;
  uint16_t w, h;
  getTextBounds(String, x, y, &x1, &y1, &w, &h, false, gfxFont);
  if (posLCR == POS_C) {
    x = x - w / 2;
  } else if (posLCR == POS_R) {
    x = x - w;
  }
  cursor.x = x;
  cursor.y = y;
  for (uint8_t i = 0; i < strlen(String); i++) {
    write(String[i], 1, 1, true, color, gfxFont);
  }
}

void PrintSmall(uint8_t x, uint8_t y, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontSmall, x, y, C_FILL, POS_L, pattern, args);
  va_end(args);
}

void PrintSmallEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                  const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontSmall, x, y, color, posLCR, pattern, args);
  va_end(args);
}

void PrintMedium(uint8_t x, uint8_t y, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontMedium, x, y, C_FILL, POS_L, pattern, args);
  va_end(args);
}

void PrintMediumEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                   const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontMedium, x, y, color, posLCR, pattern, args);
  va_end(args);
}

void PrintMediumBold(uint8_t x, uint8_t y, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontMediumBold, x, y, C_FILL, POS_L, pattern, args);
  va_end(args);
}

void PrintMediumBoldEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                       const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontMediumBold, x, y, color, posLCR, pattern, args);
  va_end(args);
}

void PrintBigDigits(uint8_t x, uint8_t y, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontBig, x, y, C_FILL, POS_L, pattern, args);
  va_end(args);
}

void PrintBigDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                      const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontBig, x, y, color, posLCR, pattern, args);
  va_end(args);
}

void PrintBiggestDigits(uint8_t x, uint8_t y, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontBiggest, x, y, C_FILL, POS_L, pattern, args);
  va_end(args);
}

void PrintBiggestDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                          const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(fontBiggest, x, y, color, posLCR, pattern, args);
  va_end(args);
}

void PrintSymbolsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                    const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  printString(&Symbols, x, y, color, posLCR, pattern, args);
  va_end(args);
}
