#ifndef __MIPI_CAMERA_IO__
#define __MIPI_CAMERA_IO__

#define LOG_NDEBUG 0
#define LOG_TAG "MIPI_Camera_IO"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>

#include <errno.h>
#include <cutils/properties.h>
#include "Base.h"
#include "MIPICameraIO.h"

namespace android {

#if 0

MIPIVideoInfo::MIPIVideoInfo(int preview , int snapshot) {
    ALOGD("mipi video info constructer!");
    mPreviewFd = preview;
    mSnapFd = snapshot;
}

MIPIVideoInfo::~MIPIVideoInfo() {
    ALOGD("mipi video info destructer!");
    mPreviewFd = -1;
    mSnapFd = -1;
    for (uint32_t i = 0; i < mem.size(); i++) {
        if (mem[i].dma_fd != -1)
            close(mem[i].dma_fd);
    }
}

//----get dmabuf file descritor according to index
int MIPIVideoInfo::export_dmabuf_fd(int v4lfd, int index, int* dmafd)
{
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf,0,sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = index;
    expbuf.flags = 0;
    expbuf.fd = -1;
    if (ioctl(v4lfd,VIDIOC_EXPBUF,&expbuf) == -1) {
        ALOGE("export buffer fail:%s",strerror(errno));
        return -1;
    } else {
        ALOGD("dma buffer fd = %d \n",expbuf.fd);
        *dmafd = expbuf.fd;
    }
    return 0;
}

//
int MIPIVideoInfo::start_capturing(void)
{
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;

        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (isStreaming)
            ALOGD("already stream on\n");

        //----allocate memory
        CLEAR(preview.rb);
        mem.resize(IO_PREVIEW_BUFFER);
        preview.rb.count = IO_PREVIEW_BUFFER;
        preview.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
            DBG_LOGB("camera idx:%d does not support "
                      "memory mapping, errno=%d\n", idx, errno);
        }
        if (preview.rb.count < 2) {
            DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                            idx, errno);
            return -EINVAL;
        }

        //----map memory to user space
        for (i = 0; i < (int)preview.rb.count; ++i) {
            CLEAR(preview.buf);
            preview.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            preview.buf.memory      = V4L2_MEMORY_MMAP;
            preview.buf.index       = i;

            if (ioctl(mPreviewFd, VIDIOC_QUERYBUF, &preview.buf) < 0) {
                    DBG_LOGB("VIDIOC_QUERYBUF, errno=%d", errno);
            }

            mem[i].size = preview.buf.length;
            mem[i].addr = mmap(NULL, // start anywhere
                                mem[i].size,
                                PROT_READ | PROT_WRITE, // required
                                MAP_SHARED, // recommended
                                mPreviewFd,        //video device fd
                                preview.buf.m.offset);

            if (MAP_FAILED == mem[i].addr) {
                ALOGE("mmap failed,%s\n", strerror(errno));
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(mPreviewFd,i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem[i].dma_fd = dma_fd;
                ALOGD("index = %d, dma_fd = %d \n",i,mem[i].dma_fd);
            }
        }
        //----queue buffer to driver's video buffer queue
        for (i = 0; i < (int)preview.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(mPreviewFd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }
        //----stream on----
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
             if (ioctl(mPreviewFd, VIDIOC_STREAMON, &type) < 0)
                  DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        }

        isStreaming = true;
        return 0;
}

int MIPIVideoInfo::stop_capturing(void)
{
        enum v4l2_buf_type type;
        int res = 0;
        int i;
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (!isStreaming)
                return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mPreviewFd, VIDIOC_STREAMOFF, &type) < 0) {
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);
                res = -1;
        }

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }

        for (i = 0; i < (int)preview.rb.count; ++i) {
                if (munmap(mem[i].addr, mem[i].size) < 0) {
                        DBG_LOGB("munmap failed errno=%d", errno);
                        res = -1;
                }
        }

        if (strstr((const char *)cap.driver, "ARM-camera-isp")) {
            preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            preview.rb.memory = V4L2_MEMORY_MMAP;
            preview.rb.count = 0;

            res = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
            if (res < 0) {
                DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
            }else{
                DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
            }
        }

        isStreaming = false;
        return res;
}

