/*
 * Copyright (C) 2017 Amlogic Corporation.
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
#include <stdlib.h>
#include <errno.h>
#include <cutils/log.h>
#include "google_aec.h"
#include "audio_aec_process.h"

#undef  LOG_TAG
#define LOG_TAG  "audio_hw_primary"

static audio_ears::GoogleAec *pGoogleAec;

int32_t* aec_spk_mic_process(int32_t *spk_buf, int spk_samples_per_channel,
    int32_t *mic_buf, int mic_samples_per_channel, int *cleaned_samples_per_channel)
{
    const int32_t *spk_samples = spk_buf;
    const int32_t *mic_samples = mic_buf;
    const int32_t *out_buf;

    out_buf = pGoogleAec->ProcessInt32InterleavedAudio(spk_samples, spk_samples_per_channel,
        mic_samples, mic_samples_per_channel, cleaned_samples_per_channel);
    if (!out_buf) {
        ALOGE("%s: AEC process failed, cleaned_samples_per_channel = %d", __func__, *cleaned_samples_per_channel);
        pGoogleAec->Reset();
        return NULL;
    }

    return (int32_t*)out_buf;
}

int aec_spk_mic_init(void)
{
    if (!pGoogleAec) {
        pGoogleAec = new audio_ears::GoogleAec(48000, 2, 2, "GoogleAecMode3");
        if (!pGoogleAec) {
            ALOGE("%s: alloc GoogleAec failed", __func__);
            return -ENOMEM;
        }
    }
    pGoogleAec->Reset();

    return 0;
}

void aec_spk_mic_release(void)
{
    delete pGoogleAec;
    pGoogleAec = NULL;
}
