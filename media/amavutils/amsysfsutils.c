/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */



#define LOG_TAG "amavutils"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/ioctl.h>
#include "include/Amsysfsutils.h"
#include <Amsyswrite.h>
#include <cutils/properties.h>
#include <media_ctl.h>

#ifndef LOGD
#define LOGV ALOGV
#define LOGD ALOGD
#define LOGI ALOGI
#define LOGW ALOGW
#define LOGE ALOGE
#endif

#ifndef NO_USE_SYSWRITE  //added by lifengcao for startup video
#define USE_SYSWRITE
#endif


#ifndef USE_SYSWRITE
int amsysfs_set_sysfs_str(const char *path, const char *val)
{
    int fd,ret;
    int bytes;
    ret = mediactl_set_str_func(path, (char *)val);
	if (ret == UnSupport)
    {
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        close(fd);
        return 0;
    } else {
        LOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return -1;
}
    else return ret;
}
int amsysfs_get_sysfs_str(const char *path, char *valstr, int size) {
    int fd, ret;
    ret = mediactl_get_str_func(path, valstr, size);
    if (ret == UnSupport) {
        if (!path || !valstr || size <= 0) {
            LOGE("Invalid input parameters");
            return -1;
        }
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            memset(valstr, 0, size);
            ssize_t n = read(fd, valstr, size - 1);
            if (n < 0) {
                LOGE("Error reading file %s: %s", path, strerror(errno));
                close(fd);
                return -1;
            } else if (n == size - 1) {
                LOGE("File %s is too large to read", path);
                close(fd);
                return -1;
            } else {
                valstr[n] = '\0';
                close(fd);
            }
        } else {
            LOGE("Unable to open file %s: %s", path, strerror(errno));
            strncpy(valstr, "fail", size);
            return -1;
        }
        return 0;
    } else {
        return ret;
    }
}
int amsysfs_set_sysfs_int(const char *path, int val)
{
    int fd,ret;
    int bytes;
    char  bcmd[16];
    ret = mediactl_set_int_func(path,val);
    if (ret == UnSupport)
    {
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        sprintf(bcmd, "%d", val);
        bytes = write(fd, bcmd, strlen(bcmd));
        close(fd);
        return 0;
    } else {
        LOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return -1;
}
    else return ret;
}
int amsysfs_get_sysfs_int(const char *path) {
    int fd, ret;
    int val = 0;
    char bcmd[16];
    ret = mediactl_get_int_func(path);
    if (ret == UnSupport) {
        if (!path) {
            LOGE("Invalid path");
            return -1;
        }
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, bcmd, sizeof(bcmd));
            if (n < 0) {
                LOGE("Error reading file %s: %s", path, strerror(errno));
                close(fd);
                return -1;
            } else if (n == sizeof(bcmd)) {
                LOGE("File %s is too large to read", path);
                close(fd);
                return -1;
            } else {
                bcmd[n] = '\0';
                val = strtol(bcmd, NULL, 10);
                close(fd);
            }
        } else {
            LOGE("Unable to open file %s: %s", path, strerror(errno));
            return -1;
        }
        return val;
    } else {
        return ret;
    }
}
int amsysfs_set_sysfs_int16(const char *path, int val)
{
    int fd,ret;
    int bytes;
    ret = mediactl_set_int_func(path,val);
    if (ret == UnSupport) {
    char  bcmd[16];
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        sprintf(bcmd, "0x%x", val);
        bytes = write(fd, bcmd, strlen(bcmd));
        close(fd);
        return 0;
    } else {
        LOGE("unable to open file %s,err: %s", path, strerror(errno));
    }

    return -1;
}
    else return ret;
}
int amsysfs_get_sysfs_int16(const char *path)
{
    int fd, ret;
    int val = 0;
    char bcmd[16];
    ret = mediactl_get_int_func(path);
    if (ret == UnSupport) {
        if (path == NULL) {
            LOGE("Invalid input parameter: path is NULL");
            return -1;
        }
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ret = read(fd, bcmd, sizeof(bcmd));
            if (ret >= 0) {
                val = strtol(bcmd, NULL, 16);
            } else {
                LOGE("Error reading from file %s: %s", path, strerror(errno));
            }
            close(fd);
        } else {
            LOGE("Error opening file %s: %s", path, strerror(errno));
        }
    } else {
        return ret;
    }
    return val;
}
unsigned long amsysfs_get_sysfs_ulong(const char *path)
{
    int fd, ret;
    char bcmd[24] = "";
    unsigned long num = 0;
    ret = mediactl_get_int_func(path);
    if (ret == UnSupport) {
        if (path == NULL) {
            LOGE("Invalid input parameter: path is NULL");
            return 0;
        }
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ret = read(fd, bcmd, sizeof(bcmd) - 1);
            if (ret >= 0) {
                bcmd[ret] = '\0';
                num = strtoul(bcmd, NULL, 0);
            } else {
                LOGE("Error reading from file %s: %s", path, strerror(errno));
            }
            close(fd);
        } else {
            LOGE("Error opening file %s: %s", path, strerror(errno));
        }
    } else {
        return ret;
    }
    return num;
}
void amsysfs_write_prop(const char* key, const char* value)
{
    int ret = 0;
    ret = property_set(key,value);
}
#else
int amsysfs_set_sysfs_str(const char *path, const char *val)
{
    int ret = 0;
    ret = mediactl_set_str_func(path,val);
	if (ret == UnSupport) {
        LOGD("%s path =%s,val=%s\n",__FUNCTION__,path,val);
        return amSystemWriteWriteSysfs(path, val);
    }
    else return ret;

}
int  amsysfs_get_sysfs_str(const char *path, char *valstr, int size)
{
    int ret = 0;
    ret = mediactl_get_str_func(path, valstr, size);
	if (ret == UnSupport) {
        LOGD("%s path =%s,val=%s,size=%d\n",__FUNCTION__,path,valstr,size);
        if (amSystemWriteReadNumSysfs(path, valstr, size) != -1) {
            return 0;
        }
        sprintf(valstr, "%s", "fail");
        return -1;
    }
    else return ret;
}