int MIPIVideoInfo::releasebuf_and_stop_capturing(void)
{
        enum v4l2_buf_type type;
        int res = 0 ,ret;
        int i;

        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (!isStreaming)
                return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
            if (ioctl(mPreviewFd, VIDIOC_STREAMOFF, &type) < 0) {
                 DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);
                 res = -1;
            }
        }
        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }
        for (i = 0; i < (int)preview.rb.count; ++i) {
                if (munmap(mem[i].addr, mem[i].size) < 0) {
                        DBG_LOGB("munmap failed errno=%d", errno);
                        res = -1;
                }
        }
        isStreaming = false;

        preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        preview.rb.count = 0;

        ret = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
           DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
           //return ret;
        }else{
           DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        return res;
}


uintptr_t MIPIVideoInfo::get_frame_phys(void)
{
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        CLEAR(preview.buf);

        preview.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(mPreviewFd, VIDIOC_DQBUF, &preview.buf) < 0) {
                switch (errno) {
                        case EAGAIN:
                                return 0;
                        case EIO:
                        default:
                                DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
                                exit(1);
                }
        DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
        }

        return (uintptr_t)preview.buf.m.userptr;
}

int MIPIVideoInfo::get_frame_index_by_fd(FrameV4L2Info& info,int fd) {
       if (fd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        CLEAR(info.buf);
        info.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        info.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &info.buf) < 0) {
            switch (errno) {
                case EAGAIN:
                    return -1;

                case EIO:
                default:
                    CAMHAL_LOGDB("VIDIOC_DQBUF failed, errno=%d\n", errno);
                    //here will generate crash, so delete.  when ocour error, should break while() loop
                    //exit(1);
                    if (errno == ENODEV) {
                        ALOGE("camera device is not exist!");
                        set_device_status();
                        close(fd);
                        fd = -1;
                    }
                    return -1;
            }
        }
        return info.buf.index;
}

int MIPIVideoInfo::get_frame_buffer(struct VideoInfoBuffer* b)
{
    int index = get_frame_index_by_fd(preview,mPreviewFd);
    ALOGD("%s:index = %d \n",__FUNCTION__,index);
    if (index < 0)
        return -1;
    else {
        index = index % IO_PREVIEW_BUFFER;
        ALOGD("%s: index=%d,dma_fd=%d\n",__FUNCTION__,index,mem[index].dma_fd);
        b->addr = mem[index].addr;
        b->size = mem[index].size;
        b->dma_fd = mem[index].dma_fd;
    }
    return 0;
}

int MIPIVideoInfo::putback_frame()
{
        if (dev_status == -1)
            return 0;

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (ioctl(mPreviewFd, VIDIOC_QBUF, &preview.buf) < 0) {
            DBG_LOGB("QBUF failed :%s\n", strerror(errno));
            if (errno == ENODEV) {
                set_device_status();
            }
        }
        ALOGD("%s:\n",__FUNCTION__);

        return 0;
}

