//#define LOG_NDEBUG 0
#define LOG_TAG "AM_DMX_Device"
#include <utils/Log.h>
#include <cutils/properties.h>

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <am_mem.h>
#include <AmLinuxDvb.h>
#include <AmDmx.h>
#include <dmx.h>
#include <AmHwMultiDemuxWrapper.h>


AM_DMX_Device::AM_DMX_Device(AmHwMultiDemuxWrapper* DemuxWrapper) :
    mDemuxWrapper (DemuxWrapper){
    ALOGI("AM_DMX_Device\n");
    drv = new AmLinuxDvd;
    //drv->dvr_open();
    open_count = 0;
    memset(&filters[0], 0, sizeof(struct AM_DMX_Filter)*DMX_FILTER_COUNT);
}

AM_DMX_Device::~AM_DMX_Device() {
    drv->dvr_close();
    drv = NULL;
    ALOGI("~AM_DMX_Device\n");
}


/**\brief 根据ID取得对应filter结构，并检查设备是否在使用*/
AM_ErrorCode_t AM_DMX_Device::dmx_get_used_filter(int filter_id, AM_DMX_Filter **pf)
{
    AM_DMX_Filter *filter;

    if ((filter_id < 0) || (filter_id >= DMX_FILTER_COUNT))
    {
        ALOGE("invalid filter id, must in %d~%d", 0, DMX_FILTER_COUNT-1);
        return AM_DMX_ERR_INVALID_ID;
    }

    filter = &filters[filter_id];

    if (!filter->used)
    {
        ALOGE("filter %d has not been allocated", filter_id);
        return AM_DMX_ERR_NOT_ALLOCATED;
    }

    *pf = filter;
    return AM_SUCCESS;
}

/**\brief 数据检测线程*/
void* AM_DMX_Device::dmx_data_thread(void *arg)
{
    AM_DMX_Device *dev = (AM_DMX_Device*)arg;
    uint8_t *sec_buf;
    uint8_t *sec;
    int sec_len;
    AM_DMX_FilterMask_t mask;
    AM_ErrorCode_t ret;

#define BUF_SIZE (4096 * 10)

    sec_buf = (uint8_t*)malloc(BUF_SIZE);
    struct dmx_non_sec_es_header *header_es;
    while (dev->enable_thread)
    {
        AM_DMX_FILTER_MASK_CLEAR(&mask);
        int id;

        ret = dev->drv->dvb_poll(dev, &mask, DMX_POLL_TIMEOUT);
        if (ret == AM_SUCCESS)
        {
            if (AM_DMX_FILTER_MASK_ISEMPTY(&mask))
                continue;

#if defined(DMX_WAIT_CB) || defined(DMX_SYNC)
            pthread_mutex_lock(&dev->lock);
            dev->flags |= DMX_FL_RUN_CB;
            pthread_mutex_unlock(&dev->lock);
#endif

            for (id=0; id<DMX_FILTER_COUNT; id++)
            {
                AM_DMX_Filter *filter = &(dev->filters[id]);
                AM_DMX_DataCb cb;
                void *data;
                if (!AM_DMX_FILTER_MASK_ISSET(&mask, id))
                    continue;
                if (!filter->enable || !filter->used)
                    continue;
                sec_len = BUF_SIZE;

#ifndef DMX_WAIT_CB
                pthread_mutex_lock(&dev->lock);
#endif
                if (!filter->enable || !filter->used)
                {
                    ret = AM_FAILURE;
                }
                else
                {
                    cb   = filter->cb;
                    data = filter->user_data;
                    int read_len = 0;
                    /* 1 read header */
                    do {
                        sec_len = sizeof(struct dmx_non_sec_es_header) - read_len;
                        ret  = dev->drv->dvb_read(dev, filter, sec_buf + read_len, &sec_len);
                        if (ret == AM_SUCCESS) {
                            read_len += sec_len;
                        }
                    } while (dev->enable_thread && !filter->to_be_stopped  && read_len < sizeof(struct dmx_non_sec_es_header));

                    /* 2 read data */
                    if (ret != AM_SUCCESS) {
                        ALOGE("dmx read header data err %0x ", ret );
                    } else {
                        header_es = (struct dmx_non_sec_es_header *)sec_buf;
                        sec_len = header_es->len;
                        if (header_es->len < 0 ||
                            (header_es->len > (BUF_SIZE - sizeof(struct dmx_non_sec_es_header)))) {
                            ALOGI("data len invalid %d ", header_es->len );
                            header_es->len = 0;
                            ret = AM_DMX_ERR_NO_DATA;
                        } else {
                            read_len = 0;
                            do {
                                  ret  = dev->drv->dvb_read(dev, filter, sec_buf + read_len + sizeof(struct dmx_non_sec_es_header), &sec_len);
                                  if (ret == AM_SUCCESS) {
                                      read_len += sec_len;
                                      sec_len = header_es->len - read_len;
                                  }
                                  if (read_len < header_es->len) {
                                    ALOGV("ret %d dvb_read audio len  %d frame len %d",ret, read_len ,header_es->len);
                                    usleep (5000);
                                  }
                            } while (dev->enable_thread && !filter->to_be_stopped && read_len < header_es->len);
                            sec_len = sizeof(struct dmx_non_sec_es_header) + header_es->len;
                        }
                    }
                }
#ifndef DMX_WAIT_CB
                pthread_mutex_unlock(&dev->lock);
#endif
                if (ret == AM_DMX_ERR_TIMEOUT)
                {
                    sec = NULL;
                    sec_len = 0;
                }
                else if (ret!=AM_SUCCESS)
                {
                    continue;
                }
                else
                {
                    sec = sec_buf;
                }

                if (cb)
                {
                    /*if (id && sec)
                    ALOGI("filter %d data callback len fd:%ld len:%d, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                        id, (long)filter->drv_data, sec_len,
                        sec[0], sec[1], sec[2], sec[3], sec[4],
                        sec[5], sec[6], sec[7], sec[8], sec[9]);*/
                    cb(dev->mDemuxWrapper, id, sec, sec_len, data);
                    //if (id && sec)
                        //ALOGI("filter %d data callback ok", id);
                }
            }
#if defined(DMX_WAIT_CB) || defined(DMX_SYNC)
            pthread_mutex_lock(&dev->lock);
            dev->flags &= ~DMX_FL_RUN_CB;
            pthread_mutex_unlock(&dev->lock);
            pthread_cond_broadcast(&dev->cond);
#endif
        }
        else
        {
            usleep(10000);
        }
    }

    if (sec_buf)
    {
        free(sec_buf);
    }

    return NULL;
}

