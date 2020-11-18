/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <utils/RefBase.h>
#include "AmSwDemuxWrapper.h"
using ::android::Am_SwDemuxWrapper;

#define INPUT_SIZE (32*1024)
static int set_osd_blank(int blank)
{
    const char *path1 = "/sys/class/graphics/fb0/blank";
    const char *path2 = "/sys/class/graphics/fb1/blank";
    int fd;
    char cmd[128] = {0};
    fd = open(path1,O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0)
    {
       sprintf(cmd,"%d",blank);
       write (fd,cmd,strlen(cmd));
       close(fd);
    }
    fd = open(path2,O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0)
    {
       sprintf(cmd,"%d",blank);
       write (fd,cmd,strlen(cmd));
       close(fd);
    }
    return 0;
}

static int set_vfm_path(void)
{

    const char *vfm_path = "/sys/class/vfm/map";
    const char *vfm_rm = "rm default";
    const char *vfm_add = "add default decoder ppmgr deinterlace amvideo";
    int fd;
    char bcmd[128]={0};
    fd = open(vfm_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
       sprintf(bcmd, "%s",vfm_rm );
       write(fd, bcmd, strlen(bcmd));
       sprintf(bcmd, "%s" ,vfm_add);
       write(fd, bcmd, strlen(bcmd));
       close(fd);
       return 0 ;
     }
     return -1;

}
int32_t main(int argc, char **argv)
{
    int32_t err = 0;
    int fd;
    static uint8_t buf[INPUT_SIZE];
    int len, left=0, send, ret;
    int cnt=10;
    int inject_running = 1;
    uint8_t *ptr = NULL;

    Am_TsPlayer_Input_buffer_t* buffer;
    buffer = (Am_TsPlayer_Input_buffer_t*)malloc(sizeof(Am_TsPlayer_Input_buffer_t));
    if (buffer)
        memset(buffer,0,sizeof(Am_TsPlayer_Input_buffer_t));

    AM_DMX_DevNo_t dmx_devno = DMX_DEV_UNKNOWN;
    Am_DemuxWrapper_OpenPara_t dmxpara;
    Am_DemuxWrapper_OpenPara_t avpara;
    Am_DemuxWrapper_OpenPara_t * pdmxpara = &dmxpara;
    Am_DemuxWrapper_OpenPara_t * pavpara = &avpara;
    if (argc < 2)
    {
        printf("input player file name\n");
        return -1;
    }
    set_vfm_path();
    set_osd_blank (1);

    AmHwDemuxWrapper* mdmx = new android::AmHwDemuxWrapper();
    AmHwDemuxWrapper * mav = new android::AmHwDemuxWrapper();

    pdmxpara->dev_no = (int)DMX_DEV_NO0;//DMX_DEV_UNKNOWN ;
    pdmxpara->device_type = Dev_Type_DMX;
    mdmx->AmDemuxWrappe_Open(pdmxpara);
    dmx_devno = (AM_DMX_DevNo_t)(pdmxpara->dev_no);// dmx no
    printf("open dmx_devno %d\n",dmx_devno);
    mdmx->AmDemuxWrapperSetTSSource (pdmxpara,AM_DMX_SRC_HIU);

    pavpara->dev_no = (int)AV_DEV_NO;
    pavpara->device_type = Dev_Type_AV;
    mav->AmDemuxWrapperOpen(pavpara);
    mav->AmDemuxWrapperSetTSSource (pavpara,AM_AV_TS_SRC_DMX0);
    mav->AmDemuxWrapperSetVideoParam(0x100,VFORMAT_H264);
    mav->AmDemuxWrapperSetAudioParam(0x101,AFORMAT_AAC);
    mav->AmDemuxWrapperStart();


    fd = open(argv[1], O_RDWR, O_RDONLY);
    while (inject_running)
    {
        len = sizeof(buf);
        ret = read(fd , buf, len);
        if (ret > 0)
        {
            //printf("recv %d bytes\n", ret);
            if (!cnt)
            {
                cnt=10;
                //printf("recv %d\n", ret);
            }
            cnt--;
            left = ret;
        }
        else
        {
            if ((ret == 0) ||(errno == EAGAIN)) {
                inject_running = 0;
                printf("read eof\n");
                break;
            } else {
                printf("read file failed [%d:%s]\n", errno, strerror(errno));
                inject_running = 0;
                break;
            }
        }

        ptr = buf;
        buffer->data = buf;
        buffer->size = &left;
        while (left > 0)
        {
            send = left;
            //printf("need %d  write...%d\n", send,*(buffer->size));
            ret = (int)mav->Am_DemuxWrapper_WriteData(buffer,0);
            //printf("after write data need %d and write...%d\n", send,*(buffer->size));
            send = *(buffer->size);
            if (ret != 0 || send != left)
            {
                printf("inject error(%d)...\n", ret);
                break;
            }
            ptr += send;
            left -= send;
            buffer->data = ptr;
            buffer->size = &left;
            //printf("inject %d bytes \n", send);
        }
    }

    mav->AmDemuxWrapperStop();
    mav->AmDemuxWrapperClose(Dev_Type_AV);

    mdmx->AmDemuxWrapperStop();
    mdmx->AmDemuxWrapperClose(Dev_Type_DMX);
    printf("playback exit@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    set_osd_blank(0);
    return err;
}
