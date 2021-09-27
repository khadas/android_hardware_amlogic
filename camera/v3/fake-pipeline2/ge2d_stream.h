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

#ifndef __GE2D_STREAM_H__
#define __GE2D_STREAM_H__

#include <linux/videodev2.h>
#include "Base.h"
#include "camera_hw.h"
#include <aml_ge2d.h>
#include <ge2d_port.h>
#include <cutils/properties.h>
#include "IonIf.h"

namespace android {

class ge2dTransform {
public:
    enum {
            NV12,
            RGB
        };
private:
        aml_ge2d_t m_amlge2d;
        int m_share_fd;
        bool mFirst;
        size_t degree;
        bool flip = false, mirror = false;
        IONInterface* mION;
public:
        ge2dTransform();
        ~ge2dTransform();
        int ge2d_rotation(int dst_fd,size_t src_w, size_t src_h, int fmt,
                                                        int degree);
        int ge2d_mirror(int dst_fd,size_t src_w,size_t src_h,int fmt);
        int ge2d_flip(int dst_fd,size_t src_w,size_t src_h,int fmt);
        int doRotationAndMirror(android::StreamBuffer &b);
        int ge2d_copy(int dst_fd, int src_fd, size_t width, size_t height, int fmt);
        int ge2d_copy_dma(int dst_fd, int src_fd, size_t width, size_t height,int fmt);

private:
        int ge2d_copy_internal(int dst_fd, int dst_alloc_type,int src_fd,
                                           int src_alloc_type, size_t width, size_t height,int fmt);
};

}
#endif
