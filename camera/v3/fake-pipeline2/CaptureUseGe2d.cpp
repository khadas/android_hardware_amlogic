#define LOG_NDEBUG 0
#define LOG_TAG "CaptureUseGe2d"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <cmath>
#include <utils/Trace.h>
#include "CaptureUseGe2d.h"
#include "ge2d_stream.h"
#define GE2D_SCALER

#define MODEL_PATH "/vendor/etc/"

static int  smooth_step;
static bool smooth_enable;
static bool dewarp_enable;

static void dump2File(const char* name, void* src, int length) {
    CAMHAL_LOGD("dump2File full_name:%s", name);
    auto fp = fopen(name, "ab+");
    if (!fp) {
        CAMHAL_LOGE("open file %s fail, error: %s !!!", name, strerror(errno));
        return;
    }
    if (src == nullptr || length <= 0) {
        CAMHAL_LOGE("invalid parameter %p %d !!!", src, length);
        fclose(fp);
        return;
    }
    fwrite(src, 1 ,length, fp);
    fclose(fp);
}

namespace android {
#ifdef CAM_DPTZ
    center_face_network_t* centerface_network = NULL;
#endif
    CaptureUseGe2d::CaptureUseGe2d(MIPIVideoInfo* info) {
        smooth_step   = property_get_int32("vendor.camera.dptz.smooth.step", 6);
        smooth_enable = property_get_bool("vendor.camera.dptz.smooth", true);
        dewarp_enable = property_get_bool("vendor.camera.dptz.dewarp", true);
        mInfo = info;
        mCameraUtil = new CameraUtil();
        mGE2D = new ge2dTransform();
#ifdef CAM_DPTZ
        if (!centerface_network && property_get_bool("vendor.camera.dptz.enable", false)) {
            centerface_network = center_face_network_init_aml(NEU_IVA_CENTER_FACE, MODEL_PATH);
        }
#endif
    }

    CaptureUseGe2d::~CaptureUseGe2d() {
        if (mCameraUtil) {
            delete mCameraUtil;
            mCameraUtil = nullptr;
        }
        if (mGE2D) {
            delete mGE2D;
            mGE2D = nullptr;
        }
        IONInterface *ion = IONInterface::get_instance();
        if (mRGBIonFd > 0)
            ion->free_buffer(mRGBIonFd);
        ion->put_instance();
    }

int CaptureUseGe2d::getPicture(StreamBuffer b, struct data_in* in, IONInterface *ion) {
    int ret = 0;
    int length;
    struct VideoInfoBuffer vb;
    ret = mInfo->get_picture_buffer(&vb);
    ret = mInfo->putback_picture_frame();//drop one frame
    ret = mInfo->get_picture_buffer(&vb);
    ret = mInfo->putback_picture_frame();//drop one frame
    ret = mInfo->get_picture_buffer(&vb);
    int dmabuf_fd = vb.dma_fd;
    if (-1 == ret || -1 == dmabuf_fd) {
        CAMHAL_LOGE("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
        usleep(5000);
        return ERROR_FRAME; // no frame data
    }

    uint32_t format = mInfo->get_picture_pixelformat();
    uint32_t width = mInfo->get_picture_width();
    uint32_t height = mInfo->get_picture_height();

#ifdef PICTURE_DEWARP_ENABLE
    int outbuf_fd = -1;
    if (format == V4L2_PIX_FMT_NV21) {
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.camhal.use.dewarp.capture", property, "false");
        if (strstr(property, "true")) {
            DeWarp* GDCObj = nullptr;
            property_get("vendor.camhal.use.dewarp.linear", property, "false");
            CameraConfig* config = CameraConfig::getInstance(DEWARP_CAM2PORT_CAPTURE);
            config->setOutputWidth(width);
            config->setOutputHeight(height);
            config->setOutputStride(width);
            config->setInputWidth(width);
            config->setInputHeight(height);
            ion->alloc_buffer(width  * height * 3/2, &outbuf_fd);
            CAMHAL_LOGV("%s-%d b.width:%d b.height:%d width:%d,height:%d",__FUNCTION__,__LINE__,b.width,b.height,\
                config->getOutputWidth(),config->getOutputHeight());
            if (strstr(property, "true")) {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE,PROJ_MODE_LINEAR,Rotation::ROTATION_0);
            } else {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE,PROJ_MODE_EQUIDISTANCE,Rotation::ROTATION_0);
            }
            if (GDCObj) {
                GDCObj->mInput_fd = dmabuf_fd;
                GDCObj->mOutput_fd = outbuf_fd;
                GDCObj->gdc_do_fisheye_correction();
            }
            dmabuf_fd = outbuf_fd;
        }
    }
#endif

