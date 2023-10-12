/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "sensorOTP"

#include <utils/Log.h>
#include <android/log.h>

#include "CamHalDebugLog.h"

#include "sensor_otp.h"

static int g_fd = -1;

int i2c_init(const char *acDevFile, const int slave_addr)
{
    int ret;

    if (g_fd > 0)
    {
        return g_fd;
    }

    g_fd = open(acDevFile, O_RDWR);

    if (g_fd < 0)
    {
        CAMHAL_LOGD("i2c open fails %d \n", g_fd);
        return -1;
    }

    ret = ioctl(g_fd, I2C_SLAVE_FORCE, slave_addr);
    if (ret < 0)
    {
        close(g_fd);
        g_fd = -1;
        return ret;
    }

    CAMHAL_LOGD("i2c init \n");
    return g_fd;
}

int i2c_exit()
{
    if (g_fd >= 0)
    {
        close(g_fd);
        g_fd = -1;
        return 0;
    }

    return -1;
}

int i2c_write(int addr, int addrType, uint8_t data)
{
    if (g_fd < 0)
        return 0;

    int idx = 0;
    int ret;
    char buf[8];

    if (addrType == 2)
    {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    } else {
        buf[idx] = addr & 0xff;
        idx++;
    }

    buf[idx] = data & 0xff;
    idx++;

    ret = write(g_fd, buf, addrType + 1);
    if (ret < 0)
    {
        CAMHAL_LOGE("I2C_WRITE error!\n");
        return -1;
    }

    return 0;
}

uint8_t i2c_read(int addr, int addrType)
{
    int idx = 0;
    int ret;
    char buf[8];

    if (g_fd < 0)
        return 0;

    if (addrType == 2) {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    } else {
        buf[idx] = addr & 0xff;
        idx++;
    }

    ret = write(g_fd, buf, addrType);
    if (ret < 0)
    {
        CAMHAL_LOGE("I2C_WRITE error!\n");
        return -1;
    }

    ret = read(g_fd, buf, 1);
    if (ret < 0)
    {
        CAMHAL_LOGE("I2C_READ error!\n");
        return -1;
    }

    return buf[0];
}

