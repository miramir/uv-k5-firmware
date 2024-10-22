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
#include <stdlib.h>  // abs()
#include "app/chFrScanner.h"
#include "app/dtmf.h"
#include "am_fix.h"
#include "bitmaps.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/graphics.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "driver/system.h"

center_line_t center_line = CENTER_LINE_NONE;

static int8_t RxBlink;
static int8_t RxBlinkLed = 0;
static int8_t RxBlinkLedCounter;
static int8_t RxLine;
static uint32_t RxOnVfofrequency;

bool isMainOnlyInputDTMF = false;

static int16_t map(int16_t x, int16_t in_min, int16_t in_max, int16_t out_min, int16_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static bool isMainOnly(bool checkGui)
{
    if(((gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2 == 0) || (gSetting_set_gui && checkGui))
        return true;
    else
        return false;
}

const int8_t dBmCorrTable[7] = {
            -15, // band 1
            -25, // band 2
            -20, // band 3
            -4, // band 4
            -7, // band 5
            -6, // band 6
             -1  // band 7
        };

const char *VfoStateStr[] = {
       [VFO_STATE_NORMAL]="",
       [VFO_STATE_BUSY]="BUSY",
       [VFO_STATE_BAT_LOW]="BAT LOW",
       [VFO_STATE_TX_DISABLE]="TX DISABLE",
       [VFO_STATE_TIMEOUT]="TIMEOUT",
       [VFO_STATE_ALARM]="ALARM",
       [VFO_STATE_VOLTAGE_HIGH]="VOLT HIGH"
};

// ***************************************************************************

static void DrawLevelBar(uint8_t xpos, uint8_t line, uint8_t level, uint8_t bars)
{
    uint8_t *p_line = gFrameBuffer[line];
    level = MIN(level, bars);

    for(uint8_t i = 0; i < level; i++) {
        if(gSetting_set_met)
        {
            const char hollowBar[] = {
                0b01111111,
                0b01000001,
                0b01000001,
                0b01111111
            };

            if(i < bars - 4) {
                for(uint8_t j = 0; j < 4; j++)
                    p_line[xpos + i * 5 + j] = (~(0x7F >> (i + 1))) & 0x7F;
            }
            else {
                memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
            }
        }
        else
        {
            const char hollowBar[] = {
                0b00111110,
                0b00100010,
                0b00100010,
                0b00111110
            };

            const char simpleBar[] = {
                0b00111110,
                0b00111110,
                0b00111110,
                0b00111110
            };

            if(i < bars - 4) {
                memcpy(p_line + (xpos + i * 5), &simpleBar, ARRAY_SIZE(simpleBar));
            }
            else {
                memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
            }
        }
    }
}

// Approximation of a logarithmic scale using integer arithmetic
uint8_t log2_approx(unsigned int value) {
    uint8_t log = 0;
    while (value >>= 1) {
        log++;
    }
    return log;
}

void UI_DisplayAudioBar(void)
{
    if (gSetting_mic_bar)
    {
        if(gLowBattery && !gLowBatteryConfirmed)
            return;

        RxBlinkLed = 0;
        RxBlinkLedCounter = 0;
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        unsigned int line;
        if (isMainOnly(false)) {
            line = 7;
        } else {
            line = 4;
        }

        if (gCurrentFunction != FUNCTION_TRANSMIT || gScreenToDisplay != DISPLAY_MAIN)
        {
            return;  // screen is in use
        }

        static uint8_t barsOld = 0;
        const uint8_t thresold = 18; // arbitrary thresold
        //const uint8_t barsList[] = {0, 0, 0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 16, 20, 25, 25};
        const uint8_t barsList[] = {0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 25, 25};
        uint8_t logLevel;
        uint8_t bars;

        unsigned int voiceLevel  = BK4819_GetVoiceAmplitudeOut();  // 15:0

        voiceLevel = (voiceLevel >= thresold) ? (voiceLevel - thresold) : 0;
        logLevel = log2_approx(MIN(voiceLevel * 16, 32768u) + 1);
        bars = barsList[logLevel];
        barsOld = (barsOld - bars > 1) ? (barsOld - 1) : bars;

        uint8_t *p_line = gFrameBuffer[line];
        memset(p_line, 0, LCD_WIDTH);

        DrawLevelBar(2, line, barsOld, 25);

        if (gCurrentFunction == FUNCTION_TRANSMIT) {
            ST7565_BlitFullScreen();
        }
    }
}

void DisplayRSSIBar(const bool now)
{
    const unsigned int txt_width    = 7 * 8;                 // 8 text chars
    const unsigned int bar_x        = 2 + txt_width + 4;     // X coord of bar graph

    unsigned int line;
    if (isMainOnly(false))
    {
        line = 7;
    }
    else
    {
        line = 4;
    }

    if(RxLine >= 0 && center_line != CENTER_LINE_IN_USE)
    {
        switch(RxBlink)
        {
            case 0:
                UI_PrintStringSmallBold("RX", 8, 0, RxLine);
                break;
            case 1:
                UI_PrintStringSmallBold("RX", 8, 0, RxLine);
                RxBlink = 2;
                break;
            case 2:
                for (uint8_t i = 8; i < 24; i++)
                {
                    gFrameBuffer[RxLine][i] = 0x00;
                }
                RxBlink = 1;
                break;
        }
        ST7565_BlitLine(RxLine);
    }

    uint8_t           *p_line        = gFrameBuffer[line];
    char               str[16];

    if ((gEeprom.KEY_LOCK && gKeypadLocked > 0) || center_line != CENTER_LINE_RSSI)
        return;     // display is in use

    if (gCurrentFunction == FUNCTION_TRANSMIT || gScreenToDisplay != DISPLAY_MAIN)
        return;     // display is in use

    if (now)
        memset(p_line, 0, LCD_WIDTH);

    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
        + dBmCorrTable[gRxVfo->Band];

    rssi_dBm = -rssi_dBm;

    if(rssi_dBm > 141) rssi_dBm = 141;
    if(rssi_dBm < 53) rssi_dBm = 53;

    uint8_t s_level = 0;
    uint8_t overS9dBm = 0;
    uint8_t overS9Bars = 0;

    if(rssi_dBm >= 93) {
        s_level = map(rssi_dBm, 141, 93, 1, 9);
    }
    else {
        s_level = 9;
        overS9dBm = map(rssi_dBm, 93, 53, 0, 40);
        overS9Bars = map(overS9dBm, 0, 40, 0, 4);
    }

    if (isMainOnly(true))
    {
        sprintf(str, "%3d", -rssi_dBm);
        UI_PrintStringSmallNormal(str, LCD_WIDTH + 8, 0, line-1);
    }
    else
    {
        sprintf(str, "% 4d %s", -rssi_dBm, "dBm");
        GUI_DisplaySmallest(str, 2, 25, false, true);
    }

    if(overS9Bars == 0) {
        sprintf(str, "S%d", s_level);
    }
    else {
        sprintf(str, "+%02d", overS9dBm);
    }

    UI_PrintStringSmallNormal(str, LCD_WIDTH + 38, 0, line - 1);

    DrawLevelBar(bar_x, line, s_level + overS9Bars, 13);
    if (now)
        ST7565_BlitLine(line);
}

#ifdef ENABLE_AGC_SHOW_DATA
void UI_MAIN_PrintAGC(bool now)
{
    char buf[20];
    memset(gFrameBuffer[3], 0, 128);
    union {
        struct {
            uint16_t _ : 5;
            uint16_t agcSigStrength : 7;
            int16_t gainIdx : 3;
            uint16_t agcEnab : 1;
        };
        uint16_t __raw;
    } reg7e;
    reg7e.__raw = BK4819_ReadRegister(0x7E);
    uint8_t gainAddr = reg7e.gainIdx < 0 ? 0x14 : 0x10 + reg7e.gainIdx;
    union {
        struct {
            uint16_t pga:3;
            uint16_t mixer:2;
            uint16_t lna:3;
            uint16_t lnaS:2;
        };
        uint16_t __raw;
    } agcGainReg;
    agcGainReg.__raw = BK4819_ReadRegister(gainAddr);
    int8_t lnaShortTab[] = {-28, -24, -19, 0};
    int8_t lnaTab[] = {-24, -19, -14, -9, -6, -4, -2, 0};
    int8_t mixerTab[] = {-8, -6, -3, 0};
    int8_t pgaTab[] = {-33, -27, -21, -15, -9, -6, -3, 0};
    int16_t agcGain = lnaShortTab[agcGainReg.lnaS] + lnaTab[agcGainReg.lna] + mixerTab[agcGainReg.mixer] + pgaTab[agcGainReg.pga];

    sprintf(buf, "%d%2d %2d %2d %3d", reg7e.agcEnab, reg7e.gainIdx, -agcGain, reg7e.agcSigStrength, BK4819_GetRSSI());
    UI_PrintStringSmallNormal(buf, 2, 0, 3);
    if(now)
        ST7565_BlitLine(3);
}
#endif

void UI_MAIN_TimeSlice500ms(void)
{
    if(gScreenToDisplay==DISPLAY_MAIN) {
#ifdef ENABLE_AGC_SHOW_DATA
        UI_MAIN_PrintAGC(true);
        return;
#endif

        if(FUNCTION_IsRx()) {
            DisplayRSSIBar(true);
        }
        else if(gSetting_set_eot > 0 && RxBlinkLed == 2)
        {
            if(RxBlinkLedCounter <= 8)
            {
                if(RxBlinkLedCounter % 2 == 0)
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
                    }
                }
                else
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
                    }
                }
                RxBlinkLedCounter += 1;
            }
            else
            {
                RxBlinkLed = 0;
            }
        }
    }
}

