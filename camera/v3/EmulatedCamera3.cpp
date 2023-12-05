/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Contains implementation of a class EmulatedCamera that encapsulates
 * functionality common to all version 3.0 emulated camera devices.  Instances
 * of this class (for each emulated camera) are created during the construction
 * of the EmulatedCameraFactory instance.  This class serves as an entry point
 * for all camera API calls that defined by camera3_device_ops_t API.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera3_Camera"
#include <android/log.h>

#include "EmulatedCamera3.h"
#include "system/camera_metadata.h"

namespace android {

/**
 * Constructs EmulatedCamera3 instance.
 * Param:
 *  cameraId - Zero based camera identifier, which is an index of the camera
 *      instance in camera factory's array.
 *  module - Emulated camera HAL module descriptor.
 */
EmulatedCamera3::EmulatedCamera3(int cameraId,
        struct hw_module_t* module):
        EmulatedBaseCamera(cameraId,
                CAMERA_DEVICE_API_VERSION_3_2,
                &common,
                module),
        mStatus(STATUS_ERROR),
        mPlugged(false)
{
    common.close = EmulatedCamera3::close;
    ops = &sDeviceOps;

    mCallbackOps = NULL;

    mVendorTagOps.get_camera_vendor_section_name =
            EmulatedCamera3::get_camera_vendor_section_name;
    mVendorTagOps.get_camera_vendor_tag_name =
            EmulatedCamera3::get_camera_vendor_tag_name;
    mVendorTagOps.get_camera_vendor_tag_type =
            EmulatedCamera3::get_camera_vendor_tag_type;
    mVendorTagOps.parent = this;
}

/* Destructs EmulatedCamera3 instance. */
EmulatedCamera3::~EmulatedCamera3() {
}

/****************************************************************************
 * Abstract API
 ***************************************************************************/

/****************************************************************************
 * Public API
 ***************************************************************************/

status_t EmulatedCamera3::Initialize() {
    CAMHAL_LOGV("%s", __FUNCTION__);

    mStatus = STATUS_CLOSED;
    return NO_ERROR;
}

bool EmulatedCamera3::getCameraStatus()
{
    CAMHAL_LOGD("%s : do nothing", __FUNCTION__);
    return false;
}

/****************************************************************************
 * Camera API implementation
 ***************************************************************************/

status_t EmulatedCamera3::connectCamera(hw_device_t** device) {
    CAMHAL_LOGV("%s", __FUNCTION__);
    if (device == NULL) return BAD_VALUE;

    if (mStatus != STATUS_CLOSED) {
        CAMHAL_LOGE("%s: Trying to open a camera in state %d!",
                __FUNCTION__, mStatus);
        return INVALID_OPERATION;
    }

    *device = &common;
    mStatus = STATUS_OPEN;
    return NO_ERROR;
}

status_t EmulatedCamera3::closeCamera() {
    mStatus = STATUS_CLOSED;
    return NO_ERROR;
}

status_t EmulatedCamera3::getCameraInfo(struct camera_info* info) {
    return EmulatedBaseCamera::getCameraInfo(info);
}

/****************************************************************************
 * Camera Device API implementation.
 * These methods are called from the camera API callback routines.
 ***************************************************************************/

status_t EmulatedCamera3::initializeDevice(
        const aml_camera_callback_ops *callbackOps) {
    if (callbackOps == NULL) {
        CAMHAL_LOGE("%s: NULL callback ops provided to HAL!",
                __FUNCTION__);
        return BAD_VALUE;
    }

    if (mStatus != STATUS_OPEN) {
        CAMHAL_LOGE("%s: Trying to initialize a camera in state %d!",
                __FUNCTION__, mStatus);
        return INVALID_OPERATION;
    }

    mCallbackOps = callbackOps;
    mStatus = STATUS_READY;

    return NO_ERROR;
}

status_t EmulatedCamera3::configureStreams(
        aml_camera_stream_configuration *streamList) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return INVALID_OPERATION;
}

status_t EmulatedCamera3::registerStreamBuffers(
        const aml_camera_stream_buffer_set *bufferSet) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return INVALID_OPERATION;
}

const camera_metadata_t* EmulatedCamera3::constructDefaultRequestSettings(
        int type) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return NULL;
}

status_t EmulatedCamera3::processCaptureRequest(
        aml_camera_capture_request *request) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return INVALID_OPERATION;
}

/** Custom tag query methods */

const char* EmulatedCamera3::getVendorSectionName(uint32_t tag) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return NULL;
}

const char* EmulatedCamera3::getVendorTagName(uint32_t tag) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return NULL;
}

int EmulatedCamera3::getVendorTagType(uint32_t tag) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return -1;
}

/** Debug methods */

