/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "am_gralloc_internal.h"
#include <gralloc_priv.h>
#include <sys/ioctl.h>

#define V4LVIDEO_IOC_MAGIC  'I'
#define V4LVIDEO_IOCTL_ALLOC_FD   _IOW(V4LVIDEO_IOC_MAGIC, 0x02, int)


#include "am_gralloc_internal.h"

#if USE_BUFFER_USAGE
#include <hardware/gralloc1.h>
#else
#include <hardware/gralloc.h>
#include "am_gralloc_usage.h"
#endif


#define UNUSED(x) (void)x

bool am_gralloc_is_omx_metadata_extend_usage(
    uint64_t usage) {
#if USE_BUFFER_USAGE
    uint64_t omx_metadata_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER
        | GRALLOC1_PRODUCER_USAGE_CPU_READ
        | GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
    if (((usage & omx_metadata_usage) == omx_metadata_usage)
        && !(usage & GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET)) {
        return true;
    }
#else
    if (usage & GRALLOC_USAGE_AML_OMX_OVERLAY) {
        return true;
    }
#endif

    return false;
}

bool am_gralloc_is_omx_osd_extend_usage(uint64_t usage) {
#if USE_BUFFER_USAGE
    uint64_t omx_osd_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER
        |GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET;
    if ((usage & omx_osd_usage) == omx_osd_usage) {
        return true;
    }
#else
    if (usage & GRALLOC_USAGE_AML_DMA_BUFFER) {
        return true;
    }
#endif
    return false;
}

bool am_gralloc_is_video_overlay_extend_usage(
    uint64_t usage) {
#if USE_BUFFER_USAGE
    uint64_t video_overlay_usage = GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER;
    if (!am_gralloc_is_omx_metadata_extend_usage(usage)
        && !am_gralloc_is_omx_osd_extend_usage(usage)
        && ((usage & video_overlay_usage) == video_overlay_usage)) {
        return true;
    }
#else
    if (usage & GRALLOC_USAGE_AML_VIDEO_OVERLAY) {
        return true;
    }
#endif
    return false;
}

bool am_gralloc_is_secure_extend_usage(
    uint64_t usage) {
#if USE_BUFFER_USAGE
    if (usage & GRALLOC1_PRODUCER_USAGE_PROTECTED) {
        return true;
    }
#else
    if (usage & GRALLOC_USAGE_AML_SECURE || usage & GRALLOC_USAGE_PROTECTED) {
        return true;
    }
#endif

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

int am_gralloc_get_secure_extend_flag() {
    return private_handle_t::PRIV_FLAGS_SECURE_PROTECTED;
}

int am_gralloc_extend_attr_allocate(uint64_t usage,
    private_handle_t *hnd) {
    if (!hnd)
        return -1;

    if (am_gralloc_is_omx_osd_extend_usage(usage)) {
        hnd->am_extend_fd = am_gralloc_alloc_v4l2video_file();
    } else if (am_gralloc_is_omx_metadata_extend_usage(usage)) {
        hnd->am_extend_fd = am_gralloc_alloc_v4lvideo_file();
    }

    if (hnd->am_extend_fd >= 0) {
        hnd->am_extend_type = AM_PRIV_EXTEND_OMX_V4L;
    }

    /*Android always need valid fd.
    So if no valid extend fd,just dup the shared fd.
    */
    if (hnd->am_extend_fd < 0) {
        hnd->am_extend_fd = ::dup(hnd->share_fd);
        hnd->am_extend_type = 0;
    }

    return 0;
}

int am_gralloc_extend_attr_free(private_handle_t *hnd) {
    if (!hnd)
        return 0;

    am_gralloc_free_v4lvideo_file(hnd->am_extend_fd);

    hnd->am_extend_fd = -1;
    return 0;
}

int am_gralloc_alloc_v4lvideo_file() {
    static int v4ldev = open("/dev/v4lvideo", O_RDONLY | O_CLOEXEC);
    if (v4ldev < 0) {
        ALOGE("open /dev/v4lvideo failed!\n");
        return -1;
    }

    int fd = -1;
    int err = ioctl(v4ldev, V4LVIDEO_IOCTL_ALLOC_FD, &fd);
    if (err < 0) {
        ALOGE("call V4LVIDEO ioctl failed (%d).", err);
        return -1;
    }

    if (fd < 0) {
        ALOGE("V4LVIDEO_IOCTL_ALLOC_FD return invalid fd (%d).", fd);
    }

    return fd;
}

int am_gralloc_alloc_v4l2video_file() {
    static int v4l2dev = open("/dev/video26", O_RDONLY | O_CLOEXEC);
    if (v4l2dev < 0) {
        ALOGE("open /dev/v4lvideo failed!\n");
        return -1;
    }

    int fd = -1;
    int err = ioctl(v4l2dev, V4LVIDEO_IOCTL_ALLOC_FD, &fd);
    if (err < 0) {
        ALOGE("call V4L2 ioctl failed (%d).", err);
        return -1;
    }

    if (fd < 0) {
        ALOGE("V4LVIDEO_IOCTL_ALLOC_FD return invalid fd (%d).", fd);
    }

    return fd;
}

int am_gralloc_free_v4lvideo_file(int fd) {
    if (fd >= 0) {
        close(fd);
    }

    return 0;
}

