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


#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

namespace android {

typedef struct pipe_info{
    const char * media_dev_name ;
    const char * sensor_ent_name ;
    const char * csiphy_ent_name;
    const char * adap_ent_name;
    const char * video_ent_name;
} pipe_info_t;



typedef struct media_stream {
    char media_dev_name[64];
    char sensor_ent_name[32];
    char csiphy_ent_name[32];
    char adap_ent_name[32];
    char video_ent_name[32];

    struct media_device  * media_dev;

    struct media_entity  * sensor_ent;
    struct media_entity  * csiphy_ent;
    struct media_entity  * adap_ent;
    struct media_entity  * video_ent;

} media_stream_t;


typedef struct aml_format {
    uint32_t width;
    uint32_t height;
    uint32_t code;
    uint32_t fourcc;
    uint32_t nplanes;
    uint32_t bpp;
} aml_format_t;


typedef struct stream_configuration{
    struct aml_format format;
} stream_configuration_t;


struct pipe_info pipe_0 = {
   .media_dev_name  = "/dev/media0",
   .sensor_ent_name = "ov5640-0",
   .csiphy_ent_name = "t7-csi2phy-0",
   .adap_ent_name   = "t7-adapter-0",
   .video_ent_name  = "t7-video-0-0"
};

struct pipe_info pipe_1 = {
    .media_dev_name  = "/dev/media1",
    .sensor_ent_name = "ov5640-1",
    .csiphy_ent_name = "t7-csi2phy-1",
    .adap_ent_name   = "t7-adapter-1",
    .video_ent_name  = "t7-video-1-0"
};


static struct pipe_info * supportedPipes[] = {
    &pipe_0,
    &pipe_1
};

static bool entInPipe(struct pipe_info * pipe, char * ent_name, int name_len)
{
    if (strncmp(ent_name, pipe->sensor_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->csiphy_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->adap_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->video_ent_name,name_len) == 0) {
        return true;
    }
    return false;
}

static void mediaLog(const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);

    ALOGI("%s \n", buf);
}


static struct pipe_info * mediaFindMatchedPipe(struct media_device * media_dev)
{
    if (0 != media_device_enumerate(media_dev) ) {
        ALOGE("media_device_enumerate fail\n");
        return NULL;
    }

    int node_num = media_get_entities_count(media_dev);

    for (int pipe_idx = 0 ; pipe_idx < ARRAY_SIZE(supportedPipes); ++pipe_idx) {
        bool all_ent_matched = true;
        for (int ii = 0; ii <node_num; ++ii) {
            struct media_entity * ent = media_get_entity(media_dev, ii);
            ALOGI("ent %d, name %s , devnode %s \n", ii, ent->info.name, ent->devname);

            if ( false == entInPipe(supportedPipes[pipe_idx], ent->info.name, strlen(ent->info.name))) {
                all_ent_matched = false;
                break;
            }
        }
        if (all_ent_matched) {
            return supportedPipes[pipe_idx];
        }
    }
    return NULL;
}


static int mediaStreamInit(media_stream_t * stream, struct pipe_info *pipe_info_ptr, struct media_device * dev)
{

    memset(stream, 0, sizeof(*stream));

    strncpy(stream->media_dev_name, pipe_info_ptr->media_dev_name, sizeof(stream->media_dev_name));

    strncpy(stream->sensor_ent_name, pipe_info_ptr->sensor_ent_name, sizeof(stream->sensor_ent_name));
    strncpy(stream->csiphy_ent_name, pipe_info_ptr->csiphy_ent_name, sizeof(stream->csiphy_ent_name));
    strncpy(stream->adap_ent_name,   pipe_info_ptr->adap_ent_name,   sizeof(stream->adap_ent_name));
    strncpy(stream->video_ent_name,  pipe_info_ptr->video_ent_name,  sizeof(stream->video_ent_name));

    stream->media_dev = dev;

    if (NULL == stream->media_dev) {
        ALOGE("new media dev fail\n");
        return -1;
    }

    media_debug_set_handler(dev, mediaLog, NULL);

    stream->sensor_ent = media_get_entity_by_name(stream->media_dev, stream->sensor_ent_name, strlen(stream->sensor_ent_name));
    if (NULL == stream->sensor_ent) {
        ALOGE("get  sensor_ent fail\n");
        return -1;
    }

    stream->csiphy_ent = media_get_entity_by_name(stream->media_dev, stream->csiphy_ent_name, strlen(stream->csiphy_ent_name));
    if (NULL == stream->csiphy_ent) {
        ALOGE("get  csiphy_ent fail\n");
        return -1;
    }

    stream->adap_ent = media_get_entity_by_name(stream->media_dev, stream->adap_ent_name, strlen(stream->adap_ent_name));
    if (NULL == stream->adap_ent) {
        ALOGE("get  adap_ent fail\n");
        return -1;
    }

    stream->video_ent = media_get_entity_by_name(stream->media_dev, stream->video_ent_name, strlen(stream->video_ent_name));
    if (NULL == stream->video_ent) {
        ALOGE("get video_ent fail\n");
        return -1;
    }

    int ret = v4l2_video_open(stream->video_ent);
    ALOGD("%s open video fd, ret %d ", __func__, ret);

    ALOGD("media stream init success\n");
    return 0;
}


