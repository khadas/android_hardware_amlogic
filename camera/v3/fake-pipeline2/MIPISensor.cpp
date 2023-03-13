#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "MIPISensor"

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
#include "MIPISensor.h"
#include "CaptureUseMemcpy.h"
#include "CaptureUseGe2d.h"
#include "V4l2Utils.h"
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

#if ANDROID_PLATFORM_SDK_VERSION >= 24
#if ANDROID_PLATFORM_SDK_VERSION >= 29
#include <amlogic/am_gralloc_usage.h>
#include <gralloc1.h>
#else
#include <gralloc_usage_ext.h>
#endif
#endif

#if ANDROID_PLATFORM_SDK_VERSION >= 28
#include <amlogic/am_gralloc_ext.h>
#endif

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

namespace android {

const usb_frmsize_discrete_t kUsbAvailablePictureSize[] = {
        {4096, 3120},
        {3840, 2160},
        {1920, 1080},
        {1440, 1080},
        {1280, 960},
        {1280, 720},
        {1024, 768},
        {960, 720},
        {720, 480},
        {640, 480},
        {352, 288},
        {320, 240},
};

extern bool IsUsbAvailablePictureSize(const usb_frmsize_discrete_t AvailablePictureSize[], uint32_t width, uint32_t height);

MIPISensor::MIPISensor() {
    mCameraVirtualDevice = nullptr;
    mVinfo = NULL;
    mCapture = NULL;
    mFrameDuration = FRAME_DURATION;
    enableZsl = false;
    PictureThreadCntler::resetAndInit(mPictureThreadCntler);
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.camera.zsl.enable", property, "false");
    if (strstr(property, "true"))
        enableZsl = true;
    if (mPictureThreadCntler.PictureThread == NULL) {
        mPictureThreadCntler.PictureThread = new std::thread([this]() {
            //uint32_t ION_try = 0;
            while (mPictureThreadCntler.PictureThreadExit != true) {
                Mutex::Autolock lock(mPictureThreadCntler.requestOperaionLock);
                if (mPictureThreadCntler.NextPictureRequest.empty()) {
                    mPictureThreadCntler.unprocessedRequest.wait(mPictureThreadCntler.requestOperaionLock);
                } else {
                    Request *PicRequest = mPictureThreadCntler.NextPictureRequest.begin();
                    Buffers * pictureBuffers = PicRequest->sensorBuffers;
                    uint32_t jpegpixelfmt = getOutputFormat();
                    bool isTakePictureDone = false;
                    if ((jpegpixelfmt == V4L2_PIX_FMT_MJPEG) || (jpegpixelfmt == V4L2_PIX_FMT_YUYV)) {
                           setOutputFormat(mMaxWidth, mMaxHeight, jpegpixelfmt, channel_capture);
                    } else {
                           setOutputFormat(mMaxWidth, mMaxHeight, V4L2_PIX_FMT_RGB24, channel_capture);
                    }
                    for (size_t i = 0; i < pictureBuffers->size(); i++) {
                        const StreamBuffer &b = (*pictureBuffers)[i];
                        CAMHAL_LOGDB("Sensor capturing buffer %d: stream %d,"
                            " %d x %d, format %x, stride %d, buf %p, img %p",
                            i, b.streamId, b.width, b.height, b.format, b.stride,
                            b.buffer, b.img);
                        if (b.format == HAL_PIXEL_FORMAT_BLOB) {
                            // Add auxillary buffer of the right size
                            // Assumes only one BLOB (JPEG) buffer in
                            // mNextCapturedBuffers
                            size_t len;
                            int orientation;
                            uint32_t stride;
                            int ION_try = 0;
                            orientation = getPictureRotate();
                            CAMHAL_LOGDB("bAux orientation=%d",orientation);
                            CAMHAL_LOGDB("%s: the picture width=%d, height=%d\n",__FUNCTION__,b.width,b.height);
                            StreamBuffer bAux;

                            bAux.streamId = 0;
                            bAux.width = b.width;
                            bAux.height = b.height;
                            bAux.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                            bAux.stride = b.width;
                            bAux.buffer = NULL;
                            len = b.width * b.height * 3/2;
                            stride = bAux.stride;
#ifdef GE2D_ENABLE
                            bAux.img = mION->alloc_buffer(len, &bAux.share_fd);
                            while (bAux.img == NULL && ION_try < 20) {
                                usleep(5 * 1000);
                                bAux.img = mION->alloc_buffer(len,&bAux.share_fd);
                                ION_try ++;
                            }
                            ION_try = 0;
#else
                            bAux.img = new uint8_t[len];
#endif
                            if (bAux.img == NULL) {//don't capture
                                ALOGE("%s:%d fatal: no buffer to capture,skip ...",__FUNCTION__,__LINE__);
                                //return -1;
                            } else {
                                takePicture(bAux, mGainFactor, b.stride);
                                pictureBuffers->push_back(bAux);
                                isTakePictureDone = true;
                            }
                        }
                    }
                    if (mListener && isTakePictureDone)
                        mListener->onSensorPicJpeg(*PicRequest);
                    mPictureThreadCntler.NextPictureRequest.erase(PicRequest);
               }
            }
            return false;
         });
        }

#if 1
    mISP = isp3a::get_instance();
#endif
    mResource = GlobalResource::getInstance();
#ifdef GE2D_ENABLE
    mION = IONInterface::get_instance();
    mGE2D = new ge2dTransform();
#endif

#ifdef GDC_ENABLE
    mIsGdcInit = false;
#endif
    mPortFds.resize(3,-1);
    ALOGD("create MIPISensor");
}

MIPISensor::~MIPISensor() {
    ALOGD("delete MIPISensor");
    if (mCapture) {
        delete(mCapture);
        mCapture = NULL;
    }
    if (mVinfo) {
        delete(mVinfo);
        mVinfo = NULL;
    }
    PictureThreadCntler::stopAndRelease(mPictureThreadCntler);

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

status_t MIPISensor::streamOff(channel ch) {
    ALOGV("%s: E", __FUNCTION__);
    status_t ret = 0;

    if (ch == channel_capture) {
        mVinfo->stop_picture();
    }
    else if(ch == channel_record) {
        mVinfo->stop_recording();
    }
    else if(ch == channel_preview) {
#ifdef GDC_ENABLE
        if (mIGdc && mIsGdcInit) {
            mIGdc->gdc_exit();
            mIsGdcInit = false;
        }
#endif
        ret = mVinfo->stop_capturing();
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
        //DeWarp::putInstance();
#endif
    }
    else
        return -1;

    return ret;
}

int MIPISensor::SensorInit(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    int ret = 0;
    if (mVinfo == NULL)
        mVinfo =  new MIPIVideoInfo();
    ret = camera_open(idx);
    if (ret < 0) {
        ALOGE("Unable to open sensor %d, errno=%d\n", mVinfo->get_index(), ret);
        return ret;
    }
    //InitVideoInfo(idx);
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
    mSensorType = SENSOR_MIPI;

    return ret;
}

status_t MIPISensor::startUp(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    int res;
    mCapturedBuffers = NULL;

    mOpenCameraID = idx;
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

int MIPISensor::camera_open(int idx) {
    int ret = 0;
    int counter = 2;
    char property[PROPERTY_VALUE_MAX];
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera.count",property,NULL) > 0) {
        sscanf(property,"%d",&counter);
        if (counter > 4)
            counter = 2;
    }

    mISP->open_isp3a_library(counter);
    mISP->print_status();

     mVinfo = new MIPIVideoInfo();
     if (mVinfo) {
        mVinfo->mWorkMode = PIC_SCALER;
        int portFd[3];
        for (int i = 0; i < 3; i++)
            portFd[i] = open(mDeviceName, O_RDWR);

        mPortFds[channel_preview] = portFd[1];// sc0
        mPortFds[channel_capture] = portFd[0];// sc3 no resize
        mPortFds[channel_record] = portFd[2];// sc1
        mVinfo->set_fds(mPortFds);
        mVinfo->set_index(idx);

        int32_t picSizes[64 * 8];
        int32_t count = sizeof(picSizes)/sizeof(picSizes[0]);
        getPictureSizes(picSizes, count, true);
        for (int i = 0; i < count; i+= 2) {
            int32_t width = picSizes[i];
            int32_t height = picSizes[i+1];
            if (width * height > mMaxWidth * mMaxHeight) {
                mMaxWidth = width;
                mMaxHeight = height;
            }
        }
        /*set the max size of isp port*/
        struct v4l2_format format;
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = mMaxWidth;
        format.fmt.pix.height = mMaxHeight;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;

        int ret = ioctl(mPortFds[channel_capture], VIDIOC_S_FMT, &format);
        if (ret < 0) {
            ALOGD("Open: VIDIOC_S_FMT Failed: %s, fd=%d\n",
                strerror(errno), mPortFds[channel_capture]);
            return -1;
        }
        ALOGI("max width %d, max height %d", mMaxWidth, mMaxHeight);
     }
    return ret;
}

void MIPISensor::camera_close(void) {
    ALOGV("%s: E", __FUNCTION__);
    mISP->close_isp3a_library();
    mISP->print_status();
    for (int i = 0;i < mPortFds.size();i++)
        close(mPortFds[i]);
    mPortFds.clear();
}

/*
void MIPISensor::InitVideoInfo(int idx) {
    if (mVinfo) {
        std::vector<int> fds;
        fds.push_back(mMIPIDevicefd[0]);
        mVinfo->mWorkMode = ONE_FD;
        mVinfo->set_fds(fds);
        mVinfo->set_index(idx);
    }
}*/

status_t MIPISensor::shutDown() {
    ALOGV("%s: E", __FUNCTION__);
    int res;
    mTimeOutCount = 0;
    res = requestExitAndWait();
    if (res != OK) {
        ALOGE("Unable to shut down sensor capture thread: %d", res);
    }
    if (mVinfo != NULL) {
        mVinfo->stop_picture();
        mVinfo->stop_recording();
        mVinfo->stop_capturing();
    }

    camera_close();

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

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    //DeWarp::putInstance();
#endif
    mSensorWorkFlag = false;
    ALOGD("%s: Exit", __FUNCTION__);
    return res;
}

uint32_t MIPISensor::getStreamUsage(int stream_type){
    ATRACE_CALL();
#if 0
    uint32_t usage = Sensor::getStreamUsage(stream_type);
    usage = (GRALLOC_USAGE_HW_TEXTURE
            | GRALLOC_USAGE_HW_RENDER
            | GRALLOC_USAGE_SW_READ_MASK
            | GRALLOC_USAGE_SW_WRITE_MASK
            );

#if ANDROID_PLATFORM_SDK_VERSION >= 28
        usage = am_gralloc_get_omx_osd_producer_usage();
#else
        usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_AML_DMA_BUFFER;
#endif
#endif
    uint32_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    ALOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;
}

void MIPISensor::captureRGB(uint8_t *img, uint32_t gain, uint32_t stride) {

}

void MIPISensor::takePicture(StreamBuffer& b, uint32_t gain, uint32_t stride) {
    int ret = 0;
    bool stop = false;
    struct data_in in;
    in.src = mKernelBuffer;
    in.share_fd = mTempFD;
    ALOGD("%s: E",__FUNCTION__);
    V4l2Utils::set_notify_3A_is_capturing(mVinfo->get_fd(), DO_CAPTURING);
    if (!isPicture()) {
        mVinfo->start_picture(0);
        enableZsl = false;
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.camera.zsl.enable", property, "false");
        if (strstr(property, "true"))
            enableZsl = true;
        stop = true;
    }
    while (1)
    {
        if (mExitSensorThread || mFlushFlag)
            break;

        ret = mCapture->getPicture(b, &in, mION);
        if (ret == ERROR_FRAME)
            break;
#ifdef GE2D_ENABLE
        //----do rotation
        mGE2D->doRotationAndMirror(b);
#endif
        mVinfo->putback_picture_frame();
        mSensorWorkFlag = true;
        break;
    }

    if (stop == true)
        mVinfo->stop_picture();
    V4l2Utils::set_notify_3A_is_capturing(mVinfo->get_fd(), NOT_CAPTURING);
    ALOGD("get picture success !");
}

void MIPISensor::captureNV21(StreamBuffer b, uint32_t gain) {
    ATRACE_CALL();
    //ALOGVV("MIPI NV21 sensor image captured");
    struct data_in in;
    in.src = mKernelBuffer;
    in.share_fd = mTempFD;
    in.src_fmt = mKernelBufferFmt;
    ALOGVV("%s:mTempFD = %d",__FUNCTION__,mTempFD);
    while (1) {
        if (mExitSensorThread) {
            break;
        }
        //----get one frame
        int ret = mCapture->captureNV21frame(b, &in);
        if (ret == ERROR_FRAME) {
          break;
       }
#ifdef GE2D_ENABLE
        //----do rotation
        if (mTempFD < 0) {
            ALOGVV("%s:doRotationAndMirror",__FUNCTION__);
            mGE2D->doRotationAndMirror(b);
        }
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
        if (ret == NEW_FRAME) {
            mKernelBuffer = b.img;
            mTempFD = b.share_fd;
            mKernelBufferFmt = V4L2_PIX_FMT_NV21;
        }
        mSensorWorkFlag = true;
        if (ret == NEW_FRAME)
            mVinfo->putback_frame();
        if (mFlushFlag) {
            break;
        }
        break;
    }
}

void MIPISensor::captureYV12(StreamBuffer b, uint32_t gain) {
    struct data_in in;
    in.src = mKernelBuffer;
    in.share_fd = mTempFD;

    while (1) {

        if (mExitSensorThread) {
            break;
        }
        int ret = mCapture->captureYV12frame(b,&in);
        if (ret == -1)
            continue;
        mKernelBuffer = b.img;
        mTempFD = b.share_fd;
        mSensorWorkFlag = true;
        mVinfo->putback_frame();
        if (mFlushFlag) {
            break;
        }
        break;
    }
    ALOGVV("YV12 sensor image captured");
}
void MIPISensor::captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride) {
    struct data_in in;
    in.src = mKernelBuffer;
    in.share_fd = mTempFD;

    while (1) {
        if (mFlushFlag) {
            break;
        }
        if (mExitSensorThread) {
            break;
        }
        int ret = mCapture->captureYUYVframe(img,&in);
        if (ret == -1)
            continue;
        mKernelBuffer = img;
        mTempFD = -1;
        mSensorWorkFlag = true;
        mVinfo->putback_frame();
        break;
    }
    ALOGVV("YUYV sensor image captured");
}
void MIPISensor::setIOBufferNum()
{
    char buffer_number[128];
    int tmp = 6;
    if (property_get("ro.vendor.mipicamera.iobuffer", buffer_number, NULL) > 0) {
        sscanf(buffer_number, "%d", &tmp);
        ALOGD(" get buffer number is %d from property \n",tmp);
    }

    ALOGD("defalut buffer number is %d\n",tmp);
    mVinfo->set_buffer_numbers(tmp);
}

status_t MIPISensor::getOutputFormat(void) {
    uint32_t ret = 0;
     ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_NV21);
    if (ret)
        return ret;

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret)
        return ret;
    ALOGE("Unable to find a supported sensor format!");
    return BAD_VALUE;
}

