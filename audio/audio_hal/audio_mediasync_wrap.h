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



#ifndef _AUDIO_MEDIASYNC_WRAP_H_
#define _AUDIO_MEDIASYNC_WRAP_H_

#include "MediaSyncInterface.h"

void* mediasync_wrap_create();

bool mediasync_wrap_allocInstance(void* handle, int32_t DemuxId,
        int32_t PcrPid,
        int32_t *SyncInsId);

bool mediasync_wrap_bindInstance(void* handle, uint32_t SyncInsId, 
										sync_stream_type streamtype);
bool mediasync_wrap_setSyncMode(void* handle, sync_mode mode);
bool mediasync_wrap_getSyncMode(void* handle, sync_mode *mode);
bool mediasync_wrap_setPause(void* handle, bool pause);
bool mediasync_wrap_getPause(void* handle, bool *pause);
bool mediasync_wrap_setStartingTimeMedia(void* handle, int64_t startingTimeMediaUs);
bool mediasync_wrap_clearAnchor(void* handle);
bool mediasync_wrap_updateAnchor(void* handle, int64_t anchorTimeMediaUs,
								int64_t anchorTimeRealUs,
								int64_t maxTimeMediaUs);
bool mediasync_wrap_setPlaybackRate(void* handle, float rate);
bool mediasync_wrap_getPlaybackRate(void* handle, float *rate);
bool mediasync_wrap_getMediaTime(void* handle, int64_t realUs,
								int64_t *outMediaUs,
								bool allowPastMaxTime);
bool mediasync_wrap_getRealTimeFor(void* handle, int64_t targetMediaUs, int64_t *outRealUs);
bool mediasync_wrap_getRealTimeForNextVsync(void* handle, int64_t *outRealUs);
bool mediasync_wrap_getTrackMediaTime(void* handle, int64_t *outMeidaUs);
bool mediasync_wrap_setParameter(void* handle, mediasync_parameter type, void* arg);
bool mediasync_wrap_getParameter(void* handle, mediasync_parameter type, void* arg);
bool mediasync_wrap_queueAudioFrame(void* handle, struct mediasync_audio_queue_info* info);
bool mediasync_wrap_AudioProcess(void* handle, int64_t apts, int64_t cur_apts,
                                 mediasync_time_unit tunit,
                                 struct mediasync_audio_policy* asyncPolicy);

bool mediasync_wrap_reset(void* handle);
void mediasync_wrap_destroy(void* handle);



#endif
