#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "V4l2MediaSensor"

#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "../EmulatedFakeCamera3.h"
#include "Sensor.h"
#include "V4l2MediaSensor.h"
#include "CaptureUseMemcpy.h"
#include "CaptureUseGe2d.h"

#if ANDROID_PLATFORM_SDK_VERSION >= 24
#if ANDROID_PLATFORM_SDK_VERSION >= 29
#include <gralloc1.h>
#else
#include <gralloc_usage_ext.h>
#endif
#endif

#if ANDROID_PLATFORM_SDK_VERSION >= 28
#include <amlogic/am_gralloc_ext.h>
#endif

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/mediaApi.h"

#include "ispMgr/staticPipe.h"

namespace android {

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

const usb_frmsize_discrete_t kUsbAvailablePictureSize[] = {
        {1920, 1080},
        {1280, 720},
        {640,  480},
};

static int fakeEnumFrameSize( struct v4l2_frmsizeenum * frmsizeenum)
{
    if (frmsizeenum->index == 0) {
        frmsizeenum->type = V4L2_FRMSIZE_TYPE_DISCRETE;
        frmsizeenum->discrete.width = 1920;
        frmsizeenum->discrete.height = 1080;
        return 0;
    }
    return -1;
}

V4l2MediaSensor::V4l2MediaSensor() {
    mCameraVirtualDevice = nullptr;
    mVinfo = NULL;
    mCapture = NULL;

#ifdef GE2D_ENABLE
    mION = IONInterface::get_instance();
    mGE2D = new ge2dTransform();
#endif

#ifdef GDC_ENABLE
    mIsGdcInit = false;
#endif
    memset(&mStreamconfig, 0, sizeof(stream_configuration_t));
    ALOGD("construct V4l2MediaSensor");
}

V4l2MediaSensor::~V4l2MediaSensor() {
    ALOGD("destruct V4l2MediaSensor");
    if (mCapture) {
        delete(mCapture);
        mCapture = NULL;
    }
    if (mVinfo) {
        delete(mVinfo);
        mVinfo = NULL;
    }

#ifdef GDC_ENABLE
    if (mIGdc) {
        if (mIsGdcInit) {
            mIGdc->gdc_exit();
            mIsGdcInit = false;
        }
        delete mIGdc;
        mIGdc = NULL;
    }
#endif
#ifdef GE2D_ENABLE
    if (mION) {
        mION->put_instance();
    }
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#endif

}

status_t V4l2MediaSensor::streamOff(void) {
    ALOGV("%s: E", __FUNCTION__);
#ifdef GDC_ENABLE
    if (mIGdc && mIsGdcInit) {
        mIGdc->gdc_exit();
        mIsGdcInit = false;
    }
#endif
    if (mIspMgr)
        mIspMgr->stop();
    return mVinfo->stop_capturing();
}

int V4l2MediaSensor::SensorInit(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    int ret = 0;

    ret = camera_open(idx);
    if (ret < 0) {
        ALOGE("Unable to open sensor %d, ALOGEno=%d\n", mVinfo->idx, ret);
        return -1;
    }
    // =========== vinfo init ============
    if (mVinfo == NULL) {
        mVinfo =  new MIPIVideoInfo();
        mVinfo->idx = idx;
    }

    // ======== pipe match & stream init ==============
    struct media_device * media_dev = media_device_new_with_fd(mMediaDevicefd);
    if (media_dev == NULL) {
        ALOGE("new media device failed \n");
        return -1;
    }
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.media.isp.enable", property, "true");
    if (strstr(property, "true")) {
        mIspMgr = new IspMgr(idx);
        ALOGD("an ispDev found");
    } else {
        ALOGD("not an ispDev");
    }
    mMediaStream = malloc( sizeof( struct media_stream) );
    if (mMediaStream == NULL) {
        ALOGE("alloc media stream mem fail\n");
        return -1;
    }
    if (0 != mediaStreamInit((media_stream_t *)mMediaStream, media_dev) ) {
        ALOGE("media stream init failed\n");
        return -1;
    }

    property_get("vendor.media.camera.dual", property, "false");
    if (strstr(property,"true")) {
        media_set_wdrMode((media_stream_t *)mMediaStream, ISP_SDR_DCAM_MODE);
    }

    if (mVinfo) {
        media_stream_t* stream = (media_stream_t*)mMediaStream;
        mVinfo->fd = stream->video_ent0->fd;
    }
    mVinfo->camera_init();

    if (!mCapture) {
#ifdef GE2D_ENABLE
        mCapture = new CaptureUseGe2d(mVinfo);
#else
        mCapture = new CaptureUseMemcpy(mVinfo);
#endif
    }
    //----set buffer number using to get image from video device
    setIOBufferNum();
    //----set camera type
    mSensorType = SENSOR_V4L2MEDIA;
    staticPipe::fetchPipeMaxResolution((media_stream_t*) mMediaStream, mMaxWidth, mMaxHeight);
    ALOGI("max width %d, max height %d", mMaxWidth, mMaxHeight);
    return ret;
}

status_t V4l2MediaSensor::startUp(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    int res;
    mCapturedBuffers = NULL;
    res = run("EmulatedFakeCamera3::Sensor",ANDROID_PRIORITY_URGENT_DISPLAY);

    if (res != OK) {
       ALOGE("Unable to start up sensor capture thread: %d", res);
    }
    res = SensorInit(idx);
#ifdef GDC_ENABLE
    if (!mIGdc)
        mIGdc = new gdcUseFd();
        //mIGdc = new gdcUseMemcpy();
#endif
    return res;
}

int V4l2MediaSensor::camera_open(int idx) {
    int ret = 0;

    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    mMediaDevicefd = mCameraVirtualDevice->openVirtualDevice(idx);
    if (mMediaDevicefd < 0) {
        ret = -ENOTTY;
    }

    return ret;
}

void V4l2MediaSensor::camera_close(void) {
    ALOGV("%s: E", __FUNCTION__);
    if (mMediaDevicefd < 0)
        return;
    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    if (mVinfo)
        mCameraVirtualDevice->releaseVirtualDevice(mVinfo->idx,  mMediaDevicefd);
    mMediaDevicefd = -1;
}


status_t V4l2MediaSensor::shutDown() {
    ALOGV("%s: E", __FUNCTION__);
    int res;
    mTimeOutCount = 0;
    res = requestExitAndWait();
    if (res != OK) {
        ALOGE("Unable to shut down sensor capture thread: %d", res);
    }
    if (mVinfo != NULL)
        mVinfo->stop_capturing();

    if (mIspMgr) {
        mIspMgr->stop();
        mIspMgr.clear();
    }

    if (mMediaStream) {
        struct media_stream * stream = (struct media_stream *)mMediaStream;
        if ( stream->media_dev ) {
            media_device_unref( stream->media_dev );
        }
        free(mMediaStream);
    }

    camera_close();

    if (mImage_buffer) {
        delete [] mImage_buffer;
        mImage_buffer = NULL;
    }
#ifdef GDC_ENABLE
    if (mIGdc) {
        if (mIsGdcInit) {
            mIGdc->gdc_exit();
            mIsGdcInit = false;
        }
        delete mIGdc;
        mIGdc = NULL;
    }
#endif
    mSensorWorkFlag = false;
    ALOGD("%s: Exit", __FUNCTION__);
    return res;
}

uint32_t V4l2MediaSensor::getStreamUsage(int stream_type){
    ATRACE_CALL();
    uint32_t usage = Sensor::getStreamUsage(stream_type);
    usage = (GRALLOC_USAGE_HW_TEXTURE
            | GRALLOC_USAGE_HW_RENDER
            | GRALLOC_USAGE_SW_READ_MASK
            | GRALLOC_USAGE_SW_WRITE_MASK
            );
#if 0
#if ANDROID_PLATFORM_SDK_VERSION >= 28
        usage = am_gralloc_get_omx_osd_producer_usage();
#else
        usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_AML_DMA_BUFFER;
#endif
#endif
    usage = GRALLOC1_PRODUCER_USAGE_CAMERA | usage;
    ALOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;
}

void V4l2MediaSensor::captureRGB(uint8_t *img, uint32_t gain, uint32_t stride) {
    ALOGE("capture RGB not supported");
}


void V4l2MediaSensor::captureNV21(StreamBuffer b, uint32_t gain){
    ATRACE_CALL();
    //ALOGVV("MIPI NV21 sensor image captured");
    // todo: captureNewImage with >=2 out bufs with different pixel fmt.
    //       simply using mKernelBuffer & mTempFd cause green image
    //       due to pixel format difference.
    struct data_in in;
    in.src = mKernelBuffer;
    in.src_fmt = mKernelBufferFmt;
    in.share_fd = mTempFD;
    ALOGVV("%s:mTempFD = %d",__FUNCTION__,mTempFD);
    while (1) {
        if (mExitSensorThread) {
            break;
        }
        //----get one frame
        int ret = mCapture->captureNV21frame(b,&in);
        if (ret == ERROR_FRAME)
            continue;
#ifdef GE2D_ENABLE
        //----do rotation
        mGE2D->doRotationAndMirror(b);
#endif

#ifdef GDC_ENABLE
        //----do fisheye corrected
        struct param p;
        p.img = b.img;
        //----get kernel dmabuf fd
        p.input_fd = in.dmabuf_fd;
        //----set output buffer fd
        p.output_fd = b.share_fd;
        mIGdc->gdc_do_fisheye_correction(&p);
#endif
        mKernelBuffer = b.img;
        mKernelBufferFmt = V4L2_PIX_FMT_NV21;
        mTempFD = b.share_fd;
        mSensorWorkFlag = true;
        if (ret == NEW_FRAME)
            mVinfo->putback_frame();
        if (mFlushFlag) {
            break;
        }
        break;
    }
}

void V4l2MediaSensor::captureYV12(StreamBuffer b, uint32_t gain) {
    ALOGE("capture YV12, not supported yet!");
}


void V4l2MediaSensor::captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride) {
    ALOGE("capture YUYV, not supported yet!");
}


