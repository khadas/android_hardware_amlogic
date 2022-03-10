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

#define LOG_TAG "FrameRenderer"

#include "include/FrameRenderer.h"


FrameRenderer::FrameRenderer(sp<GraphicBuffer> frameBuffer) {
    mFrameBuffer = frameBuffer;

    if (mFrameBuffer != nullptr) {
        mClientBuffer = mFrameBuffer->getNativeBuffer();

        mFrameBufferWidth = mFrameBuffer->getWidth();
        mFrameBufferHeight = mFrameBuffer->getHeight();

        GraphicBufferUtils::PixelFormatToGLFormat(
                mFrameBuffer->getPixelFormat(), mFrameBufferFormat);

        GLUtils::GLBufferFormatToPixelLen(mFrameBufferFormat,
                mFrameBufferPixelLen);

        mFrameBufferLen = mFrameBufferWidth * mFrameBufferHeight * mFrameBufferPixelLen;
    }

    mViewLeft = 0;
    mViewTop = 0;
    mViewWidth = mFrameBufferWidth;
    mViewHeight = mFrameBufferHeight;
}

FrameRenderer::~FrameRenderer() {
    mFrameBuffer = nullptr;
    mClientBuffer = nullptr;
}

EGLint FrameRenderer::InitEGL() {
    EGLNativeDisplayType display = EGL_DEFAULT_DISPLAY;
    EGLNativeWindowType surface = nullptr;

    EGLint eglConfig[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_SAMPLE_BUFFERS, mUseMultisample,
        EGL_SAMPLES, mMultisampleSize,
        EGL_NONE
    };

    EGLint contextConfig[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLint result = GLUtils::InitEGL(display, surface, eglConfig, contextConfig,
            1, &mContext, &mDisplay, &mSurface);
    CHECK_RESULT_AND_RETURN(result, EGL_SUCCESS);

    EGLint eglAttributes[] = {
        EGL_NONE,
        EGL_NONE,
        EGL_NONE
    };

    result = GLUtils::CreateEGLImage(mDisplay, mFrameBufferEglImageTarget,
            mClientBuffer, eglAttributes, &mFrameBufferEglImage);
    CHECK_RESULT_AND_RETURN(result, EGL_SUCCESS);

    result = GLUtils::GenFBOBuffers(1, &mFrameBufferID,
            1, &mFrameBufferTextureID);
    CHECK_RESULT_WITH_RETURN_VALUE(result, GL_NO_ERROR, EGL_BAD_PARAMETER);

    if (mUseMultisample) {
        result = GLUtils::GenRenderBuffers(1, &mDepthBufferID);
        CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);
    }

    result = GLUtils::BindFBOBufferIDWithEGLImage(
            mFrameBufferID, mFrameBufferTextureID, mFrameBufferEglImage,
            GL_TEXTURE_EXTERNAL_OES, mUseMultisample, mDepthBufferID, mDepthBufferFormat,
            mFrameBufferWidth, mFrameBufferHeight, mMultisampleSize);
    CHECK_RESULT_WITH_RETURN_VALUE(result, GL_NO_ERROR, EGL_BAD_PARAMETER);

    LOGD("frame buffer, id: %d, tex id: %d, depth buffer, id: %d",
            mFrameBufferID, mFrameBufferTextureID, mDepthBufferID);

    return EGL_SUCCESS;
}

EGLint FrameRenderer::DeinitEGL() {
    GLuint result = GLUtils::DeleteFBOBuffers(1, &mFrameBufferID,
            1, &mFrameBufferTextureID);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    if (mUseMultisample) {
        result = GLUtils::DeleteRenderBuffers(1, &mDepthBufferID);
        CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);
    }

    result = GLUtils::ReleaseEGLImage(mDisplay, &mFrameBufferEglImage);
    CHECK_RESULT_AND_RETURN(result, EGL_SUCCESS);

    result = GLUtils::DeInitEGL(&mContext, &mDisplay);
    CHECK_RESULT_AND_RETURN(result, EGL_SUCCESS);

    mSurface = EGL_NO_SURFACE;

    return EGL_SUCCESS;
}

int FrameRenderer::Begin() {
    LOGD("bind fbo: %d, texture id: %d", mFrameBufferID, mFrameBufferTextureID);
    GLenum result = GLUtils::BindFBOBufferID(mFrameBufferID,
            mFrameBufferTextureID, GL_TEXTURE_EXTERNAL_OES);
    CHECK_RESULT_WITH_RETURN_VALUE(result, GL_NO_ERROR, RESULT_BASE_MODULE_ERROR);

    // will clear color to (0, 0, 0, 0)
    // glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    // CHECK_GL_RESULT_BUT_RETURN_ERROR("glClear", RESULT_BASE_MODULE_ERROR);

    glClearColor(0.5, 0.5, 0.5, 1.0);
    CHECK_GL_RESULT_BUT_RETURN_ERROR("glClearColor", RESULT_BASE_MODULE_ERROR);

    return RESULT_OK;
}

int FrameRenderer::Finish() {
    GLenum result = GLUtils::WaitForFinish();
    CHECK_RESULT_WITH_RETURN_VALUE(result, GL_NO_ERROR, RESULT_BASE_MODULE_ERROR);


    static const GLenum attachments[3] = {
            GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, attachments);
    CHECK_GL_RESULT_BUT_RETURN_ERROR("glInvalidateFramebuffer", RESULT_BASE_MODULE_ERROR);

    result = GLUtils::UnbindFBOBufferID(GL_TEXTURE_EXTERNAL_OES);
    CHECK_RESULT_WITH_RETURN_VALUE(result, GL_NO_ERROR, RESULT_BASE_MODULE_ERROR);

    if (mDumpFrameBuffer) {
        sprintf(mFrameBufferPath, mFrameBufferPathFormat, mFrameBufferWidth, mFrameBufferHeight,
                mFrameBufferIndex);
        LOGD("dump: %s", mFrameBufferPath);

        GraphicBufferUtils::SwapWithFile(mFrameBuffer, mFrameBufferPath, 0);
        mFrameBufferIndex++;
    }

    return GL_NO_ERROR;
}