#ifdef ENABLE_SCAN_RANGES
static void ShowScanRanges(uint8_t startLine, unsigned int activeTxVFO){
    if(gScanRangeStart) {
        if(IS_FREQ_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {
            char String[22];
            UI_PrintString("ScnRng", 5, 0, startLine, 8);
            sprintf(String, "%3u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000);
            UI_PrintStringSmallNormal(String, 56, 0, startLine);
            sprintf(String, "%3u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000);
            UI_PrintStringSmallNormal(String, 56, 0, startLine + 1);
        }
        else
        {
            gScanRangeStart = 0;
        }
    }
}
#endif

// Display only one vfo main screen
static void DisplayMainOnly(void) {
    char String[22];
    center_line = CENTER_LINE_NONE;

    // clear the screen
    UI_DisplayClear();

    if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
    {   // tell user how to unlock the keyboard
        PrintSmallEx(12, LCD_HEIGHT, POS_C, C_FILL, "UNLOCK KEYBOARD");
        // UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, FRAME_LINES - 1);
    }

    unsigned int activeTxVFO = gRxVfoIsActive ? gEeprom.RX_VFO : gEeprom.TX_VFO;
    const uint8_t line0 = 1;  // text screen line
    uint8_t *p_line0    = gFrameBuffer[line0 + 0];
    enum Vfo_txtr_mode mode       = VFO_MODE_NONE;   

#ifdef ENABLE_SCAN_RANGES
    ShowScanRanges(line0 + 3, activeTxVFO);
#endif

    if (gDTMF_InputMode) {
        // todo: понять как воспроизвести
        sprintf(String, ">%s", gDTMF_InputBox);
        UI_PrintString(String, 2, 0, 6, 8);
        isMainOnlyInputDTMF = true;
        center_line = CENTER_LINE_IN_USE;
    }

    uint32_t frequency = gEeprom.VfoInfo[activeTxVFO].pRX->Frequency;
    enum VfoState_t state = VfoState[activeTxVFO];

    if(TX_freq_check(frequency) != 0 && gEeprom.VfoInfo[activeTxVFO].TX_LOCK == true)
    {
        memcpy(p_line0 + 14, BITMAP_VFO_Lock, sizeof(BITMAP_VFO_Lock));
    }

    if (gCurrentFunction == FUNCTION_TRANSMIT) {   // transmitting
        mode = VFO_MODE_TX;
        UI_PrintStringSmallBold("TX", 8, 0, line0);
    } else {   // receiving .. show the RX symbol
        mode = VFO_MODE_RX;
        if (FUNCTION_IsRx() && gEeprom.RX_VFO == activeTxVFO && state == VFO_STATE_NORMAL) {
            RxBlinkLed = 1;
            RxBlinkLedCounter = 0;
            RxLine = line0;
            RxOnVfofrequency = frequency;
            RxBlink = 0;
        } else {
            if(RxBlinkLed == 1)
                RxBlinkLed = 2;
        }
    }

    // Show widget MemoryChan or Frequency Band
    if (IS_MR_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {   // channel mode
        const unsigned int x = 2;
        const bool inputting = gInputBoxIndex != 0 && gEeprom.TX_VFO == activeTxVFO;
        if (!inputting)
            sprintf(String, "M%u", gEeprom.ScreenChannel[activeTxVFO] + 1);
        else
            sprintf(String, "M%.3s", INPUTBOX_GetAscii());  // show the input text
        UI_PrintStringSmallNormal(String, x, 0, line0 + 1);
    } else if (IS_FREQ_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {   // frequency mode
        // show the frequency band number
        const unsigned int x = 2;
        char *buf = gEeprom.VfoInfo[activeTxVFO].pRX->Frequency < _1GHz_in_KHz ? "" : "+";
        sprintf(String, "F%u%s", 1 + gEeprom.ScreenChannel[activeTxVFO] - FREQ_CHANNEL_FIRST, buf);
        UI_PrintStringSmallNormal(String, x, 0, line0 + 1);
    }

    if (state != VFO_STATE_NORMAL) {
        if (state < ARRAY_SIZE(VfoStateStr))
            UI_PrintString(VfoStateStr[state], 31, 0, line0, 8);
    } else if (gInputBoxIndex > 0 && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[activeTxVFO]) && gEeprom.TX_VFO == activeTxVFO) {   // user entering a frequency
        const char *ascii = INPUTBOX_GetAscii();
        bool isGigaF = frequency>=_1GHz_in_KHz;
        sprintf(String, "%.*s.%.3s", 3 + isGigaF, ascii, ascii + 3 + isGigaF);
        // show the frequency in the main font
        UI_PrintString(String, 32, 0, line0, 8);
    } else {
        if (gCurrentFunction == FUNCTION_TRANSMIT) {   // transmitting
            frequency = gEeprom.VfoInfo[activeTxVFO].pTX->Frequency;
        }

        if (IS_MR_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {   // it's a channel
            uint8_t countList = 0;
            uint8_t shiftList = 0;

            const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[activeTxVFO]];

            if(gMR_ChannelExclude[gEeprom.ScreenChannel[activeTxVFO]] == false)            {
                countList = att.scanlist1 + att.scanlist2 + att.scanlist3;

                if(countList == 0) {
                    memcpy(p_line0 + 127 - (1 * 6), BITMAP_ScanList0, sizeof(BITMAP_ScanList0));
                } else {
                    shiftList = countList;

                    if (att.scanlist1) {
                        memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList1, sizeof(BITMAP_ScanList1));
                        shiftList--;
                    } if (att.scanlist2) {
                        memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList2, sizeof(BITMAP_ScanList2));
                        shiftList--;
                    } if (att.scanlist3) {
                        memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList3, sizeof(BITMAP_ScanList3));
                    }
                }
            } else {
                memcpy(p_line0 + 127 - (1 * 6), BITMAP_ScanListE, sizeof(BITMAP_ScanListE));
            }
            // compander symbol
            if (att.compander)
                memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));

            switch (gEeprom.CHANNEL_DISPLAY_MODE)
            {
                case MDF_FREQUENCY: // show the channel frequency
                    sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
                    // show the frequency in the main font
                    UI_PrintString(String, 32, 0, line0, 8);
                    break;

                case MDF_CHANNEL:   // show the channel number
                    sprintf(String, "CH-%03u", gEeprom.ScreenChannel[activeTxVFO] + 1);
                    UI_PrintString(String, 32, 0, line0, 8);
                    break;

                case MDF_NAME:      // show the channel name
                case MDF_NAME_FREQ: // show the channel name and frequency

                    SETTINGS_FetchChannelName(String, gEeprom.ScreenChannel[activeTxVFO]);
                    if (String[0] == 0)
                    {   // no channel name, show the channel number instead
                        sprintf(String, "CH-%03u", gEeprom.ScreenChannel[activeTxVFO] + 1);
                    }

                    if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME) {
                        UI_PrintString(String, 32, 0, line0, 8);
                    } else {
                        UI_PrintString(String, 32, 0, line0, 8);

                        sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
                        if(frequency < _1GHz_in_KHz) {
                            // show the remaining 2 small frequency digits
                            UI_PrintStringSmallNormal(String + 7, 113, 0, line0 + 4);
                            String[7] = 0;
                            // show the main large frequency digits
                            UI_DisplayFrequency(String, 32, line0 + 3, false);
                        } else {
                            // show the frequency in the main font
                            UI_PrintString(String, 32, 0, line0 + 3, 8);
                        }
                    }
                    break;
            }
        }
        else
        {   // frequency mode
            sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
            // show the frequency in the main font
            UI_PrintString(String, 32, 0, line0, 8);

            // show the channel symbols
            const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[activeTxVFO]];
            if (att.compander)
                memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));
        }
    }

    String[0] = '\0';
    const VFO_Info_t *vfoInfo = &gEeprom.VfoInfo[activeTxVFO];

    // show the modulation symbol
    const char * s = "";
    const char * t = "";
    const ModulationMode_t mod = vfoInfo->Modulation;
    switch (mod){
        case MODULATION_FM: {
            const FREQ_Config_t *pConfig = (mode == VFO_MODE_TX) ? vfoInfo->pTX : vfoInfo->pRX;
            const unsigned int code_type = pConfig->CodeType;
            const char *code_list[] = {"", "CT", "DC", "DC"};

            if (code_type < ARRAY_SIZE(code_list))
                s = code_list[code_type];
            if(gCurrentFunction != FUNCTION_TRANSMIT)
                t = gModulationStr[mod];
            break;
        }
        default:
            t = gModulationStr[mod];
        break;
    }

    const FREQ_Config_t *pConfig = (mode == VFO_MODE_TX) ? vfoInfo->pTX : vfoInfo->pRX;
    int8_t shift = 0;

    switch((int)pConfig->CodeType)
    {
        case 1:
        sprintf(String, "%u.%u", CTCSS_Options[pConfig->Code] / 10, CTCSS_Options[pConfig->Code] % 10);
        break;

        case 2:
        sprintf(String, "%03oN", DCS_Options[pConfig->Code]);
        break;

        case 3:
        sprintf(String, "%03oI", DCS_Options[pConfig->Code]);
        break;

        default:
        sprintf(String, "%d.%02uK", vfoInfo->StepFrequency / 100, vfoInfo->StepFrequency % 100);
        shift = -10;
    }

    UI_PrintStringSmallNormal(s, LCD_WIDTH + 22, 0, line0 + 1);
    UI_PrintStringSmallNormal(t, LCD_WIDTH + 2, 0, line0 + 1);

    if (!gDTMF_InputMode)
    {
        if(shift == 0) {
            UI_PrintStringSmallNormal(String, 2, 0, 7);
        }

        if((vfoInfo->StepFrequency / 100) < 100) {
            sprintf(String, "%d.%02uK", vfoInfo->StepFrequency / 100, vfoInfo->StepFrequency % 100);
        } else {
            sprintf(String, "%dK", vfoInfo->StepFrequency / 100);               
        }
        UI_PrintStringSmallNormal(String, 46, 0, 7);
    }

    if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM) {   // show the TX power
        uint8_t currentPower = vfoInfo->OUTPUT_POWER % 8;
        uint8_t arrowPos = 19;
        bool userPower = false;
        
        if(currentPower == OUTPUT_POWER_USER) {
            currentPower = gSetting_set_pwr;
            userPower = true;
        } else {
            currentPower--;
            userPower = false;
        }

        const char pwr_short[][3] = {"L1", "L2", "L3", "L4", "L5", "M", "H"};
        sprintf(String, "%s", pwr_short[currentPower]);
        UI_PrintStringSmallNormal(String, LCD_WIDTH + 42, 0, line0 + 1);
        arrowPos = 38;

        if(userPower == true)
        {
            memcpy(p_line0 + 256 + arrowPos, BITMAP_PowerUser, sizeof(BITMAP_PowerUser));
        }
    }

    if (vfoInfo->freq_config_RX.Frequency != vfoInfo->freq_config_TX.Frequency) {   // show the TX offset symbol
        const char dir_list[][2] = {"", "+", "-"};
        int i = vfoInfo->TX_OFFSET_FREQUENCY_DIRECTION % 3;
            UI_PrintStringSmallNormal(dir_list[i], LCD_WIDTH + 60, 0, line0 + 1);
    }

    // show the TX/RX reverse symbol
    if (vfoInfo->FrequencyReverse) {
        UI_PrintStringSmallNormal("R", LCD_WIDTH + 68, 0, line0 + 1);
    }

    const char *bandWidthNames[] = {"W", "N"};
    UI_PrintStringSmallNormal(bandWidthNames[vfoInfo->CHANNEL_BANDWIDTH], LCD_WIDTH + 80, 0, line0 + 1);

    // show the audio scramble symbol
    if (vfoInfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
        UI_PrintStringSmallNormal("SCR", LCD_WIDTH + 106, 0, line0 + 1);

    if(gMonitor) {
        sprintf(String, "%s", "MONI");
    } else {
        sprintf(String, "SQL%d", gEeprom.SQUELCH_LEVEL);
    }
    UI_PrintStringSmallNormal(String, LCD_WIDTH + 98, 0, line0 + 1);

#ifdef ENABLE_AGC_SHOW_DATA
    center_line = CENTER_LINE_IN_USE;
    UI_MAIN_PrintAGC(false);
#endif

    if (center_line == CENTER_LINE_NONE) {   // we're free to use the middle line
        const bool rx = FUNCTION_IsRx();

        if (gSetting_mic_bar && gCurrentFunction == FUNCTION_TRANSMIT) {
            center_line = CENTER_LINE_AUDIO_BAR;
            UI_DisplayAudioBar();
        } else
#if defined(ENABLE_AM_FIX_SHOW_DATA)
        if (rx && gEeprom.VfoInfo[gEeprom.RX_VFO].Modulation == MODULATION_AM && gSetting_AM_fix)
        {
            if (gScreenToDisplay != DISPLAY_MAIN)
                return;

            center_line = CENTER_LINE_AM_FIX_DATA;
            AM_fix_print_data(gEeprom.RX_VFO, String);
            UI_PrintStringSmallNormal(String, 2, 0, 4);
        }
        else
#endif
        if (rx) {
            center_line = CENTER_LINE_RSSI;
            DisplayRSSIBar(false);
        } else if (rx || gCurrentFunction == FUNCTION_FOREGROUND || gCurrentFunction == FUNCTION_POWER_SAVE) {
            if (gSetting_live_DTMF_decoder && gDTMF_RX_live[0] != 0) {   // show live DTMF decode
                const unsigned int len = strlen(gDTMF_RX_live);
                const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

                if (gScreenToDisplay != DISPLAY_MAIN)
                    return;

                center_line = CENTER_LINE_DTMF_DEC;

                sprintf(String, "DTMF %s", gDTMF_RX_live + idx);
                    UI_PrintStringSmallNormal(String, 2, 0, 6);
            }
        }
    }

    if (!gDTMF_InputMode)
    {
        sprintf(String, "VFO %s", activeTxVFO ? "B" : "A");
        UI_PrintStringSmallBold(String, 92, 0, 7);
        for (uint8_t i = 92; i < 128; i++)
        {
            gFrameBuffer[7][i] ^= 0x7F;
        }
    }

    ST7565_BlitFullScreen();
}

