/*
 * Copyright (C) 2010 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#define LOG_TAG "audio_mediasync"
#define LOG_NDEBUG 0
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cutils/log.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#include "audio_mediasync_wrap.h"


typedef void* (*MediaSync_create_func)(void);

typedef mediasync_result (*MediaSync_allocInstance_func)(void* handle, int32_t DemuxId,
                                                         int32_t PcrPid,
                                                         int32_t *SyncInsId);

typedef mediasync_result (*MediaSync_bindInstance_func)(void* handle, uint32_t SyncInsId,
                                                         sync_stream_type streamtype);
typedef mediasync_result (*MediaSync_setSyncMode_func)(void* handle, sync_mode mode);

typedef mediasync_result (*MediaSync_getSyncMode_func)(void* handle, sync_mode *mode);
typedef mediasync_result (*MediaSync_setPause_func)(void* handle, bool pause);
typedef mediasync_result (*MediaSync_getPause_func)(void* handle, bool *pause);
typedef mediasync_result (*MediaSync_setStartingTimeMedia_func)(void* handle, int64_t startingTimeMediaUs);
typedef mediasync_result (*MediaSync_clearAnchor_func)(void* handle);
typedef mediasync_result (*MediaSync_updateAnchor_func)(void* handle, int64_t anchorTimeMediaUs,
                                                        int64_t anchorTimeRealUs,
                                                        int64_t maxTimeMediaUs);
typedef mediasync_result (*MediaSync_setPlaybackRate_func)(void* handle, float rate);
typedef mediasync_result (*MediaSync_getPlaybackRate_func)(void* handle, float *rate);
typedef mediasync_result (*MediaSync_getMediaTime_func)(void* handle, int64_t realUs,
                                int64_t *outMediaUs,
                                bool allowPastMaxTime);
typedef mediasync_result (*MediaSync_getRealTimeFor_func)(void* handle, int64_t targetMediaUs, int64_t *outRealUs);
typedef mediasync_result (*MediaSync_getRealTimeForNextVsync_func)(void* handle, int64_t *outRealUs);
typedef mediasync_result (*MediaSync_getTrackMediaTime_func)(void* handle, int64_t *outMediaUs);
typedef mediasync_result (*MediaSync_setParameter_func)(void* handle, mediasync_parameter type, void* arg);
typedef mediasync_result (*MediaSync_getParameter_func)(void* handle, mediasync_parameter type, void* arg);
typedef mediasync_result (*MediaSync_queueAudioFrame_func)(void* handle, int64_t apts, int size, int duration, mediasync_time_unit tunit);
typedef mediasync_result (*MediaSync_AudioProcess_func)(void* handle, int64_t apts, int64_t cur_apts, mediasync_time_unit tunit, struct mediasync_audio_policy* asyncPolicy);


typedef mediasync_result (*MediaSync_reset_func)(void* handle);
typedef void (*MediaSync_destroy_func)(void* handle);

static MediaSync_create_func gMediaSync_create = NULL;
static MediaSync_allocInstance_func gMediaSync_allocInstance = NULL;

static MediaSync_bindInstance_func gMediaSync_bindInstance = NULL;
static MediaSync_setSyncMode_func gMediaSync_setSyncMode = NULL;

static MediaSync_getSyncMode_func gMediaSync_getSyncMode = NULL;
static MediaSync_setPause_func gMediaSync_setPause = NULL;
static MediaSync_getPause_func gMediaSync_getPause = NULL;
static MediaSync_setStartingTimeMedia_func gMediaSync_setStartingTimeMedia = NULL;
static MediaSync_clearAnchor_func gMediaSync_clearAnchor = NULL;
static MediaSync_updateAnchor_func gMediaSync_updateAnchor = NULL;
static MediaSync_setPlaybackRate_func gMediaSync_setPlaybackRate = NULL;
static MediaSync_getPlaybackRate_func gMediaSync_getPlaybackRate = NULL;
static MediaSync_getMediaTime_func gMediaSync_getMediaTime = NULL;
static MediaSync_getRealTimeFor_func gMediaSync_getRealTimeFor = NULL;
static MediaSync_getRealTimeForNextVsync_func gMediaSync_getRealTimeForNextVsync = NULL;
static MediaSync_getTrackMediaTime_func gMediaSync_getTrackMediaTime = NULL;
static MediaSync_setParameter_func gMediaSync_setParameter = NULL;
static MediaSync_getParameter_func gMediaSync_getParameter = NULL;
static MediaSync_queueAudioFrame_func gMediaSync_queueAudioFrame = NULL;
static MediaSync_AudioProcess_func gMediaSync_AudioProcess = NULL;
static MediaSync_reset_func gMediaSync_reset = NULL;
static MediaSync_destroy_func gMediaSync_destroy = NULL;

static void*   glibHandle = NULL;

static bool mediasync_wrap_create_init()
{
    bool err = false;

    if (glibHandle == NULL) {
        glibHandle = dlopen("libmediahal_mediasync.so", RTLD_NOW);
        if (glibHandle == NULL) {
            ALOGE("unable to dlopen libmediahal_mediasync.so: %s", dlerror());
            return err;
        }
    }

    gMediaSync_create =
        (MediaSync_create_func)dlsym(glibHandle, "MediaSync_create");
    if (gMediaSync_create == NULL) {
        ALOGE(" dlsym MediaSync_create failed, err=%s \n", dlerror());
        return err;
    }



    gMediaSync_allocInstance =
        (MediaSync_allocInstance_func)dlsym(glibHandle, "MediaSync_allocInstance");
    if (gMediaSync_allocInstance == NULL) {
        ALOGE(" dlsym MediaSync_allocInstance failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_bindInstance =
    (MediaSync_bindInstance_func)dlsym(glibHandle, "MediaSync_bindInstance");
    if (gMediaSync_bindInstance == NULL) {
        ALOGE(" dlsym MediaSync_bindInstance failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_setSyncMode =
    (MediaSync_setSyncMode_func)dlsym(glibHandle, "MediaSync_setSyncMode");
    if (gMediaSync_setSyncMode == NULL) {
        ALOGE(" dlsym MediaSync_setSyncMode failed, err=%s \n", dlerror());
        return err;
    }
    gMediaSync_getSyncMode =
        (MediaSync_getSyncMode_func)dlsym(glibHandle, "MediaSync_getSyncMode");
    if (gMediaSync_getSyncMode == NULL) {
        ALOGE(" dlsym MediaSync_getSyncMode failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_setPause =
    (MediaSync_setPause_func)dlsym(glibHandle, "MediaSync_setPause");
    if (gMediaSync_setPause == NULL) {
        ALOGE(" dlsym MediaSync_setPause failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_getPause =
    (MediaSync_getPause_func)dlsym(glibHandle, "MediaSync_getPause");
    if (gMediaSync_getPause == NULL) {
        ALOGE(" dlsym MediaSync_getPause failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_setStartingTimeMedia =
    (MediaSync_setStartingTimeMedia_func)dlsym(glibHandle, "MediaSync_setStartingTimeMedia");
    if (gMediaSync_setStartingTimeMedia == NULL) {
        ALOGE(" dlsym MediaSync_setStartingTimeMedia failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_clearAnchor =
    (MediaSync_clearAnchor_func)dlsym(glibHandle, "MediaSync_clearAnchor");
    if (gMediaSync_clearAnchor == NULL) {
        ALOGE(" dlsym MediaSync_clearAnchor failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_updateAnchor =
        (MediaSync_updateAnchor_func)dlsym(glibHandle, "MediaSync_updateAnchor");
    if (gMediaSync_updateAnchor == NULL) {
        ALOGE(" dlsym MediaSync_updateAnchor failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_setPlaybackRate =
    (MediaSync_setPlaybackRate_func)dlsym(glibHandle, "MediaSync_setPlaybackRate");
    if (gMediaSync_setPlaybackRate == NULL) {
        ALOGE(" dlsym MediaSync_setPlaybackRate failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_getPlaybackRate =
    (MediaSync_getPlaybackRate_func)dlsym(glibHandle, "MediaSync_getPlaybackRate");
    if (gMediaSync_getPlaybackRate == NULL) {
        ALOGE(" dlsym MediaSync_getPlaybackRate failed, err=%s \n", dlerror());
        return err;
    }
    gMediaSync_getMediaTime =
        (MediaSync_getMediaTime_func)dlsym(glibHandle, "MediaSync_getMediaTime");
    if (gMediaSync_getMediaTime == NULL) {
        ALOGE(" dlsym MediaSync_getMediaTime failed, err=%s \n", dlerror());
        return err;
    }
    gMediaSync_getRealTimeFor =
    (MediaSync_getRealTimeFor_func)dlsym(glibHandle, "MediaSync_getRealTimeFor");
    if (gMediaSync_getRealTimeFor == NULL) {
        ALOGE(" dlsym MediaSync_getRealTimeFor failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_getRealTimeForNextVsync =
    (MediaSync_getRealTimeForNextVsync_func)dlsym(glibHandle, "MediaSync_getRealTimeForNextVsync");
    if (gMediaSync_getRealTimeForNextVsync == NULL) {
        ALOGE(" dlsym MediaSync_getRealTimeForNextVsync failed, err=%s \n", dlerror());
        return err;
    }
 
    gMediaSync_reset =
    (MediaSync_reset_func)dlsym(glibHandle, "MediaSync_reset");
    if (gMediaSync_reset == NULL) {
        ALOGE(" dlsym MediaSync_reset failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_destroy =
    (MediaSync_destroy_func)dlsym(glibHandle, "MediaSync_destroy");
    if (gMediaSync_destroy == NULL) {
        ALOGE(" dlsym MediaSync_destroy failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_getTrackMediaTime =
    (MediaSync_getTrackMediaTime_func)dlsym(glibHandle, "MediaSync_getTrackMediaTime");
    if (gMediaSync_getTrackMediaTime == NULL) {
        ALOGE(" dlsym MediaSync_destroy failed, err=%s \n", dlerror());
        return err;
    }

    gMediaSync_setParameter =
    (MediaSync_setParameter_func)dlsym(glibHandle, "mediasync_setParameter");
    if (gMediaSync_setParameter == NULL) {
        ALOGE(" dlsym mediasync_setParameter failed, err=%s\n", dlerror());
        return err;
    }

    gMediaSync_getParameter =
    (MediaSync_getParameter_func)dlsym(glibHandle, "mediasync_getParameter");
    if (gMediaSync_getParameter == NULL) {
        ALOGE(" dlsym mediasync_getParameter failed, err=%s\n", dlerror());
        return err;
    }

    gMediaSync_queueAudioFrame =
    (MediaSync_queueAudioFrame_func)dlsym(glibHandle, "MediaSync_queueAudioFrame");
    if (gMediaSync_queueAudioFrame == NULL) {
        ALOGE(" dlsym MediaSync_queueAudioFrame failed, err=%s\n", dlerror());
        return err;
    }

    gMediaSync_AudioProcess =
    (MediaSync_AudioProcess_func)dlsym(glibHandle, "MediaSync_AudioProcess");
    if (gMediaSync_AudioProcess == NULL) {
        ALOGE(" dlsym MediaSync_AudioProcess failed, err=%s\n", dlerror());
        return err;
    }

    return true;
}


void* mediasync_wrap_create() {
    bool ret = mediasync_wrap_create_init();
    if (!ret) {
        return NULL;
    }
    return gMediaSync_create();
}


bool mediasync_wrap_allocInstance(void* handle, int32_t DemuxId,
        int32_t PcrPid,
        int32_t *SyncInsId) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_allocInstance(handle, DemuxId, PcrPid, SyncInsId);
         ALOGD(" mediasync_wrap_allocInstance, SyncInsId=%d \n", *SyncInsId);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] fail\n", __func__);
         }

     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

bool mediasync_wrap_bindInstance(void* handle, uint32_t SyncInsId,
                                sync_stream_type streamtype) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_bindInstance(handle, SyncInsId, streamtype);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] fail ret:%d\n", __func__, ret);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_setSyncMode(void* handle, sync_mode mode) {
     if (handle != NULL)  {
         ALOGD(" mediasync_wrap_setSyncMode, mode=%d \n", mode);
         mediasync_result ret = gMediaSync_setSyncMode(handle, mode);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] fail\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getSyncMode(void* handle, sync_mode *mode) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getSyncMode(handle, mode);
         if (ret == AM_MEDIASYNC_OK) {
            ALOGD(" mediasync_wrap_getSyncMode, mode=%d \n", *mode);
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_setPause(void* handle, bool pause) {
     if (handle != NULL)  {
         ALOGD(" mediasync_wrap_setPause, pause=%d \n", pause);
         mediasync_result ret = gMediaSync_setPause(handle, pause);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         }
     }
     return false;
}
bool mediasync_wrap_getPause(void* handle, bool *pause) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getPause(handle, pause);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     }
     return false;
}
bool mediasync_wrap_setStartingTimeMedia(void* handle, int64_t startingTimeMediaUs) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_setStartingTimeMedia(handle, startingTimeMediaUs);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     }
     return false;
}
bool mediasync_wrap_clearAnchor(void* handle) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_clearAnchor(handle);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_updateAnchor(void* handle, int64_t anchorTimeMediaUs,
                                int64_t anchorTimeRealUs,
                                int64_t maxTimeMediaUs) {
     if (handle != NULL)  {
         bool ispause = false;
         mediasync_result ret = gMediaSync_getPause(handle, &ispause);
         if ((ret == AM_MEDIASYNC_OK) && ispause) {
            gMediaSync_setPause(handle, false);
         }

         ret = gMediaSync_updateAnchor(handle, anchorTimeMediaUs, anchorTimeRealUs, maxTimeMediaUs);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_setPlaybackRate(void* handle, float rate) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_setPlaybackRate(handle, rate);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getPlaybackRate(void* handle, float *rate) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getPlaybackRate(handle, rate);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getMediaTime(void* handle, int64_t realUs,
								int64_t *outMediaUs,
								bool allowPastMaxTime) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getMediaTime(handle, realUs, outMediaUs, allowPastMaxTime);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getRealTimeFor(void* handle, int64_t targetMediaUs, int64_t *outRealUs) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getRealTimeFor(handle, targetMediaUs, outRealUs);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getRealTimeForNextVsync(void* handle, int64_t *outRealUs) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getRealTimeForNextVsync(handle, outRealUs);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}
bool mediasync_wrap_getTrackMediaTime(void* handle, int64_t *outMeidaUs) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getTrackMediaTime(handle, outMeidaUs);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

bool mediasync_wrap_setParameter(void* handle, mediasync_parameter type, void* arg) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_setParameter(handle, type, arg);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

bool mediasync_wrap_getParameter(void* handle, mediasync_parameter type, void* arg) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_getParameter(handle, type, arg);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

bool mediasync_wrap_queueAudioFrame(void* handle, int64_t apts, int size, int duration, mediasync_time_unit tunit) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_queueAudioFrame(handle, apts, size, duration, tunit);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

bool mediasync_wrap_AudioProcess(void* handle, int64_t apts, int64_t cur_apts, mediasync_time_unit tunit, struct mediasync_audio_policy* asyncPolicy) {

    if (handle != NULL) {
        mediasync_result ret = gMediaSync_AudioProcess(handle, apts, cur_apts, tunit, asyncPolicy);
        if (ret == AM_MEDIASYNC_OK) {
            return true;
        } else {
            ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }

    return false;
}

bool mediasync_wrap_reset(void* handle) {
     if (handle != NULL)  {
         mediasync_result ret = gMediaSync_reset(handle);
         if (ret == AM_MEDIASYNC_OK) {
            return true;
         } else {
             ALOGE("[%s] no ok\n", __func__);
         }
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
     return false;
}

void mediasync_wrap_destroy(void* handle) {
     if (handle != NULL)  {
         gMediaSync_destroy(handle);
     } else {
        ALOGE("[%s] no handle\n", __func__);
     }
}

