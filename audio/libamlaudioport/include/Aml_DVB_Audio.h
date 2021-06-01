/*
 * Copyright (C) 2019 Amlogic Corporation.
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
 *
 *  DESCRIPTION:
 *     brief  Audio DVB API Functions.
 *
 */

#ifndef AML_DVB_AUDIO__H
#define AML_DVB_AUDIO__H

#ifdef  __cplusplus
extern "C"
{
#endif

int dvb_audio_start_decoder(int fmt, int has_video);

int dvb_audio_stop_decoder(void);

int dvb_audio_pause_decoder(void);

int dvb_audio_resume_decoder(void);

int dvb_audio_set_ad(int fmt, int pid);

int dvb_audio_set_volume(float volume);

int dvb_audio_set_mute(int mute);

int dvb_audio_set_output_mode(int mode);

int dvb_audio_set_pre_gain(int gain);

int dvb_audio_set_pre_mute(int mute);

int dvb_audio_get_latencyms(int demux_id);

int audio_hal_get_status(void *status);//TBD

#ifdef  __cplusplus
}
#endif

#endif

