/*
 * Copyright 2014 The Android Open Source Project
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

#define LOG_TAG "tv_input"
#include <fcntl.h>
#include <errno.h>

#include <cutils/native_handle.h>

#include <hardware/tv_input.h>
#include "tv_input.h"
#include <tvcmd.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <amlogic/am_gralloc_ext.h>
#include <hardware/hardware.h>
#include <linux/videodev2.h>
#include <android/native_window.h>

#include <cutils/properties.h>

static const int SCREENSOURCE_GRALLOC_USAGE = (
    GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER |
    GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_NEVER);

static int capWidth;
static int capHeight;
static int supportDevices[20];
static int count = 0;

native_handle_t *pFixedTvStream = nullptr;
native_handle_t *pTvStream = nullptr;
native_handle_t *pMainTvStream = nullptr;
native_handle_t *pPipTvStream = nullptr;


void EventCallback::onTvEvent (const source_connect_t &scrConnect) {
    tv_input_private_t *priv = (tv_input_private_t *)(mPri);

    ALOGI("callback::onTvEvent msgType = %d", scrConnect.msgType);
    switch (scrConnect.msgType) {
        case SOURCE_CONNECT_CALLBACK: {
            tv_source_input_t source = (tv_source_input_t)scrConnect.source;
            int connectState = scrConnect.state;
            ALOGI("callback::onTvEvent source = %d, status = %d", source, connectState);
            if (SOURCE_AV1 <= source && source <= SOURCE_HDMI4) {
                notifyDeviceStatus(priv, source, TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED);
            }
        }
        break;

        case CHECK_SOURCE_VALID: {
            bool source_status = priv->mpTv->getSourceStatus();
            tv_source_input_t hold_source = priv->mpTv->checkHoldSource();
            if (!source_status && (SOURCE_DTVKIT == hold_source || SOURCE_DTVKIT_PIP == hold_source)) {
                priv->mpTv->stopTv(hold_source);
            }
        }
        break;

        default:
            break;
    }
}

static int channelCheckStatus(tv_input_private_t *priv, int check_status, int device_id)
{
    int ret = 0;

    if (priv->mpTv && priv->mpTv->isTvPlatform())
        ret = priv->mpTv->checkSourceStatus((tv_source_input_t) device_id, check_status);

    return ret;
}

void channelControl(tv_input_private_t *priv, bool opsStart, int device_id, int stream_id) {
    if (priv->mpTv) {
        ALOGI ("%s, device id:%d, %s.\n", __FUNCTION__, device_id, opsStart ? "startTV": "stopTV");

        if ((SOURCE_DTVKIT == device_id || SOURCE_DTVKIT_PIP == device_id) && !(priv->mpTv->isTvPlatform())) {
            priv->mpTv->setDeviceGivenId(opsStart ? device_id : -1);
            return;
        }

        if (opsStart) {
            tv_source_input_t hold_source = priv->mpTv->checkHoldSource();
            if (SOURCE_DTVKIT == hold_source || SOURCE_DTVKIT_PIP == hold_source) {
                if (SOURCE_DTVKIT == (tv_source_input_t) device_id ||
                    SOURCE_DTVKIT_PIP == (tv_source_input_t) device_id) {
                    priv->mpTv->switchSourceInput((tv_source_input_t) device_id);
                    priv->mpTv->setDeviceGivenId(device_id);
                    priv->mpTv->setStreamGivenId(stream_id);
                    priv->mpTv->setSourceStatus(true);

                    return;
                } else {
                    priv->mpTv->stopTv(hold_source);
                }
            }

            priv->mpTv->startTv((tv_source_input_t) device_id);
            priv->mpTv->switchSourceInput((tv_source_input_t) device_id);
            priv->mpTv->setDeviceGivenId(device_id);
            priv->mpTv->setStreamGivenId(stream_id);
        } else {
            tv_source_input_t wait_source = priv->mpTv->checkWaitSource(true);

            /* Force the current source to stop when the current source blocks the start of other sources,
             * and the close action of the blocked source is also triggered.
             */
            if (priv->mpTv->getCurrentSourceInput() != device_id) {
                if (priv->mpTv->getSourceStatus() && device_id == wait_source) {
                    priv->mpTv->stopTv((tv_source_input_t) priv->mpTv->getCurrentSourceInput());
                    priv->mpTv->setDeviceGivenId(-1);
                    priv->mpTv->setStreamGivenId(-1);
                }

                return;
            }

            /* DTVKit is actually stopped only when a new source is entered */
            if (wait_source == SOURCE_INVALID &&
                (SOURCE_DTVKIT == (tv_source_input_t) device_id ||
                SOURCE_DTVKIT_PIP == (tv_source_input_t) device_id)) {
                priv->mpTv->setDeviceGivenId(-1);
                priv->mpTv->setStreamGivenId(-1);
                priv->mpTv->setSourceStatus(false);

                return;
            }

            char buf[PROPERTY_VALUE_MAX] = { 0 };
            int ret = property_get("tv.need.tvview.fast_switch", buf, NULL);
            if (ret <= 0 || strcmp(buf, "true") != 0) {
                priv->mpTv->stopTv((tv_source_input_t) device_id);
                priv->mpTv->setDeviceGivenId(-1);
                priv->mpTv->setStreamGivenId(-1);
            }

            if (wait_source != SOURCE_INVALID) {
                priv->mpTv->startTv(wait_source);
                priv->mpTv->switchSourceInput(wait_source);
                priv->mpTv->setDeviceGivenId(wait_source);
                priv->mpTv->setStreamGivenId(stream_id);
            }
        }
    }
}