void V4l2MediaSensor::setIOBufferNum()
{
    char buffer_number[128];
    int tmp = 6;
    if (property_get("ro.vendor.mipicamera.iobuffer", buffer_number, NULL) > 0) {
        sscanf(buffer_number, "%d", &tmp);
        ALOGD(" get buffer number is %d from property \n",tmp);
    }

    ALOGD("default buffer number is %d\n",tmp);
    mVinfo->set_buffer_numbers(tmp);
}

status_t V4l2MediaSensor::getOutputFormat(void) {

    // todo: output format from sensor subdev
    if (!mIspMgr)
        return V4L2_PIX_FMT_UYVY;
    else
        return V4L2_PIX_FMT_NV21;
}

status_t V4l2MediaSensor::setOutputFormat(int width, int height, int pixelformat, bool isjpeg) {
    ALOGV("%s: E", __FUNCTION__);
    mFramecount = 0;
    mCurFps = 0;
    gettimeofday(&mTimeStart, NULL);

    if (isjpeg) {
        //----set snap shot pixel format
        mVinfo->picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVinfo->picture.format.fmt.pix.width = width;
        mVinfo->picture.format.fmt.pix.height = height;
        mVinfo->picture.format.fmt.pix.pixelformat = pixelformat;
    } else {
        //----set preview pixel format
        mVinfo->preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVinfo->preview.format.fmt.pix.width = width;
        mVinfo->preview.format.fmt.pix.height = height;
        mVinfo->preview.format.fmt.pix.pixelformat = pixelformat;
        mStreamconfig.vformat[channel_preview].width  = width;
        mStreamconfig.vformat[channel_preview].height = height;
        mStreamconfig.vformat[channel_preview].fourcc = pixelformat;
        /* config & set format */
        if (mIspMgr) {
            mStreamconfig.format.width  = mMaxWidth;
            mStreamconfig.format.height = mMaxHeight;
            mStreamconfig.format.fourcc = pixelformat;
            mStreamconfig.format.code   = MEDIA_BUS_FMT_SRGGB12_1X12;
        } else {
            mStreamconfig.format.width  = width;
            mStreamconfig.format.height = height;
            mStreamconfig.format.fourcc = pixelformat;
            mStreamconfig.format.code   = MEDIA_BUS_FMT_UYVY8_2X8;
        }
        int rc = mediaStreamConfig((media_stream_t*) mMediaStream, &mStreamconfig);
        if (rc < 0) {
            ALOGE("config stream failed\n");
            return rc;
        }
#ifdef GDC_ENABLE
        if (mIGdc && !mIsGdcInit) {
            mIGdc->gdc_init(width,height,NV12,1);
            mIsGdcInit = true;
        }
#endif
    }
    return OK;
}