/**\brief 等待回调函数停止运行*/
AM_ErrorCode_t AM_DMX_Device::dmx_wait_cb(void)
{
#ifdef DMX_WAIT_CB
    if (thread != pthread_self())
    {
        while (flags & DMX_FL_RUN_CB)
            pthread_cond_wait(&cond, &lock);
    }
#else
//  UNUSED(dev);
#endif
    return AM_SUCCESS;
}

/**\brief 停止Section过滤器*/
AM_ErrorCode_t AM_DMX_Device::dmx_stop_filter(AM_DMX_Filter *filter)
{
    AM_ErrorCode_t ret = AM_SUCCESS;

    if (!filter->used || !filter->enable)
    {
        return ret;
    }

    //if(dev->drv->enable_filter)
    //{
        ret = drv->dvb_enable_filter(this, filter, AM_FALSE);
    //}

    if (ret >= 0)
    {
        filter->enable = AM_FALSE;
    }

    return ret;
}

/**\brief 释放过滤器*/
int AM_DMX_Device::dmx_free_filter(AM_DMX_Filter *filter)
{
    AM_ErrorCode_t ret = AM_SUCCESS;

    if (!filter->used)
        return ret;

    ret = dmx_stop_filter(filter);

    if (ret == AM_SUCCESS)
    {
        //if(dev->drv->free_filter)
        //{
            ret = drv->dvb_free_filter(this, filter);
        //}
    }

    if (ret == AM_SUCCESS)
    {
        filter->used=false;
    }

    return ret;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 打开解复用设备
 * \param dev_no 解复用设备号
 * \param[in] para 解复用设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_Open(int dev_no_t)
{
    //AM_DMX_Device_t *dev;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //init_dmx_dev();

    //AM_TRY(dmx_get_dev(dev_no, &dev));

//  pthread_mutex_lock(&am_gAdpLock);

    if (open_count > 0)
    {
        ALOGI("demux device %d has already been openned", dev_no);
        open_count++;
        ret = AM_SUCCESS;
        goto final;
    }

    dev_no = dev_no_t;

//  if(para->use_sw_filter){
//      dev->drv = &SW_DMX_DRV;
//  }else{
//      dev->drv = &HW_DMX_DRV;
//  }

    //if(dev->drv->open)
    //{
        ret = drv->dvb_open(this);
    //}

    if (ret == AM_SUCCESS)
    {
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&cond, NULL);
        enable_thread = true;
        flags = 0;

        if (pthread_create(&thread, NULL, dmx_data_thread, this))
        {
            pthread_mutex_destroy(&lock);
            pthread_cond_destroy(&cond);
            ret = AM_DMX_ERR_CANNOT_CREATE_THREAD;
        }
    }

    if (ret == AM_SUCCESS)
    {
        open_count = 1;
    }
final:
//  pthread_mutex_unlock(&am_gAdpLock);

    return ret;
}

