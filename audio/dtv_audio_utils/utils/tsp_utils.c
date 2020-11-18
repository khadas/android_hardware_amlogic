/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "tsp"
#include "tsp_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include "tsp_utils.h"
int strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}
enum DRMTYPE drmtype_convert(char * prefix)
{
    enum DRMTYPE type = DRMTYPE_NONE;
    int tag;
    char * p = prefix;

    if (strlen(prefix) < 4)
        return type;
    tag = MKTAG(*p,*(p+1),*(p+2),*(p+3));
    switch (tag) {
        case MKTAG('d', 'c', 'l', 'r'):
            type = DRMTYPE_DCLR ;
            break;
        case MKTAG('d', 's', 't', 'b'):
            type = DRMTYPE_DSTB ;
            break;
        case MKTAG('d', 'h', 'l', 's'):
            type = DRMTYPE_DHLS ;
            break;
        case MKTAG('v', 's', 't', 'b'):
            type = DRMTYPE_VSTB ;
            break;
        case MKTAG('v', 'w', 'c', 'h'):
            type = DRMTYPE_VWCH ;
            break;
        case MKTAG('w', 'c', 'a', 's'):
            type = DRMTYPE_WCAS ;
            break;
        default:
            type = DRMTYPE_INVD;
        DRMPTRACE("unsupport drm type = 0x%x\n", tag);
    }

    DRMPTRACE("[drmtype_convert] tag = 0x%x type = %d\n", tag,type);
    return type;
}

int file_read(const char *name, char *buf, int len)
{
    FILE *fp;
    int ret;

    fp = fopen(name, "r");
    if (!fp)
    {
        DRMPTRACE("cannot open file \"%s\"", name);
        return -1;
    }

    ret = fread(buf, 1, len, fp);
    if (!ret)
    {
        DRMPTRACE("read the file:\"%s\" error:\"%s\" failed", name, strerror(errno));
    }

    fclose(fp);

    return ret ? 0 : -1;
}

int is_ultra()
{
    char buf[32];
    int ret = 0;
    if (!file_read("/sys/class/tee_info/os_version", buf, sizeof(buf)))
    {
        if (strstr(buf, "2.4"))
        {
            DRMPTRACE("is ultra\n");
            ret = 1;
        }
    }
    return ret;
}
uint32_t time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

