/* Copyright Statement:
*
* This software/firmware and related documentation ("Amlogic Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to Amlogic Inc. and/or its licensors. Without
* the prior written permission of Amlogic inc. and/or its licensors, any
* reproduction, modification, use or disclosure of Amlogic Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* Amlogic Inc. (C) 2013. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("Amlogic SOFTWARE")
* RECEIVED FROM Amlogic AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. Amlogic EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES Amlogic PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE RENDERED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE Amlogic SOFTWARE, AND RECEIVER AGREES
* TO LORESULT_OK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN Amlogic
* SOFTWARE. Amlogic SHALL ALSO NOT BE RESPONSIBLE FOR ANY Amlogic SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND Amlogic'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE Amlogic SOFTWARE
* RELEASED HEREUNDER WILL BE, AT Amlogic'S OPTION, TO REVISE OR REPLACE THE
* Amlogic SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO Amlogic FOR SUCH Amlogic SOFTWARE AT ISSUE.
*/

#ifndef FRAME_RENDERER_H
#define FRAME_RENDERER_H

#include <GraphicBufferUtils.h>
#include "BaseRenderer.h"

class FrameRenderer : public BaseRenderer {
public:
    FrameRenderer(sp<GraphicBuffer> frameBuffer);
    virtual ~FrameRenderer();

    int Begin();
    int Finish();

protected:
    EGLContext  mContext = EGL_NO_CONTEXT;
    EGLDisplay  mDisplay = EGL_NO_DISPLAY;
    EGLSurface  mSurface = EGL_NO_SURFACE;

    int mUseMultisample = 0;
    GLsizei mMultisampleSize = 4;

    GLuint mDepthBufferID = GL_NONE;
    GLenum mDepthBufferFormat = GL_DEPTH_COMPONENT;

    sp<GraphicBuffer> mFrameBuffer = nullptr;
    EGLClientBuffer mClientBuffer = nullptr;

    size_t mFrameBufferWidth = 0;
    size_t mFrameBufferHeight = 0;

    GLenum mFrameBufferFormat = GL_NONE;

    size_t mFrameBufferPixelLen = 0;
    size_t mFrameBufferLen = 0;

    GLuint mFrameBufferID = GL_NONE;
    GLuint mFrameBufferTextureID = GL_NONE;
    EGLenum mFrameBufferEglImageTarget = EGL_NATIVE_BUFFER_ANDROID;
    EGLImageKHR mFrameBufferEglImage = EGL_NO_IMAGE_KHR;

    int mDumpFrameBuffer = 1;

    char mFrameBufferPath[128] = { 0 };
    const char *mFrameBufferPathFormat = "/data/gpu_frame_w%d_h%d_%lld.bmp";
    int64_t mFrameBufferIndex = 0;

protected:
    EGLint InitEGL();
    EGLint DeinitEGL();
};

#endif // FRAME_RENDERER_H
