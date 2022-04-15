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

#define LOG_TAG "TexturePainter"

#include "include/TexturePainter.h"


TexturePainter::TexturePainter()
    : BasePainter() {
}

int TexturePainter::InitDatas() {
    mProgram = GL_NONE;
    mPositionID = GL_NONE;
    mTexCoordID = GL_NONE;

    mPositionLoc = -1;
    mTexCoordLoc = -1;
    mMVPLoc = -1;
    mTextureLoc = -1;

    static const char vertexShader[] =
            "#version 300 es                                    \n"
            "in vec4 vPosition;                                 \n"
            "in vec2 vTexCoor;                                  \n"
            "uniform mat4 mvp;                                  \n"
            "out vec2 v_texCoor;                                \n"
            "void main()                                        \n"
            "{                                                  \n"
            "   gl_Position = mvp * vPosition;                  \n"
            "   v_texCoor = vTexCoor;                           \n"
            "}                                                  \n";

    static const char fragmentShader[] =
            "#version 300 es                                    \n"
            "precision mediump float;                           \n"
            "uniform sampler2D sTexture;                        \n"
            "in vec2 v_texCoor;                                 \n"
            "out vec4 fragColor;                                \n"
            "void main()                                        \n"
            "{                                                  \n"
            "   fragColor = texture ( sTexture, v_texCoor );    \n"
            "}                                                  \n";

    mVertexShader = vertexShader;
    mFragmentShader = fragmentShader;

    static GLfloat vertices[] = {
       -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,
       -1.0f,  1.0f, 0.0f
    };
    mVertices = vertices;

    mVertexMode = GL_TRIANGLE_FAN;
    mVertexLen = 3;
    mVertexNum = 4;
    mVertexType = GL_FLOAT;
    mVertexNormalized = GL_TRUE;
    mVertexSize = mVertexLen * mVertexNum * sizeof(GLfloat);

    static GLfloat texCoords[] = {
        0.0f,   0.0f,
        1.0f,   0.0f,
        1.0f,   1.0f,
        0.0f,   1.0f
    };
    mTexCoords = texCoords;

    mTexCoordLen = 2;
    mTexCoordNum = 4;
    mTexCoordType = GL_FLOAT;
    mTexCoordNormalized = GL_FALSE;
    mTexCoordSize = mTexCoordLen * mTexCoordNum * sizeof(GLfloat);

    //---Texture
    mTexturesNum = 0;
    mTextures = nullptr;

    mTextureEGLImages = nullptr;
    mTextureIDs = nullptr;

    mTextureObsoleted = 0;
    //---Texture

    //---MVP
    mMVPSize = sizeof(GLfloat) * MVPLEN;
    mMVP = nullptr;
    //---MVP

    return RESULT_OK;
}

int TexturePainter::DeinitDatas() {
    return RESULT_OK;
}

