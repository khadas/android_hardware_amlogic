/*
 * Copyright (C) 2014 The Android Open Source Project
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

//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0

#define LOG_TAG "ge2d_stream"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>
#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif
#define RATIO_SCALE
#include <hardware/camera3.h>
#include <DebugUtils.h>
#include "ge2d_stream.h"


namespace android {
//==================================================================================================
// ge2d transform
//==================================================================================================
ge2dTransform::ge2dTransform() {
    memset(&m_amlge2d,0,sizeof(aml_ge2d_t));
    memset(&(m_amlge2d.ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.dst_info), 0, sizeof(buffer_info_t));

    int ret = aml_ge2d_init(&m_amlge2d);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
    }
    mFirst = false;
    mION = IONInterface::get_instance();
}

ge2dTransform::~ge2dTransform() {
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_INVALID;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 0;
    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = -1;
    aml_ge2d_mem_free(&m_amlge2d);
    aml_ge2d_exit(&m_amlge2d);
}

int ge2dTransform::ge2d_copy(int dst_fd, int src_fd, size_t width, size_t height,int fmt)
{
    ALOGVV("%s: E", __FUNCTION__);
    ATRACE_CALL();
    int ret = 0;
    ge2d_copy_internal(dst_fd, AML_GE2D_MEM_ION,src_fd,
                        AML_GE2D_MEM_ION, width, height,fmt);
    return ret;
}

int ge2dTransform::ge2d_copy_dma(int dst_fd, int src_fd, size_t width, size_t height,int fmt)
{
    ALOGVV("%s: E", __FUNCTION__);
    int ret = 0;
    ret = ge2d_copy_internal(dst_fd, AML_GE2D_MEM_ION,src_fd,
                                AML_GE2D_MEM_DMABUF, width, height,fmt);
    return ret;
}
/*function: this funtion copy image from one buffer to another buffer.
  dst_fd : the share fd related to destination buffer
  dst_alloc_type: where dose the destination buffer come from. etc ION or dma buffer
  src_fd : the share fd related to source buffer
  src_alloc_type: where dose the source buffer come from. etc ION or dma buffer
  width: the width of image
  height: the height of image
 */
int ge2dTransform::ge2d_copy_internal(int dst_fd, int dst_alloc_type,int src_fd,
                                int src_alloc_type, size_t width, size_t height,int fmt)
{
    ATRACE_CALL();
    ALOGVV("%s: E", __FUNCTION__);

    switch (fmt) {
        case NV12:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
        case RGB:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_RGB_888;
            break;
        default:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
    }

    m_amlge2d.ge2dinfo.src_info[0].canvas_w = width;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = height;
    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;

    m_amlge2d.ge2dinfo.src_info[1].canvas_w = width;
    m_amlge2d.ge2dinfo.src_info[1].canvas_h = height;
    m_amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;

    m_amlge2d.ge2dinfo.dst_info.canvas_w = width;
    m_amlge2d.ge2dinfo.dst_info.canvas_h = height;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;

    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    m_amlge2d.ge2dinfo.offset = 0;
    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
    m_amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

    m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = src_alloc_type;
    m_amlge2d.ge2dinfo.src_info[1].memtype = /*GE2D_CANVAS_ALLOC;//*/GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = /*AML_GE2D_MEM_ION;//*/AML_GE2D_MEM_INVALID;
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = dst_alloc_type;

    m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.w = width;
    m_amlge2d.ge2dinfo.src_info[0].rect.h = height;
    //m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;
    m_amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
    //m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;

    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = width;
    m_amlge2d.ge2dinfo.dst_info.rect.h = height;
    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    //m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    //m_amlge2d.ge2dinfo.dst_info.plane_number = 1;

    int ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    //aml_ge2d_exit(&amlge2d);
    return 0;

}

