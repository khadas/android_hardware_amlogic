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

#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#include <definition/result_code.h>
#include <debugger/logger.h>

// CHECK
#define CHECK_RESULT_WITH_RETURN_VALUE_AND_PROC( \
    result, coord, ret, proc, arg...) \
if (result != coord) { \
    LOGE("error !"); \
    proc(##arg); \
    return ret; \
}

#define CHECK_RESULT_WITH_RETURN_VALUE(result, coord, ret) \
if (result != coord) { \
    LOGE("error !"); \
    return ret; \
}

#define CHECK_RESULT_WITH_PROC(result, coord, proc, arg...) \
CHECK_RESULT_WITH_RETURN_VALUE_AND_PROC(result, coord, result, proc, ##arg)

#define CHECK_RESULT_WITH_RETURN_VALUE_AND_UNLOCK(result, coord, ret, lock) \
CHECK_RESULT_WITH_RETURN_VALUE_AND_PROC(result, coord, ret, pthread_mutex_unlock, &lock)

#define CHECK_RESULT_AND_RETURN(result, coord) \
CHECK_RESULT_WITH_RETURN_VALUE(result, coord, result)

// CALL
#define LOG_ENTER LOGD("Enter");
#define LOG_EXIT LOGD("Exit\r\n");

#define LOG_EXIT_AND_RETURN_VALUE(value) \
LOG_EXIT \
return value;

#define LOG_EXIT_WITH_RESULT_STR(str) \
LOGD("Exit: %s\r\n", str);

#define LOG_EXIT_WITH_RESULT_CODE(result) \
LOG_EXIT_WITH_RESULT_STR(RESULT_CODE_TO_STR(result))

#define LOCK(lock) \
if (pthread_mutex_lock(&(lock)) != 0) { \
    LOG_EXIT_WITH_RESULT_STR("Lock fail !") \
}

#define UNLOCK(lock) \
if (pthread_mutex_unlock(&(lock)) != 0) { \
    LOG_EXIT_WITH_RESULT_STR("Unlock fail !") \
}

#define CALL_ENTER(lock) \
LOG_ENTER \
LOCK(lock)

#define CALL_EXIT(lock) \
LOG_EXIT_WITH_RESULT_CODE(RESULT_OK) \
UNLOCK(lock)

#define CALL_EXIT_AND_RETURN_RESULT_CODE(lock, result) \
LOG_EXIT_WITH_RESULT_CODE(result) \
UNLOCK(lock) \
return result;

#define CALL_EXIT_AND_RETURN_VALUE(lock, value) \
LOG_EXIT \
UNLOCK(lock) \
return value;

// SYNC
#define WAIT(cond, lock, timeoutNs) \
{ \
    struct timespec ts; \
    struct timeval tv; \
    gettimeofday(&tv, nullptr); \
    ts.tv_sec  = tv.tv_sec; \
    ts.tv_nsec = tv.tv_usec * 1000; \
    ts.tv_nsec += timeoutNs; \
    LOGV("Waiting for cond: %p", &(cond));\
    pthread_cond_timedwait(&(cond), &(lock), &ts); \
}

#define NOTIFY(cond, lock) \
{ \
    LOGV("Sending signal to cond: %p", &(cond));\
    pthread_cond_signal(&(cond)); \
}

// THREAD
#define THREAD_ENTER(state) \
LOG_ENTER \
// pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, state);

#define THREAD_EXIT(result) \
{ \
    char *resultStr = const_cast<char *>(RESULT_CODE_TO_STR(result)); \
    LOG_EXIT_WITH_RESULT_STR(resultStr) \
    pthread_exit(resultStr); \
    return resultStr; \
}

// CLASS
#define DISALLOW_COPY_AND_ASSIGN(cls) \
    cls(const cls&); \
    void operator=(const cls&);

#endif // UTILS_H
