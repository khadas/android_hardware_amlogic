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
#include <Renderer.h>

int main() {
    LOGI("---Prepare data");

    const char *paths[] = {
        "/data/front.bmp",
        "/data/right.bmp",
        "/data/back.bmp",
        "/data/left.bmp",
        "/data/car.bmp"
    };

    ImageReader *reader = new ImageReader(AllocatdByGraphicBuffer,
            paths, sizeof(paths)/sizeof(const char *));
    if (reader == nullptr) {
        LOGE("no memory !");
        return RESULT_NO_MEMORY;
    }

    Texture *textures = reader->Textures();

    //----------surround----------
    Texture textureOfFrontCamera = textures[0];
    Texture textureOfRightCamera = textures[1];
    Texture textureOfBackCamera = textures[2];
    Texture textureOfLeftCamera = textures[3];

    TextureInfo cameraDatas[] = {
        {
            textureOfFrontCamera,
            {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 1.5, 0.0, 1.0
            },
            1
        },
        {
            textureOfRightCamera,
            {
                0.0, -1.0, 0.0, 0.0,
                1.0,  0.0, 0.0, 0.0,
                0.0,  0.0, 1.0, 0.0,
                1.5,  0.0, 0.0, 1.0
            },
            1
        },
        {
            textureOfBackCamera,
            {
               -1.0,  0.0, 0.0, 0.0,
                0.0, -1.0, 0.0, 0.0,
                0.0,  0.0, 1.0, 0.0,
                0.0, -1.5, 0.0, 1.0
            },
            1
        },
        {
            textureOfLeftCamera,
            {
                0.0, 1.0, 0.0, 0.0,
               -1.0, 0.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
               -1.5, 0.0, 0.0, 1.0
            },
            1
        }
    };

    //----------track----------
    char angle[65];
    sprintf(angle, "%f", 0.3);

    CarInfo carInfo[] = {
        {
            "back_car",
            "1"
        },
        {
            "steering_angle",
            angle
        }
    };

    //----------car----------
    Texture carTexture = textures[4];;
    TextureInfo carModel[] = {
        {
            carTexture,
            {
                0.5,  0.0, 0.0, 0.0,
                0.0,  0.5, 0.0, 0.0,
                0.0,  0.0, 1.0, 0.0,
                0.0,  0.0, 0.0, 1.0
            },
            1
        }
    };

    LOGI("---Create frame buffer");
    sp<GraphicBuffer> frameBuffer = new GraphicBuffer(
                                        800, 1280,
                                        android::PIXEL_FORMAT_RGBA_8888,
                                        android::GraphicBuffer::USAGE_HW_TEXTURE,
                                        "LGES Test");
    if (frameBuffer == nullptr) {
        LOGE("No memory !");
        return RESULT_NO_MEMORY;
    }

    LOGI("---Create Renderer");
    Renderer *renderer = new Renderer(frameBuffer);
    if (renderer == nullptr) {
        LOGE("No memory !");
        return RESULT_NO_MEMORY;
    }

    LOGI("---Init");
    renderer->Init();

    LOGI("---SetParams");
    renderer->SetCameraData(cameraDatas,
            sizeof(cameraDatas)/sizeof(TextureInfo));

    renderer->SetCarInfo(carInfo,
            sizeof(carInfo)/sizeof(carInfo));

    renderer->SetCarModel(carModel,
            sizeof(carModel)/sizeof(TextureInfo));

    LOGI("---Render");
    renderer->Begin();
    renderer->Render();
    renderer->Finish();

    LOGI("---Deinit");
    renderer->Deinit();

    LOGI("---Release");
    delete renderer;
    renderer = nullptr;

    delete(reader);
    reader = nullptr;

    return RESULT_OK;
}
