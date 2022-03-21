/* Copyright Statement:
*
* This software/firmware and related documentation ("AutoChips Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to AutoChips Inc. and/or its licensors. Without
* the prior written permission of AutoChips inc. and/or its licensors, any
* reproduction, modification, use or disclosure of AutoChips Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* AutoChips Inc. (C) 2019. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("AUTOCHIPS SOFTWARE")
* RECEIVED FROM AUTOCHIPS AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. AUTOCHIPS EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES AUTOCHIPS PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE RENDERED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE AUTOCHIPS SOFTWARE, AND RECEIVER AGREES
* TO LORESULT_OK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN AUTOCHIPS
* SOFTWARE. AUTOCHIPS SHALL ALSO NOT BE RESPONSIBLE FOR ANY AUTOCHIPS SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND AUTOCHIPS'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE AUTOCHIPS SOFTWARE
* RELEASED HEREUNDER WILL BE, AT AUTOCHIPS'S OPTION, TO REVISE OR REPLACE THE
* AUTOCHIPS SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO AUTOCHIPS FOR SUCH AUTOCHIPS SOFTWARE AT ISSUE.
*/

#define LOG_TAG "GLUtils"

#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define DEBUG 1
#define CHECK_PARAMETERS 1

#include "include/GLUtils.h"

//------------------------------- GLUtils -------------------------------//

EGLint GLUtils::InitEGL(EGLNativeDisplayType display, NativeWindowType surface,
        const EGLint *configAttribs, EGLint *contexAttribs, int useFBO,
        EGLContext *eglContextOut, EGLDisplay *eglDisplayOut, EGLSurface *eglSurfaceOut)
{
    LOGD("display: %p, surface: %p", display, surface);

    if (configAttribs == nullptr || contexAttribs == nullptr) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }

    EGLDisplay eglDisplay = eglGetDisplay(display);
    if (eglDisplay == EGL_NO_DISPLAY) {
        CHECK_EGL_RESULT_AND_RETURN("eglGetDisplay");
        return EGL_BAD_PARAMETER;
    }

    EGLint majorVer = 0;
    EGLint minorVer = 0;
    EGLBoolean retsult = eglInitialize(eglDisplay, &majorVer, &minorVer);
    if (!retsult) {
        CHECK_EGL_RESULT_AND_RETURN("eglInitialize");
        return EGL_NOT_INITIALIZED;
    }

    EGLint maxConfigNum = 0;
    retsult = eglGetConfigs(eglDisplay, nullptr, 0, &maxConfigNum);
    if (!retsult || maxConfigNum <= 0) {
        CHECK_EGL_RESULT_AND_RETURN("eglGetConfigs");
        return EGL_BAD_CONFIG;
    }

    const EGLint configNum = maxConfigNum;
    EGLConfig configs[configNum];

    retsult = eglChooseConfig(eglDisplay, configAttribs, configs, configNum, &maxConfigNum);
    if (!retsult || maxConfigNum <= 0) {
        CHECK_EGL_RESULT_AND_RETURN("eglGetConfigs");
        return EGL_BAD_CONFIG;
    }

    EGLSurface eglSurface = EGL_NO_SURFACE;

    if (useFBO == 0) {
        eglSurface = eglCreateWindowSurface(eglDisplay, configs[0], surface, nullptr);
        if (eglSurface == EGL_NO_SURFACE) {
            CHECK_EGL_RESULT_AND_RETURN("eglGetConfigs");
            return EGL_BAD_CONFIG;
        }
    }

    EGLContext eglContext = eglCreateContext(eglDisplay, configs[0], EGL_NO_CONTEXT, contexAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        CHECK_EGL_RESULT_AND_RETURN("eglCreateContext");
        return EGL_BAD_CONTEXT;
    }

    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    CHECK_EGL_RESULT_AND_RETURN("eglMakeCurrent");

    if (eglContextOut != nullptr) {
        *eglContextOut = eglContext;
    }

    if (eglDisplayOut != nullptr) {
        *eglDisplayOut = eglDisplay;
    }

    if (eglSurfaceOut != nullptr) {
        *eglSurfaceOut = eglSurface;
    }

    LOGD("major: %d, minor: %d, eglDisplay: %p, eglSurface: %p, eglContext: %p",
            majorVer, minorVer, eglDisplay, eglSurface, eglContext);

    return EGL_SUCCESS;
}

