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

#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/graphics.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"
#include "bitmaps.h"

void UI_DisplayReleaseKeys(void)
{
    memset(gFrameBuffer[0],  0, sizeof(gFrameBuffer[0]));
    ST7565_ContrastAndInv();
    UI_DisplayClear();

    PrintMediumBoldEx(LCD_XCENTER, 16, POS_C, C_FILL, "RELEASE");
    PrintMediumBoldEx(LCD_XCENTER, 24, POS_C, C_FILL, "ALL KEYS");

    ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
    char WelcomeString0[16];
    char WelcomeString1[16];

    ST7565_ContrastAndInv();
    UI_ClearStatus();
    UI_ClearScreen();

    ST7565_BlitFullScreen();
    
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE) {
        ST7565_FillScreen(0x00);
    } else {
        memset(WelcomeString0, 0, sizeof(WelcomeString0));
        memset(WelcomeString1, 0, sizeof(WelcomeString1));

        EEPROM_ReadBuffer(0x0EB0, WelcomeString0, 16);
        EEPROM_ReadBuffer(0x0EC0, WelcomeString1, 16);
        
        if(strlen(WelcomeString0) == 0) {
            strcpy(WelcomeString0, "WELCOME");
        }

        if(strlen(WelcomeString1) == 0) {
            strcpy(WelcomeString1, "BIENVENUE");
        }

        PrintMediumBoldEx(LCD_XCENTER, 16, POS_C, C_FILL, WelcomeString0);
        PrintMediumBoldEx(LCD_XCENTER, 24, POS_C, C_FILL, WelcomeString1);
       
        PrintMediumEx(LCD_XCENTER, LCD_HEIGHT-14, POS_C, C_FILL,
            "Voltage: %u.%02uV %u%%", 
            gBatteryVoltageAverage / 100,
            gBatteryVoltageAverage % 100,
            BATTERY_VoltsToPercent(gBatteryVoltageAverage));
        DrawHLine(0, LCD_HEIGHT-10, LCD_WIDTH, C_FILL);
        PrintMediumEx(LCD_XCENTER, LCD_HEIGHT-1, POS_C, C_FILL, Version);

        ST7565_BlitFullScreen();
    }
}