int createLinks(media_stream_t *stream)
{
    // 0 = sink; 1 = src
    const int sink_pad_idx = 0;
    const int src_pad_idx = 1;

    int rtn = -1;
    struct media_pad      *src_pad;
    struct media_pad      *sink_pad;

    int flag = MEDIA_LNK_FL_ENABLED;

    ALOGD("create link ++\n");

    if (stream->media_dev == NULL)
        return 0;


    sink_pad = (struct media_pad*) media_entity_get_pad(stream->adap_ent, sink_pad_idx);
    if (!sink_pad) {
        ALOGE("Failed to get adap sink pad[0]\n");
        return rtn;
    }

    src_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, src_pad_idx);
    if (!src_pad) {
        ALOGE("Failed to get csiph src pad[1]\n");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        ALOGE( "Failed to link adap with csiphy\n");
        return rtn;
    }


    // sensor only has 1 pad
    src_pad =  (struct media_pad*)media_entity_get_pad(stream->sensor_ent, 0);
    if (!src_pad) {
        ALOGE("Failed to get sensor src pad[0]\n");
        return rtn;
    }

    sink_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, sink_pad_idx);
    if (!sink_pad) {
        ALOGE("Failed to get csiph sink pad[1]\n");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        ALOGE( "Failed to link sensor with csiphy\n");
        return rtn;
    }

    ALOGD("create link success \n");
    return rtn;
}


int setSdFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    struct v4l2_mbus_framefmt mbus_format;

    mbus_format.width  = cfg->format.width;
    mbus_format.height = cfg->format.height;
    mbus_format.code   = cfg->format.code;

    enum v4l2_subdev_format_whence which = V4L2_SUBDEV_FORMAT_ACTIVE;

    ALOGD("%s ++\n", __func__);

    // sensor source pad fmt
    rtn = v4l2_subdev_set_format(stream->sensor_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        ALOGE("Failed to set sensor format\n");
        return rtn;
    }


    // csiphy source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        ALOGE("Failed to set csiphy pad[0] format\n");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        ALOGE("Failed to set csiphy pad[1] format\n");
        return rtn;
    }

    // adap source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 0, which);

    if (rtn < 0) {
        ALOGE("Failed to set adap pad[0] format\n");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        ALOGE("Failed to set adap pad[1] format\n");
        return rtn;
    }

    ALOGD("%s success --\n", __func__);

    return rtn;
}

int setImgFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    ALOGD("%s ++\n", __func__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));
    if (cfg->format.nplanes > 1) {
        ALOGE ("not supported yet!\n");
        return -1;
    }

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = cfg->format.fourcc;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;

    rtn = v4l2_video_set_format( stream->video_ent,&v4l2_fmt);
    if (rtn < 0) {
        ALOGE("Failed to set video fmt, ret %d\n", rtn);
        return rtn;
    }

    ALOGD("%s success --\n", __func__);

    return rtn;
}



int mediaStreamConfig(media_stream_t * stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    ALOGD("%s ++\n", __func__);

    rtn = setSdFormat(stream, cfg);
    if (rtn < 0) {
        ALOGE("Failed to set subdev format\n");
        return rtn;
    }

    rtn = setImgFormat(stream, cfg);
    if (rtn < 0) {
        ALOGE("Failed to set image format\n");
        return rtn;
    }

    rtn = createLinks(stream);
    if (rtn) {
        ALOGE( "Failed to create links\n");
        return rtn;
    }

    ALOGD("Success to config media stream \n");

    return 0;
}

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

    // ======== pipe match & stream init ==============
    struct media_device * media_dev = media_device_new_with_fd(mMediaDevicefd);
    if (media_dev == NULL) {
        ALOGE("new media device failed \n");
        return -1;
    }

    struct pipe_info * matchPipe = mediaFindMatchedPipe(media_dev);
    if (NULL == matchPipe ) {
        ALOGE("media can not match supported pipes\n");
        return -1;
    }

    mMediaStream = malloc( sizeof( struct media_stream) );
    if (mMediaStream == NULL) {
        ALOGE("alloc media stream mem fail\n");
        return -1;
    }

    if (0 != mediaStreamInit((media_stream_t *)mMediaStream, matchPipe, media_dev) ) {
        ALOGE("media stream init failed\n");
        return -1;
    }

    // =========== vinfo init ============
    if (mVinfo == NULL)
        mVinfo =  new MIPIVideoInfo();

    if (mVinfo) {
        media_stream_t* stream = (media_stream_t*)mMediaStream;
        mVinfo->fd = stream->video_ent->fd;
        mVinfo->idx = idx;
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
    usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
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
        if (ret == -1)
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
    int tmp = 4;
    if (property_get("ro.vendor.mipicamera.iobuffer", buffer_number, NULL) > 0) {
        sscanf(buffer_number, "%d", &tmp);
        ALOGD(" get buffer number is %d from property \n",tmp);
    }

    ALOGD("defalut buffer number is %d\n",tmp);
    mVinfo->set_buffer_numbers(tmp);
}

