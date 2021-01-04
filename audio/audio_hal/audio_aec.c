/*
 * Copyright (C) 2019 The Android Open Source Project
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

// clang-format off
/*
 * Typical AEC signal flow:
 *
 *                          Microphone Audio
 *                          Timestamps
 *                        +--------------------------------------+
 *                        |                                      |       +---------------+
 *                        |    Microphone +---------------+      |       |               |
 *             O|======   |    Audio      | Sample Rate   |      +------->               |
 *    (from         .  +--+    Samples    | +             |              |               |
 *     mic          .  +==================> Format        |==============>               |
 *     codec)       .                     | Conversion    |              |               |   Cleaned
 *             O|======                   | (if required) |              |   Acoustic    |   Audio
 *                                        +---------------+              |   Echo        |   Samples
 *                                                                       |   Canceller   |===================>
 *                                                                       |   (AEC)       |
 *                            Reference   +---------------+              |               |
 *                            Audio       | Sample Rate   |              |               |
 *                            Samples     | +             |              |               |
 *                          +=============> Format        |==============>               |
 *                          |             | Conversion    |              |               |
 *                          |             | (if required) |      +------->               |
 *                          |             +---------------+      |       |               |
 *                          |                                    |       +---------------+
 *                          |    +-------------------------------+
 *                          |    |  Reference Audio
 *                          |    |  Timestamps
 *                          |    |
 *                       +--+----+---------+                                                       AUDIO CAPTURE
 *                       | Speaker         |
 *          +------------+ Audio/Timestamp +---------------------------------------------------------------------------+
 *                       | Buffer          |
 *                       +--^----^---------+                                                       AUDIO PLAYBACK
 *                          |    |
 *                          |    |
 *                          |    |
 *                          |    |
 *                |\        |    |
 *                | +-+     |    |
 *      (to       | | +-----C----+
 *       speaker  | | |     |                                                                  Playback
 *       codec)   | | <=====+================================================================+ Audio
 *                | +-+                                                                        Samples
 *                |/
 *
 */
// clang-format on

#define LOG_TAG "audio_hw_aec"
//#define LOG_NDEBUG 0

#include <audio_utils/primitives.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <malloc.h>
#include <sys/time.h>
#include <tinyalsa/asoundlib.h>
#include <unistd.h>
#include <log/log.h>
#include "audio_aec.h"

//#ifdef ENABLE_AEC_FUNC
//#include "audio_aec_process.h"
//#else
#define aec_spk_mic_init(...) ((int)0)
#define aec_spk_mic_reset(...) ((void)0)
#define aec_spk_mic_process(...) ((int32_t)0)
#define aec_spk_mic_release(...) ((void)0)
//#endif

#define MAX_TIMESTAMP_DIFF_USEC 200000

#define MAX_READ_WAIT_TIME_MSEC 150

uint64_t timespec_to_usec(struct timespec ts) {
    return (ts.tv_sec * 1e6L + ts.tv_nsec/1000);
}

void timestamp_adjust(struct timespec* ts, ssize_t frames, uint32_t sampling_rate) {
    /* This function assumes the adjustment (in nsec) is less than the max value of long,
     * which for 32-bit long this is 2^31 * 1e-9 seconds, slightly over 2 seconds.
     * For 64-bit long it is  9e+9 seconds. */
    long adj_nsec = (frames / (float) sampling_rate) * 1E9L;
    ts->tv_nsec += adj_nsec;
    while (ts->tv_nsec > 1E9L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1E9L;
    }
    if (ts->tv_nsec < 0) {
        ts->tv_sec--;
        ts->tv_nsec += 1E9L;
    }
}

/* Helper function to get PCM hardware timestamp.
 * Only the field 'timestamp' of argument 'ts' is updated. */
