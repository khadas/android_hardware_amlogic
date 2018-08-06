/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <hardware/gralloc1.h>
#include <gralloc_priv.h>

#include "am_gralloc_internal.h"

bool am_gralloc_is_omx_metadata_extend_usage(
    uint64_t usage) {
    uint64_t omx_metadata_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER
        | GRALLOC1_PRODUCER_USAGE_CPU_READ
        | GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
    if (((usage & omx_metadata_usage) == omx_metadata_usage)
        && !(usage & GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET)) {
        return true;
    }

    return false;
}

bool am_gralloc_is_omx_osd_extend_usage(uint64_t usage) {
    uint64_t omx_osd_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER
        |GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET;
    if ((usage & omx_osd_usage) == omx_osd_usage) {
        return true;
    }

    return false;
}

bool am_gralloc_is_video_overlay_extend_usage(
    uint64_t usage) {
    uint64_t video_overlay_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER;
    if (!am_gralloc_is_omx_metadata_extend_usage(usage)
        && !am_gralloc_is_omx_osd_extend_usage(usage)
        && ((usage & video_overlay_usage) == video_overlay_usage)) {
        return true;
    }

    return false;
}

int am_gralloc_get_omx_metadata_extend_flag() {
    return private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY
        | private_handle_t::PRIV_FLAGS_VIDEO_OMX;
}

int am_gralloc_get_coherent_extend_flag() {
    return private_handle_t::PRIV_FLAGS_USES_ION_DMA_HEAP
        | private_handle_t::PRIV_FLAGS_CONTINUOUS_BUF;
}

int am_gralloc_get_video_overlay_extend_flag() {
    return private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY;
}


