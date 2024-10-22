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

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "gfxfont.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

void UI_GenerateChannelString(char *pString, const uint8_t Channel);
void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint8_t ChannelNumber);
void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width);
void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t *buffer);
void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer);
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center);

void UI_DisplayPopup(const char *string);

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black);
//void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void PutPixel(uint8_t x, uint8_t y, bool fill);
void PutPixelStatus(uint8_t x, uint8_t y, bool fill);
void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool fill);
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);

void UI_DisplayClear();

// ---------------------------------------------

typedef enum {
  POS_L,
  POS_C,
  POS_R,
} TextPos;

typedef enum {
  C_CLEAR,
  C_FILL,
  C_INVERT,
} Color;

typedef enum {
  SYM_CONVERTER = 0x30,
  SYM_MESSAGE = 0x31,
  SYM_KEY_LOCK = 0x32,
  SYM_CROSS = 0x33,
  SYM_LOCK = 0x34,
  SYM_EEPROM_R = 0x35,
  SYM_EEPROM_W = 0x36,
  SYM_BEEP = 0x37,
  SYM_ANALOG = 0x38,
  SYM_DIGITAL = 0x39,
  SYM_HEART = 0x3A,
  SYM_MONITOR = 0x3B,
  SYM_BROADCAST = 0x3C,
  SYM_SCAN = 0x3D,
  SYM_NO_LISTEN = 0x3E,
  SYM_LOOT = 0x3F,
  SYM_FC = 0x40,
  SYM_BEACON = 0x41,
} Symbol;

typedef struct {
  uint8_t x;
  uint8_t y;
} Cursor;

void UI_ClearStatus();
void UI_ClearScreen();

void NPutPixel(uint8_t x, uint8_t y, uint8_t fill);

void NDrawVLine(int16_t x, int16_t y, int16_t h, Color color);
void DrawHLine(int16_t x, int16_t y, int16_t w, Color color);
void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
void DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);
void FillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);

void PrintSmall(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintMedium(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintMediumBold(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintBigDigits(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintBiggestDigits(uint8_t x, uint8_t y, const char *pattern, ...);

void PrintSmallEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);
void PrintMediumEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);
void PrintMediumBoldEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);
void PrintBigDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);
void PrintBiggestDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);
void PrintSymbolsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color, const char *pattern, ...);

#endif