int notifyDeviceStatus(tv_input_private_t *priv, tv_source_input_t inputSrc, int type)
{
    tv_input_event_t event;
    const char address[64] = {0};
    event.device_info.device_id = inputSrc;
    event.device_info.audio_type = AUDIO_DEVICE_NONE;
    event.device_info.audio_address = address;
    event.type = type;
    switch (inputSrc) {
        case SOURCE_TV:
        case SOURCE_DTV:
        case SOURCE_ADTV:
        case SOURCE_DTVKIT:
        case SOURCE_DTVKIT_PIP:
            event.device_info.type = TV_INPUT_TYPE_TUNER;
            event.device_info.audio_type = AUDIO_DEVICE_IN_TV_TUNER;
            break;
        case SOURCE_AV1:
        case SOURCE_AV2:
            event.device_info.type = TV_INPUT_TYPE_COMPOSITE;
            event.device_info.audio_type = AUDIO_DEVICE_IN_LINE;
            break;
        case SOURCE_HDMI1:
        case SOURCE_HDMI2:
        case SOURCE_HDMI3:
        case SOURCE_HDMI4:
            event.device_info.type = TV_INPUT_TYPE_HDMI;
            event.device_info.hdmi.port_id = inputSrc - SOURCE_YPBPR2;
            event.device_info.audio_type = AUDIO_DEVICE_IN_HDMI;
            break;
        case SOURCE_SPDIF:
            event.device_info.type = TV_INPUT_TYPE_OTHER_HARDWARE;
            event.device_info.audio_type = AUDIO_DEVICE_IN_SPDIF;
            break;
        case SOURCE_AUX:
            event.device_info.type = TV_INPUT_TYPE_OTHER_HARDWARE;
            event.device_info.audio_type = AUDIO_DEVICE_IN_LINE;
            break;
        case SOURCE_ARC:
            event.device_info.type = TV_INPUT_TYPE_OTHER_HARDWARE;
            event.device_info.audio_type = AUDIO_DEVICE_IN_HDMI_ARC;
            break;
        default:
            break;
    }
    priv->callback->notify(&priv->device, &event, priv->callback_data);
    return 0;
}


