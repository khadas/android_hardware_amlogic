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

#define LOG_TAG "BaseRenderer"

#include "include/BaseRenderer.h"


int BaseRenderer::Init() {
    LOCK(mLock);

    if (mInited == 0) {
        int result = InitDatas();
        if (result != RESULT_OK) {
            UNLOCK(mLock);
            return result;
        }

        result = InitEGL();
        if (result != EGL_SUCCESS) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }

        result = InitGL();
        if (result != GL_NO_ERROR) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }

        mInited = 1;
    }

    UNLOCK(mLock);

    return RESULT_OK;
}

int BaseRenderer::Deinit() {
    LOCK(mLock);

    if (mInited) {
        DeinitGL();
        DeinitEGL();
        DeinitDatas();

        mInited = 0;
    }

    UNLOCK(mLock);

    return RESULT_OK;
}

int BaseRenderer::SetParams(const char *type, ...) {
    if (type == nullptr) {
        LOGE("bad parameter!");
        return RESULT_BAD_PARAMETER;
    }

    if (!strcasecmp(type, "view_port")) {
        LOCK(mLock);

        va_list args;
        va_start(args, type);

        mViewLeft = va_arg(args, size_t);
        mViewTop = va_arg(args, size_t);
        mViewWidth = va_arg(args, size_t);
        mViewHeight = va_arg(args, size_t);

        va_end(args);

        UNLOCK(mLock);

        return RESULT_OK;
    }

    LOGW("unsupport the param: %s", type);
    return RESULT_OK;
}

GLenum BaseRenderer::SetViewPortInternal() {
    glViewport(mViewLeft, mViewTop, mViewWidth, mViewHeight);
    CHECK_GL_RESULT_AND_RETURN("glViewport");
    return GL_NO_ERROR;
}

int BaseRenderer::Render() {
    LOCK(mLock);

    if (mInited) {
	    int result = Prepare();
        if (result != GL_NO_ERROR) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }

        result = SetViewPortInternal();
        if (result != GL_NO_ERROR) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }

        result = Draw();
        if (result != GL_NO_ERROR) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }

        result = Complete();
        if (result != GL_NO_ERROR) {
            UNLOCK(mLock);
            return RESULT_BASE_MODULE_ERROR;
        }
    }

    UNLOCK(mLock);

    return GL_NO_ERROR;
}

int BaseRenderer::InitDatas() {
    return RESULT_OK;
}

int BaseRenderer::DeinitDatas() {
    return RESULT_OK;
}

EGLint BaseRenderer::InitEGL() {
    return EGL_SUCCESS;
}

EGLint BaseRenderer::DeinitEGL() {
    return EGL_SUCCESS;
}

GLenum BaseRenderer::InitGL() {
    return GL_NO_ERROR;
}

GLenum BaseRenderer::DeinitGL() {
    return GL_NO_ERROR;
}

int BaseRenderer::Begin() {
    return RESULT_OK;
}

GLenum BaseRenderer::Prepare() {
    return GL_NO_ERROR;
}

GLenum BaseRenderer::Draw() {
    return GL_NO_ERROR;
}

GLenum BaseRenderer::Complete() {
    return GL_NO_ERROR;
}

int BaseRenderer::Finish() {
    return RESULT_OK;
}