int get_pcm_timestamp(struct pcm* pcm, uint32_t sample_rate, struct aec_info* info,
                             bool isOutput) {
    int ret = 0;
    if (pcm_get_htimestamp(pcm, &info->available, &info->timestamp) < 0) {
        ALOGE("Error getting PCM timestamp!");
        info->timestamp.tv_sec = 0;
        info->timestamp.tv_nsec = 0;
        return -EINVAL;
    }
    ssize_t frames;
    if (isOutput) {
        frames = pcm_get_buffer_size(pcm) - info->available;
    } else {
        frames = -info->available; /* rewind timestamp */
    }
    timestamp_adjust(&info->timestamp, frames, sample_rate);
    return ret;
}

void get_reference_audio_in_place(struct aec_t *aec, size_t frames) {
    if (aec->num_reference_channels == aec->spk_num_channels) {
        /* Reference count equals speaker channels, nothing to do here. */
        return;
    } else if (aec->num_reference_channels != 1) {
        /* We don't have  a rule for non-mono references, show error on log */
        ALOGE("Invalid reference count - must be 1 or match number of playback channels!");
        return;
    }
    int16_t *src_Nch = &aec->spk_buf_playback_format[0];
    int16_t *dst_1ch = &aec->spk_buf_playback_format[0];
    int32_t num_channels = (int32_t)aec->spk_num_channels;
    size_t frame, ch;
    for (frame = 0; frame < frames; frame++) {
        int32_t acc = 0;
        for (ch = 0; ch < aec->spk_num_channels; ch++) {
            acc += src_Nch[ch];
        }
        *dst_1ch++ = clamp16(acc/num_channels);
        src_Nch += aec->spk_num_channels;
    }
}

void print_queue_status_to_log(struct aec_t *aec, bool write_side) {
    ssize_t q1 = fifo_available_to_read(aec->spk_fifo);
    ssize_t q2 = fifo_available_to_read(aec->ts_fifo);

    ALOGV("Queue available %s: Spk %zd (count %zd) TS %zd (count %zd)",
        (write_side) ? "(POST-WRITE)" : "(PRE-READ)",
        q1, q1/aec->spk_frame_size_bytes/PLAYBACK_PERIOD_SIZE,
        q2, q2/sizeof(struct aec_info));
}

void flush_aec_fifos(struct aec_t *aec) {
    if (aec == NULL) {
        return;
    }
    if (aec->spk_fifo != NULL) {
        ALOGV("Flushing AEC Spk FIFO...");
        fifo_flush(aec->spk_fifo);
    }
    if (aec->ts_fifo != NULL) {
        ALOGV("Flushing AEC Timestamp FIFO...");
        fifo_flush(aec->ts_fifo);
    }
    /* Reset FIFO read-write offset tracker */
    aec->read_write_diff_bytes = 0;
}

void aec_set_spk_running_no_lock(struct aec_t* aec, bool state) {
    aec->spk_running = state;
}

bool aec_get_spk_running_no_lock(struct aec_t* aec) {
    return aec->spk_running;
}

void destroy_aec_reference_config_no_lock(struct aec_t* aec) {
    if (!aec->spk_initialized) {
        return;
    }
    aec_set_spk_running_no_lock(aec, false);
    fifo_release(aec->spk_fifo);
    fifo_release(aec->ts_fifo);
    memset(&aec->last_spk_info, 0, sizeof(struct aec_info));
    aec->spk_initialized = false;
}

void destroy_aec_mic_config_no_lock(struct aec_t* aec) {
    if (!aec->mic_initialized) {
        return;
    }
    release_resampler(aec->spk_resampler);
    aml_audio_free(aec->mic_buf);
    aml_audio_free(aec->spk_buf);
    aml_audio_free(aec->spk_buf_playback_format);
    aml_audio_free(aec->spk_buf_resampler_out);
    memset(&aec->last_mic_info, 0, sizeof(struct aec_info));
    aec->mic_initialized = false;
}