static bool checkDeviceID(int device_id) {
    for (int i = 0; i< count; i++) {
        if (device_id == supportDevices[i]) {
           ALOGD("checkDeviceID supportDevices[%d] = %d\n", i, supportDevices[i]);
           return true;
        }
    }
    ALOGD("checkDeviceID device_id = %d fail.\n", device_id);
    return false;
}

static bool checkStreamID(int stream_id) {
    if (stream_id >= STREAM_ID_NORMAL && stream_id <= STREAM_ID_FRAME_CAPTURE)
        return true;
    else
        return false;
}

static int notifyCaptureSucceeded(tv_input_private_t *priv, int device_id, int stream_id, uint32_t seq)
{
    tv_input_event_t event;
    event.type = TV_INPUT_EVENT_CAPTURE_SUCCEEDED;
    event.capture_result.device_id = device_id;
    event.capture_result.stream_id = stream_id;
    event.capture_result.seq = seq;
    priv->callback->notify(&priv->device, &event, priv->callback_data);
    return 0;
}

static int notifyCaptureFail(tv_input_private_t *priv, int device_id, int stream_id, uint32_t seq)
{
    tv_input_event_t event;
    event.type = TV_INPUT_EVENT_CAPTURE_FAILED;
    event.capture_result.device_id = device_id;
    event.capture_result.stream_id = stream_id;
    event.capture_result.seq = seq;
    priv->callback->notify(&priv->device, &event, priv->callback_data);
    return 0;
}

static bool getAvailableStreamConfigs(int dev_id __unused, int *num_configurations, const tv_stream_config_t **configs)
{
    static tv_stream_config_t mconfig[2];
    switch (tv_source_input_t(dev_id)) {
        case SOURCE_ADTV:
        case SOURCE_DTVKIT:
            mconfig[0].stream_id = STREAM_ID_NORMAL;
            mconfig[0].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ;
            mconfig[0].max_video_width = 1920;
            mconfig[0].max_video_height = 1080;
            mconfig[1].stream_id = STREAM_ID_MAIN;
            mconfig[1].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ;
            mconfig[1].max_video_width = 1920;
            mconfig[1].max_video_height = 1080;
            break;
        case SOURCE_DTVKIT_PIP:
            mconfig[0].stream_id = STREAM_ID_PIP;
            mconfig[0].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ;
            mconfig[0].max_video_width = 320;
            mconfig[0].max_video_height = 240;
            mconfig[1].stream_id = STREAM_ID_FRAME_CAPTURE;
            mconfig[1].type = TV_STREAM_TYPE_BUFFER_PRODUCER ;
            mconfig[1].max_video_width = 1920;
            mconfig[1].max_video_height = 1080;
            break;
        default:
            mconfig[0].stream_id = STREAM_ID_NORMAL;
            mconfig[0].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ;
            mconfig[0].max_video_width = 1920;
            mconfig[0].max_video_height = 1080;
            mconfig[1].stream_id = STREAM_ID_MAIN;
            mconfig[1].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ;
            mconfig[1].max_video_width = 1920;
            mconfig[1].max_video_height = 1080;
            break;
    }
    *num_configurations = 2;
    *configs = mconfig;
    return true;
}

static int getUnavailableStreamConfigs(int dev_id __unused, int *num_configurations, const tv_stream_config_t **configs)
{
    *num_configurations = 0;
    *configs = NULL;
    return 0;
}