GLenum TexturePainter::InitGL() {
    GLenum result = GLUtils::GenBuffers(1, &mPositionID);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::GenBuffers(1, &mTexCoordID);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::BindBufferData(mPositionID, mVertices, mVertexSize,
            GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::BindBufferData(mTexCoordID, mTexCoords, mTexCoordSize,
            GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::CreateProgram(&mProgram, mVertexShader, mFragmentShader);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    mPositionLoc = glGetAttribLocation(mProgram, "vPosition");
    mTexCoordLoc = glGetAttribLocation(mProgram, "vTexCoor");
    CHECK_GL_RESULT_AND_RETURN("glGetAttribLocation");

    mMVPLoc = glGetUniformLocation(mProgram, "mvp");
    CHECK_GL_RESULT_AND_RETURN("glGetUniformLocation");

    mTextureLoc = glGetUniformLocation(mProgram, "sTexture");
    CHECK_GL_RESULT_AND_RETURN("glGetUniformLocation");

    LOGD("vbo, PositionID: %d, TexCoordID: %d", mPositionID, mTexCoordID);
    LOGD("program: %d, position location: %d, texture id: %d, "
            "texture location: %d, mvp location: %d",
            mProgram, mPositionLoc, mTexCoordLoc, mTextureLoc, mMVPLoc);

    return GL_NO_ERROR;
}

GLenum TexturePainter::DeinitGL() {
    GLenum result = GLUtils::DeleteProgram(&mProgram);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::DeleteBuffers(1, &mPositionID);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = GLUtils::DeleteBuffers(1, &mTexCoordID);
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    result = DeinitTexture();
    CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

    return GL_NO_ERROR;
}

int TexturePainter::InitTextureData(size_t num) {
    if (mTextures == nullptr) {
        mTextures = (Texture *)malloc(sizeof(Texture) * num);
        if (mTextures == nullptr) {
            LOGE("malloc failed!");
            return RESULT_NO_MEMORY;
        }

        for (size_t index = 0; index < num; index++) {
            mTextures[index].replaced = 0;
            mTextures[index].memFD = -1;
            mTextures[index].data = nullptr;
            mTextures[index].buffer = nullptr;
        }
    }

    if (mMVP == nullptr) {
        mMVP = (GLfloat **)malloc(sizeof(GLfloat *) * num);
        if (mMVP == nullptr) {
            LOGE("malloc failed!");
            return RESULT_NO_MEMORY;
        }

        for (size_t index = 0; index < num; index++) {
            mMVP[index] = (GLfloat *)malloc(mMVPSize);
            if (mMVP[index] == nullptr) {
                LOGE("malloc failed!");
                return RESULT_NO_MEMORY;
            }
        }
    }

    return RESULT_OK;
}

int TexturePainter::DeinitTextureData(size_t num) {
    if (mTextures != nullptr) {
        for (size_t index = 0; index < num; index++) {
            mTextures[index].memFD = -1;
            mTextures[index].data = nullptr;
            mTextures[index].buffer = nullptr;
        }

        free(mTextures);
        mTextures = nullptr;
    }

    if (mMVP != nullptr) {
        for (size_t index = 0; index < num; index++) {
            if (mMVP[index] != nullptr) {
                free(mMVP[index]);
                mMVP[index] = nullptr;
            }
        }

        free(mMVP);
        mMVP = nullptr;
    }

    return RESULT_OK;
}

const char* TexturePainter::TextureName() {
    return "textures";
}

int TexturePainter::SetParams(const char *type, ...) {
    if (type == nullptr) {
        LOGE("bad parameter!");
        return RESULT_BAD_PARAMETER;
    }

    const char* textureName = TextureName();

    if (textureName != nullptr) {
        if (!strcasecmp(type, textureName)) {
            va_list args;
            va_start(args, type);

            TextureInfo *textureInfos = (TextureInfo *)va_arg(args, void *);
            size_t num = va_arg(args, size_t);
            size_t index = va_arg(args, size_t);

            va_end(args);

            if (textureInfos == nullptr || num < 1) {
                LOGE("bad parameter!");
                return RESULT_BAD_PARAMETER;
            }

            if (mTexturesNum < num) {
                LOGD("reshape textue from %zu to %zu", mTexturesNum, num);
                int result = DeinitTextureData(num);

                if (result != RESULT_OK) {
                    return result;
                }

                mTextureObsoleted = 1;
                mTexturesNum = 0;
            }

            int result = InitTextureData(num);
            if (result != RESULT_OK) {
                DeinitTextureData(num);
                return result;
            }

            mTexturesNum = num;

            if (index < num) {
                num = index + 1;
            } else {
                index = 0;
            }

            void *oldBuffer;
            void *newBuffer;
            Texture *oldTexture;
            Texture *newTexture;
            TextureInfo textureInfo;

            while (index < num) {
                textureInfo = textureInfos[index];
                oldTexture = &(mTextures[index]);
                newTexture = &(textureInfo.texture);
                oldBuffer = oldTexture->buffer ? oldTexture->buffer.get() : nullptr;
                newBuffer = newTexture->buffer ? newTexture->buffer.get() : nullptr;

                if (oldTexture->replaced) {
                    if (oldBuffer) {
                        LOGW("%p will be discard!", oldBuffer);
                    }
                }

                if (mTextureObsoleted || newTexture->replaced) {
                    LOGD("%p was replaced with %p", oldBuffer, newBuffer);
                    oldTexture->buffer = nullptr;
                    memcpy(oldTexture, newTexture, sizeof(Texture));
                    oldTexture->replaced = 1;
                }

                if (textureInfo.mvpUpdated) {
                    memcpy(mMVP[index], textureInfo.mvp, mMVPSize);
                }

                index++;
            }

            return RESULT_OK;
        }
    }

    va_list args;
    va_start(args, type);
    int result = BasePainter::SetParams(type, args);
    va_end(args);

    return result;
}

GLenum TexturePainter::InitTexture() {
    if (mTextureObsoleted) {
        LOGD("deinit old textures!");

        GLenum result = DeinitTexture();
        CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

        mTextureObsoleted = 0;
    }

    if (mTextureIDs == nullptr) {
        mTextureIDs = (GLuint *)malloc(sizeof(GLuint) * mTexturesNum);
        if (mTextureIDs == nullptr)  {
            LOGE("malloc failed!");
            return GL_INVALID_OPERATION;
        }

        GLenum result = GLUtils::GenTextures(mTexturesNum, mTextureIDs);
        CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);
    }

    if (mTextureEGLImages == nullptr) {
        mTextureEGLImages = (EGLImageKHR *)malloc(sizeof(EGLImageKHR) * mTexturesNum);
        if (mTextureEGLImages == nullptr)  {
            LOGE("malloc failed!");
            return GL_INVALID_OPERATION;
        }

        for (size_t index = 0; index < mTexturesNum; index++) {
            mTextureEGLImages[index] = EGL_NO_IMAGE_KHR;
        }
    }

    static EGLint attributes[] = {
        EGL_NONE,
        EGL_NONE,
        EGL_NONE,
    };

    EGLDisplay display = eglGetCurrentDisplay();

    for (size_t index = 0; index < mTexturesNum; index++) {
        if (mTextures[index].replaced) {
            LOGD("bind texture %zu: %p", index,
                    mTextures[index].buffer?
                    mTextures[index].buffer.get():nullptr);

            EGLint result = GLUtils::ReleaseEGLImage(display, &mTextureEGLImages[index]);
            CHECK_RESULT_WITH_RETURN_VALUE(result, EGL_SUCCESS, GL_INVALID_VALUE);

            result = GLUtils::CreateEGLImage(display, mTextures[index].eglImageTarget,
                    mTextures[index].data, attributes, &mTextureEGLImages[index]);
            CHECK_RESULT_WITH_RETURN_VALUE(result, EGL_SUCCESS, GL_INVALID_VALUE);

            result = GLUtils::BindTextureWithEGLImage(mTextureIDs[index],
                    mTextureEGLImages[index]);
            CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

            mTextures[index].replaced = 0;
        }
    }

    return GL_NO_ERROR;
}

GLenum TexturePainter::DeinitTexture() {
    if (mTextureIDs) {
        GLenum result = GLUtils::DeleteTextures(mTexturesNum, mTextureIDs);
        CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

        free(mTextureIDs);
        mTextureIDs = nullptr;
    }

    if (mTextureEGLImages) {
        EGLDisplay display = eglGetCurrentDisplay();

        for (size_t index = 0; index < mTexturesNum; index++) {
            EGLint result = GLUtils::ReleaseEGLImage(display, &mTextureEGLImages[index]);
            CHECK_RESULT_WITH_RETURN_VALUE(result, EGL_SUCCESS, GL_INVALID_VALUE);
        }

        free(mTextureEGLImages);
        mTextureEGLImages = nullptr;
    }

    return GL_NO_ERROR;
}

GLenum TexturePainter::Prepare() {
    return InitTexture();
}

GLenum TexturePainter::Complete() {
    return GL_NO_ERROR;
}

GLenum TexturePainter::UpdateMVP(size_t index) {
    if (mMVP == nullptr || index >= mTexturesNum) {
        LOGE("bad parameter!");
        return GL_INVALID_VALUE;
    }

    glUniformMatrix4fv(mMVPLoc, 1, GL_FALSE, mMVP[index]);
    CHECK_GL_RESULT_AND_RETURN("glUniformMatrix4fv");

    return GL_NO_ERROR;
}

GLenum TexturePainter::DrawOneTexture() {
    glEnableVertexAttribArray(mPositionLoc);
    CHECK_GL_RESULT_AND_RETURN("glEnableVertexAttribArray");

    glEnableVertexAttribArray(mTexCoordLoc);
    CHECK_GL_RESULT_AND_RETURN("glEnableVertexAttribArray");

    glBindBuffer(GL_ARRAY_BUFFER, mPositionID);
    CHECK_GL_RESULT_AND_RETURN("glBindBuffer");

    glVertexAttribPointer(mPositionLoc, mVertexLen, mVertexType,
            mVertexNormalized, 0, nullptr);
    CHECK_GL_RESULT_AND_RETURN("glVertexAttribPointer");

    glBindBuffer(GL_ARRAY_BUFFER, mTexCoordID);
    CHECK_GL_RESULT_AND_RETURN("glBindBuffer");

    glVertexAttribPointer(mTexCoordLoc, mTexCoordLen, mTexCoordType,
        mTexCoordNormalized, 0, nullptr);
    CHECK_GL_RESULT_AND_RETURN("glVertexAttribPointer");

    LOGD("glDrawArrays");
    glDrawArrays(mVertexMode, 0, mVertexNum);
    CHECK_GL_RESULT_AND_RETURN("glDrawArrays");

    glDisableVertexAttribArray(mPositionLoc);
    CHECK_GL_RESULT_AND_RETURN("glDisableVertexAttribArray");

    glDisableVertexAttribArray(mTexCoordLoc);
    CHECK_GL_RESULT_AND_RETURN("glDisableVertexAttribArray");

    return GL_NO_ERROR;
}

GLenum TexturePainter::Draw() {
    LOGD("---use program: %d", mProgram);
    glUseProgram(mProgram);
    CHECK_GL_RESULT_AND_RETURN("glUseProgram");

    glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
    CHECK_GL_RESULT_AND_RETURN("glBlendFunc");

    glEnable(GL_BLEND);
    CHECK_GL_RESULT_AND_RETURN("glEnable");

    if (mTextureIDs != nullptr) {
        GLenum result = GL_NO_ERROR;

        for (size_t index = 0; index < mTexturesNum; index++) {
            result = UpdateMVP(index);
            CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

            result = GLUtils::BindAndActiveTexture(mTextureIDs[index],
                    mTextureLoc);
            CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

            result = DrawOneTexture();
            CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);

            result = GLUtils::UnbindTextureID();
            CHECK_RESULT_AND_RETURN(result, GL_NO_ERROR);
        }
    }

    glDisable(GL_BLEND);
    CHECK_GL_RESULT_AND_RETURN("glDisable");

    LOGD("---use program: %d", GL_NONE);
    glUseProgram(GL_NONE);
    CHECK_GL_RESULT_AND_RETURN("glUseProgram");

    return GL_NO_ERROR;
}
