/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#ifndef __HARMAN_DSP_PROCESS_H__
#define __HARMAN_DSP_PROCESS_H__

#ifdef __cplusplus
extern "C" {
#endif

enum EQ_Mode {
    EQ_STANDARD = 0,
    EQ_MOVIE,
    EQ_MUSIC,
    EQ_VOICE,
    EQ_SPORT,
    NUM_EQ_MODE
};

int set_dsp_mode(void);
int set_EQ_mode(int EQ_mode);
int set_subwoofer_volume(int volume);

int unload_DSP_lib(void);
int load_DSP_lib(void);
int dsp_init( int inStrTypeId, int numInChannels, int numOutChannels, int sampleRate );
int dsp_release(void);
int dsp_setParameter(int paramID, void* paramVal);
int dsp_process(void* inBuffer, void* outBuffer, void* aecBuffer, int num_frames);

#ifdef __cplusplus
}
#endif

#endif //__HARMAN_DSP_PROCESS_H__