int V4l2MediaSensor::halFormatToSensorFormat(uint32_t pixelfmt) {
    return getOutputFormat();
}

status_t V4l2MediaSensor::streamOn() {
    ALOGV("%s: E", __FUNCTION__);
    int rc;
    if (mIspMgr) {
        rc = mIspMgr->configure((media_stream_t *)mMediaStream);
        rc = mIspMgr->start();
    }
    rc = mVinfo->start_capturing();
    ALOGV("%s: X", __FUNCTION__);
    return rc;
}

bool V4l2MediaSensor::isStreaming() {
    return mVinfo->isStreaming;
}

bool V4l2MediaSensor::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat) {
    if ((mVinfo->preview.format.fmt.pix.width != width)
        ||(mVinfo->preview.format.fmt.pix.height != height)) {
        return true;
    }
    return false;
}


int V4l2MediaSensor::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) {
    const uint32_t length = ARRAY_SIZE(kUsbAvailablePictureSize);
    uint32_t count = 0, size_start = 0;
    char property[PROPERTY_VALUE_MAX];
    int fullsize_preview = FALSE;
    property_get("vendor.amlogic.camera.fullsize.preview", property, "true");
    if (strstr(property, "true")) {
        fullsize_preview = TRUE;
    }
    if (fullsize_preview == FALSE ) size_start = 1;
    struct v4l2_frmsizeenum frmsizeMax;
    memset(&frmsizeMax, 0, sizeof(frmsizeMax));
    frmsizeMax.pixel_format = getOutputFormat();
    staticPipe::fetchPipeMaxResolution(
        (media_stream_t*) mMediaStream, frmsizeMax.discrete.width, frmsizeMax.discrete.height);

    DBG_LOGB("get max output width=%d, height=%d, format=%d\n",
        frmsizeMax.discrete.width, frmsizeMax.discrete.height, frmsizeMax.pixel_format);
    for (uint32_t i = size_start; i < length; i++) {//preview
        if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
            kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
            continue;
        picSizes[count++] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
        picSizes[count++] = kUsbAvailablePictureSize[i].width;
        picSizes[count++] = kUsbAvailablePictureSize[i].height;
        picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    }
    for (uint32_t i = size_start; i < length; i++) { //preview
        if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
            kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
            continue;
        picSizes[count++] = HAL_PIXEL_FORMAT_YCbCr_420_888;
        picSizes[count++] = kUsbAvailablePictureSize[i].width;
        picSizes[count++] = kUsbAvailablePictureSize[i].height;
        picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    }
    for (uint32_t i = 0; i < length; i++) {
        if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
            kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
            continue;
        picSizes[count++] = HAL_PIXEL_FORMAT_BLOB;
        picSizes[count++] = kUsbAvailablePictureSize[i].width;
        picSizes[count++] = kUsbAvailablePictureSize[i].height;
        picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    }
    return (int)count;
}