int amsysfs_set_sysfs_int(const char *path, int val)
{
    int ret = 0;
    ret = mediactl_set_int_func(path,val);
	if (ret == UnSupport) {
        LOGD("%s path =%s,val=%d\n",__FUNCTION__,path,val);
        char  bcmd[16] = "";
        sprintf(bcmd, "%d", val);
        return amSystemWriteWriteSysfs(path, bcmd);
    }
    else return ret;
}

int amsysfs_get_sysfs_int(const char *path)
{
    int ret = 0;
    ret = mediactl_get_int_func(path);
	if (ret == UnSupport) {
        LOGD("%s path =%s\n",__FUNCTION__,path);
        char  bcmd[16] = "";
        int val = 0;
        if (amSystemWriteReadSysfs(path, bcmd) == 0) {
            val = strtol(bcmd, NULL, 10);
        }
        return val;
    }
    else return ret;
}

int amsysfs_set_sysfs_int16(const char *path, int val)
{
    int ret = 0;
    ret = mediactl_set_int_func(path,val);
	if (ret == UnSupport) {
        LOGD("%s path =%s,val=%d\n",__FUNCTION__,path,val);
        char  bcmd[16] = "";
        sprintf(bcmd, "0x%x", val);
        return amSystemWriteWriteSysfs(path, bcmd);
    }
    else return ret;
}

int amsysfs_get_sysfs_int16(const char *path)
{
    int fd, ret;
    int val = 0;
    char bcmd[16];
    ret = mediactl_get_int_func(path);
    if (ret == UnSupport) {
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ret = read(fd, bcmd, sizeof(bcmd));
            if (ret >= 0) {
                val = strtol(bcmd, NULL, 16);
            } else {
                LOGE("Error reading from file %s: %s", path, strerror(errno));
            }
            close(fd);
        } else {
            LOGE("Error opening file %s: %s", path, strerror(errno));
        }
    } else {
        return ret;
    }
    return val;
}

unsigned long amsysfs_get_sysfs_ulong(const char *path)
{
    int ret = 0;
    ret = mediactl_get_int_func(path);
	if (ret == UnSupport) {
        LOGD("%s path =%s\n",__FUNCTION__,path);
        char  bcmd[24] = "";
        int val = 0;
        if (amSystemWriteReadSysfs(path, bcmd) == 0) {
            val = strtoul(bcmd, NULL, 0);
        }
        return val;
    }
    else return ret;
}
void amsysfs_write_prop(const char* key, const char* value)
{
    amSystemWriteSetProperty(key,value);
}
#endif