struct aec_t *init_aec_interface() {
    ALOGV("%s enter", __func__);
    struct aec_t *aec = (struct aec_t *)aml_audio_calloc(1, sizeof(struct aec_t));
    if (aec == NULL) {
        ALOGE("Failed to allocate memory for AEC interface!");
    } else {
        pthread_mutex_init(&aec->lock, NULL);
    }

    ALOGV("%s exit", __func__);
    return aec;
}

void release_aec_interface(struct aec_t *aec) {
    ALOGV("%s enter", __func__);
    pthread_mutex_lock(&aec->lock);
    destroy_aec_mic_config_no_lock(aec);
    destroy_aec_reference_config_no_lock(aec);
    pthread_mutex_unlock(&aec->lock);
    aml_audio_free(aec);
    ALOGV("%s exit", __func__);
}

int init_aec(int sampling_rate __unused, int num_reference_channels,
                int num_microphone_channels __unused, struct aec_t **aec_ptr) {
    ALOGV("%s enter", __func__);
    int ret = 0;
    int aec_ret = aec_spk_mic_init(
                    sampling_rate,
                    num_reference_channels,
                    num_microphone_channels);
    if (aec_ret) {
        ALOGE("AEC object failed to initialize!");
        ret = -EINVAL;
    }
    struct aec_t *aec = init_aec_interface();
    if (!ret) {
        aec->num_reference_channels = num_reference_channels;
        /* Set defaults, will be overridden by settings in init_aec_(mic|referece_config) */
        /* Capture uses 2-ch, 32-bit frames */
        aec->mic_sampling_rate = CAPTURE_CODEC_SAMPLING_RATE;
        aec->mic_frame_size_bytes = CHANNEL_STEREO * sizeof(int32_t);
        aec->mic_num_channels = CHANNEL_STEREO;

        /* Playback uses 2-ch, 16-bit frames */
        aec->spk_sampling_rate = PLAYBACK_CODEC_SAMPLING_RATE;
        aec->spk_frame_size_bytes = CHANNEL_STEREO * sizeof(int16_t);
        aec->spk_num_channels = CHANNEL_STEREO;
    }

    (*aec_ptr) = aec;
    ALOGV("%s exit", __func__);
    return ret;
}

void release_aec(struct aec_t *aec) {
    ALOGV("%s enter", __func__);
    if (aec == NULL) {
        return;
    }
    release_aec_interface(aec);
    aec_spk_mic_release();
    ALOGV("%s exit", __func__);
}

int init_aec_reference_config(struct aec_t *aec, struct pcm_config config)
{
    ALOGV("%s enter", __func__);
    if (!aec) {
        ALOGE("AEC: No valid interface found!");
        return -EINVAL;
    }

    int ret = 0, frames = 0, frame_size = 0;
    pthread_mutex_lock(&aec->lock);
    if (aec->spk_initialized) {
        destroy_aec_reference_config_no_lock(aec);
    }

    frames = config.period_count * config.period_size;
    frame_size = config.channels * (pcm_format_to_bits(config.format) >> 3);
    aec->spk_fifo = fifo_init(frames * frame_size, false /* reader_throttles_writer */);
    if (aec->spk_fifo == NULL) {
        ALOGE("AEC: Speaker loopback FIFO Init failed!");
        ret = -EINVAL;
        goto exit;
    }
    aec->ts_fifo = fifo_init(
            config.period_count * sizeof(struct aec_info),
            false /* reader_throttles_writer */);
    if (aec->ts_fifo == NULL) {
        ALOGE("AEC: Speaker timestamp FIFO Init failed!");
        ret = -EINVAL;
        fifo_release(aec->spk_fifo);
        goto exit;
    }

    aec->spk_sampling_rate = PLAYBACK_CODEC_SAMPLING_RATE;
    aec->spk_frame_size_bytes = frame_size;
    aec->spk_num_channels = config.channels;
    aec->spk_initialized = true;
exit:
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
    return ret;
}