status_t V4l2MediaSensor::getOutputFormat(void) {

    // todo: output format from sensor subdev
    //return V4L2_PIX_FMT_YUYV;
    return V4L2_PIX_FMT_UYVY;
}

status_t V4l2MediaSensor::setOutputFormat(int width, int height, int pixelformat, bool isjpeg) {

    int res;
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
        res = mVinfo->setBuffersFormat();
        if (res < 0) {
            ALOGE("set buffer failed\n");
            return res;
        }

        /* config & set format */
        stream_configuration_t    stream_config ;
        stream_config.format.width  = width;
        stream_config.format.height = height;
        stream_config.format.fourcc = pixelformat;
        stream_config.format.code   = MEDIA_BUS_FMT_UYVY8_2X8;

        res = mediaStreamConfig((media_stream_t*) mMediaStream, &stream_config);
        if (res < 0) {
            ALOGE("config stream failed\n");
            return res;
        }
#ifdef GDC_ENABLE
        if (mIGdc && !mIsGdcInit) {
            mIGdc->gdc_init(width,height,NV12,1);
            mIsGdcInit = true;
        }
#endif
    }
    //----alloc memory for temporary buffer
    if (NULL == mImage_buffer) {
        mPre_width = mVinfo->preview.format.fmt.pix.width;
        mPre_height = mVinfo->preview.format.fmt.pix.height;
        DBG_LOGB("setOutputFormat :: pre_width = %d, pre_height = %d \n" , mPre_width , mPre_height);
        mImage_buffer = new uint8_t[mPre_width * mPre_height * 3 / 2];
        if (mImage_buffer == NULL) {
            ALOGE("first time allocate mTemp_buffer failed !");
            return -1;
        }
    }
    //-----free old buffer and alloc new buffer
    if ((mPre_width != mVinfo->preview.format.fmt.pix.width)
        && (mPre_height != mVinfo->preview.format.fmt.pix.height)) {
        if (mImage_buffer) {
            delete [] mImage_buffer;
            mImage_buffer = NULL;
        }
        mPre_width = mVinfo->preview.format.fmt.pix.width;
        mPre_height = mVinfo->preview.format.fmt.pix.height;
        mImage_buffer = new uint8_t[mPre_width * mPre_height * 3 / 2];
        if (mImage_buffer == NULL) {
            ALOGE("allocate mTemp_buffer failed !");
            return -1;
        }
    }
    return OK;
}

int V4l2MediaSensor::halFormatToSensorFormat(uint32_t pixelfmt) {
    return getOutputFormat();
}

