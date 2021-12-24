/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


/**************************************************
* example based on amcodec
**************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "vcodec.h"

#define READ_SIZE (64 * 1024)
#define EXTERNAL_PTS    (1)
#define SYNC_OUTSIDE    (2)
#define UNIT_FREQ       96000
#define PTS_FREQ        90000
#define AV_SYNC_THRESH    PTS_FREQ*30

static vcodec_para_t v_codec_para;
static vcodec_para_t *pcodec, *vpcodec;
static char *filename;
FILE* fp = NULL;
static int axis[8] = {0};

int osd_blank(char *path, int cmd)
{
    int fd;
    char  bcmd[16];
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);

    if (fd >= 0) {
        sprintf(bcmd, "%d", cmd);
        write(fd, bcmd, strlen(bcmd));
        close(fd);
        return 0;
    }

    return -1;
}

int set_tsync_enable(int enable)
{
    int fd;
    char *path = "/sys/class/tsync/enable";
    char  bcmd[16];
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        sprintf(bcmd, "%d", enable);
        write(fd, bcmd, strlen(bcmd));
        close(fd);
        return 0;
    }

    return -1;
}

int parse_para(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;

    if (!startp) {
        return 0;
    }

    len = strlen(startp);

    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }

        if (len == 0) {
            break;
        }

        *out++ = strtol(startp, &endp, 0);

        len -= endp - startp;
        startp = endp;
        count++;

    } while ((endp) && (count < para_num) && (len > 0));

    return count;
}

int set_display_axis(int recovery)
{
    int fd;
    char *path = "/sys/class/display/axis";
    char str[128];
    int count;

    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        if (!recovery) {
            read(fd, str, 128);
            printf("read axis %s, length %zu\n", str, strlen(str));
            count = parse_para(str, 8, axis);
        }
        if (recovery) {
            sprintf(str, "%d %d %d %d %d %d %d %d",
                    axis[0], axis[1], axis[2], axis[3], axis[4], axis[5], axis[6], axis[7]);
        } else {
            sprintf(str, "2048 %d %d %d %d %d %d %d",
                    axis[1], axis[2], axis[3], axis[4], axis[5], axis[6], axis[7]);
        }
        write(fd, str, strlen(str));
        close(fd);
        return 0;
    }

    return -1;
}

static void signal_handler(int signum)
{
    printf("Get signum=%x\n", signum);
    vcodec_close(vpcodec);
    fclose(fp);
    set_display_axis(1);
    osd_blank("/sys/class/graphics/fb0/blank", 0);
    osd_blank("/sys/class/graphics/fb0/osd_display_debug", 0);
    signal(signum, SIG_DFL);
    raise(signum);
}

int main(int argc, char *argv[])
{
    int ret = CODEC_ERROR_NONE;
    char buffer[READ_SIZE];

    uint32_t Readlen;
    uint32_t isize;
    struct buf_status vbuf;
    int end;
    int cnt = 0;
    uint32_t last_rp = 1;

    if (argc < 6) {
        printf("Corret command: esplayer <filename> <width> <height> <fps> <format(1:mpeg4 2:h264 6:vc1)> [subformat for mpeg4/vc1]\n");
        return -1;
    }
    osd_blank("/sys/class/graphics/fb0/osd_display_debug", 1);
    osd_blank("/sys/class/graphics/fb0/blank", 1);
    set_display_axis(0);

    vpcodec = &v_codec_para;
    memset(vpcodec, 0, sizeof(vcodec_para_t));

    vpcodec->has_video = 1;
    vpcodec->video_type = atoi(argv[5]);
    if (vpcodec->video_type == VFORMAT_H264) {
        vpcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
        vpcodec->am_sysinfo.param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE);
    } else if (vpcodec->video_type == VFORMAT_VC1) {
        if (argc < 7) {
            printf("No subformat for vc1, take the default VIDEO_DEC_FORMAT_WVC1\n");
            vpcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_WVC1;
        } else {
            vpcodec->am_sysinfo.format = atoi(argv[6]);
        }
    } else if (vpcodec->video_type == VFORMAT_MPEG4) {
        if (argc < 7) {
            printf("No subformat for mpeg4, take the default VIDEO_DEC_FORMAT_MPEG4_5\n");
            vpcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_MPEG4_5;
        } else {
            vpcodec->am_sysinfo.format = atoi(argv[6]);
        }
    }

    vpcodec->stream_type = STREAM_TYPE_ES_VIDEO;
    vpcodec->am_sysinfo.rate = 96000 / atoi(argv[4]);
    vpcodec->am_sysinfo.height = atoi(argv[3]);
    vpcodec->am_sysinfo.width = atoi(argv[2]);
    vpcodec->has_audio = 0;
    vpcodec->noblock = 0;

    printf("\n*********CODEC PLAYER DEMO************\n\n");
    filename = argv[1];
    printf("file %s to be played\n", filename);

    if ((fp = fopen(filename, "rb")) == NULL) {
        printf("open file error!\n");
        goto osd_restore;
    }

    ret = vcodec_init(vpcodec);
    if (ret != CODEC_ERROR_NONE) {
        fclose(fp);
        printf("codec init failed, ret=-0x%x", -ret);
        goto osd_restore;
    }
    printf("video codec ok!\n");

    set_tsync_enable(0);

    pcodec = vpcodec;
    end = 0;
    while (1) {
        if (!end) {
            Readlen = fread(buffer, 1, READ_SIZE, fp);
            //printf("Readlen %d\n", Readlen);
            if (Readlen <= 0) {
                printf("read file error!\n");
                rewind(fp);
            }
        } else
            Readlen = 0;

        isize = 0;
        if (end) {
            memset(buffer, 0 ,READ_SIZE);
            Readlen = READ_SIZE - 10;
        }
        cnt = 0;
        do {
            ret = vcodec_write(pcodec, buffer + isize, Readlen);
            if (ret <= 0) {
                if (errno != EAGAIN) {
                    printf("write data failed, errno %d\n", errno);
                    goto error;
                } else {
                    usleep(10);
                    if (++cnt > 2000000) {
                      end = 1;
                      break;
                    }
                    continue;
                }
            } else {
                isize += ret;
                cnt = 0;
            }
            //printf("ret %d, isize %d\n", ret, isize);
        } while (isize < Readlen);
        if (end)
            break;
        if (feof(fp))
            end = 1;

        signal(SIGCHLD, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGHUP, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGSEGV, signal_handler);
        signal(SIGINT, signal_handler);
        signal(SIGQUIT, signal_handler);
    }
    cnt = 0;
    do {
        ret = vcodec_get_vbuf_state(pcodec, &vbuf);
        if (ret != 0) {
            printf("vcodec_get_vbuf_state error: %x\n", -ret);
            goto error;
        }
        if (last_rp != vbuf.read_pointer)
            cnt = 0;
        last_rp = vbuf.read_pointer;
        usleep(10000);
        if (++cnt > 500)
            break;
    } while (vbuf.data_len > 0x100);

    printf("play end\n");
error:
    vcodec_close(vpcodec);
    fclose(fp);
osd_restore:
    set_display_axis(1);
    osd_blank("/sys/class/graphics/fb0/blank", 0);
    osd_blank("/sys/class/graphics/fb0/osd_display_debug", 0);

    return 0;
}

