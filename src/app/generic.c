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

#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"

#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif

#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "driver/keyboard.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void GENERIC_Key_F(bool bKeyPressed, bool bKeyHeld)
{
    if (gInputBoxIndex > 0) {
        return;
    }

    if (bKeyHeld || !bKeyPressed) { // held or released
        if (bKeyHeld || bKeyPressed) { // held or pressed (cannot be held and not pressed I guess, so it checks only if HELD?)
            if (!bKeyHeld) // won't ever pass
                return;

            if (!bKeyPressed) // won't ever pass
                return;

            COMMON_KeypadLockToggle();
        }
        else { // released
#ifdef ENABLE_FMRADIO
            if ((gFmRadioMode || gScreenToDisplay != DISPLAY_MAIN) && gScreenToDisplay != DISPLAY_FM)
                return;
#else
            if (gScreenToDisplay != DISPLAY_MAIN)
                return;
#endif

            gWasFKeyPressed = !gWasFKeyPressed; // toggle F function

            if (gWasFKeyPressed)
                gKeyInputCountdown = key_input_timeout_500ms;

            gUpdateStatus = true;
        }
    }
    else { // short pressed
#ifdef ENABLE_FMRADIO
        if (gScreenToDisplay != DISPLAY_FM)
#endif
        {
            return;
        }

#ifdef ENABLE_FMRADIO
        if (gFM_ScanState == FM_SCAN_OFF) { // not scanning
            return;
        }
#endif
        gPttWasReleased = true;
    }
}

void GENERIC_Key_PTT(bool bKeyPressed)
{
    gInputBoxIndex = 0;

    if (!bKeyPressed || SerialConfigInProgress())
    {   // PTT released
        if (gCurrentFunction == FUNCTION_TRANSMIT) {    
            // we are transmitting .. stop
            if (gFlagEndTransmission) {
                FUNCTION_Select(FUNCTION_FOREGROUND);
            }
            else {
                APP_EndTransmission();

                if (gEeprom.REPEATER_TAIL_TONE_ELIMINATION == 0)
                    FUNCTION_Select(FUNCTION_FOREGROUND);
                else
                    gRTTECountdown_10ms = gEeprom.REPEATER_TAIL_TONE_ELIMINATION * 10;
            }

            gFlagEndTransmission = false;

            RADIO_SetVfoState(VFO_STATE_NORMAL);

            if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
                gRequestDisplayScreen = DISPLAY_MAIN;
        }

        return;
    }

    // PTT pressed


    if (SCANNER_IsScanning()) { 
        SCANNER_Stop(); // CTCSS/CDCSS scanning .. stop
        goto cancel_tx;
    }

    if (gScanStateDir != SCAN_OFF) {    
        CHFRSCANNER_Stop(); // frequency/channel scanning . .stop
        goto cancel_tx;
    }



#ifdef ENABLE_FMRADIO
    if (gFM_ScanState != FM_SCAN_OFF) { // FM radio is scanning .. stop
        FM_PlayAndUpdate();
        gRequestDisplayScreen = DISPLAY_FM;
        goto cancel_tx;
    }
#endif

#ifdef ENABLE_FMRADIO
    if (gScreenToDisplay == DISPLAY_FM)
        goto start_tx;  // listening to the FM radio .. start TX'ing
#endif

    if (gCurrentFunction == FUNCTION_TRANSMIT && gRTTECountdown_10ms == 0) {// already transmitting
        gInputBoxIndex = 0;
        return;
    }

    if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
        gRequestDisplayScreen = DISPLAY_MAIN;


    if (!gDTMF_InputMode && gDTMF_InputBox_Index == 0)
        goto start_tx;  // wasn't entering a DTMF code .. start TX'ing (maybe)

    // was entering a DTMF string

    if (gDTMF_InputBox_Index > 0 || gDTMF_PreviousIndex > 0) { // going to transmit a DTMF string
        if (gDTMF_InputBox_Index == 0 && gDTMF_PreviousIndex > 0)
            gDTMF_InputBox_Index = gDTMF_PreviousIndex;           // use the previous DTMF string

        if (gDTMF_InputBox_Index < sizeof(gDTMF_InputBox))
            gDTMF_InputBox[gDTMF_InputBox_Index] = 0;             // NULL term the string

        // remember the DTMF string
        gDTMF_PreviousIndex = gDTMF_InputBox_Index;
        strcpy(gDTMF_String, gDTMF_InputBox);
        gDTMF_ReplyState = DTMF_REPLY_ANI;
    }

    DTMF_clear_input_box();

start_tx:
    // request start TX
    gFlagPrepareTX = true;
    goto done;

cancel_tx:
    if (gPttIsPressed) {
        gPttWasPressed = true;
    }

done:
    gPttDebounceCounter = 0;
    if (gScreenToDisplay != DISPLAY_MENU
#ifdef ENABLE_FMRADIO
        && gRequestDisplayScreen != DISPLAY_FM
#endif
    ) {
        // 1of11 .. don't close the menu
        gRequestDisplayScreen = DISPLAY_MAIN;
    }

    gUpdateStatus  = true;
    gUpdateDisplay = true;
}