status_t V4l2MediaSensor::streamOn() {
    return mVinfo->start_capturing();
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
    int res;
    int i, j, k, START;
    int count = 0;
    //int pixelfmt;
    struct v4l2_frmsizeenum frmsize;
    char property[PROPERTY_VALUE_MAX];
    unsigned int support_w,support_h;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("ro.media.camera_preview.maxsize", property, NULL) > 0) {
        CAMHAL_LOGDB("support Max Preview Size :%s",property);
        if (sscanf(property,"%dx%d",&support_w,&support_h) != 2) {
            support_w = 10000;
            support_h = 10000;
        }
    } else {
        support_w = 1920;
        support_h = 1080;
    }

    ALOGI("%s:support_w=%d, support_h=%d\n",__FUNCTION__,support_w,support_h);
    memset(&frmsize,0,sizeof(frmsize));
    frmsize.pixel_format = getOutputFormat();

    START = 0;
    for (i = 0; ; i++) {
        frmsize.index = i;
        res = fakeEnumFrameSize(&frmsize); //ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("VIDIOC_ENUM_FRAMESIZES index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                continue;
            if (count >= size)
                break;

            picSizes[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format=%d\n",
                frmsize.discrete.width, frmsize.discrete.height, frmsize.pixel_format);
            if (0 == i) {
                count += 4;
                continue;
            }

            for (k = count; k > START; k -= 4) {
                if (frmsize.discrete.width * frmsize.discrete.height >
                    picSizes[k - 3] * picSizes[k - 2]) {
                    picSizes[k + 1] = picSizes[k - 3];
                    picSizes[k + 2] = picSizes[k - 2];

                } else {
                    break;
                }
            }
            picSizes[k + 1] = frmsize.discrete.width;
            picSizes[k + 2] = frmsize.discrete.height;

            count += 4;
        }
    }

    START = count;
    for (i = 0; ; i++) {
        frmsize.index = i;
        res = fakeEnumFrameSize(&frmsize); //ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width % 16))
                continue;

            if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                continue;
            if (count >= size)
                break;

            picSizes[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format =\
                HAL_PIXEL_FORMAT_YCbCr_420_888\n", frmsize.discrete.width,
                                                    frmsize.discrete.height);
            if (0 == i) {
                count += 4;
                continue;
            }

            for (k = count; k > START; k -= 4) {
                if (frmsize.discrete.width * frmsize.discrete.height >
                        picSizes[k - 3] * picSizes[k - 2]) {
                    picSizes[k + 1] = picSizes[k - 3];
                    picSizes[k + 2] = picSizes[k - 2];

                } else {
                    break;
                }
            }
            picSizes[k + 1] = frmsize.discrete.width;
            picSizes[k + 2] = frmsize.discrete.height;

            count += 4;
        }
    }

    uint32_t jpgSrcfmt[] = {
        V4L2_PIX_FMT_YUYV,
    };

    START = count;
    for (j = 0; j<(int)(sizeof(jpgSrcfmt)/sizeof(jpgSrcfmt[0])); j++) {
        memset(&frmsize,0,sizeof(frmsize));
        frmsize.pixel_format = jpgSrcfmt[j];

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

                if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height >support_h))
                    continue;

                if (count >= size)
                    break;

                picSizes[count+0] = HAL_PIXEL_FORMAT_BLOB;
                picSizes[count+1] = frmsize.discrete.width;
                picSizes[count+2] = frmsize.discrete.height;
                picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
                DBG_LOGB("get output width=%d, height=%d, format =\
                    HAL_PIXEL_FORMAT_BLOB\n", frmsize.discrete.width,
                                                        frmsize.discrete.height);

                if (0 == i) {
                    count += 4;
                    continue;
                }

                //TODO insert in descend order
                for (k = count; k > START; k -= 4) {
                    if (frmsize.discrete.width * frmsize.discrete.height >
                            picSizes[k - 3] * picSizes[k - 2]) {
                        picSizes[k + 1] = picSizes[k - 3];
                        picSizes[k + 2] = picSizes[k - 2];

                    } else {
                        break;
                    }
                }

                picSizes[k + 1] = frmsize.discrete.width;
                picSizes[k + 2] = frmsize.discrete.height;

                count += 4;
            }
        }

        if (frmsize.index > 0)
            break;
    }

    if (frmsize.index == 0)
        CAMHAL_LOGDA("no support pixel fmt for jpeg");

    return count;
}

int V4l2MediaSensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag) {

    int count = 0;
    int framerate = 30;

    memset(duration, 0 ,sizeof(int64_t) * size);


    for (; size > 0; size-=4)
    {
            duration[count+0] = (int64_t)(picSizes[size-4]); // hal format
            duration[count+1] = (int64_t)(picSizes[size-3]); // width
            duration[count+2] = (int64_t)(picSizes[size-2]); // height
            duration[count+3] = (int64_t)((1.0/framerate) * 1000000000); // duration
            count += 4;
    }

    return count;
}

int64_t V4l2MediaSensor::getMinFrameDuration() {
    int64_t minFrameDuration =  1000000000L/60L ; // 60fps
    ALOGW("%s to be implemented, minframeduration  %" PRId64 "\n", __func__, minFrameDuration);
    return minFrameDuration;
}

int V4l2MediaSensor::getPictureSizes(int32_t picSizes[], int size, bool preview) {
    int res;
    int i;
    int count = 0;
    struct v4l2_frmsizeenum frmsize;
    char property[PROPERTY_VALUE_MAX];
    unsigned int support_w,support_h;
    int preview_fmt;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("ro.media.camera_preview.maxsize", property, NULL) > 0) {
        CAMHAL_LOGDB("support Max Preview Size :%s",property);
        if (sscanf(property,"%dx%d",&support_w,&support_h) !=2 ) {
            support_w = 10000;
            support_h = 10000;
        }
    } else {
            support_w = 1920;
            support_h = 1080;
    }
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
    ALOGV("Starting up mipi sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = NULL;
    DBG_LOGA("");

    return OK;
}

}
