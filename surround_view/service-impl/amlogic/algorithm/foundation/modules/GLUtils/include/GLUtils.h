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

#ifndef GLUTILS_H
#define GLUTILS_H

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES 1
#endif

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <ion/ion.h>
#include <drm/drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include <utils/utils.h>


//------------------------------- GLUtils -------------------------------//

class GLUtils {

public:
    static EGLint InitEGL(EGLNativeDisplayType displayID, NativeWindowType surface,
            const EGLint *configAttribs, EGLint *contexAttribs, int useFBO=0,
            EGLContext *eglContextOut=nullptr, EGLDisplay *eglDisplayOut=nullptr,
            EGLSurface *eglSurfaceOut=nullptr);
    static EGLint DeInitEGL(EGLDisplay *eglDisplay, EGLContext *eglContext);

    static int EGLImageFormatToGLBufferFormat(EGLint eglFormat, GLenum &glFormat);
    static int EGLImageFormatToPixelLen(EGLint format, size_t &len);
    static int GLBufferFormatToPixelLen(GLenum format, size_t &len);

    static EGLint CreateEGLImage(
            EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
            const EGLint *configAttribs, EGLImageKHR *eglImage);
    static EGLint CreateEGLImage(
            EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
            EGLint width, EGLint height, EGLint format, EGLint offset,
            EGLint pitch, int memFD, EGLImageKHR *eglImage);
    static EGLint ReleaseEGLImage(EGLDisplay eglDisplay, EGLImageKHR *eglImage);

    static GLenum GenTextures(GLsizei num, GLuint *textureIDs);
    static GLenum DeleteTextures(GLsizei num, GLuint *textureIDs);

    static GLenum BindTextureID(GLuint textureID);
    static GLenum BindAndActiveTexture(GLuint textureID, GLint texture);
    static GLenum UnbindTextureID();

    static GLenum BindTextureIdWithData(GLuint textureID,
            GLint internalformat, GLsizei width, GLsizei height,
            GLenum format, GLenum type, const void *data);

    static GLenum BindTextureWithEGLImage(GLuint textureID, EGLImageKHR eglImage);
    static GLenum CreateAndBindEGLImage(
            GLuint textureID, EGLDisplay eglDisplay, EGLenum target, EGLClientBuffer buffer,
            EGLint width, EGLint height, EGLint format, EGLint offset, EGLint pitch, int memFD,
            EGLImageKHR *eglImage);

    static GLenum GenBuffers(GLsizei num, GLuint *bufferIDs);
    static GLenum DeleteBuffers(GLsizei num, GLuint *bufferIDs);

    static GLenum BindBufferData(GLuint bufferID, const GLvoid *data,
            GLsizeiptr size, GLenum target=GL_ARRAY_BUFFER, GLenum usage=GL_STATIC_DRAW);

    static GLenum GenRenderBuffers(GLsizei renderBuffersNum, GLuint *renderBufferIDs);
    static GLenum DeleteRenderBuffers(GLsizei renderBuffersNum, GLuint *renderBufferIDs);

    static GLenum GenFBOBuffers(GLsizei frameBuffersNum, GLuint *frameBufferIDs,
            GLsizei frameBufferTextureNum, GLuint *frameBufferTextureIDs);
    static GLenum DeleteFBOBuffers(GLsizei frameBuffersNum, GLuint *frameBufferIDs,
            GLsizei frameBufferTextureNum, GLuint *frameBufferTextureIDs);

    static GLenum BindFBOBufferID(GLuint frameBufferID, GLuint frameBufferTextureID,
            GLenum textarget);
    static GLenum UnbindFBOBufferID(GLenum textarget);

    static GLenum ConfigureRenderBufferMultisample(GLuint renderBufferID,
            GLenum renderBufferFormat, GLsizei renderBufferWidth,
            GLsizei renderBufferHeight, GLsizei multisampleSize=4);