// ***************************************************************************

void UI_DisplayMain(void)
{
    if(gLowBattery && !gLowBatteryConfirmed) {
        UI_DisplayPopup("LOW BATTERY");
        ST7565_BlitFullScreen();
        return;
    }

    if(isMainOnly(false)) {
        DisplayMainOnly();
        return;
    };

    char               String[22];

    center_line = CENTER_LINE_NONE;

    // clear the screen
    UI_DisplayClear();

    if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
    {   // tell user how to unlock the keyboard
        uint8_t shift = 4;

        if(isMainOnly(false))
        {
            shift = 7;
        }
        UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, shift);
    }

    unsigned int activeTxVFO = gRxVfoIsActive ? gEeprom.RX_VFO : gEeprom.TX_VFO;

    for (unsigned int vfo_num = 0; vfo_num < 2; vfo_num++)
    {
        const unsigned int line0 = 1;  // text screen line
        const unsigned int line1 = 5;
        unsigned int line;
        if (isMainOnly(false))
        {
            line       = 1;
        }
        else
        {
            line       = (vfo_num == 0) ? line0 : line1;
        }
        const bool         isMainVFO  = (vfo_num == gEeprom.TX_VFO);
        uint8_t           *p_line0    = gFrameBuffer[line + 0];
        enum Vfo_txtr_mode mode       = VFO_MODE_NONE;      

        if (isMainOnly(false))
        {
            if (activeTxVFO != vfo_num)
            {
                continue;
            }
        }

        if (activeTxVFO != vfo_num || isMainOnly(false))
        {
#ifdef ENABLE_SCAN_RANGES
            if(gScanRangeStart) {

                //if(IS_FREQ_CHANNEL(gEeprom.ScreenChannel[0]) && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[1])) {
                if(IS_FREQ_CHANNEL(gEeprom.ScreenChannel[activeTxVFO])) {

                    uint8_t shift = 0;

                    if (isMainOnly(false))
                    {
                        shift = 3;
                    }

                    UI_PrintString("ScnRng", 5, 0, line + shift, 8);
                    sprintf(String, "%3u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000);
                    UI_PrintStringSmallNormal(String, 56, 0, line + shift);
                    sprintf(String, "%3u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000);
                    UI_PrintStringSmallNormal(String, 56, 0, line + shift + 1);

                    if (!isMainOnly(false))
                        continue;
                }
                else
                {
                    gScanRangeStart = 0;
                }
            }
#endif


            if (gDTMF_InputMode) {
                char *pPrintStr = "";
                // show DTMF stuff
                {
                    sprintf(String, ">%s", gDTMF_InputBox);
                    pPrintStr = String;
                }

                if (isMainOnly(false))
                {
                    UI_PrintString(pPrintStr, 2, 0, 5, 8);
                    isMainOnlyInputDTMF = true;
                    center_line = CENTER_LINE_IN_USE;
                }
                else
                {
                    UI_PrintString(pPrintStr, 2, 0, 0 + (vfo_num * 3), 8);
                    isMainOnlyInputDTMF = false;
                    center_line = CENTER_LINE_IN_USE;
                    continue;
                }
            }

            // highlight the selected/used VFO with a marker
            if (isMainVFO)
                memcpy(p_line0 + 0, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
        }
        else // active TX VFO
        {   // highlight the selected/used VFO with a marker
            if (isMainVFO)
                memcpy(p_line0 + 0, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
            else
                memcpy(p_line0 + 0, BITMAP_VFO_NotDefault, sizeof(BITMAP_VFO_NotDefault));
        }

        uint32_t frequency = gEeprom.VfoInfo[vfo_num].pRX->Frequency;

        if(TX_freq_check(frequency) != 0 && gEeprom.VfoInfo[vfo_num].TX_LOCK == true)
        {
            if(isMainOnly(false))
                memcpy(p_line0 + 14, BITMAP_VFO_Lock, sizeof(BITMAP_VFO_Lock));
            else
                memcpy(p_line0 + 24, BITMAP_VFO_Lock, sizeof(BITMAP_VFO_Lock));
        }

        if (gCurrentFunction == FUNCTION_TRANSMIT)
        {   // transmitting
            if (activeTxVFO == vfo_num)
            {   // show the TX symbol
                mode = VFO_MODE_TX;
                UI_PrintStringSmallBold("TX", 8, 0, line);
            }
        }
        else
        {   // receiving .. show the RX symbol
            mode = VFO_MODE_RX;
            //if (FUNCTION_IsRx() && gEeprom.RX_VFO == vfo_num) {
            if (FUNCTION_IsRx() && gEeprom.RX_VFO == vfo_num && VfoState[vfo_num] == VFO_STATE_NORMAL) {
                RxBlinkLed = 1;
                RxBlinkLedCounter = 0;
                RxLine = line;
                RxOnVfofrequency = frequency;
                if(!isMainVFO)
                {
                    RxBlink = 1;
                }
                else
                {
                    RxBlink = 0;
                }
            }
            else
            {
                if(RxOnVfofrequency == frequency && !isMainOnly(false))
                {
                    UI_PrintStringSmallNormal(">>", 8, 0, line);
                    //memcpy(p_line0 + 14, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
                }

                if(RxBlinkLed == 1)
                    RxBlinkLed = 2;
            }
        }

        if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
        {   // channel mode
            const unsigned int x = 2;
            const bool inputting = gInputBoxIndex != 0 && gEeprom.TX_VFO == vfo_num;
            if (!inputting)
                sprintf(String, "M%u", gEeprom.ScreenChannel[vfo_num] + 1);
            else
                sprintf(String, "M%.3s", INPUTBOX_GetAscii());  // show the input text
            UI_PrintStringSmallNormal(String, x, 0, line + 1);
        }
        else if (IS_FREQ_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
        {   // frequency mode
            // show the frequency band number
            const unsigned int x = 2;
            char * buf = gEeprom.VfoInfo[vfo_num].pRX->Frequency < _1GHz_in_KHz ? "" : "+";
            sprintf(String, "F%u%s", 1 + gEeprom.ScreenChannel[vfo_num] - FREQ_CHANNEL_FIRST, buf);
            UI_PrintStringSmallNormal(String, x, 0, line + 1);
        }
        // ************

        enum VfoState_t state = VfoState[vfo_num];

        if (state != VFO_STATE_NORMAL)
        {
            if (state < ARRAY_SIZE(VfoStateStr))
                UI_PrintString(VfoStateStr[state], 31, 0, line, 8);
        }
        else if (gInputBoxIndex > 0 && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[vfo_num]) && gEeprom.TX_VFO == vfo_num)
        {   // user entering a frequency
            const char * ascii = INPUTBOX_GetAscii();
            bool isGigaF = frequency>=_1GHz_in_KHz;
            sprintf(String, "%.*s.%.3s", 3 + isGigaF, ascii, ascii + 3 + isGigaF);
            // show the frequency in the main font
            UI_PrintString(String, 32, 0, line, 8);

            continue;
        }
        else
        {
            if (gCurrentFunction == FUNCTION_TRANSMIT)
            {   // transmitting
                if (activeTxVFO == vfo_num)
                    frequency = gEeprom.VfoInfo[vfo_num].pTX->Frequency;
            }

            if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo_num]))
            {   // it's a channel

                uint8_t countList = 0;
                uint8_t shiftList = 0;

                const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[vfo_num]];

                if(gMR_ChannelExclude[gEeprom.ScreenChannel[vfo_num]] == false)
                {
                    countList = att.scanlist1 + att.scanlist2 + att.scanlist3;

                    if(countList == 0)
                    {
                        memcpy(p_line0 + 127 - (1 * 6), BITMAP_ScanList0, sizeof(BITMAP_ScanList0));
                    }
                    else
                    {
                        shiftList = countList;

                        if (att.scanlist1)
                        {
                            memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList1, sizeof(BITMAP_ScanList1));
                            shiftList--;
                        }
                        if (att.scanlist2)
                        {
                            memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList2, sizeof(BITMAP_ScanList2));
                            shiftList--;
                        }
                        if (att.scanlist3)
                        {
                            memcpy(p_line0 + 127 - (shiftList * 6), BITMAP_ScanList3, sizeof(BITMAP_ScanList3));
                        }
                    }
                }
                else
                {
                    memcpy(p_line0 + 127 - (1 * 6), BITMAP_ScanListE, sizeof(BITMAP_ScanListE));
                }

                // compander symbol
                if (att.compander)
                    memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));

                switch (gEeprom.CHANNEL_DISPLAY_MODE)
                {
                    case MDF_FREQUENCY: // show the channel frequency
                        sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
                        // show the frequency in the main font
                        UI_PrintString(String, 32, 0, line, 8);

                        break;

                    case MDF_CHANNEL:   // show the channel number
                        sprintf(String, "CH-%03u", gEeprom.ScreenChannel[vfo_num] + 1);
                        UI_PrintString(String, 32, 0, line, 8);
                        break;

                    case MDF_NAME:      // show the channel name
                    case MDF_NAME_FREQ: // show the channel name and frequency

                        SETTINGS_FetchChannelName(String, gEeprom.ScreenChannel[vfo_num]);
                        if (String[0] == 0)
                        {   // no channel name, show the channel number instead
                            sprintf(String, "CH-%03u", gEeprom.ScreenChannel[vfo_num] + 1);
                        }

                        if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME) {
                            UI_PrintString(String, 32, 0, line, 8);
                        }
                        else {
                            if (isMainOnly(false))
                            {
                                UI_PrintString(String, 32, 0, line, 8);
                            }
                            else
                            {
                                if(activeTxVFO == vfo_num) {
                                    UI_PrintStringSmallBold(String, 32 + 4, 0, line);
                                }
                                else
                                {
                                    UI_PrintStringSmallNormal(String, 32 + 4, 0, line);     
                                }
                            }

                            if (isMainOnly(false))
                            {
                                sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
                                if(frequency < _1GHz_in_KHz) {
                                    // show the remaining 2 small frequency digits
                                    UI_PrintStringSmallNormal(String + 7, 113, 0, line + 4);
                                    String[7] = 0;
                                    // show the main large frequency digits
                                    UI_DisplayFrequency(String, 32, line + 3, false);
                                }
                                else
                                {
                                    // show the frequency in the main font
                                    UI_PrintString(String, 32, 0, line + 3, 8);
                                }
                            }
                            else
                            {
                                sprintf(String, "%03u.%05u", frequency / 100000, frequency % 100000);
                                UI_PrintStringSmallNormal(String, 32 + 4, 0, line + 1);
                            }
                        }

                        break;
                }
            }
            else
            {   // frequency mode
                sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);

                {
                    // show the frequency in the main font
                    UI_PrintString(String, 32, 0, line, 8);
                }

                // show the channel symbols
                const ChannelAttributes_t att = gMR_ChannelAttributes[gEeprom.ScreenChannel[vfo_num]];
                if (att.compander)
                    memcpy(p_line0 + 120 + LCD_WIDTH, BITMAP_compand, sizeof(BITMAP_compand));
            }
        }

        // ************

        String[0] = '\0';
        const VFO_Info_t *vfoInfo = &gEeprom.VfoInfo[vfo_num];

        // show the modulation symbol
        const char * s = "";
        const char * t = "";
        const ModulationMode_t mod = vfoInfo->Modulation;
        switch (mod){
            case MODULATION_FM: {
                const FREQ_Config_t *pConfig = (mode == VFO_MODE_TX) ? vfoInfo->pTX : vfoInfo->pRX;
                const unsigned int code_type = pConfig->CodeType;
                const char *code_list[] = {"", "CT", "DC", "DC"};

                if (code_type < ARRAY_SIZE(code_list))
                    s = code_list[code_type];
                if(gCurrentFunction != FUNCTION_TRANSMIT || activeTxVFO != vfo_num)
                    t = gModulationStr[mod];
                break;
            }
            default:
                t = gModulationStr[mod];
            break;
        }

        const FREQ_Config_t *pConfig = (mode == VFO_MODE_TX) ? vfoInfo->pTX : vfoInfo->pRX;
        int8_t shift = 0;

        switch((int)pConfig->CodeType)
        {
            case 1:
            sprintf(String, "%u.%u", CTCSS_Options[pConfig->Code] / 10, CTCSS_Options[pConfig->Code] % 10);
            break;

            case 2:
            sprintf(String, "%03oN", DCS_Options[pConfig->Code]);
            break;

            case 3:
            sprintf(String, "%03oI", DCS_Options[pConfig->Code]);
            break;

            default:
            sprintf(String, "%d.%02uK", vfoInfo->StepFrequency / 100, vfoInfo->StepFrequency % 100);
            shift = -10;
        }

        if (isMainOnly(true))
        {
            UI_PrintStringSmallNormal(s, LCD_WIDTH + 22, 0, line + 1);
            UI_PrintStringSmallNormal(t, LCD_WIDTH + 2, 0, line + 1);

            if (isMainOnly(false) && !gDTMF_InputMode)
            {
                if(shift == 0)
                {
                    UI_PrintStringSmallNormal(String, 2, 0, 6);
                }

                if((vfoInfo->StepFrequency / 100) < 100)
                {
                    sprintf(String, "%d.%02uK", vfoInfo->StepFrequency / 100, vfoInfo->StepFrequency % 100);
                }
                else
                {
                    sprintf(String, "%dK", vfoInfo->StepFrequency / 100);               
                }
                UI_PrintStringSmallNormal(String, 46, 0, 6);
            }
        }
        else
        {
            if ((s != NULL) && (s[0] != '\0')) {
                GUI_DisplaySmallest(s, 58, line == 0 ? 17 : 49, false, true);
            }

            if ((t != NULL) && (t[0] != '\0')) {
                GUI_DisplaySmallest(t, 3, line == 0 ? 17 : 49, false, true);
            }

            GUI_DisplaySmallest(String, 68 + shift, line == 0 ? 17 : 49, false, true);

            //sprintf(String, "%d.%02u", vfoInfo->StepFrequency / 100, vfoInfo->StepFrequency % 100);
            //GUI_DisplaySmallest(String, 91, line == 0 ? 2 : 34, false, true);
        }

        if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM)
        {   // show the TX power
            uint8_t currentPower = vfoInfo->OUTPUT_POWER % 8;
            uint8_t arrowPos = 19;
            bool userPower = false;
            
            if(currentPower == OUTPUT_POWER_USER)
            {
                currentPower = gSetting_set_pwr;
                userPower = true;
            }
            else
            {
                currentPower--;
                userPower = false;
            }

            if (isMainOnly(true))
            {
                const char pwr_short[][3] = {"L1", "L2", "L3", "L4", "L5", "M", "H"};
                sprintf(String, "%s", pwr_short[currentPower]);
                UI_PrintStringSmallNormal(String, LCD_WIDTH + 42, 0, line + 1);
                arrowPos = 38;
            }
            else
            {
                const char pwr_long[][5] = {"LOW1", "LOW2", "LOW3", "LOW4", "LOW5", "MID", "HIGH"};
                sprintf(String, "%s", pwr_long[currentPower]);
                GUI_DisplaySmallest(String, 24, line == 0 ? 17 : 49, false, true);
            }

            if(userPower == true)
            {
                memcpy(p_line0 + 256 + arrowPos, BITMAP_PowerUser, sizeof(BITMAP_PowerUser));
            }
        }

        if (vfoInfo->freq_config_RX.Frequency != vfoInfo->freq_config_TX.Frequency)
        {   // show the TX offset symbol
            const char dir_list[][2] = {"", "+", "-"};
            int i = vfoInfo->TX_OFFSET_FREQUENCY_DIRECTION % 3;
            if (isMainOnly(true)) {
                UI_PrintStringSmallNormal(dir_list[i], LCD_WIDTH + 60, 0, line + 1);
            } else {
                UI_PrintStringSmallNormal(dir_list[i], LCD_WIDTH + 41, 0, line + 1);    
            }
        }

        // show the TX/RX reverse symbol
        if (vfoInfo->FrequencyReverse)
        {
            if (isMainOnly(true))
            {
                UI_PrintStringSmallNormal("R", LCD_WIDTH + 68, 0, line + 1);
            }
            else
            {
                GUI_DisplaySmallest("R", 51, line == 0 ? 17 : 49, false, true);
            }
        }

        if (isMainOnly(true))
        {
            const char *bandWidthNames[] = {"W", "N"};
            UI_PrintStringSmallNormal(bandWidthNames[vfoInfo->CHANNEL_BANDWIDTH], LCD_WIDTH + 80, 0, line + 1);
        }
        else
        {
            const char *bandWidthNames[] = {"WIDE", "NAR"};
            GUI_DisplaySmallest(bandWidthNames[vfoInfo->CHANNEL_BANDWIDTH], 91, line == 0 ? 17 : 49, false, true);
        }

        // show the audio scramble symbol
        if (vfoInfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
            UI_PrintStringSmallNormal("SCR", LCD_WIDTH + 106, 0, line + 1);

        if(isMainVFO)   
        {
            if(gMonitor)
            {
                sprintf(String, "%s", "MONI");
            }
            
            if (isMainOnly(true))
            {
                if(!gMonitor)
                {
                    sprintf(String, "SQL%d", gEeprom.SQUELCH_LEVEL);
                }
                UI_PrintStringSmallNormal(String, LCD_WIDTH + 98, 0, line + 1);
            }
            else
            {
                if(!gMonitor)
                {
                    sprintf(String, "SQL%d", gEeprom.SQUELCH_LEVEL);
                }
                GUI_DisplaySmallest(String, 110, line == 0 ? 17 : 49, false, true);
            }
        }
    }

