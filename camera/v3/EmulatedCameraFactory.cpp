/*
 * Copyright (C) 2011 The Android Open Source Project
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

/*
 * Contains implementation of a class EmulatedCameraFactory that manages cameras
 * available for emulation.
 */

//#define LOG_NDEBUG 0
//#define LOG_NDDEBUG 0
//#define LOG_NIDEBUG 0
#define LOG_TAG "EmulatedCamera_Factory"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <android/log.h>
#include <cutils/properties.h>
#include "EmulatedFakeCamera.h"
#include "EmulatedFakeCamera2.h"
#include "EmulatedFakeCamera3.h"
#include "EmulatedCameraHotplugThread.h"
#include "EmulatedCameraFactory.h"
#include "HDMIStatus.h"
#include <utils/Trace.h>

extern camera_module_t HAL_MODULE_INFO_SYM;

/* A global instance of EmulatedCameraFactory is statically instantiated and
 * initialized when camera emulation HAL is loaded.
 */
android::EmulatedCameraFactory  gEmulatedCameraFactory;
default_camera_hal::VendorTags gVendorTags;

#ifndef MIPI_DEVICE_NUM
#if BUILD_KERNEL_4_9 == true
#define MIPI_DEVICE_NUM 2
#else
#define MIPI_DEVICE_NUM 7
#endif
#endif