void destroy_aec_reference_config(struct aec_t* aec) {
    ALOGV("%s enter", __func__);
    if (aec == NULL) {
        ALOGV("%s exit", __func__);
        return;
    }
    pthread_mutex_lock(&aec->lock);
    destroy_aec_reference_config_no_lock(aec);
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
}

int write_to_reference_fifo(struct aec_t* aec, void* buffer, struct aec_info* info) {
    ALOGV("%s enter", __func__);
    int ret = 0;
    size_t bytes = info->bytes;

    /* Write audio samples to FIFO */
    ssize_t written_bytes = fifo_write(aec->spk_fifo, buffer, bytes);
    if (written_bytes != bytes) {
        ALOGE("Could only write %zu of %zu bytes", written_bytes, bytes);
        ret = -ENOMEM;
    }

    /* Write timestamp to FIFO */
    info->bytes = written_bytes;
    ALOGV("Speaker timestamp: %ld s, %ld nsec", info->timestamp.tv_sec, info->timestamp.tv_nsec);
    ssize_t ts_bytes = fifo_write(aec->ts_fifo, info, sizeof(struct aec_info));
    ALOGV("Wrote TS bytes: %zu", ts_bytes);
    print_queue_status_to_log(aec, true);
    ALOGV("%s exit", __func__);
    return ret;
}

void get_spk_timestamp(struct aec_t* aec, ssize_t read_bytes, uint64_t* spk_time) {
    *spk_time = 0;
    uint64_t spk_time_offset = 0;
    float usec_per_byte = 1E6 / ((float)(aec->spk_frame_size_bytes * aec->spk_sampling_rate));
    if (aec->read_write_diff_bytes < 0) {
        /* We're still reading a previous write packet. (We only need the first sample's timestamp,
         * so even if we straddle packets we only care about the first one)
         * So we just use the previous timestamp, with an appropriate offset
         * based on the number of bytes remaining to be read from that write packet. */
        spk_time_offset = (aec->last_spk_info.bytes + aec->read_write_diff_bytes) * usec_per_byte;
        ALOGV("Reusing previous timestamp, calculated offset (usec) %" PRIu64, spk_time_offset);
    } else {
        /* If read_write_diff_bytes > 0, there are no new writes, so there won't be timestamps in
         * the FIFO, and the check below will fail. */
        if (!fifo_available_to_read(aec->ts_fifo)) {
            ALOGE("Timestamp error: no new timestamps!");
            return;
        }
        /* We just read valid data, so if we're here, we should have a valid timestamp to use. */
        ssize_t ts_bytes = fifo_read(aec->ts_fifo, &aec->last_spk_info, sizeof(struct aec_info));
        ALOGV("Read TS bytes: %zd, expected %zu", ts_bytes, sizeof(struct aec_info));
        aec->read_write_diff_bytes -= aec->last_spk_info.bytes;
    }

    *spk_time = timespec_to_usec(aec->last_spk_info.timestamp) + spk_time_offset;

    aec->read_write_diff_bytes += read_bytes;
    struct aec_info spk_info = aec->last_spk_info;
    while (aec->read_write_diff_bytes > 0) {
        /* If read_write_diff_bytes > 0, it means that there are more write packet timestamps
         * in FIFO (since there we read more valid data the size of the current timestamp's
         * packet). Keep reading timestamps from FIFO to get to the most recent one. */
        if (!fifo_available_to_read(aec->ts_fifo)) {
            /* There are no more timestamps, we have the most recent one. */
            ALOGV("At the end of timestamp FIFO, breaking...");
            break;
        }
        fifo_read(aec->ts_fifo, &spk_info, sizeof(struct aec_info));
        ALOGV("Fast-forwarded timestamp by %zd bytes, remaining bytes: %zd,"
              " new timestamp (usec) %" PRIu64,
              spk_info.bytes, aec->read_write_diff_bytes, timespec_to_usec(spk_info.timestamp));
        aec->read_write_diff_bytes -= spk_info.bytes;
    }
    aec->last_spk_info = spk_info;
}

