/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#ifndef _AUDIO_POST_PROCESS_H_
#define _AUDIO_POST_PROCESS_H_

#include <hardware/audio_effect.h>

#define MAX_POSTPROCESSORS 10

struct aml_native_postprocess {
    int num_postprocessors;
    effect_handle_t postprocessors[MAX_POSTPROCESSORS];
    int total_postprocessors;

    /* VirtualX effect license library exist flag */
    /* Path: (/vendor/lib/soundfx/libvx.so) */
    bool libvx_exist;
    bool vx_force_stereo;
    /* channel num of effect input */
    int effect_in_ch;
};

int audio_post_process(struct aml_native_postprocess *native_postprocess, int16_t *in_buffer, size_t in_frames);
int audio_VX_post_process(struct aml_native_postprocess *native_postprocess, int16_t *in_buffer, size_t bytes);

/* VirtualX: */
/* path of virtualx effect license library */
#define VIRTUALX_LICENSE_LIB_PATH "/vendor/lib/soundfx/libvx.so"

void VirtualX_reset(struct aml_native_postprocess *native_postprocess);
void VirtualX_Channel_reconfig(struct aml_native_postprocess *native_postprocess, int ch_num);
bool Check_VX_lib(void);

#endif
