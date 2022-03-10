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

#define LOG_TAG "GraphicBufferUtils"

#include "include/GraphicBufferUtils.h"


int GraphicBufferUtils::PixelFormatToEGLFormat(PixelFormat pixelFormat, EGLint &eglFormat) {
    switch (pixelFormat) {
        case PIXEL_FORMAT_RGBA_8888:
        case PIXEL_FORMAT_RGBX_8888:
            eglFormat = DRM_FORMAT_RGBA8888;
            return RESULT_OK;

        case PIXEL_FORMAT_RGB_888:
            eglFormat = DRM_FORMAT_RGB888;
            return RESULT_OK;

        case PIXEL_FORMAT_RGB_565:
            eglFormat = DRM_FORMAT_RGB565;
            return RESULT_OK;

        case PIXEL_FORMAT_RGBA_5551:
            eglFormat = DRM_FORMAT_RGBA5551;
            return RESULT_OK;

        case PIXEL_FORMAT_RGBA_4444:
            eglFormat = DRM_FORMAT_RGBA4444;
            return RESULT_OK;

        default:
            LOGE("format %d can not support!", pixelFormat);
            return RESULT_BAD_PARAMETER;
    }
}

int GraphicBufferUtils::EGLFormatToPixelFormat(EGLint eglFormat, PixelFormat &pixelFormat) {
    switch (eglFormat) {
        case DRM_FORMAT_RGBA8888:
            pixelFormat = PIXEL_FORMAT_RGBA_8888;
            return RESULT_OK;

        case DRM_FORMAT_RGB888:
            pixelFormat = PIXEL_FORMAT_RGB_888;
            return RESULT_OK;

        case DRM_FORMAT_RGB565:
            pixelFormat = PIXEL_FORMAT_RGB_565;
            return RESULT_OK;

        case DRM_FORMAT_RGBA5551:
            pixelFormat = PIXEL_FORMAT_RGBA_5551;
            return RESULT_OK;

        case DRM_FORMAT_RGBA4444:
            pixelFormat = PIXEL_FORMAT_RGBA_4444;
            return RESULT_OK;

        default:
            LOGE("format %d can not support!", pixelFormat);
            return RESULT_BAD_PARAMETER;
    }

}

int GraphicBufferUtils::PixelFormatToGLFormat(PixelFormat pixelFormat, GLenum &glFormat) {
    switch (pixelFormat) {
        case PIXEL_FORMAT_RGBA_8888:
            glFormat = GL_RGBA;
            return RESULT_OK;

        case PIXEL_FORMAT_RGBX_8888:
            glFormat = GL_RGBA;
            return RESULT_OK;

        case PIXEL_FORMAT_RGB_888:
            glFormat = GL_RGB;
            return RESULT_OK;

        case PIXEL_FORMAT_RGB_565:
            glFormat = GL_UNSIGNED_SHORT_5_6_5;
            return RESULT_OK;

        case PIXEL_FORMAT_RGBA_5551:
            glFormat = GL_UNSIGNED_SHORT_5_5_5_1;
            return RESULT_OK;

        case PIXEL_FORMAT_RGBA_4444:
            glFormat = GL_UNSIGNED_SHORT_4_4_4_4;
            return RESULT_OK;

        default:
            LOGE("format %d can not support!", pixelFormat);
            return RESULT_BAD_PARAMETER;
    }
}

int GraphicBufferUtils::GLFormatToPixelFormat(GLenum glFormat, PixelFormat &pixelFormat) {
    switch (glFormat) {
        case GL_RGBA:
            pixelFormat = PIXEL_FORMAT_RGBA_8888;
            return RESULT_OK;

        case GL_RGB:
            pixelFormat = PIXEL_FORMAT_RGB_888;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_5_6_5:
            pixelFormat = PIXEL_FORMAT_RGB_565;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_5_5_5_1:
            pixelFormat = PIXEL_FORMAT_RGBA_5551;
            return RESULT_OK;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            pixelFormat = PIXEL_FORMAT_RGBA_4444;
            return RESULT_OK;

        default:
            LOGE("format %d can not support!", pixelFormat);
            return RESULT_BAD_PARAMETER;
    }
}

int GraphicBufferUtils::GraphicBufferToTexture(sp<GraphicBuffer> buffer, Texture &texture) {
    texture.eglFormat = 0;
    texture.glFormat = 0;

    texture.replaced = (texture.buffer != buffer);
    texture.memType = AllocatdByGraphicBuffer;
    texture.memFD = -1;

    if (buffer == nullptr) {
        texture.width = 0;
        texture.height = 0;
        texture.type = 0;
        texture.offset = 0;
        texture.pitch = 0;
        texture.size = 0;

        texture.eglImageTarget = 0;

        texture.data = nullptr;
        texture.buffer = nullptr;
    } else {
        texture.width = buffer->getWidth();
        texture.height = buffer->getHeight();
        texture.type = GL_UNSIGNED_BYTE;
        texture.offset = 0;
        texture.pitch = 0;
        texture.size = texture.height * buffer->getStride();

        texture.eglImageTarget = EGL_NATIVE_BUFFER_ANDROID;
        texture.eglFormat = 0;
        texture.glFormat = 0;

        texture.data = buffer->getNativeBuffer();
        texture.buffer = buffer;

        int result = GraphicBufferUtils::PixelFormatToEGLFormat(buffer->getPixelFormat(),
                texture.eglFormat);
        if (result != RESULT_OK) {
            return result;
        }

        result = GraphicBufferUtils::PixelFormatToGLFormat(buffer->getPixelFormat(),
                texture.glFormat);
        if (result != RESULT_OK) {
            return result;
        }
    }

    return RESULT_OK;
}