    switch (format) {
        case V4L2_PIX_FMT_RGB24:
            CAMHAL_LOGE("%s:config format is RGB888 ",__FUNCTION__);
            length = width * height * 3;
            memcpy(b.img, vb.addr, length);
            break;
        case V4L2_PIX_FMT_NV21:
            CAMHAL_LOGD("%s:width=%d,height=%d,size=%d",__FUNCTION__,b.width,b.height,vb.size);
            mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12,
                b.width, b.height, dmabuf_fd, width, height, b.stride);
            if (property_get_bool("vendor.camhal.dump.capture", false)) {
                char path[256];
                static int index = 0;
                sprintf(path, "/data/vendor/camera/capture-in-%dx%d-%d.yuv", width, height, index);
                dump2File(path, vb.addr, width * height *3/2);
                sprintf(path, "/data/vendor/camera/capture-out-%dx%d-%d.yuv", b.width, b.height, index);
                dump2File(path, b.img, b.width * b.height * 3/2);
                index++;
            }
            break;
        default:
            CAMHAL_LOGE("%s:not support this format",__FUNCTION__);
            break;
    }

#ifdef PICTURE_DEWARP_ENABLE
    if (format == V4L2_PIX_FMT_NV21)
        ion->free_buffer(outbuf_fd);
#endif
    return 0; //get a new frame
}

