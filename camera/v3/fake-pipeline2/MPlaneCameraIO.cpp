#define LOG_NDEBUG 0
#define LOG_TAG "MPlane_Camera_IO"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>

#include <errno.h>
#include <cutils/properties.h>
#include "MPlaneCameraIO.h"

namespace android {

MPlaneCameraIO::MPlaneCameraIO() {
    isStreaming = false;
    mIon = NULL;
    if (!mIon)
        mIon = IONInterface::get_instance();
}

MPlaneCameraIO::~MPlaneCameraIO() {
    freePlaneBuffers();
    if (mIon) {
        IONInterface::put_instance();
        mIon = NULL;
    }
}

void MPlaneCameraIO::allocatePlaneBuffers() {
    for (int i = 0; i < MAX_V4L2_BUFFER_COUNT; i++) {
        pPlaneBuffers[i] = (PlaneBuffers*)calloc(1, sizeof(PlaneBuffers));
    }
}

void MPlaneCameraIO::freePlaneBuffers() {
    for (int i = 0; i < MAX_V4L2_BUFFER_COUNT; i++) {
        if (pPlaneBuffers[i]) {
            free(pPlaneBuffers[i]);
            pPlaneBuffers[i] = NULL;
        }
    }
}

int MPlaneCameraIO::openCamera() {
    int ret;
    fd = CameraVirtualDevice::getInstance()->openVirtualDevice(openIdx);
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        ALOGE("VIDIOC_QUERYCAP, %s", strerror(errno));
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("device is not video capture device");
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("HDMISensor does not support streaming i/o");
    }
    return ret;
}

void MPlaneCameraIO::closeCamera() {
    CameraVirtualDevice::getInstance()->releaseVirtualDevice(openIdx, fd);
    fd = -1;
}

int MPlaneCameraIO::startCameraIO() {
    int ret = 0;
    enum v4l2_buf_type type;
    if (!isStreaming) {
        allocatePlaneBuffers();
    }
    //reqbuffers
    CLEAR(rb);
    rb.memory = V4L2_MEMORY_DMABUF;
    rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    rb.count  = MAX_V4L2_BUFFER_COUNT;

    ret = ioctl(fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        ALOGE("VIDIOC_REQBUFS fail :%d\n",ret);
    }
    if (rb.count < 2) {
        ALOGE( "Insufficient buffer memory on HDMI, errno=%d", errno);
        return -EINVAL;
    }
    //mmap
    for (int i = 0; i < (int)rb.count; i++) {

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory      = V4L2_MEMORY_DMABUF;
        buf.index       = i;
        buf.length   = 1;
        buf.m.planes = pPlaneBuffers[i]->v4lplane;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ALOGE("VIDIOC_QUERYBUF, errno=%d", errno);
        }
        addr[i] = mIon->alloc_buffer(buffer_size_allocated, &(dma_fd[i]));
        if (MAP_FAILED == addr[i]) {
            ALOGE("mmap failed, errno=%d", errno);
        }
    }
    //queue
    for (int i = 0; i < (int)rb.count; i++) {

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = pPlaneBuffers[i]->v4lplane;
        buf.m.planes[0].m.fd = dma_fd[i];

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
            ALOGE("VIDIOC_QBUF failed, errno=%d", errno);
    }

    //streamOn
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if ((format.fmt.pix_mp.width != 0) &&
        (format.fmt.pix_mp.height != 0)) {
        if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
            ALOGE("VIDIOC_STREAMON, errno=%d", errno);
    }
    //success streamon set flag true
    isStreaming = true;
    return ret;
}

int MPlaneCameraIO::stopCameraIO() {
    int ret = 0;
    enum v4l2_buf_type type;
    if (!isStreaming)
        return -1;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("VIDIOC_STREAMOFF, errno=%d", errno);
        ret = -1;
    }
    for (int i = 0; i < (int)rb.count; ++i) {
        if (munmap(addr[i], buffer_size_allocated) < 0) {
            ALOGE("munmap failed errno=%d", errno);
            ret = -1;
        }
        mIon->free_buffer(dma_fd[i]);
        close(dma_fd[i]);
        dma_fd[i] = -1;
    }

    CLEAR(rb);
    rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    rb.memory = V4L2_MEMORY_DMABUF;
    rb.count = 0;

    ret = ioctl(fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
    } else {
        ALOGE("VIDIOC_REQBUFS delete buffer success");
    }

    isStreaming = false;
    return ret;
}

int MPlaneCameraIO::setInputPort(int* port_index) {
    int ret = ioctl(fd, VIDIOC_S_INPUT, port_index);
    if (ret < 0) {
        ALOGE("Unable set input");
    }
    return ret;
}

int MPlaneCameraIO::setOutputFormat() {
    int ret = 0;
    if ((format.fmt.pix_mp.width != 0) && (format.fmt.pix_mp.height != 0)) {
        ret = ioctl(fd, VIDIOC_S_FMT, &format);
        if (ret < 0) {
            ALOGE("Open: VIDIOC_S_FMT Failed: %s, ret=%d\n", strerror(errno), ret);
        }
        if (format.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
            buffer_size_allocated = format.fmt.pix_mp.width * format.fmt.pix_mp.height * 3 / 2;
        ALOGE("Width * Height %d x %d, get:0x%x",
                format.fmt.pix_mp.width,
                format.fmt.pix_mp.height,
                format.fmt.pix_mp.pixelformat);
    }
    return ret;
}

int MPlaneCameraIO::getFrame(VideoInfo& info) {
    ATRACE_CALL();
    struct v4l2_plane planes[4];
    CLEAR(buf);
    memset(planes, 0, sizeof(planes));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.length = 1;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        switch (errno) {
            case EAGAIN:
                return -1;

            case EIO:
            default:
                ALOGD("VIDIOC_DQBUF failed, errno=%d\n", errno);
                return -1;
        }
    }
    info.addr = addr[buf.index];
    info.dma_fd = dma_fd[buf.index];
    info.buf_idx = buf.index;
    return 0;
}

int MPlaneCameraIO::pushbackFrame(unsigned int index)
{
    ATRACE_CALL();
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = index;
    buf.length = 1;
    buf.m.planes = pPlaneBuffers[index]->v4lplane;
    buf.m.planes[0].m.fd = dma_fd[index];
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        ALOGE("QBUF failed error=%d", errno);
    }

    return 0;
}


}