int ge2dTransform::ge2d_mirror(int dst_fd,size_t src_w,
                size_t src_h,int fmt) {
   ATRACE_CALL();
   ALOGD("%s: src_w=%d, src_h=%d share_fd=%dE", __FUNCTION__,src_w,src_h,m_share_fd);
   int ret = 0;
   switch (fmt) {
       case NV12:
              m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
           m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
           m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
           break;
       case RGB:
           m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_RGB_888;
           m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_RGB_888;
           m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_RGB_888;
           break;
       default:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
           break;
   }

    m_amlge2d.ge2dinfo.src_info[0].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = m_share_fd;

    m_amlge2d.ge2dinfo.src_info[1].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[1].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;

   m_amlge2d.ge2dinfo.dst_info.canvas_w = src_w;
   m_amlge2d.ge2dinfo.dst_info.canvas_h = src_h;
   m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
   m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
   m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
   m_amlge2d.ge2dinfo.offset = 0;

   m_amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

   m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
   m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
   m_amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
   m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;
   m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
   m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;

   m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
   m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
   m_amlge2d.ge2dinfo.src_info[0].rect.w = src_w;
   m_amlge2d.ge2dinfo.src_info[0].rect.h = src_h;
   m_amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
   //m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
   m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;


   m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
   m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
   m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
   m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
   //m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
   //m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
   //m_amlge2d.ge2dinfo.dst_info.plane_number = 1;

   switch (fmt) {
       case NV12:
           m_amlge2d.ge2dinfo.color = 0x008080ff;
           break;
       case RGB:
           m_amlge2d.ge2dinfo.color = 0;
           break;
       default:
           m_amlge2d.ge2dinfo.color = 0x008080ff;
           break;
   }
    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;

    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_MIRROR_X;

    ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGVV("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
   return 0;
}

int ge2dTransform::ge2d_flip(int dst_fd,size_t src_w,
                              size_t src_h,int fmt)
{
    ATRACE_CALL();
    ALOGVV("%s: src_w=%d, src_h=%d share_fd = %d E", __FUNCTION__,src_w,src_h,m_share_fd);
    switch (fmt) {
        case NV12:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
        case RGB:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_RGB_888;
            break;
        default:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
    }


    m_amlge2d.ge2dinfo.src_info[0].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = m_share_fd;

    m_amlge2d.ge2dinfo.src_info[1].canvas_w = src_h;
    m_amlge2d.ge2dinfo.src_info[1].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;

    m_amlge2d.ge2dinfo.dst_info.canvas_w = src_w;
    m_amlge2d.ge2dinfo.dst_info.canvas_h = src_h;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    m_amlge2d.ge2dinfo.offset = 0;

    m_amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

    m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
    m_amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;

    m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].rect.h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
    //m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;

    //m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    //m_amlge2d.ge2dinfo.dst_info.plane_number = 1;

    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
    m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    switch (fmt) {
        case NV12:
            m_amlge2d.ge2dinfo.color = 0x008080ff;
            break;
        case RGB:
            m_amlge2d.ge2dinfo.color = 0;
            break;
        default:
            m_amlge2d.ge2dinfo.color = 0x008080ff;
            break;
    }
    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_MIRROR_Y;

    int ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGVV("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    return 0;
}

//scale nv21 to other format
int ge2dTransform::ge2d_scale(int dst_fd,int dst_fmt, size_t dst_w,
                size_t dst_h,int src_fd, size_t src_w, size_t src_h) {

    //ATRACE_CALL();
    //ALOGD("%s: w=%d, h=%d, src %d %d", __FUNCTION__,dst_w,dst_h,src_w,src_h);
    aml_ge2d_t amlge2d;
    int src_width = src_w;
    int src_height = src_h;
    memset(&amlge2d,0,sizeof(aml_ge2d_t));
    memset(&(amlge2d.ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d.ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d.ge2dinfo.dst_info), 0, sizeof(buffer_info_t));
    // the input format is NV21
    amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
    amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
    amlge2d.ge2dinfo.dst_info.format = dst_fmt;
    //configure the source canvas size
    amlge2d.ge2dinfo.src_info[0].canvas_w = src_width;
    amlge2d.ge2dinfo.src_info[0].canvas_h = src_height;
    amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;

    amlge2d.ge2dinfo.src_info[1].canvas_w = src_width;
    amlge2d.ge2dinfo.src_info[1].canvas_h = src_height;
    amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;
    //configure the destination canvas size
    amlge2d.ge2dinfo.dst_info.canvas_w = dst_w;
    amlge2d.ge2dinfo.dst_info.canvas_h = dst_h;
    amlge2d.ge2dinfo.dst_info.plane_number = 1;
    amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;

    amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    amlge2d.ge2dinfo.offset = 0;
    amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
    amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

    amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
    amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;
    amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;


    int ret = aml_ge2d_init(&amlge2d);
    if (ret < 0) {
        aml_ge2d_exit(&amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }

    amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    amlge2d.ge2dinfo.src_info[0].rect.w = src_width;
    amlge2d.ge2dinfo.src_info[0].rect.h = src_height;
    amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;
    amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
    amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;

    amlge2d.ge2dinfo.dst_info.rect.x = 0;
    amlge2d.ge2dinfo.dst_info.rect.y = 0;
    amlge2d.ge2dinfo.dst_info.rect.w = dst_w;
    amlge2d.ge2dinfo.dst_info.rect.h = dst_h;
    amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    amlge2d.ge2dinfo.dst_info.plane_number = 1;
    switch (dst_fmt) {
        case PIXEL_FORMAT_YCbCr_420_SP_NV12:
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3/2;
            break;
        case PIXEL_FORMAT_RGB_888:
        case PIXEL_FORMAT_RGB_888 | DST_SIGN_MDOE:
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3;
            break;
        default://nv12
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3/2;
            break;
    }
    ret = aml_ge2d_process(&amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    aml_ge2d_exit(&amlge2d);

    return 0;
}

//format convert. eg. UYVY->NV12
int ge2dTransform::ge2d_fmt_convert(int dst_fd,int dst_fmt, size_t dst_w,size_t dst_h,
                                    int src_fd, int src_fmt, size_t src_w, size_t src_h) {

    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;
    m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
    m_amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;

    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].rect.h = src_h;

    m_amlge2d.ge2dinfo.src_info[0].format = src_fmt ; //PIXEL_FORMAT_YCbCr_422_UYVY; //PIXEL_FORMAT_YCbCr_420_SP_NV12
    m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xFF; /* global plane alpha*/

    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
    m_amlge2d.ge2dinfo.dst_info.canvas_w = dst_w;
    m_amlge2d.ge2dinfo.dst_info.canvas_h = dst_h;
    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = dst_w;
    m_amlge2d.ge2dinfo.dst_info.rect.h = dst_h;
    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    m_amlge2d.ge2dinfo.dst_info.format =  dst_fmt ; //PIXEL_FORMAT_YCbCr_420_SP_NV12; //PIXEL_FORMAT_RGBA_8888;
    m_amlge2d.ge2dinfo.dst_info.plane_alpha = 0xFF; /* global plane alpha*/

    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;

    int ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        printf("ge2d process failed, %s (%d)\n", __func__, __LINE__);
        return ret;
    }
    return ret;
}


//scale nv21 to other format
int ge2dTransform::ge2d_keep_ration_scale(int dst_fd,int dst_fmt, size_t dst_w,
                size_t dst_h,int src_fd, size_t src_w, size_t src_h) {

    //ATRACE_CALL();
    //ALOGD("%s: w=%d, h=%d, src %d %d", __FUNCTION__,dst_w,dst_h,src_w,src_h);
    aml_ge2d_t amlge2d;
    int src_rect_start_row = 0;
    int src_rect_start_col = 0;
    int src_rect_width     = src_w;
    int src_rect_height    = src_h;

    int src_width = src_w;
    int src_height = src_h;
    if (src_w * dst_h != src_h * dst_w) {
        // int & out not the same ration.
        if (dst_w * src_h < dst_h * src_w) {
            // eg: src 16:9  dst 4:3.
            src_rect_width     = src_h * dst_w / dst_h;
            src_rect_start_col = (src_w - src_width) / 2;
        } else {
            src_rect_height    = src_w * dst_h / dst_w;
            src_rect_start_row = (src_h - src_height) / 2;
        }
    }
    memset(&amlge2d,0,sizeof(aml_ge2d_t));
    memset(&(amlge2d.ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d.ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d.ge2dinfo.dst_info), 0, sizeof(buffer_info_t));
    // the input format is NV21
    amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
    amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
    amlge2d.ge2dinfo.dst_info.format = dst_fmt;
    //configure the source canvas size
    amlge2d.ge2dinfo.src_info[0].canvas_w = src_width;
    amlge2d.ge2dinfo.src_info[0].canvas_h = src_height;
    amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;

    amlge2d.ge2dinfo.src_info[1].canvas_w = src_width;
    amlge2d.ge2dinfo.src_info[1].canvas_h = src_height;
    amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;
    //configure the destination canvas size
    amlge2d.ge2dinfo.dst_info.canvas_w = dst_w;
    amlge2d.ge2dinfo.dst_info.canvas_h = dst_h;
    amlge2d.ge2dinfo.dst_info.plane_number = 1;
    amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;

    amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    amlge2d.ge2dinfo.offset = 0;
    amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
    amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

    amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
    amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;
    amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;


    int ret = aml_ge2d_init(&amlge2d);
    if (ret < 0) {
        aml_ge2d_exit(&amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }

    amlge2d.ge2dinfo.src_info[0].rect.x = src_rect_start_col;
    amlge2d.ge2dinfo.src_info[0].rect.y = src_rect_start_row;
    amlge2d.ge2dinfo.src_info[0].rect.w = src_rect_width;
    amlge2d.ge2dinfo.src_info[0].rect.h = src_rect_height;
    amlge2d.ge2dinfo.src_info[0].shared_fd[0] = src_fd;
    amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
    amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;

    amlge2d.ge2dinfo.dst_info.rect.x = 0;
    amlge2d.ge2dinfo.dst_info.rect.y = 0;
    amlge2d.ge2dinfo.dst_info.rect.w = dst_w;
    amlge2d.ge2dinfo.dst_info.rect.h = dst_h;
    amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    amlge2d.ge2dinfo.dst_info.plane_number = 1;
    switch (dst_fmt) {
        case PIXEL_FORMAT_YCbCr_420_SP_NV12:
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3/2;
            break;
        case PIXEL_FORMAT_RGB_888:
        case PIXEL_FORMAT_RGB_888 | DST_SIGN_MDOE:
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3;
            break;
        default://nv12
            amlge2d.ge2dinfo.dst_info.offset[0] = 0 * dst_w *dst_h * 3/2;
            break;
    }
    ret = aml_ge2d_process(&amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&amlge2d);
        ALOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    aml_ge2d_exit(&amlge2d);

    return 0;
}

/*function: make image rotation some degree using ge2d device
  dst_fd : the share fd of destination buffer.
  src_w  : the width of source image.
  src_h  : the height of source image
  fmt    : the pixel format of source image
  degree : the rotation degree
  amlge2d: the ge2d device object which has allocate buffer for source image.
*/
int ge2dTransform::ge2d_rotation(int dst_fd,size_t src_w,
                size_t src_h,int fmt, int degree) {

    ALOGVV("%s: src_w=%d, src_h=%d share_fd =%d E", __FUNCTION__,src_w,src_h,m_share_fd);

    switch (fmt) {
        case NV12:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
        case RGB:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_RGB_888;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_RGB_888;
            break;
        default:
            m_amlge2d.ge2dinfo.src_info[0].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.src_info[1].format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            m_amlge2d.ge2dinfo.dst_info.format = PIXEL_FORMAT_YCbCr_420_SP_NV12;
            break;
    }
    m_amlge2d.ge2dinfo.src_info[0].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = m_share_fd;

    m_amlge2d.ge2dinfo.src_info[1].canvas_w = src_w;
    m_amlge2d.ge2dinfo.src_info[1].canvas_h = src_h;
    m_amlge2d.ge2dinfo.src_info[1].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[1].shared_fd[0] = -1;

    m_amlge2d.ge2dinfo.dst_info.canvas_w = src_w;
    m_amlge2d.ge2dinfo.dst_info.canvas_h = src_h;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    switch (degree) {
        case 0:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
            break;
        case 90:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_90;
            break;
        case 180:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_180;
            break;
        case 270:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_270;
            break;
        default:
            break;
    }
    m_amlge2d.ge2dinfo.offset = 0;

    m_amlge2d.ge2dinfo.blend_mode = BLEND_MODE_PREMULTIPLIED;

    m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;
    m_amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;

    m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.w = src_w;
    m_amlge2d.ge2dinfo.src_info[0].rect.h = src_h;
    m_amlge2d.ge2dinfo.src_info[0].layer_mode = 0;
    //m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xff;

    //m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dst_fd;
    //m_amlge2d.ge2dinfo.dst_info.plane_number = 1;

    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
    m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
    m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
    switch (fmt) {
        case NV12:
            m_amlge2d.ge2dinfo.color = 0x008080ff;
            break;
        case RGB:
            m_amlge2d.ge2dinfo.color = 0;
            break;
        default:
            m_amlge2d.ge2dinfo.color = 0x008080ff;
            break;
    }
    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_FILLRECTANGLE;
    int ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGVV("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }

    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
#ifdef RATIO_SCALE
    float ratio = (src_h*1.0)/(src_w*1.0);
    switch (degree) {
        case 0:
            m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
            m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
            break;
        case 90:
            m_amlge2d.ge2dinfo.dst_info.rect.x = (src_w - (ratio*src_h))/2;
            m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.w = ratio*src_h;
            m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_90;
            break;
        case 180:
            m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
            m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_180;
            break;
        case 270:
            m_amlge2d.ge2dinfo.dst_info.rect.x = (src_w - (ratio*src_h))/2;
            m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
            m_amlge2d.ge2dinfo.dst_info.rect.w = ratio*src_h;
            m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_270;
            break;
        default:
            break;
    }
#else
    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = src_w;
    m_amlge2d.ge2dinfo.dst_info.rect.h = src_h;
    switch (degree) {
        case 0:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_0;
            break;
        case 90:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_90;
            break;
        case 180:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_180;
            break;
        case 270:
            m_amlge2d.ge2dinfo.dst_info.rotation = GE2D_ROTATION_270;
            break;
        default:
            break;
    };
#endif
    ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGVV("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    return 0;
}

int ge2dTransform::doRotationAndMirror(StreamBuffer &b, bool forceMirror) {
    ATRACE_CALL();
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.camera.rotation", property, "0");
    int value = atoi(property);
    switch (value) {
        case 0:
        case 90:
        case 180:
        case 270:
            degree = value;
            break;
        default:
            degree = 0;
            break;
    };
    enum {
        HORIZANTAL = 0,
        VERTICAL,
    };
    property_get("vendor.camera.mirror", property, "false");
    if (strstr(property, "true"))
        mirror = true;
    else
        mirror = false;

    mirror = forceMirror ? true : mirror;

    property_get("vendor.camera.flip", property, "false");
    if (strstr(property, "true"))
        flip = true;
    else
        flip = false;
    /*alloc buffer using ge2d device*/
    size_t width = b.width;
    size_t height = b.height;
    size_t size;
    int GE2D_FORMAT = NV12;
    switch (b.format) {
        case HAL_PIXEL_FORMAT_RGB_888:
            GE2D_FORMAT = RGB;
            size = width * height * 3;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            GE2D_FORMAT = NV12;
            size = width * height * 3/2;

            break;
        default:
            GE2D_FORMAT = NV12;
            size = width * height * 3/2;
            break;
    }
    if (mirror ||flip|| !!degree) {
        mION->alloc_buffer(size,&m_share_fd);
    }
    if (mirror) {
        //ge2d_alloc(width,height,&share_fd,GE2D_FORMAT,m_Amlge2d);
        /*copy image to memory allocated by ge2d*/
        ge2d_copy(m_share_fd,b.share_fd,width,height,GE2D_FORMAT);
        /*if decode ok, then mirror the image*/
        ge2d_mirror(b.share_fd,width,height,GE2D_FORMAT);
    }
    if (flip) {
        //ge2d_alloc(width,height,&share_fd,GE2D_FORMAT,m_Amlge2d);
        ge2d_copy(m_share_fd,b.share_fd,width,height,GE2D_FORMAT);
        /*if decode ok, then mirror the image*/
        ge2d_flip(b.share_fd,width,height,GE2D_FORMAT);
    }
    if (!!degree) {
        //ge2d_alloc(width,height,&share_fd,GE2D_FORMAT,m_Amlge2d);
        /*copy image to memory allocated by ge2d*/
        ge2d_copy(m_share_fd,b.share_fd,width,height,GE2D_FORMAT);
        /*if decode ok, then rotate the image*/
        ge2d_rotation(b.share_fd,width,height,GE2D_FORMAT,degree);
    }
    if (mirror ||flip|| !!degree) {
        mION->free_buffer(m_share_fd);
    }
    return 0;
}

}// namespace android