#ifdef ENABLE_AGC_SHOW_DATA
    center_line = CENTER_LINE_IN_USE;
    UI_MAIN_PrintAGC(false);
#endif

    if (center_line == CENTER_LINE_NONE)
    {   // we're free to use the middle line

        const bool rx = FUNCTION_IsRx();


        if (gSetting_mic_bar && gCurrentFunction == FUNCTION_TRANSMIT) {
            center_line = CENTER_LINE_AUDIO_BAR;
            UI_DisplayAudioBar();
        }
        else

#if defined(ENABLE_AM_FIX_SHOW_DATA)
        if (rx && gEeprom.VfoInfo[gEeprom.RX_VFO].Modulation == MODULATION_AM && gSetting_AM_fix)
        {
            if (gScreenToDisplay != DISPLAY_MAIN)
                return;

            center_line = CENTER_LINE_AM_FIX_DATA;
            AM_fix_print_data(gEeprom.RX_VFO, String);
            UI_PrintStringSmallNormal(String, 2, 0, 3);
        }
        else
#endif

        if (rx) {
            center_line = CENTER_LINE_RSSI;
            DisplayRSSIBar(false);
        }
        else if (rx || gCurrentFunction == FUNCTION_FOREGROUND || gCurrentFunction == FUNCTION_POWER_SAVE)
        {
            if (gSetting_live_DTMF_decoder && gDTMF_RX_live[0] != 0)
            {   // show live DTMF decode
                const unsigned int len = strlen(gDTMF_RX_live);
                const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

                if (gScreenToDisplay != DISPLAY_MAIN)
                    return;

                center_line = CENTER_LINE_DTMF_DEC;

                sprintf(String, "DTMF %s", gDTMF_RX_live + idx);
                if (isMainOnly(false))
                {
                    UI_PrintStringSmallNormal(String, 2, 0, 5);
                }
                else
                {
                    UI_PrintStringSmallNormal(String, 2, 0, 3);
                }
            }
        }
    }

    if (isMainOnly(false) && !gDTMF_InputMode)
    {
        sprintf(String, "VFO %s", activeTxVFO ? "B" : "A");
        UI_PrintStringSmallBold(String, 92, 0, 6);
        for (uint8_t i = 92; i < 128; i++)
        {
            gFrameBuffer[6][i] ^= 0x7F;
        }
    }

    ST7565_BlitFullScreen();
}

// ***************************************************************************
