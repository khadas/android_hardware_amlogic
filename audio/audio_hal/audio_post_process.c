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
//#define LOG_NDEBUG 0

#include <dlfcn.h>
#include <cutils/log.h>

#include "audio_post_process.h"
#include "Virtualx.h"
#include "aml_dec_api.h"
#include "aml_dts_dec_api.h"

static int check_dts_config(struct aml_native_postprocess *native_postprocess) {
    int cur_channels = dca_get_out_ch_internal();

    if (native_postprocess->vx_force_stereo == 1)
        cur_channels = 2;

    if (cur_channels >= 6) {
        cur_channels = 6;
    } else {
        cur_channels = 2;
    }

    if (native_postprocess->effect_in_ch != cur_channels) {

        ALOGD("%s, reconfig VX pre_channels = %d, cur_channels = %d, vx_force_stereo = %d",
            __func__, native_postprocess->effect_in_ch,
            cur_channels, native_postprocess->vx_force_stereo);

        VirtualX_reset(native_postprocess);
        if (cur_channels == 6) {
            VirtualX_Channel_reconfig(native_postprocess, 6);
        } else {
            VirtualX_Channel_reconfig(native_postprocess, 2);
        }
        native_postprocess->effect_in_ch = cur_channels;
    }

    return 0;
}

int audio_post_process(struct aml_native_postprocess *native_postprocess, int16_t *in_buffer, size_t in_frames)
{
    int ret = 0, j = 0;
    audio_buffer_t in_buf;
    audio_buffer_t out_buf;
    int frames = in_frames;

    if (native_postprocess == NULL &&
        native_postprocess->num_postprocessors != native_postprocess->total_postprocessors) {
        return ret;
    }

    if (native_postprocess->libvx_exist) {
        check_dts_config(native_postprocess);
    }

    for (j = 0; j < native_postprocess->num_postprocessors; j++) {
        effect_handle_t effect = native_postprocess->postprocessors[j];
        if (effect != NULL) {
            if (native_postprocess->libvx_exist && native_postprocess->effect_in_ch == 6 && j == 0) {
                /* skip multi channel processing for dts streaming in VX */
                continue;
            } else {
                /* do 2 channel processing */
                in_buf.frameCount = out_buf.frameCount = frames;
                in_buf.s16 = out_buf.s16 = in_buffer;
                ret = (*effect)->process(effect, &in_buf, &out_buf);
            }
        }
    }

    if (ret < 0) {
        ALOGE("postprocess failed\n");
    }

    return ret;
}

int audio_VX_post_process(struct aml_native_postprocess *native_postprocess, int16_t *in_buffer, size_t bytes)
{
    int ret = 0;
    audio_buffer_t in_buf;
    audio_buffer_t out_buf;

    effect_handle_t effect = native_postprocess->postprocessors[0];
    if (effect != NULL && native_postprocess->libvx_exist && native_postprocess->effect_in_ch == 6) {
        /* do multi channel processing for dts streaming in VX */
        in_buf.frameCount = bytes/12;
        out_buf.frameCount = bytes/12;
        in_buf.s16 = out_buf.s16 = in_buffer;
        ret = (*effect)->process(effect, &in_buf, &out_buf);
        if (ret < 0) {
            ALOGE("postprocess failed\n");
        } else {
            ret = bytes/3;
        }
    }

    return ret;
}

static int VirtualX_setparameter(struct aml_native_postprocess *native_postprocess, int param, int ch_num, int cmdCode)
{
    effect_handle_t effect = native_postprocess->postprocessors[0];
    int32_t replyData = 0;
    uint32_t replySize = sizeof(int32_t);
    uint32_t cmdSize = (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(uint32_t));
    uint32_t buf32[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *p = (effect_param_t *)buf32;

    p->psize = sizeof(uint32_t);
    p->vsize = sizeof(uint32_t);
    *(int32_t *)p->data = param;
    *((int32_t *)p->data + 1) = ch_num;

    if (effect != NULL) {
        (*effect)->command(effect, cmdCode, cmdSize, (void *)p, &replySize, &replyData);
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
        ret = VirtualX_setparameter(native_postprocess,
                                    DTS_PARAM_CHANNEL_NUM,
                                    ch_num, EFFECT_CMD_SET_PARAM);
        if (ret != ch_num) {
            ALOGE("Set VX input channel error: channel %d, ret = %d\n", ch_num, ret);
        }
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
        /* VX effect lib is in system, set dts output as stream content */
        dca_set_out_ch_internal(0);
        return true;
    }
}