status_t MIPISensor::setOutputFormat(int width, int height, int pixelformat, channel ch) {
    int res;
    mFramecount = 0;
    mCurFps = 0;

    CAMHAL_LOGDB("%s: channel=%d \n",__FUNCTION__, ch);

    if (ch == channel_capture) {
        //setCrop(width, height);
        //----set snap shot pixel format
        mVinfo->set_picture_format(width, height, pixelformat);
    } else if (ch == channel_record) {
        //setCrop(width, height);
        //----set record pixel format
        mVinfo->set_record_format(width, height, pixelformat);
    } else if (ch == channel_preview) {
        //----set preview pixel format
        mVinfo->set_preview_format(width, height, pixelformat);
        /*
        mVinfo->preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVinfo->preview.format.fmt.pix.width = width;
        mVinfo->preview.format.fmt.pix.height = height;
        mVinfo->preview.format.fmt.pix.pixelformat = pixelformat;
        */
        res = mVinfo->setBuffersFormat();
        if (res < 0) {
            ALOGE("set preview format failed\n");
            return res;
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

int MIPISensor::halFormatToSensorFormat(uint32_t pixelfmt) {
    uint32_t ret = 0;
    uint32_t fmt = 0;
    switch (pixelfmt) {
        case HAL_PIXEL_FORMAT_YV12:
            fmt = V4L2_PIX_FMT_YVU420;
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            fmt = V4L2_PIX_FMT_NV21;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            fmt = V4L2_PIX_FMT_YUYV;
            break;
        default:
            fmt = V4L2_PIX_FMT_NV21;
            break;
        break;
    }
    ret = mVinfo->EnumerateFormat(fmt);
    if (ret)
        return ret;

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret)
        return ret;

    ALOGE("%s, Unable to find a supported sensor format!", __FUNCTION__);
    return BAD_VALUE;
}

status_t MIPISensor::streamOn(channel channel) {
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.media.camera.dual",property,"false");

    /* enable dualcamera support, but make sure hardware ready before */
    if (strstr(property,"true")) {
        setdualcam(1);
    }
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_AUTO;
    ctl.value = 0;
    if (ioctl(mPortFds[channel_capture], VIDIOC_S_CTRL, &ctl) < 0) {
        ALOGD("%s : failed to set camera focuas mode!", __FUNCTION__);
    }
    if (channel == channel_capture)
        return mVinfo->start_picture(0);
    else if(channel == channel_preview)
        return mVinfo->start_capturing();
    else if(channel == channel_record)
        return mVinfo->start_recording();
    else
        return -1;
}

bool MIPISensor::isStreaming() {
    return mVinfo->Stream_status();
}

bool MIPISensor::isPicture() {
    return mVinfo->Picture_status();
}

bool MIPISensor::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch) {
    if ((mVinfo->get_preview_width()!= width)
        ||(mVinfo->get_preview_height() != height)) {
        return true;
    }
    return false;
}

int MIPISensor::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) {
    const uint32_t length = ARRAY_SIZE(kUsbAvailablePictureSize);
    uint32_t count = 0, size_start = 0;
    char property[PROPERTY_VALUE_MAX];
    int fullsize_preview = FALSE;

    property_get("vendor.amlogic.camera.fullsize.preview", property, "true");
    if (strstr(property, "true")) {
        fullsize_preview = TRUE;
    }

    if (fullsize_preview == FALSE) size_start = 1;

    struct v4l2_frmsizeenum frmsize, frmsizeMax;
    memset(&frmsize, 0, sizeof(frmsize));
    memset(&frmsizeMax, 0, sizeof(frmsize));
    frmsize.pixel_format = getOutputFormat();

    for (uint32_t i = 0; ; i++) {
        frmsize.index = i;
        auto res = ioctl(mVinfo->get_fd(), VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type
            if (0 != (frmsize.discrete.width % 16))
                continue;
            if ((frmsize.discrete.width * frmsize.discrete.height) >
                (frmsizeMax.discrete.width * frmsizeMax.discrete.height))
                frmsizeMax = frmsize;
        }
    }
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

int MIPISensor::
getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag) {
    uint32_t count = 0, size_start = 0;

    char property[PROPERTY_VALUE_MAX];
    int fullsize_preview = FALSE;

    property_get("vendor.amlogic.camera.fullsize.preview", property, "true");
    if (strstr(property, "true")) {
        fullsize_preview = TRUE;
    }

    if (fullsize_preview == FALSE ) size_start = 1;

    struct v4l2_frmsizeenum frmsize, frmsizeMax;
    memset(&frmsize, 0, sizeof(frmsize));
    memset(&frmsizeMax, 0, sizeof(frmsize));
    frmsize.pixel_format = getOutputFormat();

    for (uint32_t i = 0; ; i++) {
        frmsize.index = i;
        auto res = ioctl(mVinfo->get_fd(), VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type
            if (0 != (frmsize.discrete.width % 16))
                continue;
            if ((frmsize.discrete.width * frmsize.discrete.height) >
                (frmsizeMax.discrete.width * frmsizeMax.discrete.height))
                frmsizeMax = frmsize;
        }
    }
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

int64_t MIPISensor::getMinFrameDuration() {
    int64_t frameDuration =  FRAME_DURATION;
    return frameDuration;
}

int MIPISensor::getPictureSizes(int32_t picSizes[], int size, bool preview) {
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

    if (preview == true)
        frmsize.pixel_format = V4L2_PIX_FMT_NV21;
    else
        frmsize.pixel_format = V4L2_PIX_FMT_RGB24;

    for (i = 0; ; i++) {
        frmsize.index = i;
        res = ioctl(mVinfo->get_fd(), VIDIOC_ENUM_FRAMESIZES, &frmsize);
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

status_t MIPISensor::force_reset_sensor() {
    DBG_LOGA("force_reset_sensor");
    status_t ret;
    mTimeOutCount = 0;
    ret = streamOff(channel_preview);
    ret = mVinfo->setBuffersFormat();
    ret = streamOn(channel_preview);
    DBG_LOGB("%s , ret = %d", __FUNCTION__, ret);
    return ret;
}

int MIPISensor::captureNewImage() {
    bool isjpeg = false;
    uint32_t gain = mGainFactor;
    mKernelBuffer = NULL;
    mKernelBufferFmt = 0;
    mTempFD = -1;
    // Might be adding more buffers, so size isn't constant
    ALOGVV("%s:buffer size=%d\n",__FUNCTION__,mNextCapturedBuffers->size());
    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
        const StreamBuffer &b = (*mNextCapturedBuffers)[i];
        ALOGVV("Sensor capturing buffer %d: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                i, b.streamId, b.width, b.height, b.format, b.stride,
                b.buffer, b.img);
        switch (b.format) {
#if PLATFORM_SDK_VERSION <= 22
            case HAL_PIXEL_FORMAT_RAW_SENSOR:
                captureRaw(b.img, gain, b.stride);
                break;
#endif
            case HAL_PIXEL_FORMAT_RGB_888:
                captureRGB(b.img, gain, b.stride);
                break;
            case HAL_PIXEL_FORMAT_RGBA_8888:
                captureRGBA(b.img, gain, b.stride);
                break;
            case HAL_PIXEL_FORMAT_BLOB:
                // Add auxillary buffer of the right size
                // Assumes only one BLOB (JPEG) buffer in
                // mNextCapturedBuffers
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                ALOGD("bAux orientation=%d",orientation);
                uint32_t pixelfmt;
                if ((b.width == mVinfo->get_preview_width() &&
                  b.height == mVinfo->get_preview_height()) && (orientation == 0)) {
                    pixelfmt = getOutputFormat();
                    if (pixelfmt == V4L2_PIX_FMT_YVU420) {
                        pixelfmt = HAL_PIXEL_FORMAT_YV12;
                    } else if (pixelfmt == V4L2_PIX_FMT_NV21) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    } else if (pixelfmt == V4L2_PIX_FMT_YUYV) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
                    } else {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    }
                } else {
                    isjpeg = true;
                    pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                }

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = pixelfmt;
                bAux.stride = b.width;
                bAux.buffer = NULL;
#ifdef GE2D_ENABLE
                bAux.img = mION->alloc_buffer(b.width * b.height * 3, &bAux.share_fd);
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

status_t MIPISensor::setdualcam(uint8_t mode) {
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = ISP_V4L2_CID_CUSTOM_DCAM_MODE;
    ctl.value = mode;

    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);

    return ret;
}

int MIPISensor::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    int ret = 0;
    struct v4l2_queryctrl qc;
    memset(&qc, 0, sizeof(qc));
    qc.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl (mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
    if ((qc.flags == V4L2_CTRL_FLAG_DISABLED) || ( ret < 0)
        || (qc.type != V4L2_CTRL_TYPE_INTEGER)) {
        ret = -1;
        *zoomMin = 0;
        *zoomMax = 0;
        *zoomStep = 1;
        CAMHAL_LOGDB("%s: Can't get zoom level!\n", __FUNCTION__);
    } else {
        if ((qc.step != 0) && (qc.minimum != 0) &&
            ((qc.minimum/qc.step) > (qc.maximum/qc.minimum))) {
                DBG_LOGA("adjust zoom step. \n");
                qc.step = (qc.minimum * qc.step);
            }
        *zoomMin = qc.minimum;
        *zoomMax = qc.maximum;
        *zoomStep = qc.step;
        DBG_LOGB("zoomMin:%dzoomMax:%dzoomStep:%d\n", *zoomMin, *zoomMax, *zoomStep);
    }
    return ret ;
}

int MIPISensor::setZoom(int zoomValue) {
    int ret = 0;
    struct v4l2_control ctl;
    memset( &ctl, 0, sizeof(ctl));
    ctl.value = zoomValue;
    ctl.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        ALOGE("%s: Set zoom level failed!\n", __FUNCTION__);
        }
    return ret ;
}
status_t MIPISensor::setEffect(uint8_t effect) {
     int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_COLORFX;
    switch (effect) {
        case ANDROID_CONTROL_EFFECT_MODE_OFF:
        ctl.value= CAM_EFFECT_ENC_NORMAL;
        break;
        case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
        ctl.value= CAM_EFFECT_ENC_COLORINV;
        break;
        case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
        ctl.value= CAM_EFFECT_ENC_SEPIA;
        break;
        default:
        ALOGE("%s: Doesn't support effect mode %d",
        __FUNCTION__, effect);
        return BAD_VALUE;
    }
    DBG_LOGB("set effect mode:%d", effect);
    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    if (ret < 0)
        CAMHAL_LOGDB("Set effect fail: %s. ret=%d", strerror(errno),ret);
    return ret ;
}

int MIPISensor::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step) {
       struct v4l2_queryctrl qc;
       int ret=0;
       int level = 0;
       int middle = 0;

       memset( &qc, 0, sizeof(qc));

           DBG_LOGA("getExposure\n");
       qc.id = V4L2_CID_EXPOSURE;
       ret = ioctl(mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
       if (ret < 0) {
           CAMHAL_LOGDB("QUERYCTRL failed, errno=%d\n", errno);
           *minExp = -4;
           *maxExp = 4;
           *def = 0;
           step->numerator = 1;
           step->denominator = 1;
           return ret;
       }

       if (0 < qc.step)
           level = ( qc.maximum - qc.minimum + 1 )/qc.step;

       if ((level > MAX_LEVEL_FOR_EXPOSURE)
         || (level < MIN_LEVEL_FOR_EXPOSURE)) {
           *minExp = -4;
           *maxExp = 4;
           *def = 0;
           step->numerator = 1;
           step->denominator = 1;
           DBG_LOGB("not in[min,max], min=%d, max=%d, def=%d\n",
                                           *minExp, *maxExp, *def);
           return true;
       }

       middle = (qc.minimum+qc.maximum)/2;
       *minExp = qc.minimum - middle;
       *maxExp = qc.maximum - middle;
       *def = qc.default_value - middle;
       step->numerator = 1;
       step->denominator = 2;//qc.step;
           DBG_LOGB("min=%d, max=%d, step=%d\n", qc.minimum, qc.maximum, qc.step);
       return ret;
}

status_t MIPISensor::setExposure(int expCmp) {
    int ret = 0;
    struct v4l2_control ctl;
    struct v4l2_queryctrl qc;

    if (mEV == expCmp) {
        return 0;
    } else {
        mEV = expCmp;
    }
    memset(&ctl, 0, sizeof(ctl));
    memset(&qc, 0, sizeof(qc));

    qc.id = V4L2_CID_EXPOSURE;

    ret = ioctl(mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
    if (ret < 0) {
        CAMHAL_LOGDB("AMLOGIC CAMERA get Exposure fail: %s. ret=%d", strerror(errno),ret);
    }

    ctl.id = V4L2_CID_EXPOSURE;
    ctl.value = expCmp + (qc.maximum - qc.minimum) / 2;

    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        CAMHAL_LOGDB("AMLOGIC CAMERA Set Exposure fail: %s. ret=%d", strerror(errno),ret);
    }
        DBG_LOGB("setExposure value%d mEVmin%d mEVmax%d\n",ctl.value, qc.minimum, qc.maximum);
    return ret ;
}

int MIPISensor::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont) {
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_POWER_LINE_FREQUENCY;
    ret = ioctl (mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->get_fd());
    } else if ( qc.type != V4L2_CTRL_TYPE_INTEGER) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->get_fd());
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        antiBanding[0] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCont)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_POWER_LINE_FREQUENCY;
            qm.index = index;
            if (ioctl (mVinfo->get_fd(), VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"50hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"60hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"auto") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
                    mode_count++;
                }
            }
        }
    }

    return mode_count;
}
status_t MIPISensor::setAntiBanding(uint8_t antiBanding) {
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_POWER_LINE_FREQUENCY;

    switch (antiBanding) {
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
        ctl.value= CAM_ANTIBANDING_OFF;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
        ctl.value= CAM_ANTIBANDING_50HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
        ctl.value= CAM_ANTIBANDING_60HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
        ctl.value= CAM_ANTIBANDING_AUTO;
        break;
    default:
            ALOGE("%s: Doesn't support ANTIBANDING mode %d",
                    __FUNCTION__, antiBanding);
            return BAD_VALUE;
    }

    CAMHAL_LOGDB("anti banding mode:%d", antiBanding);
    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    if ( ret < 0) {
        CAMHAL_LOGDA("failed to set anti banding mode!\n");
        return BAD_VALUE;
    }
    return ret;
}