static int getTvStream(tv_input_private_t *priv, tv_stream_t *stream, int input_id)
{
    int fixed_tunnel = -1;
    char value[PROPERTY_VALUE_MAX] = { 0 };
    int tunnelId = -1;

    if (property_get("vendor.tv.fixed_tunnel", value, NULL) > 0) {
        fixed_tunnel = atoi(value);
    }
    ALOGD("fixed_tunnel =%d", fixed_tunnel);
    ALOGD("getTvStream stream_id = %d", stream->stream_id);
    if (!fixed_tunnel) {
        if (pFixedTvStream == nullptr) {
            pFixedTvStream = am_gralloc_create_sideband_handle(AM_TV_SIDEBAND, 1);
        }
        if (pFixedTvStream == nullptr) {
            ALOGE("tvstream can not be initialized");
            return -EINVAL;
        }
        stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        stream->sideband_stream_source_handle = pFixedTvStream;
    } else {
        if (stream->stream_id == STREAM_ID_NORMAL) {
            if (pTvStream == nullptr) {
                if ((SOURCE_DTVKIT == input_id) || (SOURCE_ADTV == input_id)) {
                    if (priv->mpTv->isMultiDemux()) {
                        pTvStream = am_gralloc_create_sideband_handle(AM_FIXED_TUNNEL, 1);
                        tunnelId = 1;
                    } else {
                        pTvStream = am_gralloc_create_sideband_handle(AM_TV_SIDEBAND, 1);
                    }
                } else {
                    if (priv->mpTv->isMultiDemux()) {
                        pTvStream = am_gralloc_create_sideband_handle(AM_FIXED_TUNNEL, 0);
                        tunnelId = 0;
                    }
                    else
                        pTvStream = am_gralloc_create_sideband_handle(AM_TV_SIDEBAND, 1);
                }
                if (pTvStream == nullptr) {
                    ALOGE("tvstream can not be initialized");
                    return -EINVAL;
                }
            } else if (priv->mpTv->isMultiDemux()) {
                if ((SOURCE_DTVKIT == input_id) || (SOURCE_ADTV == input_id)) {
                    tunnelId = 1;
                } else {
                    tunnelId = 0;
                }
                ALOGD("STREAM_ID_NORMAL set tunnel id = %d", tunnelId);
            }
            stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
            stream->sideband_stream_source_handle = pTvStream;
        } else if (stream->stream_id == STREAM_ID_MAIN) {
            //add such for pip function
            if (pMainTvStream == nullptr) {
                ALOGD("getTvStream stream_id=%d tunnelId=%d", stream->stream_id, 1);
                if (priv->mpTv->isMultiDemux() || fixed_tunnel == 1) {
                    pMainTvStream = am_gralloc_create_sideband_handle(AM_FIXED_TUNNEL, 0);
                    tunnelId = 0;
                } else {
                    pMainTvStream = am_gralloc_create_sideband_handle(AM_TV_SIDEBAND, 1);
                }
                if (pMainTvStream == nullptr) {
                    ALOGE("tvstream can not be initialized");
                    return -EINVAL;
                }
            } else if (priv->mpTv->isMultiDemux() || fixed_tunnel == 1) {
                tunnelId = 0;
                ALOGD("STREAM_ID_MAIN set tunnel id = %d", tunnelId);
            }
            stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
            stream->sideband_stream_source_handle = pMainTvStream;
        } else if (stream->stream_id == STREAM_ID_PIP) {
            //add such for pip function
            if (pPipTvStream == nullptr) {
                ALOGD("getTvStream stream_id=%d tunnelId=%d", stream->stream_id, 2);
                pPipTvStream = am_gralloc_create_sideband_handle(AM_FIXED_TUNNEL, 2);
                tunnelId = 2;
            if (pPipTvStream == nullptr) {
                    ALOGE("pip tvstream can not be initialized");
                    return -EINVAL;
                }
            }
            stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
            stream->sideband_stream_source_handle = pPipTvStream;
        } else if (stream->stream_id == STREAM_ID_FRAME_CAPTURE) {
            stream->type = TV_STREAM_TYPE_BUFFER_PRODUCER;
        }
    }
    ALOGD("new stream tunnel id: %d",tunnelId);
    if (tunnelId != -1) {
        priv->mpTv->setStreamTunnelId(tunnelId);
    }
    return 0;
}