int V4l2MediaSensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag) {
    uint32_t count = 0, size_start = 0;
    char property[PROPERTY_VALUE_MAX];
    int fullsize_preview = FALSE;
    property_get("vendor.amlogic.camera.fullsize.preview", property, "true");
    if (strstr(property, "true")) {
        fullsize_preview = TRUE;
    }
    if (fullsize_preview == FALSE ) size_start = 1;
    struct v4l2_frmsizeenum frmsizeMax;
    memset(&frmsizeMax, 0, sizeof(frmsizeMax));
    frmsizeMax.pixel_format = getOutputFormat();
    staticPipe::fetchPipeMaxResolution(
        (media_stream_t*) mMediaStream, frmsizeMax.discrete.width, frmsizeMax.discrete.height);

    DBG_LOGB("get max output width=%d, height=%d, format=%d\n",
        frmsizeMax.discrete.width, frmsizeMax.discrete.height, frmsizeMax.pixel_format);
    for (uint32_t i = size_start; i < ARRAY_SIZE(kUsbAvailablePictureSize); i++) {
            if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
                kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
                continue;
            duration[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
            duration[count+1] = kUsbAvailablePictureSize[i].width;
            duration[count+2] = kUsbAvailablePictureSize[i].height;
            if (!flag)
                duration[count+3] = 0;
            else
                duration[count+3] = (int64_t)FRAME_DURATION;
            count += 4;
    }
    for (uint32_t i = size_start; i < ARRAY_SIZE(kUsbAvailablePictureSize); i++) {
            if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
                kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
                continue;
            duration[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
            duration[count+1] = kUsbAvailablePictureSize[i].width;
            duration[count+2] = kUsbAvailablePictureSize[i].height;
            if (!flag)
                duration[count+3] = 0;
            else
                duration[count+3] = (int64_t)FRAME_DURATION;
            count += 4;
    }
    for (uint32_t i = 0; i < ARRAY_SIZE(kUsbAvailablePictureSize); i++) {
            if (kUsbAvailablePictureSize[i].width > frmsizeMax.discrete.width ||
                kUsbAvailablePictureSize[i].height > frmsizeMax.discrete.height)
                continue;
            duration[count+0] = HAL_PIXEL_FORMAT_BLOB;
            duration[count+1] = kUsbAvailablePictureSize[i].width;
            duration[count+2] = kUsbAvailablePictureSize[i].height;
            duration[count+3] = (int64_t)FRAME_DURATION;
            count += 4;
    }
    return (int)count;
}

int64_t V4l2MediaSensor::getMinFrameDuration() {
    int64_t minFrameDuration =  1000000000L/30L ; // 30fps
    ALOGW("%s to be implemented, minframeduration  %" PRId64 "\n", __func__, minFrameDuration);
    return minFrameDuration;
}

int V4l2MediaSensor::getPictureSizes(int32_t picSizes[], int size, bool preview) {
    int res;
    int i;
    int count = 0;
    struct v4l2_frmsizeenum frmsize;
    unsigned int support_w,support_h;
    int preview_fmt;

    support_w = kUsbAvailablePictureSize[0].width;
    support_h = kUsbAvailablePictureSize[0].height;

    ALOGI("%s:support_w=%d, support_h=%d\n",__FUNCTION__,support_w,support_h);
    memset(&frmsize,0,sizeof(frmsize));
    preview_fmt = V4L2_PIX_FMT_NV21;//getOutputFormat();

    if (preview_fmt == V4L2_PIX_FMT_NV21) {
        if (preview == true)
            frmsize.pixel_format = V4L2_PIX_FMT_NV21;
        else
            frmsize.pixel_format = V4L2_PIX_FMT_RGB24;
    } else if (preview_fmt == V4L2_PIX_FMT_YVU420) {
        if (preview == true)
            frmsize.pixel_format = V4L2_PIX_FMT_YVU420;
        else
            frmsize.pixel_format = V4L2_PIX_FMT_RGB24;
    } else if (preview_fmt == V4L2_PIX_FMT_YUYV)
        frmsize.pixel_format = V4L2_PIX_FMT_YUYV;

    for (i = 0; ; i++) {
        frmsize.index = i;
        res = fakeEnumFrameSize(&frmsize); //ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }


        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height > support_h))
                continue;

            if (count >= size)
                break;

            picSizes[count] = frmsize.discrete.width;
            picSizes[count+1]  =  frmsize.discrete.height;

            if (0 == i) {
                count += 2;
                continue;
            }

            //TODO insert in descend order
            if (picSizes[count + 0] * picSizes[count + 1] > picSizes[count - 1] * picSizes[count - 2]) {
                picSizes[count + 0] = picSizes[count - 2];
                picSizes[count + 1] = picSizes[count - 1];

                picSizes[count - 2] = frmsize.discrete.width;
                picSizes[count - 1] = frmsize.discrete.height;
            }

            count += 2;
        }
    }

    return count;
}

