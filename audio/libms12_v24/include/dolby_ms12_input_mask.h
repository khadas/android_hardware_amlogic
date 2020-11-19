/*
 * Copyright (C) 2020 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOLBY_MS12_INPUT_MASK_H_
#define DOLBY_MS12_INPUT_MASK_H_

/*
 *-im  *           <str>  Main program input filename [REQUIRED]
 *-im2 *           <str>  2nd Main program input filename
 *-iui *           <str>  UI sounds input filename
 *-ia              <str>  Associated program input filename
 *-is              <str>  System Sounds input filename
 *-ias             <str>  Application Sounds input filename
 *-it              <str>  Use case input type (DDP|AAC|AC4|PCM|MAT|MLP)
 *                        default: derived from -im filename extension
 *                       can be used to select the codec in mp4 container files
 */
#define MS12_INPUT_MASK_MAIN_DDP            0x0001 /*dolby format as main input*/
#define MS12_INPUT_MASK_MAIN2               0x0002
#define MS12_INPUT_MASK_UI_SOUND            0x0004
#define MS12_INPUT_MASK_ASSOCIATE           0x0008
#define MS12_INPUT_MASK_SYSTEM              0x0010
#define MS12_INPUT_MASK_INPUT_TYPE          0x0020
#define MS12_INPUT_MASK_MAIN_PCM            0x0040 /*pcm format as main inut, such as pcm&hdmi-in*/
#define MS12_INPUT_MASK_MAIN_AC4            0x0080
#define MS12_INPUT_MASK_MAIN_MAT            0x0100
#define MS12_INPUT_MASK_MAIN_MLP            0x0200
#define MS12_INPUT_MASK_MAIN_HEAAC          0x0400



#endif