/**\brief 关闭解复用设备
 * \param dev_no 解复用设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_Close(void)
{
    //AM_DMX_Device_t *dev;
    AM_ErrorCode_t ret = AM_SUCCESS;
    int i;

//  AM_TRY(dmx_get_openned_dev(dev_no, &dev));

//  pthread_mutex_lock(&am_gAdpLock);

    if (open_count == 1)
    {
        enable_thread = AM_FALSE;
        drv->dvb_poll_exit(this);
        pthread_join(thread, NULL);

        for (i=0; i<DMX_FILTER_COUNT; i++)
        {
            dmx_free_filter(&filters[i]);
        }

        //if(dev->drv->close)
        //{
            drv->dvb_close(this);
        //}

        pthread_mutex_destroy(&lock);
        pthread_cond_destroy(&cond);
    }
    open_count--;

//  pthread_mutex_unlock(&am_gAdpLock);

    return ret;
}

/**\brief 分配一个过滤器
 * \param dev_no 解复用设备号
 * \param[out] fhandle 返回过滤器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_AllocateFilter(int *fhandle)
{
    //AM_DMX_Device_t *dev;
    AM_ErrorCode_t ret = AM_SUCCESS;
    int fid;

    assert(fhandle);

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

    for (fid=0; fid < DMX_FILTER_COUNT; fid++)
    {
        if (!filters[fid].used)
            break;
    }

    if (fid >= DMX_FILTER_COUNT)
    {
        ALOGI("no free section filter");
        ret = AM_DMX_ERR_NO_FREE_FILTER;
    }

    if (ret == AM_SUCCESS)
    {
        dmx_wait_cb();

        filters[fid].id   = fid;
        //if(dev->drv->alloc_filter)
        //{
            ret = drv->dvb_alloc_filter(this, &filters[fid]);
        //}
    }

    if (ret == AM_SUCCESS)
    {
        filters[fid].used = true;
        *fhandle = fid;
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 设定Section过滤器
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \param[in] params Section过滤器参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_SetSecFilter(int fhandle, const struct dmx_sct_filter_params *params)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    assert(params);

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        dmx_wait_cb();
        ret = dmx_stop_filter(filter);
    }

    if (ret == AM_SUCCESS)
    {
        ret = drv->dvb_set_sec_filter(this, filter, params);
        ALOGI("set sec filter %d PID: %d filter: %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x",
                fhandle, params->pid,
                params->filter.filter[0], params->filter.mask[0],
                params->filter.filter[1], params->filter.mask[1],
                params->filter.filter[2], params->filter.mask[2],
                params->filter.filter[3], params->filter.mask[3],
                params->filter.filter[4], params->filter.mask[4],
                params->filter.filter[5], params->filter.mask[5],
                params->filter.filter[6], params->filter.mask[6],
                params->filter.filter[7], params->filter.mask[7]);
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 设定PES过滤器
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \param[in] params PES过滤器参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_SetPesFilter(int fhandle, const struct dmx_pes_filter_params *params)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    assert(params);

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    //if(!drv->dvb_set_pes_filter)
    //{
    //  printf("demux do not support set_pes_filter");
    //  return AM_DMX_ERR_NOT_SUPPORTED;
    //}

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        dmx_wait_cb();
        ret = dmx_stop_filter(filter);
    }

    if (ret == AM_SUCCESS)
    {
        ret = drv->dvb_set_pes_filter(this,filter, params);
        ALOGI("set pes filter %d PID %d", fhandle, params->pid);
    }

    pthread_mutex_unlock(&lock);

    return ret;
}
AM_ErrorCode_t AM_DMX_Device::AM_DMX_GetSTC(int fhandle)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));
    //printf("%s line:%d\n", __FUNCTION__, __LINE__);
    //if(!dev->drv->get_stc)
    //{
    //  printf("demux do not support set_pes_filter");
    //  return AM_DMX_ERR_NOT_SUPPORTED;
    //}

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        ret = drv->dvb_get_stc(this, filter);
    }

    pthread_mutex_unlock(&lock);
    ALOGI("%s line:%d\n", __FUNCTION__, __LINE__);

    return ret;
}

/**\brief 释放一个过滤器
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_FreeFilter(int fhandle)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        dmx_wait_cb();
        ret = dmx_free_filter(filter);
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 让一个过滤器开始运行
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_StartFilter(int fhandle)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter = NULL;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (!filter->enable)
    {
        if (ret == AM_SUCCESS)
        {
            //if(dev->drv->enable_filter)
            //{
                ret = drv->dvb_enable_filter(this, filter, true);
            //}
        }

        if (ret == AM_SUCCESS)
        {
            filter->enable = true;
            filter->to_be_stopped = false;
        }
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 停止一个过滤器
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_StopFilter(int fhandle)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter = NULL;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));
    ret = dmx_get_used_filter(fhandle, &filter);



    if (ret == AM_SUCCESS)
    {
        filter->to_be_stopped = true;
        if (filter->enable)
        {
            pthread_mutex_lock(&lock);
            dmx_wait_cb();
            ret = dmx_stop_filter(filter);
            filter->enable = false;
            pthread_mutex_unlock(&lock);
        }
    }


    return ret;
}

/**\brief 设置一个过滤器的缓冲区大小
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \param size 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_SetBufferSize(int fhandle, int size)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

//  if(!drv->set_buf_size)
//  {
    //  printf("do not support set_buf_size");
//      ret = AM_DMX_ERR_NOT_SUPPORTED;
    //}

    if (ret == AM_SUCCESS)
        ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
        ret = drv->dvb_set_buf_size(this, filter, size);

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 取得一个过滤器对应的回调函数和用户参数
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \param[out] cb 返回过滤器对应的回调函数
 * \param[out] data 返回用户参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_GetCallback(int fhandle, AM_DMX_DataCb *cb, void **data)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        if (cb)
            *cb = filter->cb;

        if (data)
            *data = filter->user_data;
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 设置一个过滤器对应的回调函数和用户参数
 * \param dev_no 解复用设备号
 * \param fhandle 过滤器句柄
 * \param[in] cb 回调函数
 * \param[in] data 回调函数的用户参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_SetCallback(int fhandle, AM_DMX_DataCb cb, void *data)
{
    //AM_DMX_Device_t *dev;
    AM_DMX_Filter *filter;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);

    ret = dmx_get_used_filter(fhandle, &filter);

    if (ret == AM_SUCCESS)
    {
        dmx_wait_cb();

        filter->cb = cb;
        filter->user_data = data;
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

/**\brief 设置解复用设备的输入源
 * \param dev_no 解复用设备号
 * \param src 输入源
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
 #if 0
AM_ErrorCode_t AM_DMX_Device::AM_DMX_SetSource(AM_DMX_Source_t src)
{
    //AM_DMX_Device_t *dev;
    AM_ErrorCode_t ret = AM_SUCCESS;

//  AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);
//if (!dev->drv->set_source)
    //{
    //  printf("do not support set_source");
    //  ret = AM_DMX_ERR_NOT_SUPPORTED;
//  }

    if (ret == AM_SUCCESS)
    {
        ret = drv->dvb_set_source(this, src);
    }

    pthread_mutex_unlock(&lock);

    if (ret == AM_SUCCESS)
    {
//      pthread_mutex_lock(&am_gAdpLock);
        src = src;
//      pthread_mutex_unlock(&am_gAdpLock);
    }

    return ret;
}
#endif
/**\brief DMX同步，可用于等待回调函数执行完毕
 * \param dev_no 解复用设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dmx.h)
 */