status_t MIPISensor::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctl.value = ((x0 + x1) / 2 + 1000) << 16;
    ctl.value |= ((y0 + y1) / 2 + 1000) & 0xffff;

    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    return ret;
}

int MIPISensor::getAutoFocus(uint8_t *afMode, uint8_t maxCount) {
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_FOCUS_AUTO;
    ret = ioctl (mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->get_fd());
    } else if ( qc.type != V4L2_CTRL_TYPE_MENU) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->get_fd());
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        afMode[0] = ANDROID_CONTROL_AF_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_FOCUS_AUTO;
            qm.index = index;
            if (ioctl (mVinfo->get_fd(), VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-video") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-picture") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}
status_t MIPISensor::setAutoFocus(uint8_t afMode) {
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_AUTO;
    ALOGD("%s ++", __FUNCTION__);
    ALOGD("%s: ctl.id = %x", __FUNCTION__, V4L2_CID_FOCUS_AUTO);
    switch (afMode) {
        case ANDROID_CONTROL_AF_MODE_AUTO:
            ctl.value = CAM_FOCUS_MODE_AUTO;
            break;
        case ANDROID_CONTROL_AF_MODE_MACRO:
            ctl.value = CAM_FOCUS_MODE_MACRO;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            ctl.value = CAM_FOCUS_MODE_CONTI_VID;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            ctl.value = CAM_FOCUS_MODE_CONTI_PIC;
            break;
        default:
            ALOGE("%s: Emulator doesn't support AF mode %d",
                    __FUNCTION__, afMode);
            return BAD_VALUE;
    }
    ALOGD("%s: --", __FUNCTION__);
    ctl.value = 1;
    if (ioctl(mPortFds[channel_capture], VIDIOC_S_CTRL, &ctl) < 0) {
        ALOGD("%s : failed to set camera focuas mode!", __FUNCTION__);
        return BAD_VALUE;
    }

    return OK;
}
int MIPISensor::getAWB(uint8_t *awbMode, uint8_t maxCount) {
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_DO_WHITE_BALANCE;
    ret = ioctl (mVinfo->get_fd(), VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->get_fd());
    } else if ( qc.type != V4L2_CTRL_TYPE_MENU) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->get_fd());
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        awbMode[0] = ANDROID_CONTROL_AWB_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_DO_WHITE_BALANCE;
            qm.index = index;
            if (ioctl (mVinfo->get_fd(), VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"incandescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"warm-fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"cloudy-daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"twilight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_TWILIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"shade") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_SHADE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}