int get_reference_samples(struct aec_t* aec, void* buffer, struct aec_info* info) {
    ALOGV("%s enter", __func__);

    if (!aec->spk_initialized) {
        ALOGE("%s called with no reference initialized", __func__);
        return -EINVAL;
    }

    size_t bytes = info->bytes;
    const size_t frames = bytes / aec->mic_frame_size_bytes;
    const size_t sample_rate_ratio = aec->spk_sampling_rate / aec->mic_sampling_rate;

    /* Read audio samples from FIFO */
    const size_t req_bytes = frames * sample_rate_ratio * aec->spk_frame_size_bytes;
    ssize_t available_bytes = 0;
    unsigned int wait_count = MAX_READ_WAIT_TIME_MSEC;
    while (true) {
        available_bytes = fifo_available_to_read(aec->spk_fifo);
        if (available_bytes >= req_bytes) {
            break;
        } else if (available_bytes < 0) {
            ALOGE("fifo_read returned code %zu ", available_bytes);
            return -ENOMEM;
        }

        ALOGV("Sleeping, required bytes: %zu, available bytes: %zd", req_bytes, available_bytes);
        usleep(1000);
        if ((wait_count--) == 0) {
            ALOGE("Timed out waiting for read from reference FIFO");
            return -ETIMEDOUT;
        }
    }

    const size_t read_bytes = fifo_read(aec->spk_fifo, aec->spk_buf_playback_format, req_bytes);
    /* Get timestamp*/
    get_spk_timestamp(aec, read_bytes, &info->timestamp_usec);

    /* Get reference - could be mono, downmixed from multichannel.
     * Reference stored at spk_buf_playback_format */
    const size_t resampler_in_frames = frames * sample_rate_ratio;
    get_reference_audio_in_place(aec, resampler_in_frames);

    int16_t* resampler_out_buf;
    /* Resample to mic sampling rate (16-bit resampler) */
    if (aec->spk_resampler != NULL) {
        size_t in_frame_count = resampler_in_frames;
        size_t out_frame_count = frames;
        aec->spk_resampler->resample_from_input(aec->spk_resampler, aec->spk_buf_playback_format,
                                                &in_frame_count, aec->spk_buf_resampler_out,
                                                &out_frame_count);
        resampler_out_buf = aec->spk_buf_resampler_out;
    } else {
        if (sample_rate_ratio != 1) {
            ALOGE("Speaker sample rate %d, mic sample rate %d but no resampler defined!",
                  aec->spk_sampling_rate, aec->mic_sampling_rate);
        }
        resampler_out_buf = aec->spk_buf_playback_format;
    }

    /* Convert to 32 bit */
    int16_t* src16 = resampler_out_buf;
    int32_t* dst32 = buffer;
    size_t frame, ch;
    for (frame = 0; frame < frames; frame++) {
        for (ch = 0; ch < aec->num_reference_channels; ch++) {
            *dst32++ = ((int32_t)*src16++) << 16;
        }
    }

    info->bytes = bytes;

    ALOGV("%s exit", __func__);
    return 0;
}