EGLint GLUtils::DeInitEGL(EGLContext *eglContext, EGLDisplay *eglDisplay) {
    if (eglContext == nullptr || eglDisplay == nullptr) {
        LOGE("Bad Parameter !");
        return EGL_BAD_PARAMETER;
    }

    EGLBoolean eRetStatus = eglDestroyContext(*eglDisplay, *eglContext);
    if (eRetStatus != EGL_TRUE) {
        CHECK_EGL_RESULT_AND_RETURN("eglDestroyContext");
        return EGL_BAD_CONTEXT;
    }

    *eglDisplay = EGL_NO_DISPLAY;
    *eglContext = EGL_NO_CONTEXT;

    return EGL_SUCCESS;
}

int GLUtils::EGLImageFormatToGLBufferFormat(EGLint eglFormat, GLenum &glFormat) {
    switch (eglFormat) {
        case DRM_FORMAT_RGBA8888:
            glFormat = GL_RGBA;
            return RESULT_OK;

        case DRM_FORMAT_RGB888:
            glFormat = GL_RGB;
            return RESULT_OK;

        case DRM_FORMAT_RGB565:
            glFormat = GL_UNSIGNED_SHORT_5_6_5;
            return RESULT_OK;

        case DRM_FORMAT_RGBA5551:
            glFormat = GL_UNSIGNED_SHORT_5_5_5_1;
            return RESULT_OK;

        case DRM_FORMAT_RGBA4444:
            glFormat = GL_UNSIGNED_SHORT_4_4_4_4;
            return RESULT_OK;

        default:
            LOGE("Can not support format: %d", eglFormat);
            return RESULT_UNSUPPORTED;
    }
}

int GLUtils::EGLImageFormatToPixelLen(EGLint format, size_t &len) {
    switch (format) {
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_NV16:
        case DRM_FORMAT_NV61:
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YVU420:
            len = 1;
            return RESULT_OK;

        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_XRGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_XBGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_RGBX4444:
        case DRM_FORMAT_BGRA4444:
        case DRM_FORMAT_BGRX4444:
        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_XRGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_XBGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_RGBX5551:
        case DRM_FORMAT_BGRA5551:
        case DRM_FORMAT_BGRX5551:
        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
        case DRM_FORMAT_UYVY:
        case DRM_FORMAT_VYUY:
        case DRM_FORMAT_YUYV:
        case DRM_FORMAT_YVYU:
            len = 2;
            return RESULT_OK;

        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_RGB888:
            len = 3;
            return RESULT_OK;

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_BGRA8888:
        case DRM_FORMAT_BGRX8888:
        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_RGBX1010102:
        case DRM_FORMAT_BGRA1010102:
        case DRM_FORMAT_BGRX1010102:
            len = 4;
            return RESULT_OK;

        default:
            LOGE("Can not support format: %d", format);
            return RESULT_UNSUPPORTED;
    }
}

int GLUtils::GLBufferFormatToPixelLen(GLenum format, size_t &len) {
    switch (format) {
        case GL_RGBA:
            len = 4;
            return RESULT_OK;

        case GL_RGB:
            len = 3;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_5_6_5:
            len = 2;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_5_5_5_1:
            len = 2;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            len = 2;
            return RESULT_OK;

        default:
            LOGE("Can not support format: 0x%x", format);
            return RESULT_UNSUPPORTED;
    }
}