void initTvDevices(tv_input_private_t *priv)
{
    priv->mpTv->init();
    priv->mpTv->getSupportInputDevices(supportDevices, &count);

    if (count == 0) {
        ALOGE("tv.source.input.ids.default is not set.");
        return;
    }

    bool isHotplugDetectOn = priv->mpTv->getHdmiAvHotplugDetectOnoff();
    ALOGI("hdmi/av hotplug detect on: %s", isHotplugDetectOn?"YES":"NO");
    if (isHotplugDetectOn)
        priv->mpTv->setTvObserver(priv->eventCallback);

    for (int i = 0; i < count; i++) {
        tv_source_input_t inputSrc = (tv_source_input_t)supportDevices[i];
        notifyDeviceStatus(priv, inputSrc, TV_INPUT_EVENT_DEVICE_AVAILABLE);
    }
}

static int tv_input_initialize(struct tv_input_device *dev,
                               const tv_input_callback_ops_t *callback, void *data)
{
    if (dev == NULL || callback == NULL) {
        return -EINVAL;
    }
    tv_input_private_t *priv = (tv_input_private_t *)dev;
    /*if (priv->callback != NULL) {
        ALOGE("tv input had been init done, do not need init again");
        return -EEXIST;
    }*/
    priv->callback = callback;
    priv->callback_data = data;

    initTvDevices(priv);

    ALOGD("%s", __FUNCTION__);

    return 0;
}

static int tv_input_get_stream_configurations(const struct tv_input_device *dev __unused,
        int device_id, int *num_configurations,
        const tv_stream_config_t **configs)
{
    tv_input_private_t *priv = (tv_input_private_t *)dev;

    if (!checkDeviceID(device_id))
        return -EINVAL;

    int isHotplugDetectOn = priv->mpTv->getHdmiAvHotplugDetectOnoff();
    if (isHotplugDetectOn == 0) {
        LOGD("%s:Hot plug disabled!\n", __FUNCTION__);
        getAvailableStreamConfigs(device_id, num_configurations, configs);
    } else {
        LOGD("%s:Hot plug enabled!\n", __FUNCTION__);
        bool status = true;
        if (SOURCE_AV1 <= device_id && device_id <= SOURCE_HDMI4) {
            status = priv->mpTv->getSourceConnectStatus((tv_source_input_t)device_id);
        }
        LOGD("tv_input_get_stream_configurations  source = %d, status = %d", device_id, status);
        if (status) {
            getAvailableStreamConfigs(device_id, num_configurations, configs);
        } else {
            getUnavailableStreamConfigs(device_id, num_configurations, configs);
        }
    }

    return 0;
}


static int tv_input_open_stream(struct tv_input_device *dev, int device_id,
                                tv_stream_t *stream)
{
    tv_input_private_t *priv = (tv_input_private_t *)dev;

    if (!priv || !stream)
        return -EINVAL;

    ALOGD("open_stream: device_id = %d, streamid = %d, mStreamGivenId = %d, mDeviceGivenId = %d\n",
            device_id, stream->stream_id, priv->mpTv->getStreamGivenId(), priv->mpTv->getDeviceGivenId());

    if (!checkDeviceID(device_id) || !checkStreamID(stream->stream_id))
        return -EINVAL;

    if ((stream->stream_id == STREAM_ID_MAIN || stream->stream_id == STREAM_ID_PIP) || stream->stream_id != priv->mpTv->getStreamGivenId() ||
            device_id != priv->mpTv->getDeviceGivenId())
        priv->mpTv->setStreamGivenId(stream->stream_id);
    else {
        ALOGD("stream has been opened");
        return -EEXIST;
    }

    if (getTvStream(priv, stream, device_id) != 0) {
        return -EINVAL;
    }

    if (SOURCE_TV <= device_id && device_id < SOURCE_ADTV) {
        priv->mpTv->writeSurfaceTypetoVpp(TVIN_SOURCE_TYPE_VDIN);
    } else if (device_id == SOURCE_DTVKIT) {
        priv->mpTv->writeSurfaceTypetoVpp(TVIN_SOURCE_TYPE_DECODER);
    } else {
        priv->mpTv->writeSurfaceTypetoVpp(TVIN_SOURCE_TYPE_OTHERS);
    }

    if (stream->stream_id == STREAM_ID_NORMAL || stream->stream_id == STREAM_ID_MAIN || stream->stream_id == STREAM_ID_PIP) {
        if (!channelCheckStatus(priv, 0, device_id))
            channelControl(priv, true, device_id, stream->stream_id);
    } else if (stream->stream_id == STREAM_ID_FRAME_CAPTURE) {
        ALOGE("tv_input_open_stream STREAM_ID_FRAME_CAPTURE is not supported");
        /*
        aml_screen_module_t* screenModule;
        if (hw_get_module(AML_SCREEN_HARDWARE_MODULE_ID, (const hw_module_t **)&screenModule) < 0) {
            ALOGE("can not get screen source module");
        } else {
            screenModule->common.methods->open((const hw_module_t *)screenModule,
                AML_SCREEN_SOURCE, (struct hw_device_t**)&(priv->mDev));
            //do test here, we can use ops of mDev to operate vdin source
        }

        if (priv->mDev) {
            if (capWidth == 0 || capHeight == 0) {
                capWidth = stream->buffer_producer.width;
                capHeight = stream->buffer_producer.height;
            }
            priv->mDev->ops.set_format(priv->mDev, capWidth, capHeight, V4L2_PIX_FMT_NV21);
            priv->mDev->ops.set_port_type(priv->mDev, (int)0x4000); //TVIN_PORT_HDMI0 = 0x4000
            priv->mDev->ops.start_v4l2_device(priv->mDev);
        }
        */
    }

    return 0;
}

