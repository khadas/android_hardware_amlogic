/* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: C++ Header file
 */

#ifndef _EARC_UTILS_H_
#define _EARC_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <aml_alsa_mixer.h>

#define CDS_VERSION 0x1
#define CDS_MAX  256

/**
 *  Audio Format Description of CEC Short Audio Descriptor
 *  CEA-861-D: Table 37. RequestShortAudioDescriptorAction.java
 */
typedef enum {
    AML_HDMI_FORMAT_RESERVED1             = 0x0,    // SAD_CODEC_RESERVED1
    AML_HDMI_FORMAT_LPCM                  = 0x1,    // SAD_CODEC_LPCM
    AML_HDMI_FORMAT_AC3                   = 0x2,    // SAD_CODEC_AC3
    AML_HDMI_FORMAT_MPEG1                 = 0x3,    // SAD_CODEC_MPEG1
    AML_HDMI_FORMAT_MP3                   = 0x4,    // SAD_CODEC_MP3
    AML_HDMI_FORMAT_MPEG2MC               = 0x5,    // SAD_CODEC_MPEG2MC
    AML_HDMI_FORMAT_AAC                   = 0x6,    // SAD_CODEC_AAC
    AML_HDMI_FORMAT_DTS                   = 0x7,    // SAD_CODEC_DTS
    AML_HDMI_FORMAT_ATRAC                 = 0x8,    // SAD_CODEC_ATRAC
    AML_HDMI_FORMAT_OBA                   = 0x9,    // SAD_CODEC_OBA
    AML_HDMI_FORMAT_DDP                   = 0xA,    // SAD_CODEC_DDP
    AML_HDMI_FORMAT_DTSHD                 = 0xB,    // SAD_CODEC_DTSHD
    AML_HDMI_FORMAT_MAT                   = 0xC,    // SAD_CODEC_MAT
    AML_HDMI_FORMAT_DST                   = 0xD,    // SAD_CODEC_DST
    AML_HDMI_FORMAT_WMAPRO                = 0xE,    // SAD_CODEC_WMAPRO
    AML_HDMI_FORMAT_RESERVED2             = 0xF,    // SAD_CODEC_RESERVED2
} AML_HDMI_FORMAT_E;

struct format_desc {
    AML_HDMI_FORMAT_E fmt;
    bool is_support;
    unsigned int max_channels;
    /*
     * bit:    6     5     4    3    2    1    0
     * rate: 192  176.4   96  88.2  48  44.1   32
     */
    unsigned int sample_rate_mask;
    unsigned int max_bit_rate;
    /* only used by dd+ format */
    bool   atmos_supported;
    /* parse Dolby Vendor-Specific Audio Data Block.
     * Dolby Audio and Dolby Atmos over HDMI Specification doc.
     * The audio_hw_profile.c fils also has the detail description.
     * Sink device supports Dolby MAT PCM decoding at 48 kHz only,
     * and does not support Dolby TrueHD decoding.
     */
    bool   MAT_PCM_48kHz_only;
};

/*
 *A Short Audio Descriptor is used by HDMI sink devices and HDMI ARC/eARC receiver devices to indicate
 *support for an audio format (for example, Dolby Digital Plus) to a connected HDMI source device or HDMI
 *ARC/eARC transmitter device.
 */
#define EDID_ARRAY_MAX_LEN 38 /* 3 bytes for each audio format, max 30 bytes for audio edid, 8 bytes for TLV header */
struct aml_arc_hdmi_desc {
    int EDID_length;
    unsigned int avr_port;
    char target_EDID_array[EDID_ARRAY_MAX_LEN];
    bool default_edid;
    struct format_desc pcm_fmt;
    struct format_desc dts_fmt;
    struct format_desc dtshd_fmt;
    struct format_desc dd_fmt;
    struct format_desc ddp_fmt;
    struct format_desc mat_fmt;
    struct format_desc mpegh_fmt;
};

int earcrx_fetch_cds(struct mixer *pMixer, char *cds_str);
int earctx_fetch_cds(struct aml_mixer_handle *amixer, char *cds_str, int hex, struct aml_arc_hdmi_desc *hdmi_descs);

#endif