EGLint GLUtils::CreateEGLImage(
        EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
        const EGLint *eglImageAttribs, EGLImageKHR *eglImage)
{
#if CHECK_PARAMETERS
    if (eglDisplay == EGL_NO_DISPLAY
            || eglImageAttribs == nullptr
            || eglImage == nullptr
            || *eglImage != EGL_NO_IMAGE_KHR) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    *eglImage = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, target,
            buffer, eglImageAttribs);
    CHECK_EGL_RESULT_AND_RETURN("eglCreateImageKHR");

    LOGD("eglDisplay: %p, eglImage: %p", eglDisplay, *eglImage);
    return EGL_SUCCESS;
}

EGLint GLUtils::CreateEGLImage(
        EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
        EGLint width, EGLint height, EGLint format, EGLint offset,
        EGLint pitch, int memFD, EGLImageKHR *eglImage)
{
#if CHECK_PARAMETERS
    if (width < 1 || height < 1 || pitch < 1 || memFD < 0) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    const EGLint configAttribs [] = {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,

            EGL_LINUX_DRM_FOURCC_EXT, format,
            EGL_DMA_BUF_PLANE0_FD_EXT, memFD,

            EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, pitch,
            EGL_NONE
    };

    return CreateEGLImage(eglDisplay, target, buffer,
            configAttribs, eglImage);
}

EGLint GLUtils::ReleaseEGLImage(
        EGLDisplay eglDisplay, EGLImageKHR *eglImage)
{
#if CHECK_PARAMETERS
    if (eglDisplay == EGL_NO_DISPLAY || eglImage == nullptr) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    if (*eglImage != EGL_NO_IMAGE_KHR) {
        LOGD("display: %p, eglImage: %p", eglDisplay, *eglImage);

        eglDestroyImageKHR(eglDisplay, *eglImage);
        CHECK_EGL_RESULT_AND_RETURN("eglDestroyImageKHR");

        *eglImage = EGL_NO_IMAGE_KHR;
    }

    return EGL_SUCCESS;
}

GLenum GLUtils::GenTextures(GLsizei num, GLuint *textureIDs) {
    if (num <= 0 || textureIDs == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    glGenTextures(num, textureIDs);
    CHECK_GL_RESULT_AND_RETURN("glGenTextures");

#if DEBUG
    for (int index = 0; index < num; index++) {
        LOGD("index: %d, textureID: %d", index, textureIDs[index]);
    }
#endif

    return GL_NO_ERROR;
}

GLenum GLUtils::DeleteTextures(GLsizei num, GLuint *textureIDs) {
    if (num <= 0 || textureIDs == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    glDeleteTextures(num, textureIDs);
    CHECK_GL_RESULT_AND_RETURN("glDeleteTextures");

    for (int index = 0; index < num; index++) {
        textureIDs[index] = GL_NONE;
    }

    return GL_NO_ERROR;
}

GLenum GLUtils::BindTextureID(GLuint textureID) {
#if CHECK_PARAMETERS
    if (textureID == GL_NONE) {
        LOGE("bad parameter!\n");
        return GL_INVALID_VALUE;
    }
#endif

    glBindTexture(GL_TEXTURE_2D, textureID);
    CHECK_GL_RESULT_AND_RETURN("glBindTexture");

    return GL_NO_ERROR;
}

GLenum GLUtils::BindAndActiveTexture(GLuint textureID, GLint texture) {
#if CHECK_PARAMETERS
    if (textureID == GL_NONE || texture < 0) {
        LOGE("bad parameter!\n");
        return GL_INVALID_VALUE;
    }
#endif

    GLenum result = BindTextureID(textureID);
    CHECK_GL_RESULT_AND_RETURN_ERROR("BindTextureID", result, result);

    glActiveTexture(GL_TEXTURE0 + texture);
    CHECK_GL_RESULT_AND_RETURN("glActiveTexture");

    return GL_NO_ERROR;
}

GLenum GLUtils::UnbindTextureID() {
    glBindTexture(GL_TEXTURE_2D, GL_NONE);
    CHECK_GL_RESULT_AND_RETURN("glBindTexture");

    return GL_NO_ERROR;
}

GLenum GLUtils::BindTextureIdWithData(GLuint textureID,
        GLint internalformat, GLsizei width, GLsizei height,
        GLenum format, GLenum type, const void *data)
{
#if CHECK_PARAMETERS
    if (textureID <= 0 || width <= 0 || height <=0) {
        LOGE("bad parameter!\n");
        return GL_INVALID_VALUE;
    }
#endif

    GLenum result = BindTextureID(textureID);
    CHECK_GL_RESULT_AND_RETURN_ERROR("BindTextureID", result, result);

    glTexImage2D (
        GL_TEXTURE_2D,
        0,
        internalformat,
        width,
        height,
        0,
        format,
        type,
        data
    );

    result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("glTexImage2D, error: 0x%x", result);

        UnbindTextureID();
        return result;
    }

    // glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("glTexParameteri, error: 0x%x", result);

        UnbindTextureID();
        return result;
    }

    return UnbindTextureID();
}

