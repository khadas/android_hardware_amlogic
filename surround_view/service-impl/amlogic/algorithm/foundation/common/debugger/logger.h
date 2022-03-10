/* Copyright Statement:
*
* This software/firmware and related documentation ("AutoChips Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to AutoChips Inc. and/or its licensors. Without
* the prior written permission of AutoChips inc. and/or its licensors, any
* reproduction, modification, use or disclosure of AutoChips Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* AutoChips Inc. (C) 2019. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("AUTOCHIPS SOFTWARE")
* RECEIVED FROM AUTOCHIPS AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. AUTOCHIPS EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES AUTOCHIPS PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE RENDERED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE AUTOCHIPS SOFTWARE, AND RECEIVER AGREES
* TO LORESULT_OK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN AUTOCHIPS
* SOFTWARE. AUTOCHIPS SHALL ALSO NOT BE RESPONSIBLE FOR ANY AUTOCHIPS SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND AUTOCHIPS'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE AUTOCHIPS SOFTWARE
* RELEASED HEREUNDER WILL BE, AT AUTOCHIPS'S OPTION, TO REVISE OR REPLACE THE
* AUTOCHIPS SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO AUTOCHIPS FOR SUCH AUTOCHIPS SOFTWARE AT ISSUE.
*/

#ifndef LOGGER_H
#define LOGGER_H

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
// #include <stdarg.h>

#ifdef NATIVE_ACTIVITY
#include <android/log.h>
#endif

#define LOG_LEVEL_VERBOSE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_NONE 5

#ifndef LOG_TAG
#define LOG_TAG "UNDEFINED"
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#define NOWMS Time::NowMs()
#define TID getpid()
#define PID pthread_self()

#ifdef NATIVE_ACTIVITY
#define PRINT_(arg...) \
        __android_log_print(ANDROID_LOG_INFO, "", ##arg)
#else
#define PRINT_ printf
#endif

#define LOG(type, format, arg...) \
        PRINT_("%12ld %d %lu %s [%s] [%s:%d] " format , NOWMS, \
        TID, PID, type, LOG_TAG, __FUNCTION__, __LINE__, ##arg)

#if (LOG_LEVEL <= LOG_LEVEL_VERBOSE)
#define LOGV(format, arg...) LOG("V", format "\r\n", ##arg)
#else
#define LOGV(format, arg...)
#endif

#if (LOG_LEVEL <= LOG_LEVEL_DEBUG)
#define LOGD(format, arg...) LOG("D", format "\r\n", ##arg)
#else
#define LOGD(format, arg...)
#endif

#if (LOG_LEVEL <= LOG_LEVEL_INFO)
#define LOGI(format, arg...) LOG("I", format "\r\n", ##arg)
#else
#define LOGI(format, arg...)
#endif

#if (LOG_LEVEL <= LOG_LEVEL_WARNING)
#define LOGW(format, arg...) LOG("W", format "\r\n", ##arg)
#else
#define LOGW(format, arg...)
#endif

#if (LOG_LEVEL <= LOG_LEVEL_ERROR)
#define LOGE(format, arg...) LOG("E", format "\r\n", ##arg)
#else
#define LOGE(format, arg...)
#endif

#define LOGV_IF(cond, format, arg...) \
if(cond) { \
    LOGV(format, ##arg); \
}

#define LOGD_IF(cond, format, arg...) \
if(cond) { \
    LOGD(format, ##arg); \
}

#define LOGI_IF(cond, format, arg...) \
if(cond) { \
    LOGI(format, ##arg); \
}

#define LOGW_IF(cond, format, arg...) \
if(cond) { \
    LOGW(format, ##arg); \
}

#define LOGE_IF(cond, format, arg...) \
if(cond) { \
    LOGE(format, ##arg); \
}

class Time {
public:
    static long NowMs() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
};

#ifdef LINUX
#include <execinfo.h>

#define PRINT_CALL_STACK(depth) \
{ \
    size_t index = 0; \
    size_t size = 0; \
 \
    void *array[depth]; \
    char **strings = nullptr; \
 \
    size = backtrace(array, depth); \
    strings = backtrace_symbols(array, size); \
 \
    if (strings) { \
        for (index = 0; index < size; index++) { \
            LOGI("%s", strings[i]); \
        } \
        free(strings); \
    } \
}

#elif defined ANDROID
#include <utils/CallStack.h>

#define PRINT_CALL_STACK(depth) \
android::CallStack cs("CallStack")

#else
#define PRINT_CALL_STACK(depth) LOGD("FIXME: no call stack!")
#endif

class Debug {
public:
    void signal_handler(int signo) {
        LOGE("receive signal: %d", signo);
        PRINT_CALL_STACK(20);
    }
};

#define HANDLE_SIGNAL(signo) \
Debug::signal(signo, signal_handler)

#define HANDLE_ALL_SIGNALS \
HANDLE_SIGNAL(SIGINT); \
HANDLE_SIGNAL(SIGTERM); \
HANDLE_SIGNAL(SIGBUS); \
HANDLE_SIGNAL(SIGSEGV); \
HANDLE_SIGNAL(SIGABRT); \
HANDLE_SIGNAL(SIGUSR1); \
HANDLE_SIGNAL(SIGKILL); \
HANDLE_SIGNAL(SIGSTOP);

#endif // LOGGER_H