int init_aec_mic_config(struct aec_t *aec, struct aml_stream_in *in) {
    ALOGV("%s enter", __func__);
#if DEBUG_AEC
    remove("/data/local/traces/aec_in.pcm");
    remove("/data/local/traces/aec_out.pcm");
    remove("/data/local/traces/aec_ref.pcm");
    remove("/data/local/traces/aec_timestamps.txt");
#endif /* #if DEBUG_AEC */

    if (!aec) {
        ALOGE("AEC: No valid interface found!");
        return -EINVAL;
    }

    int ret = 0;
    pthread_mutex_lock(&aec->lock);
    if (aec->mic_initialized) {
        destroy_aec_mic_config_no_lock(aec);
    }
    aec->mic_sampling_rate = in->config.rate;
    aec->mic_frame_size_bytes = audio_stream_in_frame_size(&in->stream);
    aec->mic_num_channels = in->config.channels;

    aec->mic_buf_size_bytes = in->config.period_size * audio_stream_in_frame_size(&in->stream);
    aec->mic_buf = (int32_t *)aml_audio_malloc(aec->mic_buf_size_bytes);
    if (aec->mic_buf == NULL) {
        ret = -ENOMEM;
        goto exit;
    }
    memset(aec->mic_buf, 0, aec->mic_buf_size_bytes);
    /* Reference buffer is the same number of frames as mic,
     * only with a different number of channels in the frame. */
    aec->spk_buf_size_bytes = in->config.period_size * aec->spk_frame_size_bytes;
    aec->spk_buf = (int32_t *)aml_audio_malloc(aec->spk_buf_size_bytes);
    if (aec->spk_buf == NULL) {
        ret = -ENOMEM;
        goto exit_1;
    }
    memset(aec->spk_buf, 0, aec->spk_buf_size_bytes);

    /* Pre-resampler buffer */
    size_t spk_frame_out_format_bytes = aec->spk_sampling_rate / aec->mic_sampling_rate *
                                            aec->spk_buf_size_bytes;
    aec->spk_buf_playback_format = (int16_t *)aml_audio_malloc(spk_frame_out_format_bytes);
    if (aec->spk_buf_playback_format == NULL) {
        ret = -ENOMEM;
        goto exit_2;
    }
    /* Resampler is 16-bit */
    aec->spk_buf_resampler_out = (int16_t *)aml_audio_malloc(aec->spk_buf_size_bytes);
    if (aec->spk_buf_resampler_out == NULL) {
        ret = -ENOMEM;
        goto exit_3;
    }

    /* Don't use resampler if it's not required */
    if (in->config.rate == aec->spk_sampling_rate) {
        aec->spk_resampler = NULL;
    } else {
        int resampler_ret = create_resampler(
                aec->spk_sampling_rate, in->config.rate, aec->num_reference_channels,
                RESAMPLER_QUALITY_MAX - 1, /* MAX - 1 is the real max */
                NULL,                      /* resampler_buffer_provider */
                &aec->spk_resampler);
        if (resampler_ret) {
            ALOGE("AEC: Resampler initialization failed! Error code %d", resampler_ret);
            ret = resampler_ret;
            goto exit_4;
        }
    }

    flush_aec_fifos(aec);
    aec_spk_mic_reset();
    aec->mic_initialized = true;

exit:
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
    return ret;

exit_4:
    aml_audio_free(aec->spk_buf_resampler_out);
exit_3:
    aml_audio_free(aec->spk_buf_playback_format);
exit_2:
    aml_audio_free(aec->spk_buf);
exit_1:
    aml_audio_free(aec->mic_buf);
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
    return ret;
}

void aec_set_spk_running(struct aec_t *aec, bool state) {
    ALOGV("%s enter", __func__);
    pthread_mutex_lock(&aec->lock);
    aec_set_spk_running_no_lock(aec, state);
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
}

bool aec_get_spk_running(struct aec_t *aec) {
    ALOGV("%s enter", __func__);
    pthread_mutex_lock(&aec->lock);
    bool state = aec_get_spk_running_no_lock(aec);
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
    return state;
}

void destroy_aec_mic_config(struct aec_t* aec) {
    ALOGV("%s enter", __func__);
    if (aec == NULL) {
        ALOGV("%s exit", __func__);
        return;
    }

    pthread_mutex_lock(&aec->lock);
    destroy_aec_mic_config_no_lock(aec);
    pthread_mutex_unlock(&aec->lock);
    ALOGV("%s exit", __func__);
}