GLenum GLUtils::BindTextureWithEGLImage(
        GLuint textureID, EGLImageKHR eglImage)
{
#if CHECK_PARAMETERS
    if (textureID <= GL_NONE || eglImage == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }
#endif

    GLenum result = BindTextureID(textureID);
    CHECK_GL_RESULT_AND_RETURN_ERROR("BindTextureID", result, result);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);

    result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("After glEGLImageTargetTexture2DOES, error: 0x%x", result);

        UnbindTextureID();
        return result;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("After glTexParameteri, error: 0x%x", result);

        UnbindTextureID();
        return result;
    }

    return UnbindTextureID();
}

GLenum GLUtils::CreateAndBindEGLImage(
        GLuint textureID, EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
        EGLint width, EGLint height, EGLint format, EGLint offset, EGLint pitch, int memFD,
        EGLImageKHR *eglImage)
{
    GLenum result = CreateEGLImage(eglDisplay, target, buffer, width, height, format, offset,
            pitch, memFD, eglImage);
    if (result != EGL_SUCCESS) {
        ReleaseEGLImage(eglDisplay, eglImage);
        return GL_INVALID_VALUE;
    }

    return BindTextureWithEGLImage(textureID, *eglImage);
}

GLenum GLUtils::GenBuffers(GLsizei num, GLuint *bufferIDs) {
    if (num <= 0 || bufferIDs == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    glGenBuffers(num, bufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glGenBuffers");

#if DEBUG
    for (int index = 0; index < num; index++) {
        LOGD("index: %d, bufferID: %d", index, bufferIDs[index]);
    }
#endif

    return GL_NO_ERROR;
}

GLenum GLUtils::DeleteBuffers(GLsizei num, GLuint *bufferIDs) {
    if (num <= 0 || bufferIDs == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    glDeleteBuffers(num, bufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glDeleteBuffers");

    return GL_NO_ERROR;
}

GLenum GLUtils::BindBufferData(GLuint bufferID, const GLvoid *data,
        GLsizeiptr size, GLenum target, GLenum usage)
{
#if CHECK_PARAMETERS
    if (bufferID == GL_NONE || size < 1) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }
#endif

    glBindBuffer(target, bufferID);
    CHECK_GL_RESULT_AND_RETURN("glBindBuffer");

    glBufferData(target, size, data, usage);

    GLenum result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("After glBufferData, error: 0x%x", result);

        glBindBuffer(target, GL_NONE);
        return result;
    }

    glBindBuffer(target, GL_NONE);
    CHECK_GL_RESULT_AND_RETURN("glBindBuffer");

    return GL_NO_ERROR;
}

GLenum GLUtils::GenRenderBuffers(GLsizei renderBuffersNum,
        GLuint *renderBufferIDs)
{
    if (renderBuffersNum < 1 || renderBufferIDs == nullptr) {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }

    for (int index = 0; index < renderBuffersNum; index++) {
        if (renderBufferIDs[index] != GL_NONE) {
            LOGE("index: %d, renderBufferID: %d", index, renderBufferIDs[index]);
            return GL_INVALID_VALUE;
        }
    }

    glGenRenderbuffers(renderBuffersNum, renderBufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glGenRenderbuffers");

#if DEBUG
    for (int index = 0; index < renderBuffersNum; index++) {
        if (renderBufferIDs[index] != GL_NONE) {
            LOGD("index: %d, renderBufferID: %d", index, renderBufferIDs[index]);
        }
    }
#endif

    return GL_NO_ERROR;
}

GLenum GLUtils::DeleteRenderBuffers(GLsizei renderBuffersNum, GLuint *renderBufferIDs) {
    if (renderBuffersNum < 1 || renderBufferIDs == nullptr) {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }

    glDeleteRenderbuffers(renderBuffersNum, renderBufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glDeleteRenderbuffers");

    return GL_NO_ERROR;
}

GLenum GLUtils::GenFBOBuffers(GLsizei frameBuffersNum, GLuint *frameBufferIDs,
        GLsizei frameBufferTextureNum, GLuint *frameBufferTextureIDs) {
    if (frameBuffersNum < 1 || frameBufferIDs == nullptr
            || frameBufferTextureNum < 1 || frameBufferTextureIDs == nullptr)
    {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }

    for (int index = 0; index < frameBuffersNum; index++) {
        if (frameBufferIDs[index] != GL_NONE) {
            LOGE("index: %d, frameBufferID: %d", index, frameBufferIDs[index]);
            return GL_INVALID_VALUE;
        }
    }

    for (int index = 0; index < frameBuffersNum; index++) {
        if (frameBufferTextureIDs[index] != GL_NONE) {
            LOGE("index: %d, frameBufferTextureID: %d", index, frameBufferTextureIDs[index]);
            return GL_INVALID_VALUE;
        }
    }

    glGenFramebuffers(frameBuffersNum, frameBufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glGenFramebuffers");

    glGenTextures(frameBufferTextureNum, frameBufferTextureIDs);
    CHECK_GL_RESULT_AND_RETURN("glGenTextures");

#if DEBUG
    for (int index = 0; index < frameBuffersNum; index++) {
        LOGD("index: %d, frameBufferIDs: %d", index, frameBufferIDs[index]);
    }

    for (int index = 0; index < frameBuffersNum; index++) {
        LOGD("index: %d, frameBufferTextureIDs: %d", index, frameBufferTextureIDs[index]);
    }
#endif

    return GL_NO_ERROR;
}

GLenum GLUtils::DeleteFBOBuffers(GLsizei frameBuffersNum, GLuint *frameBufferIDs,
        GLsizei frameBufferTextureNum, GLuint *frameBufferTextureIDs)
{
    if (frameBuffersNum < 1 || frameBufferIDs == nullptr
            || frameBufferTextureNum < 1 || frameBufferTextureIDs == nullptr)
    {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }

    glDeleteFramebuffers(frameBuffersNum, frameBufferIDs);
    CHECK_GL_RESULT_AND_RETURN("glDeleteFramebuffers");

    glDeleteTextures(frameBufferTextureNum, frameBufferTextureIDs);
    CHECK_GL_RESULT_AND_RETURN("glDeleteTextures");

    return GL_NO_ERROR;
}

GLenum GLUtils::BindFBOBufferID(GLuint frameBufferID, GLuint frameBufferTextureID,
        GLenum textarget)
{
#if CHECK_PARAMETERS
    if (frameBufferID == GL_NONE || frameBufferTextureID == GL_NONE) {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
    CHECK_GL_RESULT_AND_RETURN("glBindFramebuffer");

    glBindTexture(textarget, frameBufferTextureID);

    GLenum result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("glBindTexture, error: 0x%x", result);

        glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
        return result;
    }

    return GL_NO_ERROR;
}

GLenum GLUtils::UnbindFBOBufferID(GLenum textarget) {
    glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    glBindTexture(textarget, GL_NONE);

    CHECK_GL_RESULT_AND_RETURN("glBindTexture");
    return GL_NO_ERROR;
}

GLenum GLUtils::ConfigureRenderBufferMultisample(
        GLuint renderBufferID, GLenum renderBufferFormat,
        GLsizei renderBufferWidth, GLsizei renderBufferHeight,
        GLsizei multisampleSize)
{
#if CHECK_PARAMETERS
    if (renderBufferID == GL_NONE || renderBufferWidth < 1
            || renderBufferHeight < 1 || multisampleSize < 1)
    {
        LOGE("Bad Parameter !");
        return GL_INVALID_VALUE;
    }
#endif

    glBindRenderbuffer(GL_RENDERBUFFER, renderBufferID);
    CHECK_GL_RESULT_AND_RETURN("glBindRenderbuffer");

    glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER,
            renderBufferID);

    GLenum result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("glFramebufferRenderbuffer, error: 0x%x", result);

        glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
        return result;
    }

    glRenderbufferStorageMultisample(
            GL_RENDERBUFFER,
            multisampleSize,
            renderBufferFormat,
            renderBufferWidth,
            renderBufferHeight);

    result = glGetError();
    if (result != GL_NO_ERROR) {
        LOGE("glRenderbufferStorageMultisample, error: 0x%x", result);

        glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
        return result;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
    CHECK_GL_RESULT_AND_RETURN("glBindRenderbuffer");

    LOGD("renderBufferID: %d, multisampleSize: %d", renderBufferID,
            multisampleSize);

    return GL_NO_ERROR;
}

GLenum GLUtils::BindFBOBufferIDWithEGLImage(GLuint frameBufferID,
        GLuint frameBufferTextureID, GLeglImageOES eglImage, GLenum texTarget,
        int useMultisample, GLuint renderBufferID, GLenum renderBufferFormat,
        GLsizei renderBufferWidth, GLsizei renderBufferHeight, GLsizei multisampleSize)
{
#if CHECK_PARAMETERS
    if (frameBufferID == GL_NONE || frameBufferTextureID == GL_NONE
            || (eglImage != EGL_NO_IMAGE_KHR && texTarget != GL_TEXTURE_EXTERNAL_OES))
    {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    if (useMultisample) {
        if (renderBufferID == GL_NONE || renderBufferWidth < 1
                || renderBufferHeight < 1 || multisampleSize < 1) {
            LOGE("Bad parameter !");
            return GL_INVALID_VALUE;
        }
    }
#endif

    // glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    // CHECK_GL_RESULT_AND_RETURN("glPixelStorei");

    GLenum result = BindFBOBufferID(frameBufferID, frameBufferTextureID, texTarget);
    if (result != GL_NO_ERROR) {
        return result;
    }

    if (eglImage != EGL_NO_IMAGE_KHR) {
        glEGLImageTargetTexture2DOES(texTarget, eglImage);

        result = glGetError();
        if (result != GL_NO_ERROR) {
                LOGE("glEGLImageTargetTexture2DOES, error: 0x%x", result);

                UnbindFBOBufferID(texTarget);
                return result;
        }

        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // This is necessary to work with user-generated frame buffers with
        // dimensions that are NOT powers of 2.
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        result = glGetError();
        if (result != GL_NO_ERROR) {
            LOGE("glTexParameteri, error: 0x%x", result);

            UnbindFBOBufferID(texTarget);
            return result;
        }
    }

    // Attach frameBufferTextureID to frame buffer.
    glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            texTarget,
            frameBufferTextureID,
            0);

    result = glGetError();
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("glFramebufferTexture2D, error: 0x%x, status: 0x%x",
                result, status);

        UnbindFBOBufferID(texTarget);
        return result;
    }

    if (useMultisample) {
        result = GLExtension::FramebufferTexture2DMultisample(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                texTarget,
                frameBufferTextureID,
                0,
                multisampleSize);

        if (result != GL_NO_ERROR) {
            LOGE("glFramebufferTexture2DMultisampleIMG, error: 0x%x", result);

            UnbindFBOBufferID(texTarget);
            return result;
        }

        result = ConfigureRenderBufferMultisample(
                renderBufferID,
                renderBufferFormat,
                renderBufferWidth,
                renderBufferHeight,
                multisampleSize
        );

        if (result != GL_NO_ERROR) {
            UnbindFBOBufferID(texTarget);
            return result;
        }
    }

    UnbindFBOBufferID(texTarget);

    LOGD("frameBufferID: %d, frameBufferTextureID: %d, eglImage: %p",
            frameBufferID, frameBufferTextureID, eglImage);

    return GL_NO_ERROR;
}

GLenum GLUtils::LoadShader(GLuint *shader, GLenum type, const char *shaderSrc) {
    if (shader == nullptr || shaderSrc == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    *shader = glCreateShader(type);
    if (*shader == GL_NONE) {
        CHECK_GL_RESULT_AND_RETURN("glCreateShader");
        return GL_INVALID_VALUE;
    }

    glShaderSource(*shader, 1, &shaderSrc, nullptr);
    CHECK_GL_RESULT_AND_RETURN("glShaderSource");

    GLint compiled;
    glCompileShader(*shader);
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen >= 1) {
            char* infoLog = (char *)malloc(sizeof(char) * infoLen);

            if (infoLog != nullptr) {
                glGetShaderInfoLog(*shader, infoLen, nullptr, infoLog);
                LOGE("Error compiling shader:\n%s", infoLog);
                free(infoLog);
            }
        }

        glDeleteShader(*shader);
        return GL_INVALID_VALUE;
    }

    LOGD("type: %d, shader: %d", type, *shader);
    return GL_NO_ERROR;
}

GLenum GLUtils::CreateProgram(GLuint *program, GLuint vertexShader,
        GLuint fragmentShader)
{
    if (program == nullptr || vertexShader == GL_NONE
            || fragmentShader == GL_NONE)
    {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    if (*program != GL_NONE) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    *program = glCreateProgram();
    if (*program == GL_NONE) {
        LOGE("After glCreateProgram, error: 0x%x", glGetError());

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return GL_INVALID_VALUE;
    }

    glAttachShader(*program, vertexShader);
    glAttachShader(*program, fragmentShader);
    CHECK_GL_RESULT_AND_RETURN("glAttachShader");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    CHECK_GL_RESULT_AND_RETURN("glDeleteShader");

    GLint linked = GL_NONE;
    glLinkProgram(*program);
    glGetProgramiv(*program, GL_LINK_STATUS, &linked);

    if (linked == GL_NONE) {
        GLint infoLen = 0;
        glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen >= 1) {
            char *infoLog = (char *)malloc(sizeof ( char ) * infoLen);

            if (infoLog != nullptr) {
                glGetProgramInfoLog(*program, infoLen, nullptr, infoLog);
                LOGE("Error linking program:\n%s", infoLog);
                free(infoLog);
            }
        }

        DeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return GL_INVALID_VALUE;
    }

    LOGD("program: %d, vertexShader: %d, fragmentShader: %d",
            *program, vertexShader, fragmentShader);
    return GL_NO_ERROR;
}

GLenum GLUtils::CreateProgram(GLuint *program, const char *vertexSrc,
        const char *fragmentSrc)
{
    if (program == nullptr || vertexSrc == nullptr || fragmentSrc == nullptr) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    if (*program != GL_NONE) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }

    GLuint vertexShader = GL_NONE;
    GLenum result = LoadShader(&vertexShader, GL_VERTEX_SHADER, vertexSrc);
    if (result != GL_NO_ERROR) {
        return result;
    }

    GLuint fragmentShader = GL_NONE;
    result = LoadShader(&fragmentShader, GL_FRAGMENT_SHADER, fragmentSrc);
    if (result != GL_NO_ERROR) {
        glDeleteShader(vertexShader);
        return result;
    }

    return CreateProgram(program, vertexShader, fragmentShader);
}

GLenum GLUtils::DeleteProgram(GLuint *program) {
    if (program != nullptr) {
        if (*program != GL_NONE) {
            glDeleteProgram(*program);
            CHECK_GL_RESULT_AND_RETURN("glDeleteProgram");
            *program = GL_NONE;
        }
    }

    return GL_NO_ERROR;
}

EGLint GLUtils::CreateFence(EGLDisplay eglDisplay, EGLSyncKHR *fence) {
#if CHECK_PARAMETERS
    if (eglDisplay == EGL_NO_DISPLAY || fence == nullptr || *fence != EGL_NO_SYNC_KHR) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    *fence = eglCreateSyncKHR(eglDisplay, EGL_SYNC_FENCE_KHR, nullptr);
    CHECK_EGL_RESULT_AND_RETURN("eglCreateFenceKHR");

    return EGL_SUCCESS;
}

EGLint GLUtils::ReleaseFence(EGLDisplay eglDisplay, EGLSyncKHR *fence) {
#if CHECK_PARAMETERS
    if (eglDisplay == EGL_NO_DISPLAY || fence == nullptr) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    if (*fence != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(eglDisplay, *fence);
        CHECK_EGL_RESULT_AND_RETURN("eglCreateFenceKHR");

        *fence = EGL_NO_SYNC_KHR;
    }

    return EGL_SUCCESS;
}

EGLint GLUtils::WaitForFence(EGLDisplay eglDisplay, EGLSyncKHR fence, int timeout) {
#if CHECK_PARAMETERS
    if (eglDisplay == EGL_NO_DISPLAY) {
        LOGE("Bad parameter !");
        return EGL_BAD_PARAMETER;
    }
#endif

    if (fence == EGL_NO_SYNC_KHR) {
        LOGW("Warning: no fence to wait !");
        return EGL_SUCCESS;
    }

    if (timeout < 0) {
        timeout = __INT_MAX__;
    }

    // eglWaitFenceKHR(eglDisplay, fence, 0);
    EGLint waitStatus = eglClientWaitSyncKHR(eglDisplay, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
            timeout);
    if (waitStatus != EGL_CONDITION_SATISFIED_KHR) {
        CHECK_EGL_RESULT_AND_RETURN("eglClientWaitSyncKHR");
        return GL_INVALID_OPERATION;
    }

    return EGL_SUCCESS;
}

GLenum GLUtils::Flush() {
    glFlush();
    CHECK_GL_RESULT_AND_RETURN("glFlush");

    return GL_NO_ERROR;
}

GLenum GLUtils::WaitForFinish() {
    glFinish();
    CHECK_GL_RESULT_AND_RETURN("glFinish");

    return GL_NO_ERROR;
}

//------------------------------- GLExtension -------------------------------//

typedef void (GL_APIENTRY *GLFramebufferTexture2DMultisample)(
        GLenum target, GLenum attachment, GLenum textarget,
        GLuint texture, GLint level, GLsizei samples);


GLenum GLExtension::FramebufferTexture2DMultisample(
        GLenum target, GLenum attachment, GLenum textarget,
        GLuint texture, GLint level, GLsizei samples) {
#if CHECK_PARAMETERS
    if (texture == GL_NONE) {
        LOGE("Bad parameter !");
        return GL_INVALID_VALUE;
    }
#endif

    static GLFramebufferTexture2DMultisample
            glFramebufferTexture2DMultisample = reinterpret_cast<GLFramebufferTexture2DMultisample> (
            eglGetProcAddress("glFramebufferTexture2DMultisampleEXT") ?
            eglGetProcAddress("glFramebufferTexture2DMultisampleEXT") :
            eglGetProcAddress("glFramebufferTexture2DMultisampleIMG"));

    if (glFramebufferTexture2DMultisample == nullptr) {
        LOGE("RESULT_UNSUPPORTED !");
        return GL_INVALID_OPERATION;
    }

    glFramebufferTexture2DMultisample(target, attachment, textarget, texture, level, samples);
    CHECK_GL_RESULT_AND_RETURN("glFramebufferTexture2DMultisample");

    return GL_NO_ERROR;
}