int MIPIVideoInfo::putback_picture_frame()
{
        if (mSnapFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (ioctl(mSnapFd, VIDIOC_QBUF, &picture.buf) < 0)
                DBG_LOGB("QBUF failed error=%d\n", errno);

        return 0;
}

int MIPIVideoInfo::start_picture(int rotate)
{
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        bool usbcamera = false;

        CLEAR(picture.rb);
        if (mSnapFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        //step 1 : ioctl  VIDIOC_S_FMT
        for (int i = 0; i < 3; i++) {
            ret = ioctl(mSnapFd, VIDIOC_S_FMT, &picture.format);
            if (ret < 0 ) {
             switch (errno) {
                 case  -EBUSY:
                 case  0:
                    usleep(3000); //3ms
                    continue;
                    default:
                    DBG_LOGB("Open: VIDIOC_S_FMT Failed: %s, errno=%d\n", strerror(errno), ret);
                 return ret;
             }
            }else
            break;
        }
        //step 2 : request buffer
        picture.rb.count = IO_PICTURE_BUFFER;
        picture.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(mSnapFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0 ) {
                DBG_LOGB("camera idx:%d does not support "
                                "memory mapping, errno=%d\n", idx, errno);
        }

        if (picture.rb.count < 1) {
                DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                                idx, errno);
                return -EINVAL;
        }

        //step 3: mmap buffer
        for (i = 0; i < (int)picture.rb.count; ++i) {
            CLEAR(picture.buf);
            picture.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            picture.buf.memory      = V4L2_MEMORY_MMAP;
            picture.buf.index       = i;

            if (ioctl(mSnapFd, VIDIOC_QUERYBUF, &picture.buf) < 0) {
                    ALOGE("VIDIOC_QUERYBUF, errno=%d", errno);
            }
            mem_pic[i].size = picture.buf.length;
            mem_pic[i].addr = mmap(NULL, // start anywhere
                                    mem_pic[i].size,
                                    PROT_READ | PROT_WRITE, // required
                                    MAP_SHARED, //recommended
                                    mSnapFd,
                                    picture.buf.m.offset);

            if (MAP_FAILED == mem_pic[i].addr) {
                    ALOGE("mmap failed, errno=%d\n", errno);
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(mSnapFd,i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem_pic[i].dma_fd = dma_fd;
            }
        }

        //step 4 : QBUF
        for (i = 0; i < (int)picture.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(mSnapFd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }

        if (isPicture) {
                DBG_LOGA("already stream on\n");
        }

        if (strstr((const char *)cap.driver, "uvcvideo")) {
            usbcamera = true;
        }

        //step 5: Stream ON
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapFd, VIDIOC_STREAMON, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        isPicture = true;

        return 0;
}

int MIPIVideoInfo::get_picture_fd()
{
    DBG_LOGA("get picture\n");
    int index = get_frame_index_by_fd(picture,mSnapFd);
    if (index < 0)
        return -1;
    return mem_pic[picture.buf.index].dma_fd;
}

void MIPIVideoInfo::stop_picture()
{
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        int i;
        int ret;
        if (mSnapFd < 0) {
            ALOGE("camera not be init!");
            return ;
        }

        if (!isPicture)
                return ;

        //QBUF
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = picture.buf.index;
        if (ioctl(mSnapFd, VIDIOC_QBUF, &buf) < 0)
            DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);

        //stream off and unmap buffer
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapFd, VIDIOC_STREAMOFF, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

        for (i = 0; i < (int)picture.rb.count; i++)
        {
            if (munmap(mem_pic[i].addr, mem_pic[i].size) < 0)
                DBG_LOGB("munmap failed errno=%d", errno);
        }
        picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;
        picture.rb.count = 0;

        ret = ioctl(mSnapFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0) {
            DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
        } else {
            DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        isPicture = false;
        setBuffersFormat();
        start_capturing();
}

void MIPIVideoInfo::releasebuf_and_stop_picture()
{
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        int i,ret;
        if (mSnapFd < 0) {
            ALOGE("camera not be init!");
            return;
        }

        if (!isPicture)
                return ;

        //QBUF
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = picture.buf.index;
        if (ioctl(mSnapFd, VIDIOC_QBUF, &buf) < 0)
            DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);

        //stream off and unmap buffer
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapFd, VIDIOC_STREAMOFF, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

        for (i = 0; i < (int)picture.rb.count; i++)
        {
            if (munmap(mem_pic[i].addr, mem_pic[i].size) < 0)
                DBG_LOGB("munmap failed errno=%d", errno);
        }

        isPicture = false;

        picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;
        picture.rb.count = 0;
        ret = ioctl(mSnapFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0) {
          DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
          //return ret;
        }else{
          DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        setBuffersFormat();
        start_capturing();
}
#endif
    void MIPIVideoInfo::set_index(int idx) {
        switch (mWorkMode) {
            case ONE_FD:
            mOneFd.idx = idx;
            break;
            case TWO_FD:
            mTwoFd.idx =idx;
            break;
            case PIC_SCALER:
            mPicScaler.idx =idx;
            break;
            default:
                break;
        }
    }

    uint32_t MIPIVideoInfo::get_index(void) {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.idx;
            case TWO_FD:
                return mTwoFd.idx;
            case PIC_SCALER:
                return mPicScaler.idx;
            default:
                return -1;
        }
    }

    void MIPIVideoInfo::set_fds(std::vector<int>& fds) {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.V4LDevicefd = fds[channel_preview];
                break;
            case TWO_FD:
                mTwoFd.mPreviewFd = fds[channel_preview];
                mTwoFd.mSnapShotFd = fds[channel_capture];
                break;
            case PIC_SCALER:
                if (fds.size() == 2) {
                    mPicScaler.mPreviewFd = fds[channel_preview];
                    mPicScaler.mSnapShotFd = fds[channel_capture];
                } else if (fds.size() == 3) {
                    mPicScaler.mRecordFd = fds[channel_record];
                    mPicScaler.mPreviewFd = fds[channel_preview];
                    mPicScaler.mSnapShotFd = fds[channel_capture];
                }
                break;
            default:
                break;
        }
    }

    int MIPIVideoInfo::get_fd(void) {
        switch (mWorkMode) {
        case ONE_FD:
             return mOneFd.V4LDevicefd;
        case TWO_FD:
            return mTwoFd.mPreviewFd;
        case PIC_SCALER:
            return mPicScaler.mPreviewFd;
         default:
            ALOGE("%s: work mode error",__FUNCTION__);
            return -1;
        }
    }

    int MIPIVideoInfo::camera_init(void) {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.camera_init();
            case TWO_FD:
                return mTwoFd.camera_init();
            case PIC_SCALER:
                return mPicScaler.camera_init();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }


    int MIPIVideoInfo::setBuffersFormat(void) {
        switch (mWorkMode) {
            case ONE_FD:
                 return mOneFd.setBuffersFormat();
            case TWO_FD:
                return mTwoFd.setBuffersFormat();
            case PIC_SCALER:
                return mPicScaler.setBuffersFormat();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    int MIPIVideoInfo::start_capturing(void) {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.start_capturing();
            case TWO_FD:
                return mTwoFd.start_capturing();
            case PIC_SCALER:
                return mPicScaler.start_capturing();
            default:
            ALOGE("%s: work mode error",__FUNCTION__);
             return -1;
        }
    }

    int MIPIVideoInfo::start_recording(void) {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.start_recording();
            default:
            ALOGE("%s: work mode error",__FUNCTION__);
             return -1;
        }
    }

    int MIPIVideoInfo::start_picture(int rotate) {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.start_picture(rotate);
            case TWO_FD:
                return mTwoFd.start_picture(rotate);
            case PIC_SCALER:
                return mPicScaler.start_picture(0);
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    void MIPIVideoInfo::stop_picture() {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.stop_picture();
                break;
            case TWO_FD:
                mTwoFd.stop_picture();
                break;
            case PIC_SCALER:
                mPicScaler.stop_picture();
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    void MIPIVideoInfo::releasebuf_and_stop_picture() {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.releasebuf_and_stop_picture();
                break;
            case TWO_FD:
                mTwoFd.releasebuf_and_stop_picture();
                break;
            case PIC_SCALER:
                mPicScaler.releasebuf_and_stop_capturing();
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    int MIPIVideoInfo::stop_capturing() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.stop_capturing();
            case TWO_FD:
                return mTwoFd.stop_capturing();
            case PIC_SCALER:
                return mPicScaler.stop_capturing();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    int MIPIVideoInfo::stop_recording() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.stop_recording();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

#if 0
    int MIPIVideoInfo::onMuteKeyChanged(KeyEvent keyStatus) {
        switch (mWorkMode) {
            case PIC_SCALER:
                mPicScaler.setMuteKeyStatus(keyStatus);
                return 0;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }
#endif
    int MIPIVideoInfo::releasebuf_and_stop_capturing() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.releasebuf_and_stop_capturing();
            case TWO_FD:
                return mTwoFd.releasebuf_and_stop_capturing();
            case PIC_SCALER:
                return mPicScaler.releasebuf_and_stop_capturing();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    uintptr_t MIPIVideoInfo::get_frame_phys() {

        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.get_frame_phys();
            case TWO_FD:
                return mTwoFd.get_frame_phys();
            case PIC_SCALER:
                return mPicScaler.get_frame_phys();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    void MIPIVideoInfo::set_device_status() {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.set_device_status();
                break;
            case TWO_FD:
                mTwoFd.set_device_status();
                break;
            case PIC_SCALER:
                mPicScaler.set_device_status();
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    int MIPIVideoInfo::get_device_status() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.get_device_status();
            case TWO_FD:
                return mTwoFd.get_device_status();
            case PIC_SCALER:
                return mPicScaler.get_device_status();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    void *MIPIVideoInfo::get_frame() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.get_frame();
            case TWO_FD:
                return mTwoFd.get_frame();
            case PIC_SCALER:
                return mPicScaler.get_frame();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return nullptr;
        }
    }

    void *MIPIVideoInfo::get_picture() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.get_picture();
            case TWO_FD:
                return mTwoFd.get_picture();
            case PIC_SCALER:
                ALOGE("%s: not support this function",__FUNCTION__);
                return nullptr;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return nullptr;
        }
    }

    int MIPIVideoInfo::get_frame_buffer(struct VideoInfoBuffer* b) {
        //ATRACE_CALL();
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.get_frame_buffer(b);
            case TWO_FD:
                return mTwoFd.get_frame_buffer(b);
            case PIC_SCALER:
                return mPicScaler.get_frame_buffer(b);
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return -1;
        }
    }

    int MIPIVideoInfo::get_record_buffer(struct VideoInfoBuffer* b) {
        //ATRACE_CALL();
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.get_record_buffer(b);
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return -1;
        }
    }

    int MIPIVideoInfo::putback_frame() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.putback_frame();
            case TWO_FD:
                return mTwoFd.putback_frame();
            case PIC_SCALER:
                return mPicScaler.putback_frame();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return -1;
        }
    }

    int MIPIVideoInfo::putback_record_frame() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.putback_record_frame();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return -1;
        }
    }

    int MIPIVideoInfo::putback_picture_frame() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.putback_picture_frame();
            case TWO_FD:
                return mTwoFd.putback_picture_frame();
            case PIC_SCALER:
                return mPicScaler.putback_picture_frame();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    int MIPIVideoInfo::EnumerateFormat(uint32_t pixelformat) {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.EnumerateFormat(pixelformat);
            case TWO_FD:
                return mTwoFd.EnumerateFormat(pixelformat);
            case PIC_SCALER:
                return mPicScaler.EnumerateFormat(pixelformat);
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return -1;
        }
    }

    bool MIPIVideoInfo::IsSupportRotation() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.IsSupportRotation();
            case TWO_FD:
                return mTwoFd.IsSupportRotation();
            case PIC_SCALER:
                return mPicScaler.IsSupportRotation();
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return false;
        }
    }

    void MIPIVideoInfo::set_buffer_numbers(int io_buffer) {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.set_buffer_numbers(io_buffer);
                 break;
            case TWO_FD:
                 mTwoFd.set_buffer_numbers(io_buffer);
                 break;
            case PIC_SCALER:
                 mPicScaler.set_buffer_numbers(io_buffer);
                 break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    uint32_t MIPIVideoInfo::get_preview_pixelformat() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.preview.format.fmt.pix.pixelformat;
            case TWO_FD:
                return mTwoFd.preview.format.fmt.pix.pixelformat;
            case PIC_SCALER:
                return mPicScaler.preview.format.fmt.pix.pixelformat;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_preview_width() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.preview.format.fmt.pix.width;
            case TWO_FD:
                return mTwoFd.preview.format.fmt.pix.width;
            case PIC_SCALER:
                return mPicScaler.preview.format.fmt.pix.width;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_preview_height(){
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.preview.format.fmt.pix.height;
            case TWO_FD:
                return mTwoFd.preview.format.fmt.pix.height;
            case PIC_SCALER:
                return mPicScaler.preview.format.fmt.pix.height;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    void MIPIVideoInfo::set_preview_format(uint32_t width, uint32_t height, uint32_t pixelformat) {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mOneFd.preview.format.fmt.pix.pixelformat = pixelformat;
                mOneFd.preview.format.fmt.pix.width = width;
                mOneFd.preview.format.fmt.pix.height = height;
                break;
            case TWO_FD:
                mTwoFd.preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mTwoFd.preview.format.fmt.pix.pixelformat = pixelformat;
                mTwoFd.preview.format.fmt.pix.width = width;
                mTwoFd.preview.format.fmt.pix.height = height;
                break;
            case PIC_SCALER:
                mPicScaler.preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mPicScaler.preview.format.fmt.pix.pixelformat = pixelformat;
                mPicScaler.preview.format.fmt.pix.width = width;
                mPicScaler.preview.format.fmt.pix.height = height;
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    uint32_t MIPIVideoInfo::get_preview_buf_length() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.preview.buf.length;
            case TWO_FD:
                return mTwoFd.preview.buf.length;
            case PIC_SCALER:
                return mPicScaler.preview.buf.length;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_preview_buf_bytesused() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.preview.buf.bytesused;
            case TWO_FD:
                return mTwoFd.preview.buf.bytesused;
            case PIC_SCALER:
                return mPicScaler.preview.buf.bytesused;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    uint32_t MIPIVideoInfo::get_record_pixelformat() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.record.format.fmt.pix.pixelformat;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_record_width() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.record.format.fmt.pix.width;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_record_height(){
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.record.format.fmt.pix.height;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    void MIPIVideoInfo::set_record_format(uint32_t width, uint32_t height, uint32_t pixelformat) {
        switch (mWorkMode) {
            case PIC_SCALER:
                mPicScaler.record.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mPicScaler.record.format.fmt.pix.pixelformat = pixelformat;
                mPicScaler.record.format.fmt.pix.width = width;
                mPicScaler.record.format.fmt.pix.height = height;
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
    }

    uint32_t MIPIVideoInfo::get_record_buf_length() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.record.buf.length;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_record_buf_bytesused() {
        switch (mWorkMode) {
            case PIC_SCALER:
                return mPicScaler.record.buf.bytesused;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }


    uint32_t MIPIVideoInfo::get_picture_pixelformat() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.picture.format.fmt.pix.pixelformat;
            case TWO_FD:
                return mTwoFd.picture.format.fmt.pix.pixelformat;
            case PIC_SCALER:
                return mPicScaler.picture_config.format.fmt.pix.pixelformat;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    uint32_t MIPIVideoInfo::get_picture_width() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.picture.format.fmt.pix.width;
            case TWO_FD:
                return mTwoFd.picture.format.fmt.pix.width;
            case PIC_SCALER:
                return mPicScaler.picture_config.format.fmt.pix.width;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    uint32_t MIPIVideoInfo::get_picture_height() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.picture.format.fmt.pix.height;
            case TWO_FD:
                return mTwoFd.picture.format.fmt.pix.height;
            case PIC_SCALER:
                return mPicScaler.picture_config.format.fmt.pix.height;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 return 0;
        }

    }

    void MIPIVideoInfo::set_picture_format(uint32_t width, uint32_t height, uint32_t pixelformat) {
        switch (mWorkMode) {
            case ONE_FD:
                mOneFd.picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mOneFd.picture.format.fmt.pix.pixelformat = pixelformat;
                mOneFd.picture.format.fmt.pix.width = width;
                mOneFd.picture.format.fmt.pix.height = height;
            break;
            case TWO_FD:
                CAMHAL_LOGDB("%s: E w %d h %d",__FUNCTION__, width, height);
                mTwoFd.picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mTwoFd.picture.format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
                mTwoFd.picture.format.fmt.pix.width = width;
                mTwoFd.picture.format.fmt.pix.height = height;
                break;
            case PIC_SCALER:
                CAMHAL_LOGDB("%s: E w %d h %d",__FUNCTION__, width, height);
                mPicScaler.picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mPicScaler.picture.format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
                mPicScaler.picture.format.fmt.pix.width = width/*2592*/;
                mPicScaler.picture.format.fmt.pix.height = height/*1944*/;

                mPicScaler.picture_config.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                mPicScaler.picture_config.format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
                mPicScaler.picture_config.format.fmt.pix.width = width;
                mPicScaler.picture_config.format.fmt.pix.height = height;
                break;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                 break;
        }
   }

    uint32_t MIPIVideoInfo::get_picture_buf_length() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.picture.buf.length;
            case TWO_FD:
                return mTwoFd.picture.buf.length;
            case PIC_SCALER:
                ALOGE("%s: not support this value",__FUNCTION__);
                return  0;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return 0;
        }
    }

    uint32_t MIPIVideoInfo::get_picture_buf_bytesused() {
        switch (mWorkMode) {
            case ONE_FD:
                return mOneFd.picture.buf.bytesused;
            case TWO_FD:
                return mTwoFd.picture.buf.bytesused;
            case PIC_SCALER:
                ALOGE("%s: not support this value",__FUNCTION__);
                return 0;
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return 0;
        }
    }

    bool MIPIVideoInfo::Stream_status() {
        switch (mWorkMode) {
            case ONE_FD:
            return mOneFd.isStreaming;
            case TWO_FD:
            return mTwoFd.isStreaming;
            case PIC_SCALER:
            return mPicScaler.isStreaming;
            default:
            ALOGE("%s: work mode error",__FUNCTION__);
            return false;
         }
    }

    bool MIPIVideoInfo::Stream_record_status() {
        switch (mWorkMode) {
            case PIC_SCALER:
            return mPicScaler.mIsRecording;
            default:
            ALOGE("%s: work mode error",__FUNCTION__);
            return false;
         }
    }

    bool MIPIVideoInfo::Picture_status() {
        switch (mWorkMode) {
            case ONE_FD:
            return mOneFd.isPicture;
            case TWO_FD:
            return mTwoFd.mIsPicture;
            case PIC_SCALER:
            return mPicScaler.mIsPicture;
            default:
            ALOGE("%s: work mode error",__FUNCTION__);
            return false;
         }
    }

    int MIPIVideoInfo::get_picture_buffer(struct VideoInfoBuffer* b) {
        switch (mWorkMode) {
            case ONE_FD:
                ALOGE("%s: not support this function",__FUNCTION__);
                return -1;
            case TWO_FD:
                ALOGE("%s: not support this function",__FUNCTION__);
                return -1;
            case PIC_SCALER:
                return mPicScaler.get_picture_buffer(b);
            default:
                ALOGE("%s: work mode error",__FUNCTION__);
                return -1;
        }
    }
#endif
}
