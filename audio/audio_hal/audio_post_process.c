/*
 * hardware/amlogic/audio/audioeffect/audio_post_process.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 */

#define LOG_TAG "audio_post_process"

#include <dlfcn.h>
#include <cutils/log.h>
#include "audio_post_process.h"
#include "Virtualx.h"

int audio_post_process(effect_handle_t effect, int16_t *in_buffer, size_t out_frames)
{
    int ret = 0;
    audio_buffer_t in_buf;
    audio_buffer_t out_buf;

    if (effect == NULL) {
        return ret;
    }

    in_buf.frameCount = out_buf.frameCount = out_frames;
    in_buf.s16 = out_buf.s16 = in_buffer;
    ret = (*effect)->process(effect, &in_buf, &out_buf);
    if (ret < 0) {
        ALOGE("postprocess failed\n");
    }

    return ret;
}

static int VirtualX_setparameter(struct aml_native_postprocess *native_postprocess, int param, int ch_num, int cmdCode)
{
    effect_descriptor_t tmpdesc;
    int32_t replyData = 0;
    uint32_t replySize = sizeof(int32_t);
    uint32_t cmdSize = (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(uint32_t));
    uint32_t buf32[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *p = (effect_param_t *)buf32;

    p->psize = sizeof(uint32_t);
    p->vsize = sizeof(uint32_t);
    *(int32_t *)p->data = param;
    *((int32_t *)p->data + 1) = ch_num;

    if (native_postprocess->postprocessors[0] != NULL) {
        (*(native_postprocess->postprocessors[0]))->get_descriptor(native_postprocess->postprocessors[0], &tmpdesc);
        if (0 == strcmp(tmpdesc.name,"VirtualX")) {
            (*native_postprocess->postprocessors[0])->command(native_postprocess->postprocessors[0],
                cmdCode, cmdSize, (void *)p, &replySize, &replyData);
        }
    }
    return replyData;
}

void VirtualX_reset(struct aml_native_postprocess *native_postprocess)
{
     if (native_postprocess->libvx_exist) {
        VirtualX_setparameter(native_postprocess, 0, 0, EFFECT_CMD_RESET);
        ALOGI("VirtualX_reset!\n");
     }
     return;
}

void VirtualX_Channel_reconfig(struct aml_native_postprocess *native_postprocess, int ch_num)
{
    int ret = -1;

    if (native_postprocess->libvx_exist) {
        ret = VirtualX_setparameter(native_postprocess, DTS_PARAM_CHANNEL_NUM,
                                    ch_num, EFFECT_CMD_SET_PARAM);
        if (ret > 0) {
            native_postprocess->effect_in_ch = ret;
            if (native_postprocess->effect_in_ch < ch_num) {
                native_postprocess->vx_force_stereo = 1;
            } else {
                native_postprocess->vx_force_stereo = 0;
            }
        } else {
            native_postprocess->vx_force_stereo = 0;
        }
        ALOGD("VirtualX_Channel_reconfig: ch_num_set = %d, ch_num_reply = %d, vx_force_stereo = %d\n",
              ch_num, native_postprocess->effect_in_ch, native_postprocess->vx_force_stereo);
    }
    return;
}

bool Check_VX_lib(void)
{
    void *h_libvx_hanle = NULL;
    if (access(VIRTUALX_LICENSE_LIB_PATH, R_OK) != 0) {
        ALOGI("%s, %s does not exist", __func__, VIRTUALX_LICENSE_LIB_PATH);
        return false;
    }
    h_libvx_hanle = dlopen(VIRTUALX_LICENSE_LIB_PATH, RTLD_NOW);
    if (!h_libvx_hanle) {
        ALOGE("%s, fail to dlopen %s(%s)", __func__, VIRTUALX_LICENSE_LIB_PATH, dlerror());
        return false;
    } else {
        ALOGD("%s, success to dlopen %s", __func__, VIRTUALX_LICENSE_LIB_PATH);
        dlclose(h_libvx_hanle);
        h_libvx_hanle = NULL;
        return true;
    }
}