int GraphicBufferUtils::Align(int n, int divisor) {
    if (divisor != 0) {
        int remain = n % divisor;
        if (remain != 0) {
            n = n - remain + divisor;
        }
    }
    return n;
}

int GraphicBufferUtils::SwapWithFile(sp<GraphicBuffer> buffer,
        const char *path, int fromFile) {
    if (buffer == nullptr || path == nullptr) {
        LOGE("bad parameter!");
        return RESULT_BAD_PARAMETER;
    }

    FILE *file = fopen(path, fromFile ? "rb" : "wb");
    if (file == nullptr) {
        LOGE("can not open %s", path);
        return RESULT_BASE_MODULE_ERROR;
    }

    int32_t format = buffer->getPixelFormat();
    int32_t width = buffer->getStride();
    int32_t height = buffer->getHeight();

    if (format == HAL_PIXEL_FORMAT_YCRCB_420_SP
            || format == HAL_PIXEL_FORMAT_YCRCB_420_SP
            || format == HAL_PIXEL_FORMAT_YV12
            || format == HAL_PIXEL_FORMAT_YCbCr_422_SP) {
        android_ycbcr ycbcr = android_ycbcr();
        buffer->lockYCbCr(GRALLOC_USAGE_SW_READ_OFTEN, &ycbcr);

        void *ydata = ycbcr.y;
        void *udata = ycbcr.cb;
        void *vdata = ycbcr.cr;

        if (ydata == nullptr || udata == nullptr || vdata == nullptr) {
            LOGE("can not get addr of graphic buffer!");

            buffer->unlock();
            fclose(file);

            return RESULT_BASE_MODULE_ERROR;
        }

        size_t size = width * height;

        if (fromFile) {
            fread(ydata, 1, size, file);
        } else {
            fwrite(ydata, 1, size, file);
        }

        if (format == HAL_PIXEL_FORMAT_YCbCr_422_SP) {
            size = size >> 1;
        } else {
            size = size >> 2;
        }

        if (fromFile) {
            fread(udata, 1, size, file);
            fread(vdata, 1, size, file);
        } else {
            fwrite(udata, 1, size, file);
            fwrite(vdata, 1, size, file);
        }

        buffer->unlock();
    } else {
        void *data = nullptr;
        int32_t pixelLen = 0;

        buffer->lock(GRALLOC_USAGE_SW_READ_OFTEN, &data, &pixelLen, nullptr);

        if (data == nullptr) {
            LOGE("can not get addr of graphic buffer!");

            buffer->unlock();
            fclose(file);

            return RESULT_BASE_MODULE_ERROR;
        }

        static const size_t headerLen = 54;
        size_t size = width * height * pixelLen;

        if (fromFile) {
            fseek(file, headerLen, SEEK_SET);
            fread(data, size, 1, file);
        } else {
            unsigned char header[headerLen] = { 0 };
            header[0] = (unsigned char)0x42;
            header[1] = (unsigned char)0x4d;
            header[10] = (unsigned char)54;
            header[14] = (unsigned char)40;
            header[26] = (unsigned char)1;
            header[28] = (unsigned char)(pixelLen << 3);

            long fileSize = (long)(size + headerLen);

            header[2] = (unsigned char)(fileSize & 0x000000ff);
            header[3] = (unsigned char)((fileSize >> 8) & 0x000000ff);
            header[4] = (unsigned char)((fileSize >> 16) & 0x000000ff);
            header[5] = (unsigned char)((fileSize >> 24) & 0x000000ff);

            header[18] = (unsigned char)(width & 0x000000ff);
            header[19] = (unsigned char)((width >> 8) & 0x000000ff);
            header[20] = (unsigned char)((width >> 16) & 0x000000ff);
            header[21] = (unsigned char)((width >> 24) & 0x000000ff);

            header[22] = (unsigned char)(height & 0x000000ff);
            header[23] = (unsigned char)((height >> 8) & 0x000000ff);
            header[24] = (unsigned char)((height >> 16) & 0x000000ff);
            header[25] = (unsigned char)((height >> 24) & 0x000000ff);

            fwrite((const void *)header, 1, headerLen, file);
            fwrite(data, 1, size, file);

            const long diff = Align(width * pixelLen, 4)
                    * height - size;

            if (diff != 0) {
                /*
                unsigned char fill[diff] = { 0 };
                fwrite((const void *)fill, sizeof(unsigned char), diff, file);
                */

                unsigned char fill = 0;
                for (long index = 0; index < diff; index++) {
                    fwrite((const void *)&fill, 1, 1, file);
                }
            }
        }

        buffer->unlock();
    }

    fclose(file);

    return RESULT_OK;
}