status_t V4l2MediaSensor::force_reset_sensor() {
    DBG_LOGA("force_reset_sensor");
    // todo: reset pipeline
    status_t ret;
    mTimeOutCount = 0;
    ret = streamOff();
    ret = mVinfo->setBuffersFormat();
    ret = streamOn();
    DBG_LOGB("%s , ret = %d", __FUNCTION__, ret);
    return ret;
}

int V4l2MediaSensor::captureNewImage() {
    uint32_t gain = mGainFactor;
    mKernelBuffer = NULL;
    mKernelBufferFmt = 0;
    mTempFD = -1;
    // Might be adding more buffers, so size isn't constant
    ALOGVV("%s:buffer size=%zu\n",__FUNCTION__,mNextCapturedBuffers->size());
    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
        const StreamBuffer &b = (*mNextCapturedBuffers)[i];
        ALOGVV("Sensor capturing buffer %zu: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                i, b.streamId, b.width, b.height, b.format, b.stride,
                b.buffer, b.img);
        switch (b.format) {
            case HAL_PIXEL_FORMAT_BLOB:
                // Add auxillary buffer of the right size
                // Assumes only one BLOB (JPEG) buffer in
                // mNextCapturedBuffers
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                ALOGD("bAux orientation=%d",orientation);

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                bAux.stride = b.width;
                bAux.buffer = NULL;
#ifdef GE2D_ENABLE
                bAux.img = mION->alloc_buffer(b.width * b.height * 3,&bAux.share_fd);
#else
                bAux.img = new uint8_t[b.width * b.height * 3];
#endif
                mNextCapturedBuffers->push_back(bAux);
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                captureNV21(b, gain);
                break;
            case HAL_PIXEL_FORMAT_YV12:
                captureYV12(b, gain);
                break;
            case HAL_PIXEL_FORMAT_YCbCr_422_I:
                captureYUYV(b.img, gain, b.stride);
                break;
            default:
                ALOGE("%s: Unknown format %x, no output", __FUNCTION__,
                        b.format);
                break;
        }
    }
    return 0;
}


