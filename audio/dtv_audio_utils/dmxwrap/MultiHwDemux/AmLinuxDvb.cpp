//#define printf_LEVEL 5

//#include "am_mem.h"
//#include <am_misc.h>
//#define LOG_NDEBUG 0
#define LOG_TAG "AmLinuxDvb"

#include <utils/Log.h>
#include <cutils/properties.h>

#include <stdio.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
/*add for config define for linux dvb *.h*/
//#include <am_config.h>
#include <AmLinuxDvb.h>
#include <AmDmx.h>
#include <sys/eventfd.h>
#include <dmx.h>
#include <TSPHandler.h>

#include <inttypes.h>

#define UNUSED(x) (void)(x)
AmLinuxDvd::AmLinuxDvd() {
    ALOGI("AmLinuxDvd\n");
    mDvrFd = -1;
}

AmLinuxDvd::~AmLinuxDvd() {
    ALOGI("~AmLinuxDvd\n");
}

AM_ErrorCode_t AmLinuxDvd::dvb_open(AM_DMX_Device *dev) {
    DVBDmx_t *dmx;
    int i;

    //UNUSED(para);

    dmx = (DVBDmx_t*)malloc(sizeof(DVBDmx_t));
    if (!dmx)
    {
        ALOGE("not enough memory");
        return AM_DMX_ERR_NO_MEM;
    }
    ALOGI("dev->dev_no %d",dev->dev_no);
    snprintf(dmx->dev_name, sizeof(dmx->dev_name), "/dev/dvb0.demux%d", dev->dev_no);
    //snprintf(dmx->dev_name, sizeof(dmx->dev_name), DVB_DEMUX);
    for (i=0; i<DMX_FILTER_COUNT; i++)
        dmx->fd[i] = -1;
    dmx->evtfd = eventfd(0, 0);
    if (dmx->evtfd == -1)
        ALOGI("eventfd error");
    dev->drv_data = dmx;
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_close(AM_DMX_Device *dev) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    close(dmx->evtfd);
    free(dmx);
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_alloc_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int fd;

    fd = open(dmx->dev_name, O_RDWR);
    if (fd == -1)
    {
        ALOGE("cannot open \"%s\" (%s)", dmx->dev_name, strerror(errno));
        return AM_DMX_ERR_CANNOT_OPEN_DEV;
    }

    dmx->fd[filter->id] = fd;

    filter->drv_data = (void*)(long)fd;

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_free_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int fd = (long)filter->drv_data;

    close(fd);
    dmx->fd[filter->id] = -1;

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_get_stc(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    int fd = (long)filter->drv_data;
    int ret;
    struct dmx_stc stc;
    int i = 0;

    UNUSED(dev);

    for (i = 0; i < 3; i++) {
        memset(&stc, 0, sizeof(struct dmx_stc));
        stc.num = i;
        ret = ioctl(fd, DMX_GET_STC, &stc);
        if (ret == 0) {
            ALOGI("get stc num %d: base:0x%0x, stc:0x%llx\n", stc.num, stc.base, stc.stc);
        } else {
            ALOGE("get stc %d, fail\n", i);
        }
    }
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_sec_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_sct_filter_params *params) {
    struct dmx_sct_filter_params p;
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    p = *params;
    /*
    if (p.filter.mask[0] == 0) {
        p.filter.filter[0] = 0x00;
        p.filter.mask[0]   = 0x80;
    }
    */
    ret = ioctl(fd, DMX_SET_FILTER, &p);
    if (ret == -1)
    {
        ALOGE("set section filter failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_pes_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_pes_filter_params *params) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    fcntl(fd,F_SETFL,O_NONBLOCK);

    ret = ioctl(fd, DMX_SET_PES_FILTER, params);
    if (ret == -1)
    {
        ALOGE("set section filter failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }
    ALOGI("%s success\n", __FUNCTION__);
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_enable_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, bool enable) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    if (enable)
        ret = ioctl(fd, DMX_START, 0);
    else
        ret = ioctl(fd, DMX_STOP, 0);

    if (ret == -1)
    {
        ALOGE("start filter failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_buf_size(AM_DMX_Device *dev, AM_DMX_Filter *filter, int size) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    ret = ioctl(fd, DMX_SET_BUFFER_SIZE, size);
    if (ret == -1)
    {
        ALOGE("set buffer size failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }

    return AM_SUCCESS;
}
AM_ErrorCode_t AmLinuxDvd::dvb_poll_exit(AM_DMX_Device *dev) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int64_t pad = 1;
    ALOGV("dvb_poll_exit");
    write(dmx->evtfd, &pad, sizeof(pad));
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_poll(AM_DMX_Device *dev, AM_DMX_FilterMask_t *mask, int timeout) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    struct pollfd fds[DMX_FILTER_COUNT + 1];
    int fids[DMX_FILTER_COUNT + 1];
    int i, cnt = 0, ret;
    for (i = 0; i < DMX_FILTER_COUNT; i++)
    {
        if (dmx->fd[i] != -1)
        {
            fds[cnt].events = POLLIN|POLLERR;
            fds[cnt].fd     = dmx->fd[i];
            fids[cnt] = i;
            cnt++;
            ALOGV("dvb_poll %d",cnt);
        }
    }

    if (!cnt)
        return AM_DMX_ERR_TIMEOUT;

    if (dmx->evtfd != -1) {
        fds[cnt].events = POLLIN|POLLERR;
        fds[cnt].fd     = dmx->evtfd;
        fids[cnt] = i;
        cnt++;
    }

    ret = poll(fds, cnt, timeout);
    if (ret <= 0)
    {
        ALOGI("timeout %d",timeout);
        return AM_DMX_ERR_TIMEOUT;
    }

    for (i = 0; i < cnt; i++)
    {
        if (fds[i].revents&(POLLIN|POLLERR))
        {
            AM_DMX_FILTER_MASK_SET(mask, fids[i]);
            ALOGV("fids[i] %d mask %d",fids[i],*mask);
        }
    }

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_read(AM_DMX_Device *dev, AM_DMX_Filter *filter, uint8_t *buf, int *size) {
    int fd = (long)filter->drv_data;
    int len = *size;
    int ret;
    struct pollfd pfd;

    UNUSED(dev);

    if (fd == -1)
        return AM_DMX_ERR_NOT_ALLOCATED;

    pfd.events = POLLIN|POLLERR;
    pfd.fd     = fd;

    ret = poll(&pfd, 1, 0);
    if (ret <= 0)
        return AM_DMX_ERR_NO_DATA;

    ret = read(fd, buf, len);
    if (ret <= 0)
    {
        if (errno == ETIMEDOUT)
            return AM_DMX_ERR_TIMEOUT;
        ALOGE("read demux failed (%s) %d", strerror(errno), errno);
        return AM_DMX_ERR_SYS;
    }

    *size = ret;
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvr_open(void) {
    int ret;
    mDvrFd = open(DVB_DVR, O_WRONLY);
    if (mDvrFd == -1)
    {
        printf("cannot open \"%s\" (%s)", DVB_DVR, strerror(errno));
        return -1;
    }
    ret = ioctl(mDvrFd, DMX_SET_INPUT, INPUT_DEMOD);
    if (ret < 0) {
        ALOGE("dvr_open ioctl failed \n");
    }
    return AM_SUCCESS;
}

int AmLinuxDvd::dvr_data_write(uint8_t *buf, int size,uint64_t timeout)
{
    int ret;
    int left = size;
    uint8_t *p = buf;
    int64_t nowUs = TSPLooper::GetNowUs();
    timeout *= 10;
    while (left > 0)
    {
        //ALOGI("write start\n");
        ret = write(mDvrFd, p, left);
        //ALOGI("write end\n");
        if (ret == -1)
        {
            if (errno != EINTR)
            {
                ALOGE("Write DVR data failed: %s", strerror(errno));
                break;
            }
            ret = 0;
        } else {
        //  ALOGI("%s write cnt:%d\n",__FUNCTION__,ret);
        }

        left -= ret;
        p += ret;
        if (TSPLooper::GetNowUs() - nowUs > timeout) {
            ALOGE("dvr_data_write timeout %" PRIu64 " \n",timeout);
            break;
        }
    }

    return (size - left);
}

AM_ErrorCode_t AmLinuxDvd::dvr_close(void) {
    if (mDvrFd > 0)
        close(mDvrFd);
    return AM_SUCCESS;
}

#if 0
AM_ErrorCode_t AmLinuxDvd::dvb_set_source(AM_DMX_Device *dev, AM_DMX_Source_t src) {
    char buf[32];
    char *cmd;

    snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", dev->dev_no);

    switch (src)
    {
        case AM_DMX_SRC_TS0:
            cmd = "ts0";
        break;
        case AM_DMX_SRC_TS1:
            cmd = "ts1";
        break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
        case AM_DMX_SRC_TS2:
            cmd = "ts2";
        break;
#endif
        case AM_DMX_SRC_TS3:
            cmd = "ts3";
        break;
        case AM_DMX_SRC_HIU:
            cmd = "hiu";
        break;
        case AM_DMX_SRC_HIU1:
            cmd = "hiu1";
        break;
        default:
            ALOGE("do not support demux source %d", src);
        return AM_DMX_ERR_NOT_SUPPORTED;
    }
    return 0;
    return AM_FileEcho(buf, cmd);
}
#endif
