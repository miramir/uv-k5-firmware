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

#ifndef BK1080_H
#define BK1080_H

#ifdef ENABLE_FMRADIO

#include <stdbool.h>
#include <stdint.h>
#include "driver/bk1080-regs.h"

extern uint16_t BK1080_BaseFrequency;
extern uint16_t BK1080_FrequencyDeviation;

void BK1080_Init0(void);
void BK1080_Init(uint16_t Frequency, uint8_t band/*, uint8_t space*/);
uint16_t BK1080_ReadRegister(BK1080_Register_t Register);
void BK1080_WriteRegister(BK1080_Register_t Register, uint16_t Value);
void BK1080_Mute(bool Mute);
uint16_t BK1080_GetFreqLoLimit(uint8_t band);
uint16_t BK1080_GetFreqHiLimit(uint8_t band);
void BK1080_SetFrequency(uint16_t frequency, uint8_t band/*, uint8_t space*/);
void BK1080_GetFrequencyDeviation(uint16_t Frequency);

#endif

#endif