static int tv_input_close_stream(struct tv_input_device *dev, int device_id,
                                 int stream_id)
{
    tv_input_private_t *priv = (tv_input_private_t *)dev;

    if (!priv)
        return -EINVAL;

    ALOGD("close_stream: device_id = %d, stream_id = %d, mStreamGivenId = %d, mDeviceGivenId = %d\n",
            device_id, stream_id, priv->mpTv->getStreamGivenId(), priv->mpTv->getDeviceGivenId());

    if (!checkDeviceID(device_id) || !checkStreamID(stream_id))
        return -EINVAL;

    if ((stream_id == STREAM_ID_MAIN || stream_id == STREAM_ID_PIP) || priv->mpTv->getStreamGivenId() == stream_id)
        priv->mpTv->setStreamGivenId(-1);
    else {
        ALOGD("stream doesn't open");
        return -ENOENT;
    }

    priv->mpTv->writeSurfaceTypetoVpp(TVIN_SOURCE_TYPE_OTHERS);

    if (stream_id == STREAM_ID_NORMAL || stream_id == STREAM_ID_MAIN || stream_id == STREAM_ID_PIP) {
        if (!channelCheckStatus(priv, 1, device_id))
            channelControl(priv, false, device_id, stream_id);
        return 0;
    } else if (stream_id == STREAM_ID_FRAME_CAPTURE) {
        ALOGD("tv_input_close_stream STREAM_ID_FRAME_CAPTURE is not supported");
        /*
        if (priv->mDev) {
            priv->mDev->ops.stop_v4l2_device(priv->mDev);
        }*/
        return 0;
    }
    return -EINVAL;
}

