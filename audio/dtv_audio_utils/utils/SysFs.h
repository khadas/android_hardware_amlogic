/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
//#define LOG_TAG "dvbfs"
#include "tsp_platform.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
int drmsysfs_set_sysfs_str(const char *path, const char *val)
{
    int fd,ret;
    int bytes;
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        close(fd);
        return 0;
    }
    return fd;
}
int  drmsysfs_get_sysfs_str(const char *path, char *valstr, int size)
{
    int fd,ret;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        memset(valstr, 0, size);
        read(fd, valstr, size - 1);
        valstr[strlen(valstr)] = '\0';
        close(fd);
        return 0;
    }
    else {
        ALOGE("amsysfs_get_sysfs_str open failed\n");
    }
    return fd;
}