    static GLenum BindFBOBufferIDWithEGLImage(GLuint frameBufferID,
            GLuint frameBufferTextureID, GLeglImageOES eglImage, GLenum texTarget,
            int useMultisample=0, GLuint renderBufferID=GL_NONE,
            GLenum renderBufferFormat=GL_DEPTH_COMPONENT,  GLsizei renderBufferWidth=0,
            GLsizei renderBufferHeight=0, GLsizei multisampleSize=4);

    static GLenum LoadShader(GLuint *shader, GLenum type, const char *shaderSrc);

    static GLenum CreateProgram(GLuint *program, GLuint vertexShader,
            GLuint fragmentShader);
    static GLenum CreateProgram(GLuint *program, const char *vertexSrc,
            const char *fragmentSrc);
    static GLenum DeleteProgram(GLuint *program);

    static EGLint CreateFence(EGLDisplay eglDisplay, EGLSyncKHR *fence);
    static EGLint ReleaseFence(EGLDisplay eglDisplay, EGLSyncKHR *fence);
    static EGLint WaitForFence(EGLDisplay eglDisplay, EGLSyncKHR fence, int timeout);

    static GLenum Flush();
    static GLenum WaitForFinish();
};

//------------------------------- GLExtension -------------------------------//

class GLExtension {

public:
    static GLenum FramebufferTexture2DMultisample(
            GLenum target, GLenum attachment, GLenum textarget,
            GLuint texture, GLint level, GLsizei samples);
};

//------------------------------- macro -------------------------------//

#ifdef DEBUG
//---EGL
#define CHECK_EGL_RESULT_WITHOUT_RETURN(name) \
{ \
    EGLint result = eglGetError(); \
    if (result != EGL_SUCCESS) \
    { \
        LOGE("After %s, error: 0x%x", name, error); \
    } \
}

#define CHECK_EGL_RESULT_AND_RETURN_ERROR(name, result, error) \
{ \
    if (result != EGL_SUCCESS) \
    { \
        LOGE("After %s, error: 0x%x", name, error); \
        return result; \
    } \
}

#define CHECK_EGL_RESULT_AND_RETURN(name) \
{ \
    EGLint result = eglGetError(); \
    CHECK_EGL_RESULT_AND_RETURN_ERROR(name, result, result); \
}

#define CHECK_EGL_RESULT_BUT_RETURN_ERROR(name, error) \
{ \
    EGLint result = eglGetError(); \
    CHECK_EGL_RESULT_AND_RETURN_ERROR(name, result, error); \
}

//---GL
#define CHECK_GL_RESULT_WITHOUT_RETURN(name) \
{ \
    GLenum result = glGetError(); \
    if (result != GL_NO_ERROR) \
    { \
        LOGE("After %s, error: 0x%x", name, result); \
    } \
}

#define CHECK_GL_RESULT_AND_RETURN_ERROR(name, result, error) \
{ \
    if (result != GL_NO_ERROR) \
    { \
        LOGE("After %s, error: 0x%x", name, error); \
        return result; \
    } \
}

#define CHECK_GL_RESULT_AND_RETURN(name) \
{ \
    GLenum result = glGetError(); \
    CHECK_GL_RESULT_AND_RETURN_ERROR(name, result, result); \
}

#define CHECK_GL_RESULT_BUT_RETURN_ERROR(name, error) \
{ \
    GLenum result = glGetError(); \
    CHECK_GL_RESULT_AND_RETURN_ERROR(name, result, error); \
}

#else // DEBUG

#define CHECK_EGL_RESULT_WITHOUT_RETURN(name)
#define CHECK_EGL_RESULT_AND_RETURN_ERROR(name)
#define CHECK_EGL_RESULT_AND_RETURN(name)
#define CHECK_EGL_RESULT_BUT_RETURN_ERROR(name, error)

#define CHECK_GL_RESULT_WITHOUT_RETURN(name)
#define CHECK_GL_RESULT_AND_RETURN_ERROR(name)
#define CHECK_GL_RESULT_AND_RETURN(name)
#define CHECK_GL_RESULT_BUT_RETURN_ERROR(name, error)

#endif // DEBUG


#endif // GLUTILS_H
