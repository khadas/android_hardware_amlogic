/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#include "CameraConfig.h"
#include "ispMgr/staticPipe.h"


#define DEFAULT_WIDTH_GROUP0 (1440)
#define DEFAULT_HEIGHT_GROUP0 (1088)

#define DEFAULT_WIDTH_GROUP1 (640)
#define DEFAULT_HEIGHT_GROUP1 (480)


namespace android {

CameraConfig* CameraConfig::mInstance[ISP_PORT_NUM] = {nullptr};

CameraConfig::CameraConfig(int groupId) {

    if (groupId == 0) {

        mGDCParam.width = DEFAULT_WIDTH_GROUP0;
        mGDCParam.height = DEFAULT_HEIGHT_GROUP0;
        mGDCParam.planeNum = 1;

        mGE2DParam.src_width = DEFAULT_WIDTH_GROUP0;
        mGE2DParam.src_height = DEFAULT_HEIGHT_GROUP0;

    } else { //

        mGDCParam.width = DEFAULT_WIDTH_GROUP1;
        mGDCParam.height = DEFAULT_HEIGHT_GROUP1;
        mGDCParam.planeNum = 1;

        mGE2DParam.src_width = DEFAULT_WIDTH_GROUP1;
        mGE2DParam.src_height = DEFAULT_HEIGHT_GROUP1;
    }
    memset(&mSensorParam, 0, sizeof(struct media_stream));
    mGDCParam.input_width = 0;
    mGDCParam.input_height = 0;
    mGDCParam.stride = 0;
    mGDCParam.input_stride = 0;
}

CameraConfig* CameraConfig::getInstance(int groupId) {

    if (groupId >= ISP_PORT_NUM) return nullptr;

    if (mInstance[groupId])
        return mInstance[groupId];
    else {
        mInstance[groupId] = new CameraConfig(groupId);
        return mInstance[groupId];
    }
}

CameraConfig::~CameraConfig() {

}

void CameraConfig::deleteInstance() {
    int i  = 0;
    for (i = 0; i < ISP_PORT_NUM; i++) {
        if (mInstance[i]) {
            delete mInstance[i];
            mInstance[i] = nullptr;
        }
    }
}
uint32_t CameraConfig::getWidth() {

    return mGDCParam.width;
}

void CameraConfig::setWidth(uint32_t width) {

    mGDCParam.width = width;
    mGE2DParam.src_width = width;
}

uint32_t CameraConfig::getHeight() {

    return mGDCParam.height;
}

void CameraConfig::setHeight(uint32_t height) {

    mGDCParam.height = height;
    mGE2DParam.src_height = height;
}

uint32_t CameraConfig::getStride() {
    return mGDCParam.stride;
}

void CameraConfig::setStride(uint32_t stride) {
    mGDCParam.stride = stride;
}

uint32_t CameraConfig::getInputStride() {
    return 0;
}

void CameraConfig::setInputStride(uint32_t stride) {

}

void CameraConfig::setSensorCfg(media_stream_t& stream) {
    mSensorParam = stream;
}

uint32_t CameraConfig::getInputWidth() {

    return mGDCParam.input_width;
}

void CameraConfig::setInputWidth(uint32_t width) {
    mGDCParam.input_width = width;
}

uint32_t CameraConfig::getInputHeight() {

    return mGDCParam.input_height;
}

void CameraConfig::setInputHeight(uint32_t height) {

    mGDCParam.input_height = height;
}

void CameraConfig::setCropInfo(CropInfo inputCropInfo) {
    mGDCParam.mCropInfo = inputCropInfo;

}

CropInfo CameraConfig::getCropInfo(void) {
    return mGDCParam.mCropInfo;
}

}