int V4l2MediaSensor::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);

    return ret ;
}

int V4l2MediaSensor::setZoom(int zoomValue) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);

    return ret ;
}


status_t V4l2MediaSensor::setEffect(uint8_t effect) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);
    return ret ;
}

int V4l2MediaSensor::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step) {
   int ret=0;
   ALOGW("%s not implemented yet!", __func__);
   return ret;
}

status_t V4l2MediaSensor::setExposure(int expCmp) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);
    return ret ;
}

int V4l2MediaSensor::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont) {

    int mode_count = -1;
    ALOGW("%s not implemented yet!", __func__);

    return mode_count;
}



status_t V4l2MediaSensor::setAntiBanding(uint8_t antiBanding) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);
    return ret;
}

status_t V4l2MediaSensor::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    int ret = 0;
    ALOGW("%s not implemented yet!", __func__);
    return ret;
}

int V4l2MediaSensor::getAutoFocus(uint8_t *afMode, uint8_t maxCount) {
    int mode_count = -1;
    ALOGW("%s not implemented yet!", __func__);

    return mode_count;
}



status_t V4l2MediaSensor::setAutoFocus(uint8_t afMode) {
    ALOGW("%s not implemented yet!", __func__);
    return 0;
}



int V4l2MediaSensor::getAWB(uint8_t *awbMode, uint8_t maxCount) {
    int mode_count = -1;
    ALOGW("%s not implemented yet!", __func__);
    return mode_count;
}



status_t V4l2MediaSensor::setAWB(uint8_t awbMode) {
    ALOGW("%s not implemented yet!", __func__);
    return 0;
}



void V4l2MediaSensor::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}


status_t V4l2MediaSensor::readyToRun() {
    //int res;
    ATRACE_CALL();
    ALOGV("Starting up media sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = NULL;
    DBG_LOGA("");

    return OK;
}

}