int CaptureUseGe2d::captureYUYVframe(uint8_t *img, struct data_in* in) {
    uint8_t* src = nullptr;
    uint32_t format = mInfo->get_preview_pixelformat();
    src = in->src;
    if (src) {
        switch (format) {
            case V4L2_PIX_FMT_YUYV:
                break;
            default:
                CAMHAL_LOGE("Unable known sensor format: %d", format);
                break;
        }
        return 0;
    }

    src = (uint8_t *)mInfo->get_frame();
    if (nullptr == src) {
        CAMHAL_LOGV("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }

    if (format == V4L2_PIX_FMT_YUYV)
        memcpy(img, src, mInfo->get_preview_buf_length());

    return 0;
}

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
static bool isNeedDestroyDewarp (dewarpInfo &info_exist, dewarpInfo &info) {
    if (info_exist.o_width && info_exist.o_height && info_exist.i_width && info_exist.i_height
            && info.i_width && info.i_height && info.o_width && info.o_height) {
        if ((info_exist.o_width != info.o_width) || (info_exist.o_height != info.o_height)
                || (info_exist.i_width != info.i_width) || (info_exist.i_height != info.i_height)) {
            return true;
        }
    }
    return false;
}
#endif

int CaptureUseGe2d::captureNV21frame(StreamBuffer b, struct data_in* in) {
    ATRACE_CALL();
    uint32_t width = mInfo->get_preview_width();
    uint32_t height = mInfo->get_preview_height();
    uint32_t stride = mInfo->get_preview_stride();
    uint32_t format = mInfo->get_preview_pixelformat();
    int dmabuf_fd = -1;
    if (in->src) {
        switch (in->src_fmt) {
            case V4L2_PIX_FMT_NV21:
                //  we assume that [in] is always preview stream
                if (width >= b.width && height >= b.height) {
                    mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height, in->dmabuf_fd, width, height);
                    CAMHAL_LOGD("ge2d-rec %d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
                           __LINE__, b.width, b.height, b.share_fd, width, height, in->dmabuf_fd);
                    if (property_get_bool("vendor.camhal.dump", false)) {
                        char path[256];
                        static int index = 0;
                        if (index % 10 == 0) {
                            sprintf(path, "/data/vendor/camera/video-in-%dx%d-%d.yuv",
                                width, height, index);
                            dump2File(path, in->src, width*height*3/2);
                            sprintf(path, "/data/vendor/camera/video-out-%dx%d-%d.yuv", b.width, b.height, index);
                            dump2File(path, b.img, b.width * b.height * 3/2);
                        }
                        index++;
                    }
                } else {
                    struct VideoInfoBuffer vb_rec;
                    auto ret = mInfo->get_record_buffer(&vb_rec);
                    int dmabuf_fd_rec = vb_rec.dma_fd;
                    if (-1 == ret || -1 == dmabuf_fd_rec) {
                        CAMHAL_LOGE("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
                        usleep(5000);
                        return ERROR_FRAME; // no frame data
                    }
                    if (mInfo->get_record_width() != b.width || mInfo->get_record_height() != b.height) {
                        //  this is snapshot capture case
                        CAMHAL_LOGW("%s:config miss match src: %dx%d, dst: %dx%d", __FUNCTION__,
                            mInfo->get_record_width(), mInfo->get_record_height(), b.width, b.height);
                        mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          dmabuf_fd_rec, mInfo->get_record_width(), mInfo->get_record_height());
                    } else {
#ifdef PREVIEW_DEWARP_ENABLE
                        if (property_get_bool("vendor.camhal.use.dewarp.rec", true)) { //dewarp rec
                            DeWarp* GDCObj = nullptr;
                            dewarpInfo dewarpInfo;
                            dewarpcam2port port = DEWARP_CAM2PORT_RECORD;
                            //  fill dewarp info for check dewarp config
                            {
                                dewarpInfo.i_width  = mInfo->get_record_width();
                                dewarpInfo.i_height = mInfo->get_record_height();
                                dewarpInfo.o_width  = b.width;
                                dewarpInfo.o_height = b.height;
                            }
                            bool needDestroy = isNeedDestroyDewarp(mPreDewarpInfo[port], dewarpInfo);
                            if (needDestroy) {
                                DeWarp::putInstance(port);
                            }
                            CameraConfig* config = CameraConfig::getInstance(port);
                            config->setOutputWidth(b.width);
                            config->setOutputHeight(b.height);
                            config->setOutputStride(b.stride);
                            config->setInputWidth(mInfo->get_record_width());
                            config->setInputHeight(mInfo->get_record_height());
                            CAMHAL_LOGD("dewarp-rec %d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
                                  __LINE__, b.width, b.height, b.share_fd, config->getInputWidth(),
                                  config->getInputHeight(), dmabuf_fd_rec);
                            if (property_get_bool("vendor.camhal.use.dewarp.linear", true)) {
                                GDCObj = DeWarp::getInstance(port, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
                            } else {
                                GDCObj = DeWarp::getInstance(port, PROJ_MODE_EQUISOLID, Rotation::ROTATION_0);
                            }
                            if (GDCObj) {
                                GDCObj->mInput_fd = dmabuf_fd_rec;
                                GDCObj->mOutput_fd = b.share_fd;
                                GDCObj->gdc_do_fisheye_correction();
                            }
                            mPreDewarpInfo[port].o_width  = b.width;
                            mPreDewarpInfo[port].o_height = b.height;
                            mPreDewarpInfo[port].i_width  = mInfo->get_record_width();
                            mPreDewarpInfo[port].i_height = mInfo->get_record_height();
                        }
#else
                        mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          dmabuf_fd_rec, mInfo->get_record_width(), mInfo->get_record_height());
#endif
                    }
                    ret = mInfo->putback_record_frame();
                    if (property_get_bool("vendor.camhal.dump", false)) {
                        char path[256];
                        static int index = 0;
                        if (index % 10 == 0) {
                            sprintf(path, "/data/vendor/camera/video-in-%dx%d-%d.yuv",
                                mInfo->get_record_width(), mInfo->get_record_height(), index);
                            dump2File(path, vb_rec.addr, mInfo->get_record_buf_length());
                            sprintf(path, "/data/vendor/camera/video-out-%dx%d-%d.yuv", b.width, b.height, index);
                            dump2File(path, b.img, b.width * b.height * 3/2);
                        }
                        index++;
                    }
                }
                break;
            default:
                CAMHAL_LOGE("Unable known sensor format: %d", mInfo->get_preview_pixelformat());
                break;
        }
        return NO_NEW_FRAME;
    }

    struct VideoInfoBuffer vb;
    int ret = mInfo->get_frame_buffer(&vb);
    dmabuf_fd = vb.dma_fd;
    if (-1 == ret || -1 == dmabuf_fd) {
        CAMHAL_LOGV("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
        usleep(5000);
        return ERROR_FRAME;
    }
#ifdef PREVIEW_DEWARP_ENABLE
        if (property_get_bool("vendor.camhal.use.dewarp", false)) {//dewarp
            DeWarp* GDCObj = nullptr;
            dewarpInfo dewarpInfo;
            dewarpcam2port port = DEWARP_CAM2PORT_PREVIEW;
            //  fill dewarp info for check dewarp config
            {
                dewarpInfo.i_width  = width;
                dewarpInfo.i_height = height;
                dewarpInfo.o_width  = b.width;
                dewarpInfo.o_height = b.height;
            }
            bool needDestroy = isNeedDestroyDewarp(mPreDewarpInfo[port], dewarpInfo);
            if (needDestroy) {
                DeWarp::putInstance(port);
            }
            CameraConfig* config = CameraConfig::getInstance(port);
            config->setOutputWidth(b.width);
            config->setOutputHeight(b.height);
            config->setOutputStride(b.stride);
            config->setInputWidth(width);
            config->setInputHeight(height);
            CAMHAL_LOGD("dewarp-prev %d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
                __LINE__, b.width, b.height, b.share_fd, config->getInputWidth(),
                    config->getInputHeight(), dmabuf_fd);
            if (property_get_bool("vendor.camhal.use.dewarp.linear", true)) {
                GDCObj = DeWarp::getInstance(port, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
            } else {
                GDCObj = DeWarp::getInstance(port, PROJ_MODE_EQUISOLID, Rotation::ROTATION_0);
            }
            if (GDCObj) {
                GDCObj->mInput_fd = dmabuf_fd;
                GDCObj->mOutput_fd = b.share_fd;
                GDCObj->gdc_do_fisheye_correction();
            }
            mPreDewarpInfo[port].o_width  = b.width;
            mPreDewarpInfo[port].o_height = b.height;
            mPreDewarpInfo[port].i_width  = width;
            mPreDewarpInfo[port].i_height = height;
        }
        else {
            switch (format) {
                case V4L2_PIX_FMT_NV21:
                    if (width == b.width && height == b.height && stride == b.stride) {
                        CAMHAL_LOGV("line %d ge2d copy dmabuf_fd %d  w %d stride %d h %d \n", __LINE__, dmabuf_fd, b.width, b.stride, b.height);
                        mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride, b.height, ge2dTransform::NV12);
                    } else {
                        CAMHAL_LOGV("line %d ge2d scale in w %d stride %d h %d , out w %d stride %d h %d", __LINE__, width, stride, height,
                                  b.width, b.stride, b.height);
                        mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.stride, b.height,
                                                  dmabuf_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, width, stride, height);
                    }

                    break;
                case V4L2_PIX_FMT_UYVY:
                    CAMHAL_LOGV("line %d ge2d_convert_scale b fd %d w %d stride %d h %d; src fd %d w %d h %d \n", __LINE__,
                                   b.share_fd, b.width, b.stride, b.height, dmabuf_fd, width, height);

                    mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCrCb_420_SP, b.width, b.width, b.height,
                                               dmabuf_fd, PIXEL_FORMAT_YCbCr_422_UYVY, width, width * 2, height);

                    break;
                default:
                    break;
            }
        }
        if (property_get_bool("vendor.camhal.dump", false)) {
            char path[256];
            static int index2 = 0;
            if (index2 % 10 == 0) {
                sprintf(path, "/data/vendor/camera/preview-in-%dx%d-%d.yuv",
                    mInfo->get_preview_width(), mInfo->get_preview_height(), index2);
                dump2File(path, vb.addr, mInfo->get_preview_buf_length());
                sprintf(path, "/data/vendor/camera/preview-out-%dx%d-%d.yuv", b.width, b.height, index2);
                dump2File(path, b.img, b.width *  b.height * 3/2);
            }
            index2++;
        }
#else
    switch (format) {
        case V4L2_PIX_FMT_NV21:
            if (width == b.width && height == b.height && stride == b.stride) {
                CAMHAL_LOGV("line %d ge2d copy dmabuf_fd %d  w %d stride %d h %d \n", __LINE__, dmabuf_fd, b.width, b.stride, b.height);
                mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride,b.height, ge2dTransform::NV12);
            } else {
                CAMHAL_LOGV("line %d ge2d scale in w %d stride %d h %d , out w %d stride %d h %d", __LINE__, width, stride, height,
                          b.width, b.stride, b.height);
                mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.stride, b.height,
                                          dmabuf_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, width, stride, height);
            }
            break;
        case V4L2_PIX_FMT_UYVY:
            CAMHAL_LOGV("line %d ge2d_convert_scale b fd %d w %d stride %d h %d; src fd %d w %d h %d \n", __LINE__,
                           b.share_fd, b.width, b.stride, b.height, dmabuf_fd, width, height);

            mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCrCb_420_SP, b.width, b.width, b.height,
                                       dmabuf_fd, PIXEL_FORMAT_YCbCr_422_UYVY, width, width * 2, height);
            break;
        default:
            break;
    }