status_t MIPISensor::setAWB(uint8_t awbMode) {
     int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_DO_WHITE_BALANCE;

    switch (awbMode) {
        case ANDROID_CONTROL_AWB_MODE_AUTO:
            ctl.value = CAM_WB_AUTO;
            break;
        case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
            ctl.value = CAM_WB_INCANDESCENCE;
            break;
        case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
            ctl.value = CAM_WB_FLUORESCENT;
            break;
        case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
            ctl.value = CAM_WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_SHADE:
            ctl.value = CAM_WB_SHADE;
            break;
        default:
            ALOGE("%s: Emulator doesn't support AWB mode %d",
                    __FUNCTION__, awbMode);
            return BAD_VALUE;
    }
    ret = ioctl(mVinfo->get_fd(), VIDIOC_S_CTRL, &ctl);
    return ret;
}
void MIPISensor::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}

void MIPISensor::dump(int& frame_index, uint8_t* buf,
                            int length, std::string name) {

    ALOGD("%s:frame_index= %d",__FUNCTION__,frame_index);
    const int frame_num = 10;
    static FILE* fp = NULL;
    if (frame_index > frame_num)
        return;
    else if (frame_index == 0) {
        std::string path("/data/vendor/camera/");
        path.append(name);
        ALOGD("full_name:%s",path.c_str());

        fp = fopen(path.c_str(),"ab+");
        if (!fp) {
            ALOGE("open file %s fail, error: %s !!!",
                    path.c_str(),strerror(errno));
            return;
        }
    }
    if (frame_index++ == frame_num) {
        int fd = fileno(fp);
        fsync(fd);
        fclose(fp);
        close(fd);
        return ;
    }else {
        ALOGE("write frame %d ",frame_index);
        fwrite((void*)buf,1,length,fp);
    }
}

}