static int tv_input_request_capture(
    struct tv_input_device *dev, int device_id,
    int stream_id, buffer_handle_t buffer, uint32_t seq)
{
ALOGE("tv_input_request_capture dev:%p, device_id:%x, stream_id:%x, buffer:%p, seq:%x",
        dev, device_id, stream_id, buffer, seq);

/*
    tv_input_private_t *priv = (tv_input_private_t *)dev;
    unsigned char *dest = NULL;
    if (priv->mDev) {
        aml_screen_buffer_info_t buffInfo = { NULL, 0 ,0 ,0 ,0};
        int ret = priv->mDev->ops.acquire_buffer(priv->mDev, &buffInfo);
        if (ret != 0 || (buffInfo.buffer_mem == nullptr)) {
            ALOGE("Get V4l2 buffer failed");
            notifyCaptureFail(priv,device_id,stream_id,--seq);
            return -EWOULDBLOCK;
        }
        long *src = (long *)buffInfo.buffer_mem;

        ANativeWindowBuffer *buf = container_of(buffer, ANativeWindowBuffer, handle);
        sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf->handle, GraphicBuffer::WRAP_HANDLE,
                buf->width, buf->height,
                buf->format, buf->layerCount,
                buf->usage, buf->stride));
        graphicBuffer->lock(SCREENSOURCE_GRALLOC_USAGE, (void **)&dest);
        if (dest == NULL) {
            ALOGE("Invalid Gralloc Handle");
            return -EWOULDBLOCK;
        }
        memcpy(dest, src, capWidth*capHeight);
        graphicBuffer->unlock();
        graphicBuffer.clear();
        priv->mDev->ops.release_buffer(priv->mDev, src);

        notifyCaptureSucceeded(priv, device_id, stream_id, seq);
        return 0;
    }
    return -EWOULDBLOCK;
*/
    return 0;
}

static int tv_input_cancel_capture(struct tv_input_device *, int, int, uint32_t)
{
    return -EINVAL;
}
/*
static int tv_input_set_capturesurface_size(struct tv_input_device *dev __unused, int width, int height)
{
    if (width == 0 || height == 0) {
        return -EINVAL;
    } else {
        capWidth = width;
        capHeight = height;
        return 1;
    }
}
*/
static int tv_input_device_close(struct hw_device_t *dev)
{
    tv_input_private_t *priv = (tv_input_private_t *)dev;
    if (priv) {
        if (priv->mpTv) {
            delete priv->mpTv;
            priv->mpTv = nullptr;
        }

        /*
        if (priv->mDev) {
            delete priv->mDev;
            priv->mDev = nullptr;
        }*/

        if (priv->eventCallback) {
            delete priv->eventCallback;
            priv->eventCallback = nullptr;
        }
        free(priv);
        if (pFixedTvStream) {
            native_handle_delete((native_handle_t*)pFixedTvStream);
        }
        if (pTvStream) {
            native_handle_delete((native_handle_t*)pTvStream);
        }
        if (pMainTvStream) {
            native_handle_delete((native_handle_t*)pMainTvStream);
        }
        if (pPipTvStream) {
            native_handle_delete((native_handle_t*)pPipTvStream);
        }
    }

    ALOGD("%s", __FUNCTION__);

    return 0;
}

int tv_input_device_open(const struct hw_module_t *module,
                                const char *name, struct hw_device_t **device)
{
    int status = -EINVAL;
    if (!strcmp(name, TV_INPUT_DEFAULT_DEVICE)) {
        tv_input_private_t *dev = (tv_input_private_t *)malloc(sizeof(*dev));
        if (!dev)
            return status;

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));
        dev->mpTv = new TvInputIntf();
        dev->eventCallback = new EventCallback(dev);
        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = TV_INPUT_DEVICE_API_VERSION_0_1;
        dev->device.common.module = const_cast<hw_module_t *>(module);
        dev->device.common.close = tv_input_device_close;

        dev->device.initialize = tv_input_initialize;
        dev->device.get_stream_configurations =
            tv_input_get_stream_configurations;
        dev->device.open_stream = tv_input_open_stream;
        dev->device.close_stream = tv_input_close_stream;
        dev->device.request_capture = tv_input_request_capture;
        dev->device.cancel_capture = tv_input_cancel_capture;
        //dev->device.set_capturesurface_size = tv_input_set_capturesurface_size;

        *device = &dev->device.common;
        status = 0;
    }
    return status;
}

static struct hw_module_methods_t tv_input_module_methods = {
    .open = tv_input_device_open
};

/*
 * The tv input Module
 */
tv_input_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .version_major      = 0,
        .version_minor      = 1,
        .id                 = TV_INPUT_HARDWARE_MODULE_ID,
        .name               = "Amlogic tv input Module",
        .author             = "Amlogic Corp.",
        .methods            = &tv_input_module_methods,
    }
};
