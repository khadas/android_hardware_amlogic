/* Copyright Statement:
*
* This software/firmware and related documentation ("Amlogic Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to Amlogic Inc. and/or its licensors. Without
* the prior written permission of Amlogic inc. and/or its licensors, any
* reproduction, modification, AllocatdBy or disclosure of Amlogic Software, and
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

#define LOG_TAG "ImageReader"

#include <malloc.h>
#include <string.h>

#include "include/ImageReader.h"


ImageReader::ImageReader(MemType memType, const char **paths, size_t num) {
    InitDatas(memType);
    InitTextures(paths, num);
}

ImageReader::~ImageReader() {
    DeinitTextures();
    DeinitDatas();
}

int ImageReader::InitDatas(MemType memType) {
    mMemType = memType;
    mTextures = nullptr;
    mTexturesNum = 0;

    return RESULT_OK;
}

int ImageReader::DeinitDatas() {
    return RESULT_OK;
}

int ImageReader::ReadDateFromFile(
        void *data, const char *filePath, long offset, size_t size)
{
    if (data == nullptr || filePath == nullptr || offset < 0
            || size < 1)
    {
        LOGE("Bad parameter !");
        return RESULT_BAD_PARAMETER;
    }

    FILE *file = fopen(filePath, "rb");
    if (file == nullptr) {
        LOGE("open file: %s, error: %s", filePath, strerror(errno));
        return RESULT_BASE_MODULE_ERROR;
    }

    fseek(file, offset, SEEK_SET);
    fread(data, 1, size, file);

    fclose(file);
    file = nullptr;

    return RESULT_OK;
}

int ImageReader::ParseBmpInfo(const char *bmpPath, size_t &width,
        size_t &height, size_t &pixelLen, size_t &size)
{
    uint32_t data = 0;
    size_t dataLen = sizeof(uint32_t);

    // ReadDateFromFile(&data, bmpPath, 2, dataLen);
    // size = (size_t)(data - 54);

    ReadDateFromFile(&data, bmpPath, 18, dataLen);
    width = (size_t)data;

    ReadDateFromFile(&data, bmpPath, 22, dataLen);
    height = (size_t)data;

    ReadDateFromFile(&data, bmpPath, 28, dataLen);
    pixelLen = (data & 0x000000ff) >> 3;

    size = width * height * pixelLen;

    LOGD("%s, width: %zu, height: %zu, size: %zu, pixelLen: %zu",
            bmpPath, width, height, size, pixelLen);

    return RESULT_OK;
}

int ImageReader::ParseTexture(const char *path, Texture &texture) {
    if (path == nullptr) {
        LOGE("Bad parameter !");
        return RESULT_BAD_PARAMETER;
    }

    size_t len = strlen(path);
    if (len < 4) {
        LOGE("can not support this format: %s", path);
        return RESULT_UNSUPPORTED;
    }

    const char *format = path + len - 4;
    if (strncmp(format, ".bmp", 4) == 0) {
        size_t width = 0;
        size_t height = 0;
        size_t pixelLen = 0;
        size_t size = 0;

        int result = ParseBmpInfo(path, width, height,
                pixelLen, size);
        if (result != RESULT_OK) {
            return result;
        }

        if (pixelLen != 3 && pixelLen != 4) {
            LOGE("Unsupport format: %zu in %s", pixelLen, path);
            return RESULT_UNSUPPORTED;
        }

        texture.width = width;
        texture.height = height;

        if (pixelLen == 3) {
            texture.eglFormat = DRM_FORMAT_RGB888;
        } else {
            texture.eglFormat = DRM_FORMAT_ABGR8888;
        }

        result = GLUtils::EGLImageFormatToGLBufferFormat(
                texture.eglFormat, texture.glFormat);
        if (result != RESULT_OK) {
            return result;
        }

        texture.type = GL_UNSIGNED_BYTE;

        texture.offset = 54;
        texture.pitch = texture.width * pixelLen;

        texture.size = size;

        return RESULT_OK;
    }

    return RESULT_UNSUPPORTED;
}

int ImageReader::MallocTexture(Texture &texture) {
    texture.memFD = -1;
    texture.data = nullptr;
    texture.eglImageTarget = 0;

    if (mMemType == AllocatdByGraphicBuffer) {
        PixelFormat pixelFormat;
        int result = GraphicBufferUtils::GLFormatToPixelFormat(texture.glFormat, pixelFormat);
        if (result != RESULT_OK) {
            return result;
        }

        texture.buffer = new android::GraphicBuffer(texture.width, texture.height,
                pixelFormat, android::GraphicBuffer::USAGE_HW_TEXTURE, "Texture");
        if (texture.buffer == nullptr) {
            LOGE("new GraphicBuffer failed!");
            return RESULT_BASE_MODULE_ERROR;
        }

        texture.data = texture.buffer->getNativeBuffer();

        texture.eglImageTarget = EGL_NATIVE_BUFFER_ANDROID;

        return RESULT_OK;
    }

    if (mMemType == AllocatdByMalloc) {
        texture.data = malloc(texture.size);
        if (texture.data == nullptr) {
            LOGE("malloc failed! size: %d", texture.size);
            return RESULT_BASE_MODULE_ERROR;
        }

        return RESULT_OK;
    }

    LOGE("can not support this type!");
    return RESULT_UNSUPPORTED;
}

int ImageReader::FreeTexture(Texture &texture) {
    if (mMemType == AllocatdByGraphicBuffer) {
        texture.buffer = nullptr;
        texture.data = nullptr;
        return RESULT_OK;
    }

    if (mMemType == AllocatdByMalloc) {
        if (texture.data != nullptr) {
            free(texture.data);
            texture.data = nullptr;
        }
        return RESULT_OK;
    }

    LOGE("can not support this type!");
    return RESULT_UNSUPPORTED;
}

int ImageReader::LoadTexture(const char *path, Texture &texture) {
    if (mMemType == AllocatdByGraphicBuffer) {
        return GraphicBufferUtils::SwapWithFile(texture.buffer,
                path, 1);
    }

    if (mMemType == AllocatdByMalloc) {
        return ReadDateFromFile(
                texture.data,
                path,
                texture.offset,
                texture.size);
    }

    LOGE("can not support this type!");
    return RESULT_UNSUPPORTED;
}

int ImageReader::InitTexture(const char *path, Texture &texture) {
    texture.memFD = -1;
    texture.data = nullptr;

    int result = ParseTexture(path, texture);
    if (result != RESULT_OK) {
        return result;
    }

    result = MallocTexture(texture);
    if (result != RESULT_OK) {
        return result;
    }

    result = LoadTexture(path, texture);
    if (result != RESULT_OK) {
        return result;
    }

    texture.offset = 0;
    return RESULT_OK;
}

int ImageReader::InitTextures(const char **paths, size_t num) {
    if (paths == nullptr || num < 1) {
        LOGE("bad parameter!");
        return RESULT_BAD_PARAMETER;
    }

    mTextures = (Texture *)malloc(sizeof(Texture) * num);
    if (mTextures == nullptr) {
        LOGE("malloc failed!");
        return RESULT_NO_MEMORY;
    }

    for (size_t index = 0; index < num; index ++) {
        LOGD("load texture: %s", paths[index]);
        int result = InitTexture(paths[index], mTextures[index]);
        CHECK_RESULT_AND_RETURN(result, RESULT_OK);
    }

    mTexturesNum = num;

    return RESULT_OK;
}

int ImageReader::DeinitTextures() {
    if (mTextures != nullptr) {
        for (size_t index = 0; index < mTexturesNum; index ++) {
            FreeTexture(mTextures[index]);
        }

        free(mTextures);
        mTextures = nullptr;
    }

    return RESULT_OK;
}

size_t ImageReader::TexturesNum() {
    return mTexturesNum;
}

Texture *ImageReader::Textures() {
    return mTextures;
}