AM_ErrorCode_t AM_DMX_Device::AM_DMX_Sync()
{
    //AM_DMX_Device_t *dev;
    AM_ErrorCode_t ret = AM_SUCCESS;

    //AM_TRY(dmx_get_openned_dev(dev_no, &dev));

    pthread_mutex_lock(&lock);
    if (thread != pthread_self())
    {
        while (flags & DMX_FL_RUN_CB)
            pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    return ret;
}
#if 0
AM_ErrorCode_t AM_DMX_Device::AM_DMX_GetScrambleStatus(AM_Bool_t dev_status[2])
{
#if 0
    char buf[32];
    char class_file[64];
    int vflag, aflag;
    int i;

    dev_status[0] = dev_status[1] = AM_FALSE;
    snprintf(class_file,sizeof(class_file), "/sys/class/dmx/demux%d_scramble", dev_no);
    for (i=0; i<5; i++)
    {
        if (AM_FileRead(class_file, buf, sizeof(buf)) == AM_SUCCESS)
        {
            sscanf(buf,"%d %d", &vflag, &aflag);
            if (!dev_status[0])
                dev_status[0] = vflag ? AM_TRUE : AM_FALSE;
            if (!dev_status[1])
                dev_status[1] = aflag ? AM_TRUE : AM_FALSE;
            //AM_DEBUG(1, "AM_DMX_GetScrambleStatus video scamble %d, audio scamble %d\n", vflag, aflag);
            if (dev_status[0] && dev_status[1])
            {
                return AM_SUCCESS;
            }
            usleep(10*1000);
        }
        else
        {
            printf("AM_DMX_GetScrambleStatus read scamble status failed\n");
            return AM_FAILURE;
        }
    }
#endif
    return AM_SUCCESS;
}

#endif

AM_ErrorCode_t AM_DMX_Device::AM_DMX_WriteTs(uint8_t* data,int32_t size,uint64_t timeout) {
    if (drv->dvr_data_write(data,size,timeout) != 0) {
        return AM_FAILURE;
    }
    return AM_SUCCESS;
}

