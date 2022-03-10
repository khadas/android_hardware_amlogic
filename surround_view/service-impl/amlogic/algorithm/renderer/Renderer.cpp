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

#define LOG_TAG "Renderer"

#include "SurroundPainter.h"
#include "TrackPainter.h"
#include "CarPainter.h"

#include "Renderer.h"


Renderer::Renderer(sp<GraphicBuffer> frameBuffer)
        : FrameRenderer(frameBuffer) {
    mPaintersNum = sizeof(mPainters) / sizeof(BasePainter*);

    mPainters[1] = new SurroundPainter();
    mPainters[0] = new TrackPainter();
    mPainters[2] = new CarPainter();
}

int Renderer::InitDatas() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->InitDatas();
        }
    }
    return RESULT_OK;
}

int Renderer::DeinitDatas() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->DeinitDatas();
        }
    }
    return RESULT_OK;
}

GLenum Renderer::InitGL() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->InitGL();
        }
    }
    return GL_NO_ERROR;
}

GLenum Renderer::DeinitGL() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->DeinitGL();
        }
    }
    return GL_NO_ERROR;
}

int Renderer::SetViewPort(ViewPort viewPort) {
    return SetParams("view_port", viewPort.top,
            viewPort.left, viewPort.width, viewPort.height);
}

int Renderer::SetCameraData(TextureInfo *textInfos, size_t num) {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->SetParams("camera_datas", (void *)textInfos, num, num);
        }
    }
    return GL_NO_ERROR;
}

int Renderer::SetCarInfo(CarInfo *carInfos, size_t num) {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->SetParams("car_infos", (void *)carInfos, num);
        }
    }
    return GL_NO_ERROR;
}

int Renderer::SetCarModel(TextureInfo *carModel, size_t num) {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->SetParams("car_model", (void *)carModel, num, num);
        }
    }
    return GL_NO_ERROR;
}

GLenum Renderer::Prepare() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->Prepare();
        }
    }
    return GL_NO_ERROR;
}

GLenum Renderer::Draw() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->Draw();
        }
    }
    return GL_NO_ERROR;
}

GLenum Renderer::Complete() {
    for (size_t index = 0; index < mPaintersNum; index++) {
        if (mPainters[index]) {
            mPainters[index]->Complete();
        }
    }
    return GL_NO_ERROR;
}