namespace android {

EmulatedCameraFactory::EmulatedCameraFactory()
        : mCameraVirtualDevice(NULL),
          mEmulatedCameraNum(0),
          mConstructedOK(false),
          mCallbacks(NULL),
          mHDMIStatusInstance(NULL)
{
    status_t res;
    /* Connect to the factory service in the emulator, and create Qemu cameras. */
    int cameraId = 0;
    ATRACE_CALL();

    memset(mEmulatedCameras, 0,(MAX_CAMERA_NUM) * sizeof(EmulatedBaseCamera*));
    if (!mCameraVirtualDevice)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    if (!mHDMIStatusInstance)
        mHDMIStatusInstance = HDMIStatus::getInstance();
    mEmulatedCameraNum = mCameraVirtualDevice->getLegacyCameraNum();
    CAMHAL_LOGD("Camera num = %d", mEmulatedCameraNum);

    for( int i = 0; i < mEmulatedCameraNum; i++ ) {
        cameraId = i;
        mEmulatedCameras[i] = new EmulatedFakeCamera3(cameraId, &HAL_MODULE_INFO_SYM.common);
        if (mEmulatedCameras[i] != NULL) {
            CAMHAL_LOGV("%s: camera device version is %d", __FUNCTION__,
                    getFakeCameraHalVersion(cameraId));
            res = mEmulatedCameras[i]->Initialize();
            if (res != NO_ERROR) {
                CAMHAL_LOGE("%s: Unable to initialize camera %d: %s (%d)",
                    __FUNCTION__, i, strerror(-res), res);
                delete mEmulatedCameras[i];
                mEmulatedCameras[i] = NULL;
            }
        }
    }

    CAMHAL_LOGD("%d cameras are being created",
          mEmulatedCameraNum);

    /* Create hotplug thread */

    mHDMIStatusInstance->startDetectStatus();
    CAMHAL_LOGD("start detect hdmi");
    mConstructedOK = true;
}

EmulatedCameraFactory::~EmulatedCameraFactory()
{
    CAMHAL_LOGD("Camera Factory deconstruct the BaseCamera\n");
    for (int n = 0; n < MAX_CAMERA_NUM; n++) {
        if (mEmulatedCameras[n] != NULL) {
            delete mEmulatedCameras[n];
        }
    }

    if (mHotplugThread != NULL) {
        mHotplugThread->requestExit();
        mHotplugThread->join();
        mHotplugThread = NULL;
    }

    if (mHDMIStatusInstance) {
        HDMIStatus::putInstance();
        mHDMIStatusInstance = NULL;
    }
}

/****************************************************************************
 * Camera HAL API handlers.
 *
 * Each handler simply verifies existence of an appropriate EmulatedBaseCamera
 * instance, and dispatches the call to that instance.
 *
 ***************************************************************************/

void EmulatedCameraFactory::searchExternalSensor()
{
    //int cameraId = 0;
    int name_id = -1;
    ATRACE_CALL();

    if (!mCameraVirtualDevice)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    for (int i = 0; i < USB_DEVICE_NUM; i++ ) {
        CAMHAL_LOGD("search external camera id %d", i);
        if (mCameraVirtualDevice->isNormalExternalCameraByIndex(i)) {
            name_id = mCameraVirtualDevice->getDeviceNameIdbyIndex(i);
            while (mCallbacks == NULL || mCallbacks->camera_device_status_change == NULL) {
                usleep(1000*20);
            }
            onStatusChanged(name_id, CAMERA_DEVICE_STATUS_PRESENT);
        }
    }
}

int EmulatedCameraFactory::cameraDeviceOpen(int camera_id, hw_device_t** device)
{
    CAMHAL_LOGV("%s: id = %d", __FUNCTION__, camera_id);
    //int valid_id;
    *device = NULL;
    ATRACE_CALL();

    updateCamHalLogLevel();

    if (!isConstructedOK()) {
        CAMHAL_LOGE("%s: EmulatedCameraFactory has failed to initialize", __FUNCTION__);
        return -EINVAL;
    }

    if (camera_id < 0 || camera_id >= MAX_CAMERA_NUM) {
        CAMHAL_LOGE("%s: Camera id %d is out of bounds (%d)",
             __FUNCTION__, camera_id, MAX_CAMERA_NUM);
        return -ENODEV;
    }
    return mEmulatedCameras[camera_id]->connectCamera(device);
}

int EmulatedCameraFactory::getCameraInfo(int camera_id, struct camera_info* info)
{
    ATRACE_CALL();
    CAMHAL_LOGV("%s: id = %d", __FUNCTION__, camera_id);
    //int valid_id;
    if (!isConstructedOK()) {
        CAMHAL_LOGE("%s: EmulatedCameraFactory has failed to initialize", __FUNCTION__);
        return -EINVAL;
    }

    if (camera_id < 0 || camera_id >= MAX_CAMERA_NUM) {
        CAMHAL_LOGE("%s: Camera id %d is out of bounds (%d)",
             __FUNCTION__, camera_id, MAX_CAMERA_NUM);
        return -ENODEV;
    }
    if (!mEmulatedCameras[camera_id])
        return -ENODEV;
    return mEmulatedCameras[camera_id]->getCameraInfo(info);
}

int EmulatedCameraFactory::setCallbacks(
        const camera_module_callbacks_t *callbacks)
{
    CAMHAL_LOGV("%s: callbacks = %p", __FUNCTION__, callbacks);

    mCallbacks = callbacks;
    /* Create hotplug thread */
    {
        searchExternalSensor();
        mHotplugThread = new EmulatedCameraHotplugThread(NULL,
                                                         mEmulatedCameraNum);
        mHotplugThread->run("");
    }
    return OK;
}

static int get_tag_count(const vendor_tag_ops_t* ops)
{
    return gVendorTags.getTagCount(ops);
}
static void get_all_tags(const vendor_tag_ops_t* ops, uint32_t* tag_array)
{
    gVendorTags.getAllTags(ops, tag_array);
}
static const char* get_section_name(const vendor_tag_ops_t* ops, uint32_t tag)
{
    return gVendorTags.getSectionName(ops, tag);
}
static const char* get_tag_name(const vendor_tag_ops_t* ops, uint32_t tag)
{
    return gVendorTags.getTagName(ops, tag);
}
static int get_tag_type(const vendor_tag_ops_t* ops, uint32_t tag)
{
    return gVendorTags.getTagType(ops, tag);
}
void EmulatedCameraFactory::getvendortagops(vendor_tag_ops_t* ops)
{
    CAMHAL_LOGV("%s : ops=%p", __func__, ops);
    ops->get_tag_count      = get_tag_count;
    ops->get_all_tags       = get_all_tags;
    ops->get_section_name   = get_section_name;
    ops->get_tag_name       = get_tag_name;
    ops->get_tag_type       = get_tag_type;
}

int EmulatedCameraFactory::setTorchMode(const char* camera_id, bool enabled)
{
    //OPERATION_NOT_SUPPORTED
    CAMHAL_LOGW("%s : operation not supported !!", __func__);
    return -ENOSYS;
}

int EmulatedCameraFactory::isStreamCombinationSupported(int camera_id, const camera_stream_combination_t *streams) {
    if (camera_id < 0 || camera_id > MAX_CAMERA_NUM)
        return -ENOSYS;
    if (mEmulatedCameras[camera_id] == nullptr)
        return -ENOSYS;
    return mEmulatedCameras[camera_id]->isStreamCombinationSupported(streams);

}

/****************************************************************************
 * Camera HAL API callbacks.
 ***************************************************************************/

EmulatedBaseCamera* EmulatedCameraFactory::getValidCameraObject()
{
    EmulatedBaseCamera* cam = NULL;
    for (int i = 0; i < MAX_CAMERA_NUM; i++) {
        if (mEmulatedCameras[i] != NULL) {
            cam =  mEmulatedCameras[i];
            break;
        }
    }
    return cam;
}

int EmulatedCameraFactory::getValidCameraObjectId()
{
    int id = -1;
    for (int i = 0; i < MAX_CAMERA_NUM; i++) {
        if (mEmulatedCameras[i] != NULL) {
            id = i;
            break;
        }
    }
    return id;
}

int EmulatedCameraFactory::device_open(const hw_module_t* module,
                                       const char* name,
                                       hw_device_t** device)
{
    ATRACE_CALL();

    /*
     * Simply verify the parameters, and dispatch the call inside the
     * EmulatedCameraFactory instance.
     */

    if (module != &HAL_MODULE_INFO_SYM.common) {
        CAMHAL_LOGE("%s: Invalid module %p expected %p",
             __FUNCTION__, module, &HAL_MODULE_INFO_SYM.common);
        return -EINVAL;
    }
    if (name == NULL) {
        CAMHAL_LOGE("%s: NULL name is not expected here", __FUNCTION__);
        return -EINVAL;
    }

    return gEmulatedCameraFactory.cameraDeviceOpen(atoi(name), device);
}

int EmulatedCameraFactory::get_number_of_cameras(void)
{
    ATRACE_CALL();
    int i = 0;
    EmulatedBaseCamera* cam = gEmulatedCameraFactory.getValidCameraObject();
    while (i < 6) {
        if (cam != NULL) {
            if (!cam->getHotplugStatus()) {
                CAMHAL_LOGD("here we wait usb camera plug");
                usleep(50000);
                i++;
            } else {
                break;
            }
        } else {
            CAMHAL_LOGD("%s : cam is NULL", __FUNCTION__);
            break;
        }
    }
    CAMHAL_LOGD("%s : cam is %d",__FUNCTION__, gEmulatedCameraFactory.getEmulatedCameraNum());
    return gEmulatedCameraFactory.getEmulatedCameraNum();
}

int EmulatedCameraFactory::get_camera_info(int camera_id,
                                           struct camera_info* info)
{
    return gEmulatedCameraFactory.getCameraInfo(camera_id, info);
}

int EmulatedCameraFactory::set_callbacks(
        const camera_module_callbacks_t *callbacks)
{
    return gEmulatedCameraFactory.setCallbacks(callbacks);
}

void EmulatedCameraFactory::get_vendor_tag_ops(vendor_tag_ops_t* ops)
{
    gEmulatedCameraFactory.getvendortagops(ops);
}

int EmulatedCameraFactory::set_torch_mode(const char* camera_id, bool enabled)
{
    return gEmulatedCameraFactory.setTorchMode(camera_id, enabled);
}

int EmulatedCameraFactory::is_stream_combination_supported(int camera_id, const camera_stream_combination_t *streams) {
    return gEmulatedCameraFactory.isStreamCombinationSupported(camera_id, streams);
}

/********************************************************************************
 * Internal API
 *******************************************************************************/

/*
 * Camera information tokens passed in response to the "list" factory query.
 */

/* Device name token. */
//static const char lListNameToken[]    = "name=";
/* Frame dimensions token. */
//static const char lListDimsToken[]    = "framedims=";
/* Facing direction token. */
//static const char lListDirToken[]     = "dir=";

void EmulatedCameraFactory::createQemuCameras()
{
#if 0
    /* Obtain camera list. */
    char* camera_list = NULL;
    status_t res = mQemuClient.listCameras(&camera_list);
    /* Empty list, or list containing just an EOL means that there were no
     * connected cameras found. */
    if (res != NO_ERROR || camera_list == NULL || *camera_list == '\0' ||
        *camera_list == '\n') {
        if (camera_list != NULL) {
            free(camera_list);
        }
        return;
    }

    /*
     * Calculate number of connected cameras. Number of EOLs in the camera list
     * is the number of the connected cameras.
     */

    int num = 0;
    const char* eol = strchr(camera_list, '\n');
    while (eol != NULL) {
        num++;
        eol = strchr(eol + 1, '\n');
    }

    /* Allocate the array for emulated camera instances. Note that we allocate
     * two more entries for back and front fake camera emulation. */
    mEmulatedCameras = new EmulatedBaseCamera*[num + 2];
    if (mEmulatedCameras == NULL) {
        CAMHAL_LOGE("%s: Unable to allocate emulated camera array for %d entries",
             __FUNCTION__, num + 1);
        free(camera_list);
        return;
    }
    memset(mEmulatedCameras, 0, sizeof(EmulatedBaseCamera*) * (num + 1));

    /*
     * Iterate the list, creating, and initializing emulated qemu cameras for each
     * entry (line) in the list.
     */

    int index = 0;
    char* cur_entry = camera_list;
    while (cur_entry != NULL && *cur_entry != '\0' && index < num) {
        /* Find the end of the current camera entry, and terminate it with zero
         * for simpler string manipulation. */
        char* next_entry = strchr(cur_entry, '\n');
        if (next_entry != NULL) {
            *next_entry = '\0';
            next_entry++;   // Start of the next entry.
        }

        /* Find 'name', 'framedims', and 'dir' tokens that are required here. */
        char* name_start = strstr(cur_entry, lListNameToken);
        char* dim_start = strstr(cur_entry, lListDimsToken);
        char* dir_start = strstr(cur_entry, lListDirToken);
        if (name_start != NULL && dim_start != NULL && dir_start != NULL) {
            /* Advance to the token values. */
            name_start += strlen(lListNameToken);
            dim_start += strlen(lListDimsToken);
            dir_start += strlen(lListDirToken);

            /* Terminate token values with zero. */
            char* s = strchr(name_start, ' ');
            if (s != NULL) {
                *s = '\0';
            }
            s = strchr(dim_start, ' ');
            if (s != NULL) {
                *s = '\0';
            }
            s = strchr(dir_start, ' ');
            if (s != NULL) {
                *s = '\0';
            }

            /* Create and initialize qemu camera. */
            EmulatedQemuCamera* qemu_cam =
                new EmulatedQemuCamera(index, &HAL_MODULE_INFO_SYM.common);
            if (NULL != qemu_cam) {
                res = qemu_cam->Initialize(name_start, dim_start, dir_start);
                if (res == NO_ERROR) {
                    mEmulatedCameras[index] = qemu_cam;
                    index++;
                } else {
                    delete qemu_cam;
                }
            } else {
                CAMHAL_LOGE("%s: Unable to instantiate EmulatedQemuCamera",
                     __FUNCTION__);
            }
        } else {
            CAMHAL_LOGW("%s: Bad camera information: %s", __FUNCTION__, cur_entry);
        }

        cur_entry = next_entry;
    }

    mEmulatedCameraNum = index;
#else
    CAMHAL_LOGD("delete this function");
#endif
}

bool EmulatedCameraFactory::isFakeCameraFacingBack(int cameraId)
{
    if (cameraId%mEmulatedCameraNum == 1)
        return false;

    return true;
}

int EmulatedCameraFactory::getFakeCameraHalVersion(int cameraId __unused)
{
    /* Defined by 'qemu.sf.back_camera_hal_version' boot property: if the
     * property doesn't exist, it is assumed to be 1. */
#if 0
    char prop[PROPERTY_VALUE_MAX];
    if (property_get("qemu.sf.back_camera_hal", prop, NULL) > 0) {
        char *prop_end = prop;
        int val = strtol(prop, &prop_end, 10);
        if (*prop_end == '\0') {
            return val;
        }
        // Badly formatted property, should just be a number
        CAMHAL_LOGE("qemu.sf.back_camera_hal is not a number: %s", prop);
    }
    return 1;
#else
    return 3;
#endif
}


// only for cameras which will be lazy inited
// cameras become ready after camera provider is running
// and will not be unplugged.
void EmulatedCameraFactory::onStatusReady(char * dev_name)
{
    int i = 0;
    status_t res;
    const camera_module_callbacks_t* cb = mCallbacks;
    if (mCameraVirtualDevice->checkUsbDeviceExist(dev_name)) {
        CAMHAL_LOGD("Donot response %s StatusChanged", dev_name);
        return;
    }

    int cameraId = mEmulatedCameraNum;

    /*suppose only usb camera produce uevent, and it is facing back*/
    EmulatedFakeCamera3 *cam = new EmulatedFakeCamera3(cameraId, &HAL_MODULE_INFO_SYM.common);
    if (cam != NULL) {
        CAMHAL_LOGD("%s: new camera device version is %d", __FUNCTION__,
                getFakeCameraHalVersion(cameraId));
        //sleep 10ms for /dev/video* create
        usleep(50000);
        while (i < 20) {
            if (0 == access(dev_name, F_OK | R_OK | W_OK)) {
                CAMHAL_LOGD("access %s success\n", dev_name);
                break;
            } else {
                CAMHAL_LOGD("access %s fail , i = %d .\n", dev_name,i);
                usleep(50000);
                i++;
            }
        }
        res = cam->Initialize();
        if (res != NO_ERROR) {
            CAMHAL_LOGE("%s: Unable to initialize camera %d: %s (%d)",
                __FUNCTION__, cameraId, strerror(-res), res);
            delete cam;
            return ;
        }

        /*Open the camera. then send the callback to framework*/
        mEmulatedCameras[cameraId] = cam;
        mEmulatedCameraNum ++;
        cam->plugCamera();
        if (cb != NULL && cb->camera_device_status_change != NULL) {
            cb->camera_device_status_change(cb, cameraId, CAMERA_DEVICE_STATUS_PRESENT);
        }
    }

    CAMHAL_LOGD("mEmulatedCameraNum step1 = %d\n", mEmulatedCameraNum);
    return ;
}

void EmulatedCameraFactory::onStatusChanged(int videoId, int newStatus)
{
    Mutex::Autolock al(mMutex);
    ATRACE_CALL();
    status_t res;
    char dev_name[128];
    int j = 0;

    int cameraId = -1;
    //EmulatedBaseCamera *cam = mEmulatedCameras[cameraId];
    const camera_module_callbacks_t* cb = mCallbacks;
    sprintf(dev_name, "%s%d", "/dev/video", videoId);
    /* ignore cameraid >= MAX_USB_CAMERA_NUM to avoid overflow, we now have
     * ion device with device like /dev/video13
     */
    /*
     * exception for hdmi camera hotpug. hdmi camera uses video70
     */
    if (videoId >=  MAX_USB_CAM_VIDEO_ID && videoId != HDMI_VDIN_DEV_BEGIN_NUM)
        return;

    // no existed cameras, ignore plug-out events;
    if ((mEmulatedCameraNum == 0)  &&  (newStatus == CAMERA_DEVICE_STATUS_NOT_PRESENT)) {
        //video70 plug boot
        return;
    }

    if (mCameraVirtualDevice->checkUsbDeviceExist(dev_name)) {
        CAMHAL_LOGD("%s device name is error", dev_name);
        return;
    }

    if (newStatus == CAMERA_DEVICE_STATUS_PRESENT) {
        if (mCameraVirtualDevice->isNormalExternalCameraByName(dev_name)) {
            CAMHAL_LOGD("%s device will been plugged", dev_name);
            mCameraVirtualDevice->addUsbDevice(dev_name);
        } else {
            CAMHAL_LOGE("%s devices can't access", dev_name);
            return;
        }
    }

    cameraId = mCameraVirtualDevice->returnUsbDeviceId(dev_name);

    if (cameraId < 0) {
        CAMHAL_LOGD("Prepare StatusChanged %s, Id %d", dev_name, cameraId);
        return;
    }

    // plug-out event, reference camera obj must be valid.
    // if reference camera obj is null; ignore this event;
    if ((mEmulatedCameras[cameraId] == NULL) && (newStatus == CAMERA_DEVICE_STATUS_NOT_PRESENT)) {
        CAMHAL_LOGD("%s: not camera using video node status, newStatus: %d", __FUNCTION__, newStatus);
        return;
    }

    CAMHAL_LOGD("mEmulatedCameraNum step0 = %d\n", mEmulatedCameraNum);
    EmulatedBaseCamera *cam = mEmulatedCameras[cameraId];

    /**
     * 1、this camera has been plugged before and the status is unplugged
     */
    if (mEmulatedCameras[cameraId] != NULL && (!mEmulatedCameras[cameraId]->getHotplugStatus())) {
        if (newStatus == CAMERA_DEVICE_STATUS_PRESENT) {
            res = cam->Initialize();
            if (res != NO_ERROR) {
                ALOGE("%s: Unable to initialize camera %d: %s (%d)",
                    __FUNCTION__, cameraId, strerror(-res), res);
                delete cam;
                return ;
            }

            /* Open the camera. then send the callback to framework*/
            mEmulatedCameraNum ++;
            cam->plugCamera();
            if (cb != NULL && cb->camera_device_status_change != NULL) {
                cb->camera_device_status_change(cb, cameraId, newStatus);
            }
        } else {
            CAMHAL_LOGE("camera id %d has been unplugged", cameraId);
        }
        return;
    }

    /**
     * 2、this camera has not been plugged yet and plug it now
     */
    if ((!cam) && (newStatus == CAMERA_DEVICE_STATUS_PRESENT)) {
        /*suppose only usb camera produce uevent, and it is facing back*/
        cam = new EmulatedFakeCamera3(cameraId, &HAL_MODULE_INFO_SYM.common);
        if (cam != NULL) {
            CAMHAL_LOGD("%s: new camera device version is %d", __FUNCTION__,
                    getFakeCameraHalVersion(cameraId));
            res = cam->Initialize();
            if (res != NO_ERROR) {
                CAMHAL_LOGE("%s: Unable to initialize camera %d: %s (%d)",
                    __FUNCTION__, cameraId, strerror(-res), res);
                delete cam;
                return ;
            }

            /* Open the camera. then send the callback to framework*/
            mEmulatedCameras[cameraId] = cam;
            mEmulatedCameraNum ++;
            cam->plugCamera();
            if (cb != NULL && cb->camera_device_status_change != NULL) {
                cb->camera_device_status_change(cb, cameraId, newStatus);
            }
        }

        CAMHAL_LOGD("mEmulatedCameraNum step1 = %d\n", mEmulatedCameraNum);
        return ;
    }

    if (mEmulatedCameraNum >= 1 && mEmulatedCameras[mEmulatedCameraNum - 1] != NULL)
        CAMHAL_LOGD("mEmulatedCameraNum step2 = %d - status: %d\n", mEmulatedCameraNum, mEmulatedCameras[mEmulatedCameraNum - 1]->getCameraStatus());

    /**
     * 3、unplugged this camera
     */
    if (newStatus == CAMERA_DEVICE_STATUS_NOT_PRESENT) {
        mEmulatedCameraNum --;
        j = cameraId;
        if (mEmulatedCameras[j] != NULL) {
            mEmulatedCameras[j]->closeCamera();
            mEmulatedCameras[j]->unplugCamera();
        }
        if (cb != NULL && cb->camera_device_status_change != NULL) {
            CAMHAL_LOGD("%d callback unplug status to framework.\n", j);
            cb->camera_device_status_change(cb, j, newStatus);
        }

        CAMHAL_LOGD("%s device will been unplugged", dev_name);
        mCameraVirtualDevice->deleteUsbDevice(dev_name);
    }

    CAMHAL_LOGD("mEmulatedCameraNum step3 = %d\n", mEmulatedCameraNum);
}

/********************************************************************************
 * Initializer for the static member structure.
 *******************************************************************************/

/* Entry point for camera HAL API. */
struct hw_module_methods_t EmulatedCameraFactory::mCameraModuleMethods = {
    .open = EmulatedCameraFactory::device_open
};

}; /* namespace android */
