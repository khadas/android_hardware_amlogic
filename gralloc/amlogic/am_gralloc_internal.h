/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AM_GRALLOC_EXT_INTERNAL_H
#define AM_GRALLOC_EXT_INTERNAL_H

#include <gralloc_priv.h>
#include <utils/NativeHandle.h>

#define OMX_VIDEOLAYER_ALLOC_BUFFER_WIDTH     192
#define OMX_VIDEOLAYER_ALLOC_BUFFER_HEIGHT    90

/*
For gralloc to check producer usage.
The usage is usage defined in gralloc.h/gralloc1.h.
*/
bool am_gralloc_is_omx_metadata_extend_usage(uint64_t usage);
bool am_gralloc_is_omx_osd_extend_usage(uint64_t usage);
bool am_gralloc_is_video_overlay_extend_usage(uint64_t usage);
bool am_gralloc_is_secure_extend_usage(uint64_t usage);


/*
For gralloc to set special buffer flag.
*/
int am_gralloc_get_omx_metadata_extend_flag();
int am_gralloc_get_coherent_extend_flag();
int am_gralloc_get_video_overlay_extend_flag();
int am_gralloc_get_secure_extend_flag();
int am_gralloc_alloc_v4lvideo_file();
int am_gralloc_alloc_v4l2video_file();
int am_gralloc_free_v4lvideo_file(int fd);

/*
Handle amlogic extend attrbuites.
*/
typedef enum {
    AM_PRIV_EXTEND_OMX_V4L = 1,
} AM_PRIV_EXTEND_FLAG;

int am_gralloc_extend_attr_allocate(uint64_t usage, private_handle_t *hnd);
int am_gralloc_extend_attr_free(private_handle_t *hnd);

#endif/*AM_GRALLOC_EXT_INTERNAL_H*/
