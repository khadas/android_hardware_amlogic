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

#define LOG_TAG "main"

#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <Renderer.h>
#include "StreamHandler.h"

using namespace android;


typedef struct {
    const char *path;
    GLfloat mvp[MVPLEN];
} TextureData;


int main() {
    LOGI("---Prepare data");

    //--------------surround--------------

    TextureData cameras[] = {
        {
            "/dev/video0",
            {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 1.5, 0.0, 1.0
            }
        }
    };

    Texture frontCameraTexture;
    TextureInfo cameraDatas[] = {
        {
            frontCameraTexture,
            {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 1.5, 0.0, 1.0
            },
            1
        }
    };

    //--------------track--------------

    char angle[64] = { 0 };
    CarInfo carInfos[2] = {
        {
            "back_car",
            "1"
        },
        {
            "steering_angle",
            angle
        }
    };

    //--------------car model--------------

    TextureData carModel[1] = {
        {
            "/data/car.bmp",
            {
                0.5,  0.0, 0.0, 0.0,
                0.0,  0.5, 0.0, 0.0,
                0.0,  0.0, 1.0, 0.0,
                0.0,  0.0, 0.0, 1.0
            }
        }
    };

    Texture carModelTexture;
    TextureInfo carModelDatas[] = {
        {
            carModelTexture,
            {
                0.5,  0.0, 0.0, 0.0,
                0.0,  0.5, 0.0, 0.0,
                0.0,  0.0, 1.0, 0.0,
                0.0,  0.0, 0.0, 1.0
            },
            1
        }
    };

    memcpy(cameraDatas[0].mvp, cameras[0].mvp, MVPSIZE);
    memcpy(carModelDatas[0].mvp, carModel[0].mvp, MVPSIZE);

    const char *paths[1] = { carModel[0].path };
    ImageReader *reader = new ImageReader(AllocatdByGraphicBuffer,
            paths, 1);
    if (reader == nullptr) {
        LOGE("no memory !");
        return RESULT_NO_MEMORY;
    }

    Texture *textures = reader->Textures();
    memcpy(&carModelTexture, textures, sizeof(Texture));

    //--------------run--------------

    LOGI("---Get cameras and display");
    android::sp<IEvsEnumerator> evs = IEvsEnumerator::getService("default");
    if (evs == nullptr) {
        LOGE("can get evs server client!");
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    int displayId;
    evs->getDisplayIdList([&displayId](auto idList) {
        displayId = idList[0];
    });

    android::sp<IEvsDisplay> display = evs->openDisplay_1_1(displayId);
    if (display.get() == nullptr) {
        LOGE("can not open display %d", displayId);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    BufferDesc_1_0 target = {};
    display->getTargetBuffer(
            [&target](const BufferDesc_1_0& buff) {
                target = buff;
            }
    );

    /*
    if (target.memHandle == nullptr) {
        LOGE("can not get requested output buffer!");
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }
    */

    /*
    sp<GraphicBuffer> frame = new GraphicBuffer(target.memHandle,
                                                GraphicBuffer::CLONE_HANDLE,
                                                target.width,
                                                target.height,
                                                target.format,
                                                1, // layer count
                                                GRALLOC_USAGE_HW_RENDER,
                                                target.stride);
    */

    sp<GraphicBuffer> frame = new GraphicBuffer(
                                            target.width,
                                            target.height,
                                            android_pixel_format_t::HAL_PIXEL_FORMAT_RGBA_8888,
                                            android::GraphicBuffer::USAGE_HW_TEXTURE,
                                            "sv");

    sp<IEvsCamera> camera = IEvsCamera::castFrom(evs->openCamera(cameras[0].path))
            .withDefault(nullptr);
    if (camera == nullptr) {
        LOGE("can get evs camera!");
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    sp<StreamHandler> stream = new StreamHandler(camera, 2, false,
            android_pixel_format_t::HAL_PIXEL_FORMAT_RGBA_8888);
    if (stream == nullptr) {
        LOGE("create stream failed!");
        evs->closeCamera(camera);
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    if (!stream->startStream()) {
        LOGE("can not start stream of %s", cameras[0].path);
        evs->closeCamera(camera);
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    while (!stream->newFrameAvailable()) {
        usleep(5000);
    }

    BufferDesc buffer = stream->getNewFrame();

    const AHardwareBuffer_Desc* desc =
            reinterpret_cast<const AHardwareBuffer_Desc *>(&buffer.buffer.description);
    if (desc == nullptr) {
        LOGE("convert to desc failed!");
        stream->asyncStopStream();
        evs->closeCamera(camera);
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }

    sp<GraphicBuffer> texture =
            new GraphicBuffer(buffer.buffer.nativeHandle,
                                GraphicBuffer::CLONE_HANDLE,
                                desc->width,
                                desc->height,
                                desc->format,
                                1,//desc->layers,
                                GRALLOC_USAGE_HW_TEXTURE,
                                desc->stride);
    if (texture == nullptr) {
        LOGE("create graphicBuffer failed!");
        stream->doneWithFrame(buffer);
        stream->asyncStopStream();
        evs->closeCamera(camera);
        evs->closeDisplay(display);
        delete reader;
        return RESULT_BASE_MODULE_ERROR;
    }
    LOGD("---fucky: %d, expect: %d", frame->getPixelFormat(), HAL_PIXEL_FORMAT_RGBA_8888);

    LOGI("---Create Renderer");
    Renderer *renderer = new Renderer(frame);
    if (renderer == nullptr) {
        LOGE("No memory !");
        stream->doneWithFrame(buffer);
        stream->asyncStopStream();
        evs->closeCamera(camera);
        evs->closeDisplay(display);
        delete reader;
        return RESULT_NO_MEMORY;
    }

    LOGI("---Init");
    renderer->Init();

    LOGI("---SetParams");
    GraphicBufferUtils::GraphicBufferToTexture(texture, frontCameraTexture);
    // GraphicBufferUtils::SwapWithFile(texture, "/data/camera.bmp", 0);

    renderer->SetCameraData(cameraDatas,
            sizeof(cameraDatas)/sizeof(TextureInfo));

    sprintf(angle, "%f", 0.3);
    renderer->SetCarInfo(carInfos,
            sizeof(carInfos)/sizeof(CarInfo));

    renderer->SetCarModel(carModelDatas,
            sizeof(carModelDatas)/sizeof(TextureInfo));

    LOGI("---Render");
    renderer->Render();

    LOGI("---Deinit");
    renderer->Deinit();

    delete renderer;
    renderer = nullptr;

    LOGI("---Release");
    stream->doneWithFrame(buffer);
    display->returnTargetBufferForDisplay(target);

    stream->asyncStopStream();

    evs->closeCamera(camera);

    evs->closeDisplay(display);

    delete reader;

    LOGI("---Exit");
    return RESULT_OK;
}
