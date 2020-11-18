#ifndef AMLINUX_DVD_H
#define AMLINUX_DVD_H

#include <AmDmx.h>
#include "RefBase.h"

#define DVB_DVR     "/dev/dvb0.dvr0"
#define DVB_DEMUX    "/dev/dvb0.demux0"


typedef struct
{
    char   dev_name[32];
    int    fd[DMX_FILTER_COUNT];
    int    evtfd;
} DVBDmx_t;

class AmLinuxDvd : public RefBase {

public:
    AmLinuxDvd();
    ~AmLinuxDvd();
    AM_ErrorCode_t dvb_open(AM_DMX_Device *dev);
    AM_ErrorCode_t dvb_close(AM_DMX_Device *dev);
    AM_ErrorCode_t dvb_alloc_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter);
    AM_ErrorCode_t dvb_free_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter);
    AM_ErrorCode_t dvb_get_stc(AM_DMX_Device *dev, AM_DMX_Filter *filter);
    AM_ErrorCode_t dvb_set_sec_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_sct_filter_params *params);
    AM_ErrorCode_t dvb_set_pes_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_pes_filter_params *params);
    AM_ErrorCode_t dvb_enable_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, bool enable);
    AM_ErrorCode_t dvb_set_buf_size(AM_DMX_Device *dev, AM_DMX_Filter *filter, int size);
    AM_ErrorCode_t dvb_poll(AM_DMX_Device *dev, AM_DMX_FilterMask_t *mask, int timeout);
    AM_ErrorCode_t dvb_poll_exit(AM_DMX_Device *dev);
    AM_ErrorCode_t dvb_read(AM_DMX_Device *dev, AM_DMX_Filter *filter, uint8_t *buf, int *size);
    //AM_ErrorCode_t dvb_set_source(AM_DMX_Device *dev, AM_DMX_Source_t src);
    AM_ErrorCode_t dvr_open(void);
    int dvr_data_write(uint8_t *buf, int size,uint64_t timeout);
    AM_ErrorCode_t dvr_close(void);
private:
    int mDvrFd;
};

#endif