#endif
    in->src = (uint8_t *)vb.addr;
    in->dmabuf_fd = dmabuf_fd;
    in->src_fmt = V4L2_PIX_FMT_NV21;
    return NEW_FRAME;
}

int CaptureUseGe2d::captureYV12frame(StreamBuffer b, struct data_in* in) {
        //ATRACE_CALL();
        int dmabuf_fd = -1;
        uint32_t format = mInfo->get_preview_pixelformat();

        struct VideoInfoBuffer vb;
        int ret = mInfo->get_frame_buffer(&vb);
        if (-1 == ret) {
            CAMHAL_LOGV("get frame NULL, sleep 5ms");
            usleep(5000);
            return -1;
        }
        dmabuf_fd = vb.dma_fd;
        switch (format) {
            case V4L2_PIX_FMT_YVU420:
                if (mInfo->get_preview_buf_length() == b.width * b.height * 3/2) {
                    mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride, b.height, ge2dTransform::NV12);
                }
                break;
            default:
                break;
            }
            return 0;
}

int CaptureUseGe2d::captureRGBAframe(StreamBuffer b, struct data_in* in){
    uint32_t width = mInfo->get_preview_width();
    uint32_t height = mInfo->get_preview_height();
    uint32_t format = mInfo->get_preview_pixelformat();
    uint8_t *src = nullptr;
    int dmabuf_fd = -1;

    src = in->src;
    if (src && in->src_fmt > 0) {
        switch (in->src_fmt) {
            case V4L2_PIX_FMT_NV21:
                break;
                default:
                    CAMHAL_LOGE("Unable known sensor format: %d", mInfo->get_preview_pixelformat());
                    break;
        }
        return NO_NEW_FRAME;
    }
    struct VideoInfoBuffer vb;
    int ret = mInfo->get_frame_buffer(&vb);
    if (-1 == ret) {
        CAMHAL_LOGV("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }
    dmabuf_fd = vb.dma_fd;
    switch (format) {
        case V4L2_PIX_FMT_NV21:
            mGE2D->ge2d_fmt_convert(b.share_fd, PIXEL_FORMAT_RGBA_8888, b.stride, b.height,
                                                  dmabuf_fd,  PIXEL_FORMAT_YCrCb_420_SP, width, height);
            break;
        default:
            CAMHAL_LOGE("error: only support NV21 -> RGBA");
            break;
    }
    return NEW_FRAME;
}


int CaptureUseGe2d::captureDPTZframe(StreamBuffer b, struct data_in * in) {
    CAMHAL_LOGVV("%s", __FUNCTION__);
    ATRACE_CALL();
#ifdef CAM_DPTZ

    uint32_t width_dptz = mInfo->get_preview_width();
    uint32_t height_dptz = mInfo->get_preview_height();

    uint32_t width_detect = mInfo->get_record_width();
    uint32_t height_detect = mInfo->get_record_height();

    int dmabuf_fd_dptz = -1;
    int dmabuf_fd_detect = -1;

    CAMHAL_LOGV("%s width_dptz %d, height_dptz %d, width_detect %d, height_detect %d",
                __FUNCTION__, width_dptz, height_dptz, width_detect, height_detect);

    int ret = 0;
    //1、get frame from preview and record channel
    struct VideoInfoBuffer vb_preview;
    ret = mInfo->get_frame_buffer(&vb_preview);
    dmabuf_fd_dptz = vb_preview.dma_fd;
    if (-1 == ret) {
        CAMHAL_LOGE("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }
    struct VideoInfoBuffer vb_record;
    ret = mInfo->get_record_buffer(&vb_record);
    dmabuf_fd_detect = vb_record.dma_fd;
    if (-1 == ret || -1 == dmabuf_fd_detect) {
        CAMHAL_LOGE("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }
    //2、convert the frame from record to RGB and detect
    if (mRGBIonFd < 0) {
        IONInterface *ion = IONInterface::get_instance();
        mRGBIonVa = ion->alloc_buffer(NN_RGB_WIDTH * NN_RGB_HEIGHT * 3, &mRGBIonFd);
        if (mRGBIonFd < 0) {
            ALOGE("allocate buffer fail");
            return ERROR_FRAME;
        }
        ion->put_instance();
    }
    mGE2D->ge2d_fmt_convert(mRGBIonFd, PIXEL_FORMAT_RGB_888, NN_RGB_WIDTH, NN_RGB_HEIGHT, \
         dmabuf_fd_detect, PIXEL_FORMAT_YCrCb_420_SP, width_detect, height_detect);
     if (property_get_bool("vendor.camhal.dump.detect", false)) {
        char dataPath[256];
        static int frameIndex = 0;
        sprintf(dataPath, "/data/vendor/camera/rgb-%dx%d_%d.rgb", NN_RGB_WIDTH, NN_RGB_HEIGHT, frameIndex);
        dump2File(dataPath, mRGBIonVa, NN_RGB_WIDTH * NN_RGB_HEIGHT * 3);
        sprintf(dataPath, "/data/vendor/camera/yuv-%dx%d_%d.yuv", width_detect, height_detect, frameIndex);
        dump2File(dataPath, vb_record.addr, width_detect * height_detect * 3 / 2);
        frameIndex ++;
    }
    mInfo->putback_record_frame();
    nn_rgb_frame_t rgb_frame;
    rgb_frame.data_ptr = mRGBIonVa;
    rgb_frame.width    = NN_RGB_WIDTH;
    rgb_frame.height   = NN_RGB_HEIGHT;

    ssize_t crop_x = 0, crop_y = 0, crop_w = b.width, crop_h = b.height;
    ssize_t face_center_x = -1, face_center_y = -1, face_width = 0, face_height = 0;

    bool needDetect = false;
    if (mDectNum++ % 5 == 0 || mPrevCenter_x == -1 || mPrevCenter_y == -1)
        needDetect = true;
    if (needDetect) {
        CAMHAL_LOGV("AICam[%zu] detect +", mDectNum);
        ret = center_face_network_process_aml(centerface_network, rgb_frame);
        if (ret != 0) {
            ALOGE("The face_attv2 inference fail\n");
            return ERROR_FRAME;
        } else {
            const float ratio_x = (float) width_dptz / (float) NN_RGB_WIDTH;
            const float ratio_y = (float) height_dptz / (float) NN_RGB_HEIGHT;
            CAMHAL_LOGV("AICam[%zu] detect %d faces", mDectNum, centerface_network->faceNum);
            if (centerface_network->faceNum > 0) {
                ssize_t maxFace = 0;
                for (int i = 0; i < centerface_network->faceNum; i++) {
                    ssize_t face_x = (ssize_t)(centerface_network->cFace[i].cBox.f32X1 * ratio_x);
                    ssize_t face_y = (ssize_t)(centerface_network->cFace[i].cBox.f32Y1 * ratio_y);
                    ssize_t face_w = (ssize_t)((centerface_network->cFace[i].cBox.f32X2 - centerface_network->cFace[i].cBox.f32X1) * ratio_x);
                    ssize_t face_h = (ssize_t)((centerface_network->cFace[i].cBox.f32Y2 - centerface_network->cFace[i].cBox.f32Y1) * ratio_y);
                    if (maxFace < face_w * face_h) {
                        crop_x = face_x;
                        crop_y = face_y;
                        crop_w = face_w;
                        crop_h = face_h;
                        maxFace = face_w * face_h;
                    }
                    CAMHAL_LOGV("AICam[%zu] face[%d] rect (x%zd, y%zd, w%zd, h%zd)", mDectNum, i, face_x, face_y, face_w, face_h);
                }
                face_center_x = crop_x + crop_w/2;
                face_center_y = crop_y + crop_h/2;
                face_width    = crop_w;
                face_height   = crop_h;
                CAMHAL_LOGV("AICam[%zu] face_center (%zd %zd)", mDectNum, face_center_x, face_center_y);
            }
        }
    }

    bool keep_crop = false;

    if (smooth_enable) {
        if (!mSmoothing) {
            if (needDetect == true && (mPrevCenter_x == -1 || mPrevCenter_y == -1)) {
                mPrevCenter_x = face_center_x;
                mPrevCenter_y = face_center_y;
                CAMHAL_LOGV("AICam[%zu] init state", mDectNum);
            } else if (needDetect == false || (face_center_x == -1 || face_center_y == -1)) {
                face_center_x = mPrevCenter_x;
                face_center_y = mPrevCenter_y;
                CAMHAL_LOGV("AICam[%zu] cache state", mDectNum);
            } else if (fabs(mPrevCenter_x - face_center_x) < smooth_step ||
                       fabs(mPrevCenter_y - face_center_y) < smooth_step) {
                face_center_x = mPrevCenter_x;
                face_center_y = mPrevCenter_y;
                CAMHAL_LOGV("AICam[%zu] shift state", mDectNum);
            } else if (face_center_x >= 0 && face_center_y >= 0) {
                mSmoothing = true;
                mPrevCenter_dstx = face_center_x;
                mPrevCenter_dsty = face_center_y;
                mSmoothing_x  = mPrevCenter_x;
                mSmoothing_y  = mPrevCenter_y;
                CAMHAL_LOGV("AICam[%zu] smoothing state: start face_center src (%zd %zd) dst (%zd %zd)",
                      mDectNum, mPrevCenter_x, mPrevCenter_y, mPrevCenter_dstx, mPrevCenter_dsty);
            }
        } else if (needDetect) {
            if (face_center_x == -1 || face_center_y == -1) {
                mPrevCenter_dstx = width_dptz/2;
                mPrevCenter_dsty = height_dptz/2;
            } else if (fabs(mSmoothing_x - face_center_x) >= smooth_step ||
                       fabs(mSmoothing_y - face_center_y) >= smooth_step) {
                mPrevCenter_dstx = face_center_x;
                mPrevCenter_dsty = face_center_y;
                mPrevCenter_x = mSmoothing_x;
                mPrevCenter_y = mSmoothing_y;
            }
            CAMHAL_LOGV("AICam[%zu] smoothing update face_center src (%zd %zd) dst (%zd %zd)",
                  mDectNum, mPrevCenter_x, mPrevCenter_y, mPrevCenter_dstx, mPrevCenter_dsty);
        }
        if (mSmoothing) {
            if (fabs(mPrevCenter_dstx - mSmoothing_x) < smooth_step &&
                    fabs(mPrevCenter_dsty - mSmoothing_y) >= smooth_step) {
                if (mPrevCenter_dsty > mSmoothing_y) {
                    mSmoothing_y += smooth_step;
                    if (mSmoothing_y > height_dptz)
                        mSmoothing_y = height_dptz;
                } else {
                    mSmoothing_y -= smooth_step;
                    if (mSmoothing_y < 0)
                        mSmoothing_y = 0;
                }
                face_center_x = mSmoothing_x;
                face_center_y = mSmoothing_y;
            } else if (fabs(mPrevCenter_dstx - mSmoothing_x) >= smooth_step &&
                       fabs(mPrevCenter_dsty - mSmoothing_y) < smooth_step) {
                if (mPrevCenter_dstx > mSmoothing_x) {
                    mSmoothing_x += smooth_step;
                    if (mSmoothing_x > width_dptz)
                        mSmoothing_x = width_dptz;
                } else {
                    mSmoothing_x -= smooth_step;
                    if (mSmoothing_x < 0)
                        mSmoothing_x = 0;
                }
                face_center_y = mSmoothing_y;
                face_center_x = mSmoothing_x;
            } else if (fabs(mPrevCenter_dstx - mSmoothing_x) >= smooth_step &&
                       fabs(mPrevCenter_dsty - mSmoothing_y) >= smooth_step) {
                if (mPrevCenter_dstx > mSmoothing_x) {
                    mSmoothing_x += smooth_step;
                    if (mSmoothing_x > width_dptz)
                        mSmoothing_x = width_dptz;
                    mSmoothing_y = ceil((float)(mSmoothing_x-mPrevCenter_x)*(mPrevCenter_dsty-mPrevCenter_y)/(float)(mPrevCenter_dstx-mPrevCenter_x)) + mPrevCenter_y;
                    face_center_x = mSmoothing_x;
                    face_center_y = mSmoothing_y;
                } else {
                    mSmoothing_x -= smooth_step;
                    if (mSmoothing_x < 0)
                        mSmoothing_x = 0;
                    mSmoothing_y = ceil((float)(mSmoothing_x-mPrevCenter_x)*(mPrevCenter_dsty-mPrevCenter_y)/(float)(mPrevCenter_dstx-mPrevCenter_x)) + mPrevCenter_y;
                    face_center_x = mSmoothing_x;
                    face_center_y = mSmoothing_y;
                }
            } else {
                CAMHAL_LOGV("AICam[%zu] smoothing state no need move", mDectNum);
            }
            if (fabs(face_center_x - mPrevCenter_dstx) < smooth_step && fabs(face_center_y - mPrevCenter_dsty) < smooth_step) {
                mSmoothing = false;
                mPrevCenter_x = mPrevCenter_dstx;
                mPrevCenter_y = mPrevCenter_dsty;
                CAMHAL_LOGV("AICam[%zu] smoothing state end", mDectNum);
            }
        }
        CAMHAL_LOGV("AICam[%zu] Smoothing(%s) face_center src (%zd %zd) dst (%zd %zd) current (%zd %zd)",
            mDectNum, mSmoothing ? "enable" : "disable", mPrevCenter_x, mPrevCenter_y, mPrevCenter_dstx, mPrevCenter_dsty, face_center_x, face_center_y);

        bool reset = false;
        crop_x = 0;
        crop_y = 0;
        crop_w = b.width; //640
        crop_h = b.height; //480
        if (face_center_x >= b.width/2 && face_center_x <= width_dptz - b.width/2) {
            crop_x = face_center_x - b.width/2;
        } else if (face_center_x < b.width/2 && face_center_x > face_width/2) {
            crop_x = 0;
        } else if (face_center_x > width_dptz - b.width/2 && face_center_x < width_dptz - face_width/2) {
            crop_x = width_dptz - b.width;
        } else {
            reset = true;
            CAMHAL_LOGV("AICam[%zu] reset x:face_center_x %zd margin %zd %zd %zd",
                mDectNum, face_center_x, (ssize_t)b.width/2, (ssize_t)width_dptz - b.width/2, (ssize_t)width_dptz - face_width/2);
        }

        if (face_center_y >= b.height/2 && face_center_y <= height_dptz - b.height/2) {
            crop_y = face_center_y - b.height/2;
        } else if (face_center_y < b.height/2 && face_center_y > face_height/2) {
            crop_y = 0;
        } else if (face_center_y > height_dptz - b.height/2 && face_center_y < height_dptz - face_height/2) {
            crop_y = height_dptz - b.height;
        } else {
            reset = true;
            CAMHAL_LOGV("AICam[%zu] reset y:face_center_y %zd margin %zd %zd %zd",
                mDectNum, face_center_y, (ssize_t)b.height/2, (ssize_t)height_dptz - b.height/2, (ssize_t)height_dptz - face_height/2);
        }

        if (mPrevCrop_x > 0 && mPrevCrop_y > 0) {
            if (reset) {
                CAMHAL_LOGV("AICam[%zu] resetting smooth invalid cropinfo (%zd %zd %zd %zd), recover cropinfo (%zd %zd %zd %zd)",
                    mDectNum, crop_x, crop_y, crop_w, crop_h, mPrevCrop_x, mPrevCrop_y, mPrevCrop_w, mPrevCrop_h);
                crop_x = mPrevCrop_x;
                crop_y = mPrevCrop_y;
                crop_w = mPrevCrop_w;
                crop_h = mPrevCrop_h;
                keep_crop = true;
            } else if ((face_width > 0 && fabs(mPrevCrop_x - crop_x) > face_width) ||
                       (face_height > 0 && fabs(mPrevCrop_y - crop_y) > face_height)) {
                CAMHAL_LOGV("AICam[%zu] smooth invalid cropinfo (%zd %zd %zd %zd), recover cropinfo (%zd %zd %zd %zd)",
                    mDectNum, crop_x, crop_y, crop_w, crop_h, mPrevCrop_x, mPrevCrop_y, mPrevCrop_w, mPrevCrop_h);
                crop_x = mPrevCrop_x;
                crop_y = mPrevCrop_y;
                crop_w = mPrevCrop_w;
                crop_h = mPrevCrop_h;
                keep_crop = true;
            } else {
                mPrevCrop_x = crop_x;
                mPrevCrop_y = crop_y;
                mPrevCrop_w = crop_w;
                mPrevCrop_h = crop_h;
            }
        } else {
            mPrevCrop_x = crop_x;
            mPrevCrop_y = crop_y;
            mPrevCrop_w = crop_w;
            mPrevCrop_h = crop_h;
        }
        CAMHAL_LOGV("AICam[%zu] cropinfo (%zd %zd %zd %zd)", mDectNum, crop_x, crop_y, crop_w, crop_h);
    }
    if (dewarp_enable) {
#ifdef PREVIEW_DEWARP_ENABLE
        char property[PROPERTY_VALUE_MAX];
        DeWarp* GDCObj = nullptr;
        property_get("vendor.camhal.use.dewarp.linear", property, "true");
        CameraConfig* config = CameraConfig::getInstance(DEWARP_CAM2PORT_DPTZ_PREVIEW);
        config->setOutputWidth(b.width);
        config->setOutputHeight(b.height);
        config->setOutputStride(b.stride);
        config->setInputWidth(width_dptz);
        config->setInputHeight(height_dptz);
        if (!keep_crop) {
            CropInfo cropInfo;
            cropInfo.width     = crop_w;
            cropInfo.height    = crop_h;
            cropInfo.srcWidth  = width_dptz;
            cropInfo.srcHeight = height_dptz;
            cropInfo.offset_x  = ((crop_x + crop_w/2) - width_dptz/2)  + 1;
            cropInfo.offset_y  = ((crop_y + crop_h/2) - height_dptz/2) + 1;
            config->setCropInfo(cropInfo);
            CAMHAL_LOGD("%s-%d crop srcWidth:%d srcHeight:%d crop width:%d height:%d offset_x:%d offset_y:%d",
                  __FUNCTION__, __LINE__, cropInfo.width, cropInfo.height, cropInfo.srcWidth,
                                          cropInfo.srcHeight, cropInfo.offset_x, cropInfo.offset_y);
        }
        CAMHAL_LOGD("%s-%d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
              __FUNCTION__, __LINE__, b.width, b.height, b.share_fd,
              config->getOutputWidth(), config->getOutputHeight(), dmabuf_fd_detect);
        if (strstr(property, "true")) {
            GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_DPTZ_PREVIEW, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
        } else {
            GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_DPTZ_PREVIEW, PROJ_MODE_EQUISOLID, Rotation::ROTATION_0);
        }
        if (!keep_crop)
            GDCObj->setCrop();
        if (GDCObj) {
            GDCObj->mInput_fd = dmabuf_fd_dptz;
            GDCObj->mOutput_fd = b.share_fd;
            GDCObj->gdc_do_fisheye_correction();
        }
#endif
    } else {
         mGE2D->ge2d_scale2(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                            dmabuf_fd_dptz, width_dptz, height_dptz, crop_x, crop_y, crop_w, crop_h);
    }
    return ret;
#else
    ALOGD("CAM_DPTZ not enable");
    return 0;
#endif
}

}