void EmulatedCamera3::dump(int fd) {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return;
}

int EmulatedCamera3::flush_all_requests() {
    CAMHAL_LOGE("%s: Not implemented", __FUNCTION__);
    return -1;
}

/****************************************************************************
 * Protected API. Callbacks to the framework.
 ***************************************************************************/

void EmulatedCamera3::sendCaptureResult(aml_camera_capture_result_t *result) {
    mCallbackOps->process_capture_result(mCallbackOps, result);
}

void EmulatedCamera3::sendNotify(aml_notify_message_t *msg) {
    if (mCallbackOps && mCallbackOps->notify)
        mCallbackOps->notify(mCallbackOps, msg);
}

/****************************************************************************
 * Private API.
 ***************************************************************************/

/****************************************************************************
 * Camera API callbacks as defined by camera3_device_ops structure.  See
 * hardware/libhardware/include/hardware/camera3.h for information on each
 * of these callbacks. Implemented in this class, these callbacks simply
 * dispatch the call into an instance of EmulatedCamera3 class defined by the
 * 'camera_device3' parameter, or set a member value in the same.
 ***************************************************************************/

EmulatedCamera3* getInstance(const aml_camera_device_t *d) {
    const EmulatedCamera3* cec = static_cast<const EmulatedCamera3*>(d);
    return const_cast<EmulatedCamera3*>(cec);
}

int EmulatedCamera3::initialize(const struct aml_camera_device *d,
        const aml_camera_callback_ops_t *callback_ops) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->initializeDevice(callback_ops);
}

int EmulatedCamera3::configure_streams(const struct aml_camera_device *d,
        aml_camera_stream_configuration_t *stream_list) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->configureStreams(stream_list);
}

int EmulatedCamera3::register_stream_buffers(
        const struct aml_camera_device *d,
        const aml_camera_stream_buffer_set_t *buffer_set) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->registerStreamBuffers(buffer_set);
}

int EmulatedCamera3::process_capture_request(
        const struct aml_camera_device *d,
        aml_camera_capture_request_t *request) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->processCaptureRequest(request);
}

const camera_metadata_t* EmulatedCamera3::construct_default_request_settings(
        const aml_camera_device_t *d, int type) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->constructDefaultRequestSettings(type);
}

void EmulatedCamera3::get_metadata_vendor_tag_ops(const aml_camera_device_t *d,
        vendor_tag_query_ops_t *ops) {
    ops->get_camera_vendor_section_name = get_camera_vendor_section_name;
    ops->get_camera_vendor_tag_name = get_camera_vendor_tag_name;
    ops->get_camera_vendor_tag_type = get_camera_vendor_tag_type;
}

const char* EmulatedCamera3::get_camera_vendor_section_name(
        const vendor_tag_query_ops_t *v,
        uint32_t tag) {
    EmulatedCamera3* ec = static_cast<const TagOps*>(v)->parent;
    return ec->getVendorSectionName(tag);
}

const char* EmulatedCamera3::get_camera_vendor_tag_name(
        const vendor_tag_query_ops_t *v,
        uint32_t tag) {
    EmulatedCamera3* ec = static_cast<const TagOps*>(v)->parent;
    return ec->getVendorTagName(tag);
}

int EmulatedCamera3::get_camera_vendor_tag_type(
        const vendor_tag_query_ops_t *v,
        uint32_t tag)  {
    EmulatedCamera3* ec = static_cast<const TagOps*>(v)->parent;
    return ec->getVendorTagType(tag);
}

void EmulatedCamera3::dump(const aml_camera_device_t *d, int fd) {
    EmulatedCamera3* ec = getInstance(d);
    ec->dump(fd);
}

int EmulatedCamera3::flush(const struct aml_camera_device *d) {
    EmulatedCamera3* ec = getInstance(d);
    return ec->flush_all_requests();
}

int EmulatedCamera3::close(struct hw_device_t* device) {
    EmulatedCamera3* ec =
            static_cast<EmulatedCamera3*>(
                reinterpret_cast<aml_camera_device_t*>(device) );
    if (ec == NULL) {
        CAMHAL_LOGE("%s: Unexpected NULL camera3 device", __FUNCTION__);
        return BAD_VALUE;
    }
    return ec->closeCamera();
}

aml_camera_device_ops_t EmulatedCamera3::sDeviceOps = {
    EmulatedCamera3::initialize,
    EmulatedCamera3::configure_streams,
    NULL,//EmulatedCamera3::register_stream_buffers,
    EmulatedCamera3::construct_default_request_settings,
    EmulatedCamera3::process_capture_request,
    NULL,//EmulatedCamera3::get_metadata_vendor_tag_ops,
    EmulatedCamera3::dump,
    EmulatedCamera3::flush,
};

}; /* namespace android */
