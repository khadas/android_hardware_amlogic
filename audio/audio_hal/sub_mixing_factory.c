
#define LOG_TAG "sub_mixing_factory"
//#define LOG_NDEBUG 0
#define __USE_GNU

#include <errno.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <inttypes.h>
#include <aml_volume_utils.h>

#include "sub_mixing_factory.h"
#include "amlAudioMixer.h"
#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "hw_avsync_callbacks.h"
#include "../libms12/include/aml_audio_ms12.h"
#include "dolby_lib_api.h"
#include "alsa_device_parser.h"
#include "a2dp_hal.h"
#include "aml_malloc_debug.h"
#ifdef ENABLE_AEC_APP
#include "audio_aec.h"
#endif
#include "aml_audio_timer.h"
#include "karaoke_manager.h"

//#define DEBUG_TIME

#define SUBMIX_USECASE_MASK            (0xffffff7e)  /* PCM_NORMAL(0) and PCM_MMAP(7) have been cleared*/
static int on_notify_cbk(void *data);
static int on_input_avail_cbk(void *data);
static ssize_t out_write_subMixingPCM(struct audio_stream_out *stream,
                      const void *buffer,
                      size_t bytes);
static int out_pause_subMixingPCM(struct audio_stream_out *stream);
static int out_resume_subMixingPCM(struct audio_stream_out *stream);
static int out_flush_subMixingPCM(struct audio_stream_out *stream);

struct pcm *getSubMixingPCMdev(struct subMixing *sm)
{
    return pcm_mixer_get_pcm_handle(sm->mixerData);
}

static int startMixingThread(struct subMixing *sm)
{
    return pcm_mixer_thread_run(sm->mixerData);
}

static int exitMixingThread(struct subMixing *sm)
{
    return pcm_mixer_thread_exit(sm->mixerData);
}

static int initSubMixngOutput(
        struct subMixing *sm,
        struct aml_audio_device *adev)
{
    R_CHECK_POINTER_LEGAL(-EINVAL, sm, "");
    if (sm->type == MIXER_LPCM) {
        struct amlAudioMixer *amixer = newAmlAudioMixer(adev);
        R_CHECK_POINTER_LEGAL(-ENOMEM, amixer, "newAmlAudioMixer failed");
        sm->mixerData = amixer;
        startMixingThread(sm);
    } else if (sm->type == MIXER_MS12) {
        //TODO
        AM_LOGW("not support yet, in TODO list");
    } else {
        AM_LOGE("not support");
        return -EINVAL;
    }
    return 0;
};

static int releaseSubMixingOutput(struct subMixing *sm)
{
    R_CHECK_POINTER_LEGAL(-EINVAL, sm, "");
    AM_LOGI("++");
    exitMixingThread(sm);
    freeAmlAudioMixer(sm->mixerData);
    sm->mixerData = NULL;

    return 0;
}

static ssize_t aml_out_write_to_mixer(struct audio_stream_out *stream, const void* buffer,
                                    size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    const char *data = (char *)buffer;
    size_t written_total = 0, frame_size = 4;
    uint32_t latency_frames = 0;
    struct timespec ts;

    if (adev->is_netflix && STREAM_PCM_NORMAL == out->usecase) {
        aml_audio_data_handle(stream, buffer, bytes);
    }

    do {
        ssize_t written = 0;
        AM_LOGV("stream usecase: %s, written_total %zu, bytes %zu",
            usecase2Str(out->usecase), written_total, bytes);

        written = mixer_write_inport(audio_mixer,
                out->inputPortID, data, bytes - written_total);
        if (written < 0) {
            AM_LOGE("write failed, errno = %zu", written);
            return written;
        }

        if (written > 0) {
            written_total += written;
            data += written;
            //latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->port_index) +
             //       mixer_get_outport_latency_frames(audio_mixer);
            //pthread_mutex_lock(&out->lock);
            //clock_gettime(CLOCK_MONOTONIC, &out->timestamp);
            //out->last_frames_postion += written / frame_size - latency_frames;
            //pthread_mutex_unlock(&out->lock);
        }
        AM_LOGV("portindex(%d) written(%zu), written_total(%zu), bytes(%zu)",
            out->inputPortID, written, written_total, bytes);

        if (written_total >= bytes) {
            AM_LOGV("exit");
            break;
        }

        //usleep((bytes- written_total) * 1000 / 5 / 48);
        //if (out->port_index == 1) {
            ts_wait_time_us(&ts, 5000);
            AM_LOGV("-wait....");
            pthread_mutex_lock(&out->cond_lock);
            pthread_cond_timedwait(&out->cond, &out->cond_lock, &ts);
            AM_LOGV("--wait wakeup");
            pthread_mutex_unlock(&out->cond_lock);
        //}
    } while (1);

    return written_total;
}

static int consume_meta_data(void *cookie,
        uint32_t frame_size, int64_t pts, uint64_t offset)
{
    struct aml_stream_out *out = (struct aml_stream_out *)cookie;
    struct aml_audio_device *adev = out->dev;
    //struct aml_audio_mixer *audio_mixer = adev->audio_mixer;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    struct meta_data_list *mdata_list = aml_audio_calloc(1, sizeof(struct meta_data_list));

    R_CHECK_POINTER_LEGAL(-ENOMEM, mdata_list, "no memory");
    if (out->pause_status) {
        AM_LOGE("write in pause status");
    }

    mdata_list->mdata.frame_size = frame_size;
    mdata_list->mdata.pts = pts;
    mdata_list->mdata.payload_offset = offset;

    if (out->debug_stream) {
        AM_LOGD("frame_size %d, pts %" PRId64 "ms, payload offset %" PRId64 "",
                frame_size, pts/1000000, offset);
    }
    if (get_mixer_hwsync_frame_size(audio_mixer) != frame_size) {
        AM_LOGI("resize frame_size %d", frame_size);
        set_mixer_hwsync_frame_size(audio_mixer, frame_size);
    }
    pthread_mutex_lock(&out->mdata_lock);
    list_add_tail(&out->mdata_list, &mdata_list->list);
    pthread_mutex_unlock(&out->mdata_lock);
    return 0;
}

static int consume_output_data(void *cookie, const void* buffer, size_t bytes)
{
    ssize_t written = 0;
    uint64_t latency_frames = 0;
    struct audio_stream_out *stream = (struct audio_stream_out *)cookie;
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    uint16_t *in_buf_16 = (uint16_t *)buffer;
    struct timespec tval, new_tval;
    uint64_t us_since_last_write = 0;
    int64_t throttle_timeus = 0;
    int frame_size = 4;
    void * out_buf = (void*)buffer;
    size_t out_size = bytes;
    int bResample = 0;
    int channels = audio_channel_count_from_out_mask(out->hal_channel_mask);

    AM_LOGV("++bytes = %zu", bytes);
    if (out->pause_status) {
        AM_LOGE("write in pause status");
    }

    clock_gettime(CLOCK_MONOTONIC, &tval);

    apply_volume_fade(out->last_volume_l, out->volume_l, in_buf_16, sizeof(uint16_t), channels, bytes);
    out->last_volume_l = out->volume_l;
    out->last_volume_r = out->volume_r;
    if (out->hw_sync_mode && out->resample_outbuf != NULL) {
        int out_frame = bytes >> 2;
        out_frame = resample_process (&out->aml_resample, out_frame,
                (int16_t *) buffer, (int16_t *) out->resample_outbuf);
        out_size = out_frame << 2;
        out_buf = out->resample_outbuf;
        bResample = 1;
    }
    written = aml_out_write_to_mixer(stream, out_buf, out_size);

    if (written < 0) {
        AM_LOGE("written failed, %zd", written);
        goto exit;
    }

    /*here may be a problem, after resample, the size write to mixer is changed,
      to avoid some problem, we assume it is totally wirtten.
    */
    if (bResample) {
        written = bytes;
    }

    clock_gettime(CLOCK_MONOTONIC, &new_tval);
    us_since_last_write = (new_tval.tv_sec - out->timestamp.tv_sec) * 1000000 +
            (new_tval.tv_nsec - out->timestamp.tv_nsec) / 1000;
    //out->timestamp = new_tval;

    int used_this_write = (new_tval.tv_sec - tval.tv_sec) * 1000000 +
            (new_tval.tv_nsec - tval.tv_nsec) / 1000;
    int target_us = bytes * 1000 / 4 / 48;
    // calculate presentation frames and timestamps
    //clock_gettime(CLOCK_MONOTONIC, &out->timestamp);
    //latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->port_index) +
    //    mixer_get_outport_latency_frames(audio_mixer);
    //latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->port_index);
    //out->frame_write_sum += written;

    //if (out->last_frames_postion > out->frame_write_sum)
    //    out->last_frames_postion = out->frame_write_sum - latency_frames;
    //else
    //    out->last_frames_postion = out->frame_write_sum;
    AM_LOGV("++written = %zd", written);
    if (getprop_bool("vendor.media.audiohal.hwsync")) {
        aml_audio_dump_audio_bitstreams("/data/audio/consumeout.raw", buffer, written);
    }
    if (0) {
        AM_LOGD("last_frames_postion(%" PRId64 ") latency_frames(%" PRId64 ")",
            out->last_frames_postion, latency_frames);
    }
    throttle_timeus = target_us - us_since_last_write;
    if (throttle_timeus > 0 && throttle_timeus < 200000) {
        AM_LOGV("throttle time %" PRId64 " us", throttle_timeus);
        if (throttle_timeus > 1000)
            usleep(throttle_timeus - 1000);
    }

    //throttle simply 4/5 duration
    //usleep(bytes * 1000 / 4 / 48 * 1 / 2);
exit:
    clock_gettime(CLOCK_MONOTONIC, &out->timestamp);
    out->lasttimestamp.tv_sec = out->timestamp.tv_sec;
    out->lasttimestamp.tv_nsec = out->timestamp.tv_nsec;
    if (written >= 0) {
        //TODO
        if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
            latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->inputPortID)
                    + a2dp_out_get_latency(adev) * out->hal_rate / 1000;
        else
            latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->inputPortID)
                    + mixer_get_outport_latency_frames(audio_mixer);
        out->frame_write_sum += written / frame_size;

        if (out->frame_write_sum > latency_frames)
            out->last_frames_postion = out->frame_write_sum - latency_frames;
        else
            out->last_frames_postion = 0;//out->frame_write_sum;
    }
    if (out->debug_stream) {
        AM_LOGD("frames sum %" PRId64 ", last frames %" PRId64 "", out->frame_write_sum, out->last_frames_postion);
    }
    return written;
}

static ssize_t out_write_hwsync_lpcm(struct audio_stream_out *stream, const void* buffer,
                                    size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    size_t channel_count = audio_channel_count_from_out_mask(out->hal_channel_mask);
    size_t frame_size = audio_bytes_per_frame(channel_count, out->hal_format);;
    int written_total = 0;
    struct timespec ts;
    memset(&ts, 0, sizeof(struct timespec));

    // when connect bt, bt stream maybe open before hdmi stream close,
    // bt stream mediasync is set to adev->hw_mediasync, and it would be
    // release in hdmi stream close, so bt stream mediasync is invalid
    if (out->hwsync->mediasync != NULL && adev->hw_mediasync == NULL) {
        adev->hw_mediasync = aml_hwsync_mediasync_create();
        out->hwsync->use_mediasync = true;
        out->hwsync->mediasync = adev->hw_mediasync;
        aml_audio_hwsync_set_id(out->hwsync, out->hwsync->hwsync_id);
        aml_audio_hwsync_init(out->hwsync, out);
    }
    if (out->standby) {
        AM_LOGI("start hwsync lpcm stream: %p", out);
        aml_audio_set_cpu23_affinity();
        out->hwsync_extractor = new_hw_avsync_header_extractor(consume_meta_data,
                consume_output_data, out);
        out->first_pts_set = false;
        out->need_first_sync = false;
        out->last_pts = 0;
        out->last_payload_offset = 0;
        pthread_mutex_init(&out->mdata_lock, NULL);
        list_init(&out->mdata_list);
        pthread_mutex_lock(&adev->lock);
        init_mixer_input_port(sm->mixerData, &out->audioCfg, out->flags,
            on_notify_cbk, out, on_input_avail_cbk, out,
            on_meta_data_cbk, out, out->volume_l);
        pthread_mutex_unlock(&adev->lock);
        AM_LOGI("hwsync port type = %d",
                get_input_port_type(&out->audioCfg, out->flags));
        out->standby = false;
        mixer_set_continuous_output(sm->mixerData, false);
        /*wait video ready*/
        {
            int vframe_ready_cnt = 0;
            int delay_count = 0;
            while (delay_count < 10) {
                vframe_ready_cnt = get_sysfs_int("/sys/class/video/vframe_ready_cnt");
                if (vframe_ready_cnt < 2) {
                    usleep(10000);
                    delay_count++;
                    continue;
                }
                break;
            }
            AM_LOGI("/sys/class/video/vframe_ready_cnt is %d delay count=%d", vframe_ready_cnt, delay_count);
        }
    }
    if (out->pause_status) {
        AM_LOGW("write in pause status!!");
        out->pause_status = false;
    }
    written_total = header_extractor_write(out->hwsync_extractor, buffer, bytes);
    AM_LOGV("bytes %zu, out->last_frames_postion %" PRId64 " frame_sum %" PRId64 "",
            bytes, out->last_frames_postion, out->frame_write_sum);

    if (getprop_bool("vendor.media.audiohal.hwsync")) {
        aml_audio_dump_audio_bitstreams("/data/audio/audiomain.raw", buffer, written_total);
    }

    if (written_total > 0) {
        AM_LOGV("--out(%p)written %d, write_sum after %" PRId64 "",
                out, written_total, out->frame_write_sum);
        if ((size_t)written_total != bytes)
            AM_LOGE("--written %d, but bytes = %zu", written_total, bytes);
        return written_total;
    } else {
        AM_LOGE("--written %d, but return bytes", written_total);
        //return 1;
        return bytes;
    }
    return written_total;
}

static ssize_t out_write_system(struct audio_stream_out *stream, const void *buffer,
                                    size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    //struct aml_audio_mixer *audio_mixer = adev->audio_mixer;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    struct timespec tval, new_tval;
    uint64_t us_since_last_write = 0;
    //uint64_t begain_time, end_time;
    ssize_t written = 0;
    size_t remain = 0;
    size_t channel_count = audio_channel_count_from_out_mask(out->hal_channel_mask);
    size_t frame_size = audio_bytes_per_frame(channel_count, out->hal_format);;
    //uint64_t throttle_timeus = THROTLE_TIME_US;//aml_audio_get_throttle_timeus();
    int64_t throttle_timeus = 0;//aml_audio_get_throttle_timeus(bytes);

    if (out->standby) {
        AM_LOGI("standby to unstandby");
        out->standby = false;
    }

    if (bytes == 0) {
        AM_LOGW("inval to write bytes 0");
        usleep(512 * 1000 / 48 / frame_size);
        written = 0;
        goto exit;
        //return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &tval);
    //begain_time = get_systime_ns();
    written = aml_out_write_to_mixer(stream, buffer, bytes);
    if (written >= 0) {
        remain = bytes - written;
        out->frame_write_sum += written / frame_size;
        if (remain > 0) {
            AM_LOGE("INVALID partial written");
        }
        clock_gettime(CLOCK_MONOTONIC, &new_tval);
        if (tval.tv_sec > new_tval.tv_sec)
            AM_LOGE("FATAL ERROR");
        AM_LOGV("++bytes %zu, out->port_index %d", bytes, out->inputPortID);
        //AM_LOGD(" %lld us, %lld", new_tval.tv_sec, tval.tv_sec);

        us_since_last_write = (new_tval.tv_sec - out->timestamp.tv_sec) * 1000000 +
                (new_tval.tv_nsec - out->timestamp.tv_nsec) / 1000;
        //out->timestamp = new_tval;

        int used_this_write = (new_tval.tv_sec - tval.tv_sec) * 1000000 +
                (new_tval.tv_nsec - tval.tv_nsec) / 1000;
        int target_us = bytes * 1000 / frame_size / 48;

        AM_LOGV("time spent on write %" PRId64 " us, written %zd", us_since_last_write, written);
        AM_LOGV("used_this_write %d us, target %d us", used_this_write, target_us);
        throttle_timeus = target_us - us_since_last_write;
        if (throttle_timeus > 0 && throttle_timeus < 200000) {
            AM_LOGV("throttle time %" PRId64 " us", throttle_timeus);
            if (throttle_timeus > 1800) {
                //usleep(throttle_timeus - 1800);
                AM_LOGV("actual throttle %" PRId64 " us, since last %" PRId64 " us",
                        throttle_timeus, us_since_last_write);
            } else {
                AM_LOGV("%" PRId64 " us, but un-throttle", throttle_timeus);
            }
        } else if (throttle_timeus != 0) {
            // first time write, sleep
            //usleep(target_us - 100);
            AM_LOGV("invalid throttle time %" PRId64 " us, us since last %" PRId64 " us", throttle_timeus, us_since_last_write);
            AM_LOGV("\n\n");
        }
    } else {
        AM_LOGE("write fail, err = %zd", written);
    }

    // TODO: means first write, need check this by method
    if (us_since_last_write > 500000) {
        usleep(bytes * 1000 / 48 / frame_size);
        AM_LOGV("invalid duration %" PRIu64 " us", us_since_last_write);
        //AM_LOGE("last   write %ld s,  %ld ms", out->timestamp.tv_sec, out->timestamp.tv_nsec/1000000);
        //AM_LOGE("before write %ld s,  %ld ms", tval.tv_sec, tval.tv_nsec/1000000);
        //AM_LOGE("after  write %ld s,  %ld ms", new_tval.tv_sec, new_tval.tv_nsec/1000000);
    }

exit:
    // update new timestamp
    clock_gettime(CLOCK_MONOTONIC, &out->timestamp);
    out->lasttimestamp.tv_sec = out->timestamp.tv_sec;
    out->lasttimestamp.tv_nsec = out->timestamp.tv_nsec;
    if (written >= 0) {
        uint32_t latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->inputPortID);
                //+ mixer_get_outport_latency_frames(audio_mixer);
        if (out->frame_write_sum > latency_frames)
            out->last_frames_postion = out->frame_write_sum - latency_frames;
        else
            out->last_frames_postion = out->frame_write_sum;

        if (0) {
            AM_LOGI("last position %" PRId64 ", latency_frames %d", out->last_frames_postion, latency_frames);
        }
    }

    return written;
}

static ssize_t out_write_direct_pcm(struct audio_stream_out *stream, const void *buffer,
                                    size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    struct timespec tval, new_tval;
    uint64_t us_since_last_write = 0;
    //uint64_t begain_time, end_time;
    ssize_t written = 0;
    size_t remain = 0;
    int frame_size = 4;
    int64_t throttle_timeus = 0;//aml_audio_get_throttle_timeus(bytes);

    if (out->standby) {
        init_mixer_input_port(sm->mixerData, &out->audioCfg, out->flags,
            on_notify_cbk, out, on_input_avail_cbk, out,
            NULL, NULL, 1.0);
        AM_LOGI("direct port:%s", mixerInputType2Str(get_input_port_type(&out->audioCfg, out->flags)));
        out->standby = false;
    }

    clock_gettime(CLOCK_MONOTONIC, &tval);
    //begain_time = get_systime_ns();
    written = aml_out_write_to_mixer(stream, buffer, bytes);
    if (written >= 0) {
        remain = bytes - written;
        out->frame_write_sum += written / frame_size;
        if (remain > 0) {
            AM_LOGE("INVALID partial written");
        }
        clock_gettime(CLOCK_MONOTONIC, &new_tval);
        if (tval.tv_sec > new_tval.tv_sec)
            AM_LOGE("FATAL ERROR");
        AM_LOGV("++bytes %zu, out->port_index %d", bytes, out->inputPortID);
        //AM_LOGD(" %lld us, %lld", new_tval.tv_sec, tval.tv_sec);

        us_since_last_write = (new_tval.tv_sec - out->timestamp.tv_sec) * 1000000 +
                (new_tval.tv_nsec - out->timestamp.tv_nsec) / 1000;
        //out->timestamp = new_tval;

        int used_this_write = (new_tval.tv_sec - tval.tv_sec) * 1000000 +
                (new_tval.tv_nsec - tval.tv_nsec) / 1000;
        int target_us = bytes * 1000 / frame_size / 48;

        AM_LOGV("time spent on write %" PRId64 " us, written %zd", us_since_last_write, written);
        AM_LOGV("used_this_write %d us, target %d us", used_this_write, target_us);
        throttle_timeus = target_us - us_since_last_write;
        if (throttle_timeus > 0 && throttle_timeus < 200000) {
            AM_LOGV("throttle time %" PRId64 " us", throttle_timeus);
            if (throttle_timeus > 1800) {
                usleep(throttle_timeus - 1800);
                AM_LOGV("actual throttle %" PRId64 " us, since last %" PRId64 " us",
                        throttle_timeus, us_since_last_write);
            } else {
                AM_LOGV("%" PRId64 " us, but un-throttle", throttle_timeus);
            }
        } else if (throttle_timeus != 0) {
            // first time write, sleep
            //usleep(target_us - 100);
            AM_LOGV("invalid throttle time %" PRId64 " us, us since last %" PRId64 " us", throttle_timeus, us_since_last_write);
            AM_LOGV("\n\n");
        }
    } else {
        AM_LOGE("write fail, err = %zd", written);
    }

    // TODO: means first write, need check this by method
    if (us_since_last_write > 500000) {
        usleep(bytes * 1000 / 48 / frame_size);
        AM_LOGV("invalid duration %" PRIu64 " us", us_since_last_write);
        //AM_LOGE("last   write %ld s,  %ld ms", out->timestamp.tv_sec, out->timestamp.tv_nsec/1000000);
        //AM_LOGE("before write %ld s,  %ld ms", tval.tv_sec, tval.tv_nsec/1000000);
        //AM_LOGE("after  write %ld s,  %ld ms", new_tval.tv_sec, new_tval.tv_nsec/1000000);
    }

exit:
    // update new timestamp
    clock_gettime(CLOCK_MONOTONIC, &out->timestamp);
    out->lasttimestamp.tv_sec = out->timestamp.tv_sec;
    out->lasttimestamp.tv_nsec = out->timestamp.tv_nsec;
    if (written >= 0) {
        uint32_t latency_frames = mixer_get_inport_latency_frames(audio_mixer, out->inputPortID);
                //+ mixer_get_outport_latency_frames(audio_mixer);
        if (out->frame_write_sum > latency_frames)
            out->last_frames_postion = out->frame_write_sum - latency_frames;
        else
            out->last_frames_postion = out->frame_write_sum;

        if (0) {
            AM_LOGI("last position %" PRId64 ", latency_frames %d", out->last_frames_postion, latency_frames);
        }
    }

    return written;
}

static int on_notify_cbk(void *data)
{
    struct aml_stream_out *out = data;
    pthread_cond_broadcast(&out->cond);
    return 0;
}

static int on_input_avail_cbk(void *data)
{
    struct aml_stream_out *out = data;
    pthread_cond_broadcast(&out->cond);
    return 0;
}

static int out_get_presentation_position_port(
        const struct audio_stream_out *stream,
        uint64_t *frames,
        struct timespec *timestamp)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    uint64_t frames_written_hw = out->frame_write_sum;
    int ret = 0;
    int tuning_latency_frame= 0;
    int frame_latency = 0;
    R_CHECK_POINTER_LEGAL(-EINVAL, frames, "");
    R_CHECK_POINTER_LEGAL(-EINVAL, timestamp, "");

    /* add this code for VTS. */
    if (0 == frames_written_hw) {
        *frames = frames_written_hw;
        *timestamp = out->timestamp;
        return ret;
    }

    if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        // Timestamp adjustment for AV sync
        struct timespec adjusted_timestamp;
        int64_t inport_latency_nanos = (out->standby ? 0 :
                mixer_get_inport_latency_frames(audio_mixer, out->inputPortID) * NSEC_PER_SEC / out->hal_rate);
        int64_t bt_latency_nanos = (long long)(a2dp_out_get_latency(adev)) * NSEC_PER_MSEC;
        int64_t latency_nanos = inport_latency_nanos + bt_latency_nanos;

        // libaudioclient code expects HAL position to lag behind server position.
        // If the two are the same, it resets timestamp to the current time.
        // As a temporary work-around, get around this by subtracting one
        // frame from both position and timestamp.
        int64_t frame_diff_for_client = 1;
        int64_t time_diff_for_client = NSEC_PER_SEC / out->hal_rate;
        int64_t adjusted_nanos = (long long)out->timestamp.tv_sec * NSEC_PER_SEC
                + (long long)out->timestamp.tv_nsec + latency_nanos - time_diff_for_client;
        if (adjusted_nanos < 0) {
               adjusted_nanos = 0;
        }
        adjusted_timestamp.tv_sec = adjusted_nanos / NSEC_PER_SEC;
        adjusted_timestamp.tv_nsec = adjusted_nanos % NSEC_PER_SEC;
        AM_LOGV("latency adjustment: %lld (%lld ms), adjusted_nanos: %lld, frame drift: %lld, hal_rate: %u",
            latency_nanos, latency_nanos / NSEC_PER_MSEC, adjusted_nanos, frame_diff_for_client, out->hal_rate);

        *frames = frames_written_hw - frame_diff_for_client;
        *timestamp = adjusted_timestamp;
    } else if (!adev->audio_patching) {
        ret = mixer_get_presentation_position(audio_mixer,
                out->inputPortID, frames, timestamp);
        tuning_latency_frame = aml_audio_get_pcm_latency_offset(adev->sink_format, adev->is_netflix, out->usecase)*48;
        AM_LOGV("usecase:%s tuning_latency_frame:%d", usecase2Str(out->usecase), tuning_latency_frame);
        if (tuning_latency_frame > 0 && *frames < (uint64_t)tuning_latency_frame) {
            *frames = 0;
        } else {
            *frames = *frames - tuning_latency_frame;
        }

        if (ret == 0) {
            out->last_frames_postion = *frames;
        } else {
            *frames = out->last_frames_postion;
            AM_LOGW("pts not valid yet");
        }
    } else {
        *frames = frames_written_hw;
        *timestamp = out->timestamp;
    }

    int latency_ms = 0;
    if (!adev->is_netflix && ret == 0) {
        latency_ms = aml_audio_get_latency_offset(adev->active_outport,
                                                         out->hal_internal_format,
                                                         adev->sink_format,
                                                         adev->ms12.dolby_ms12_enable);
        frame_latency = latency_ms * (out->hal_rate / 1000);
        *frames += frame_latency ;
    }
    if (adev->debug_flag) {
         AM_LOGI("tunned_latency_ms %d ",latency_ms);
    }

    if (adev->debug_flag) {
        AM_LOGI("out %p %"PRIu64", sec = %ld, nanosec = %ld\n", out, *frames, timestamp->tv_sec, timestamp->tv_nsec);
        int64_t  frame_diff_ms =  (*frames - out->last_frame_reported) * 1000 / out->hal_rate;
        int64_t  system_time_ms = 0;
        if (timestamp->tv_nsec < out->last_timestamp_reported.tv_nsec) {
            system_time_ms = (timestamp->tv_nsec + 1000000000 - out->last_timestamp_reported.tv_nsec)/1000000;
        }
        else
            system_time_ms = (timestamp->tv_nsec - out->last_timestamp_reported.tv_nsec)/1000000;
        int64_t jitter_diff = llabs(frame_diff_ms - system_time_ms);
        if  (jitter_diff > JITTER_DURATION_MS) {
            AM_LOGI("jitter out last pos info: %p %"PRIu64", sec = %ld, nanosec = %ld\n", out, out->last_frame_reported,
                out->last_timestamp_reported.tv_sec, out->last_timestamp_reported.tv_nsec);
            AM_LOGI("jitter  system time diff %"PRIu64" ms, position diff %"PRIu64" ms, jitter %"PRIu64" ms \n",
                system_time_ms,frame_diff_ms,jitter_diff);
        }
        out->last_frame_reported = *frames;
        out->last_timestamp_reported = *timestamp;
    }

    return ret;
}

static int initSubMixingInputPcm(
        struct audio_config *config,
        struct aml_stream_out *out)
{
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    bool hwsync_lpcm = false;
    int flags = out->flags;
    int channel_count = popcount(config->channel_mask);

    hwsync_lpcm = (flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && config->sample_rate <= 48000 &&
               audio_is_linear_pcm(config->format) && channel_count <= 2);
    AM_LOGI("++out %p, flags %#x, hwsync lpcm %d", out, flags, hwsync_lpcm);
    out->audioCfg = *config;
    out->stream.write = out_write_subMixingPCM;
    out->stream.pause = out_pause_subMixingPCM;
    out->stream.resume = out_resume_subMixingPCM;
    out->stream.flush = out_flush_subMixingPCM;
    out->stream.common.standby = out_standby_subMixingPCM;
    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        AM_LOGI("primary presentation");
        out->stream.get_presentation_position = out_get_presentation_position_port;
    }
    list_init(&out->mdata_list);
    if (hwsync_lpcm) {
        AM_LOGI("lpcm case");
        mixer_set_continuous_output(sm->mixerData, true);
    }
    return 0;
}

static int deleteSubMixingInputPcm(struct aml_stream_out *out)
{
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    struct audio_config *config = &out->audioCfg;
    bool hwsync_lpcm = false;
    int flags = out->flags;
    int channel_count = popcount(config->channel_mask);

    hwsync_lpcm = (flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && config->sample_rate <= 48000 &&
               audio_is_linear_pcm(config->format) && channel_count <= 2);

    AM_LOGI("cnt_stream_using_mixer %d", sm->cnt_stream_using_mixer);
    struct meta_data_list *mdata_list;
    struct listnode *item;

    if (out->hw_sync_mode) {
        pthread_mutex_lock(&out->mdata_lock);
        while (!list_empty(&out->mdata_list)) {
            item = list_head(&out->mdata_list);
            mdata_list = node_to_item(item, struct meta_data_list, list);
            list_remove(item);
            //AM_LOGI("free medata list=%p", mdata_list);
            aml_audio_free(mdata_list);
        }
        pthread_mutex_unlock(&out->mdata_lock);
    }

    if (hwsync_lpcm) {
        AM_LOGI("lpcm case");
        mixer_set_continuous_output(sm->mixerData, false);
    }
    return 0;
}

int initSubMixingInput(struct aml_stream_out *out,
        struct audio_config *config)
{
    R_CHECK_POINTER_LEGAL(-EINVAL, out, "");
    R_CHECK_POINTER_LEGAL(-EINVAL, config, "");
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    int ret = 0;

    if (sm->type == MIXER_LPCM) {
        ret = initSubMixingInputPcm(config, out);
    } else if (sm->type == MIXER_MS12) {
        //ret = initSubMixingInputMS12(sm, config, out);
        AM_LOGE("MS12 not supported yet");
        ret = -1;
    }

    return ret;
};

int deleteSubMixingInput(struct aml_stream_out *out)
{
    R_CHECK_POINTER_LEGAL(-EINVAL, out, "");
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    int ret = 0;

    if (sm->type == MIXER_LPCM) {
        ret = deleteSubMixingInputPcm(out);
    } else if (sm->type == MIXER_MS12) {
        //ret = deleteSubMixingInputMS12(out);
        AM_LOGE("MS12 not supported yet");
        ret = -1;
    }

    return ret;
}

#if 0
int subMixerWriteInport(
        struct subMixing *sm,
        void *buf,
        size_t bytes,
        aml_mixer_input_port_type_e port_index)
{
    int ret = 0;
    if (sm->type == MIXER_LPCM) {
        ret = mixer_write_inport(sm->mixerData, port_index, buf, bytes);
    } else {
        ret = -EINVAL;
        AM_LOGE("not support");
    }

    return 0;
};

int mainWriteMS12(
            struct subMixing *sm,
            void *buf,
            size_t bytes)
{
    (void *)buf;
    (void *)bytes;
    struct dolby_ms12_desc *ms12 = (struct dolby_ms12_desc *)sm->mixerData;
    return 0;
}

int sysWriteMS12(
            struct subMixing *sm,
            void *buf,
            size_t bytes)
{
    (void *)buf;
    (void *)bytes;
    struct dolby_ms12_desc *ms12 = (struct dolby_ms12_desc *)sm->mixerData;
    return 0;
}
#endif
/* mixing only support pcm 16&32 format now */
static int newSubMixingFactory(
            struct subMixing **smixer,
            enum MIXER_TYPE type,
            void *data)
{
    (void *)data;
    struct subMixing *sm = NULL;
    int res = 0;

    AM_LOGI("type %d", type);
    sm = aml_audio_calloc(1, sizeof(struct subMixing));
    R_CHECK_POINTER_LEGAL(-ENOMEM, sm, "No mem!");

    switch (type) {
    case MIXER_LPCM:
        sm->type = MIXER_LPCM;
        strncpy(sm->name, "LPCM", 16);
        //sm->writeMain = mainWritePCM;
        //sm->writeSys = sysWritePCM;
        //sm->mixerData = data;
        break;
    case MIXER_MS12:
        sm->type = MIXER_MS12;
        strncpy(sm->name, "MS12", 16);
        //sm->writeMain = mainWriteMS12;
        //sm->writeSys = sysWriteMS12;
        //sm->mixerData = data;
        break;
    default:
        AM_LOGE("type %d not support!", type);
        break;
    };

    *smixer = sm;
exit:
    return res;
};

static void deleteSubMixing(struct subMixing *sm)
{
    AM_LOGI("++");
    if (sm != NULL) {
        aml_audio_free(sm);
    }
}

int initHalSubMixing(struct subMixing **smixer,
        enum MIXER_TYPE type,
        struct aml_audio_device *adev,
        bool isTV)
{
    int ret = 0;

    ALOGI("type %d, isTV %d", type, isTV);
    R_CHECK_POINTER_LEGAL(-EINVAL, smixer, "");
    ret = newSubMixingFactory(smixer, type, NULL);
    R_CHECK_RET(ret, "fail to new mixer");
    ret = initSubMixngOutput(*smixer, adev);
    if (ret < 0) {
        AM_LOGE("fail to init mixer");
        goto err1;
    }
    return 0;
err1:
    deleteSubMixing(*smixer);
    return ret;
}

int deleteHalSubMixing(struct subMixing *smixer)
{
    releaseSubMixingOutput(smixer);
    deleteSubMixing(smixer);
    return 0;
}
#if 0
int mainWritePCM(
        struct subMixing *mixer,
        void *buf,
        size_t bytes)
{
    (void *)buf;
    (void *)bytes;
    struct amlAudioMixer *am = (struct amlAudioMixer *)mixer->mixerData;
    return 0;
}

int sysWritePCM(
        struct subMixing *mixer,
        void *buf,
        size_t bytes)
{
    (void *)buf;
    (void *)bytes;
    struct amlAudioMixer *am = (struct amlAudioMixer *)mixer->mixerData;
    return 0;
}

int subWrite(
        struct subMixing *mixer,
        void *buf,
        size_t bytes)
{
    (void *)buf;
    (void *)bytes;
    struct amlAudioMixer *am = (struct amlAudioMixer *)mixer->mixerData;
    return 0;
}
#endif

int outSubMixingWrite(
            struct audio_stream_out *stream,
            const void *buf,
            size_t bytes)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    char *buffer = (char *)buf;

    sm->write(sm, buffer, bytes);
    return 0;
}
ssize_t mixer_main_buffer_write_sm (struct audio_stream_out *stream, const void *buffer,
                                 size_t bytes)
{
    struct aml_stream_out       *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device     *adev = aml_out->dev;
    ssize_t                     write_bytes = 0;

    if (buffer == NULL || bytes == 0) {
        AM_LOGW("stream:%p, buffer is null, or bytes:%zu invalid", stream, bytes);
        return -1;
    }

    if (adev->debug_flag) {
        AM_LOGD("stream:%p, out_device:%#x, bytes:%zu, format:%#x, hw_sync_mode:%d",
            stream, aml_out->out_device, bytes, aml_out->hal_internal_format, aml_out->hw_sync_mode);
    }

    if (popcount(adev->usecase_masks & SUBMIX_USECASE_MASK) > 1) {
        AM_LOGE("usemask:%#x, not support two direct stream", adev->usecase_masks);
        return bytes;
    }

    /* handle HWSYNC audio data*/
    if (aml_out->hw_sync_mode) {
        write_bytes = out_write_hwsync_lpcm(stream, buffer, bytes);
    } else {
        write_bytes = out_write_direct_pcm(stream, buffer, bytes);
    }

    if (write_bytes > 0) {
        aml_out->input_bytes_size += write_bytes;
    }

    return bytes;
}

ssize_t mixer_aux_buffer_write_sm(struct audio_stream_out *stream, const void *buffer,
                               size_t bytes)
{
    struct aml_stream_out       *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device     *adev = aml_out->dev;
    struct subMixing            *sm = adev->sm;
    size_t                      in_frames = bytes / audio_stream_out_frame_size(stream);
    ssize_t                     bytes_written = 0;
#ifdef DEBUG_TIME
    uint64_t                    us_since_last_write = 0;
    struct timespec             tval_begin, tval_end;
    int64_t                     throttle_timeus = 0;
    clock_gettime(CLOCK_MONOTONIC, &tval_begin);
#endif

    if (buffer == NULL || bytes == 0) {
        AM_LOGW("stream:%p, buffer is null, or bytes:%zu invalid", stream, bytes);
        return -1;
    }

    if (adev->debug_flag) {
        AM_LOGD("stream:%p, out_device:%#x, bytes:%zu",
            stream, aml_out->out_device, bytes);
    }

    if (adev->out_device != aml_out->out_device) {
        AM_LOGD("stream:%p, switch from device:%#x to device:%#x",
             stream, adev->out_device, aml_out->out_device);
        aml_out->out_device = adev->out_device;
        aml_out->stream.common.standby(&aml_out->stream.common);
        goto exit;
    } else if (aml_out->out_device == 0) {
        AM_LOGW("output device is none");
        goto exit;
    }

    /* this process will lead audio late about 30ms delay. */
    if (aml_out->standby) {
        char *padding_buf = NULL;
        int padding_bytes = MIXER_FRAME_COUNT * 4 * MIXER_OUT_FRAME_SIZE;
        if (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
            padding_bytes = 0;

        aml_out->audio_data_handle_state = AUDIO_DATA_HANDLE_START;
        //set_thread_affinity();
        init_mixer_input_port(sm->mixerData, &aml_out->audioCfg, aml_out->flags,
            on_notify_cbk, aml_out, on_input_avail_cbk, aml_out,
            NULL, NULL, 1.0);

        AM_LOGI("stream %p input port:%s", stream,
            mixerInputType2Str(get_input_port_type(&aml_out->audioCfg, aml_out->flags)));
        aml_out->standby = false;
#ifdef ENABLE_AEC_APP
        aec_set_spk_running(adev->aec, true);
#endif
        /* start padding zero to fill padding data to alsa buffer*/
        padding_buf = aml_audio_calloc(1, MIXER_FRAME_COUNT * 4);
        R_CHECK_POINTER_LEGAL(-ENOMEM, padding_buf, "no memory");
        mixer_set_padding_size(sm->mixerData, aml_out->inputPortID, padding_bytes);
        while (padding_bytes > 0) {
            AM_LOGI("padding_bytes %d", padding_bytes);
            aml_out_write_to_mixer(stream, padding_buf, MIXER_FRAME_COUNT * 4);
            padding_bytes -= MIXER_FRAME_COUNT * 4;
        }
        aml_audio_free(padding_buf);
    }

    bytes_written = aml_out_write_to_mixer(stream, buffer, bytes);

#ifdef DEBUG_TIME
    clock_gettime(CLOCK_MONOTONIC, &tval_end);
    us_since_last_write = (tval_end.tv_sec - aml_out->timestamp.tv_sec) * 1000000 +
            (tval_end.tv_nsec - aml_out->timestamp.tv_nsec) / 1000;
    int used_this_write = (tval_end.tv_sec - tval_begin.tv_sec) * 1000000 +
            (tval_end.tv_nsec - tval_begin.tv_nsec) / 1000;
    int target_us = in_frames * 1000 / 48;

    AM_LOGV("time spent on write %" PRId64 " us, written %d", us_since_last_write, bytes_written);
    AM_LOGV("used_this_write %d us, target %d us", used_this_write, target_us);
    throttle_timeus = target_us - us_since_last_write;

    if (throttle_timeus > 0 && throttle_timeus < 200000) {
        AM_LOGV("throttle time %" PRId64 " us", throttle_timeus);
        if (throttle_timeus > 1800 && aml_out->us_used_last_write < (uint64_t)target_us/2) {
            usleep(throttle_timeus - 1800);
            AM_LOGV("actual throttle %" PRId64 " us3, since last %" PRId64 " us",
                    throttle_timeus, us_since_last_write);
        } else {
            AM_LOGV("%" PRId64 " us, but un-throttle", throttle_timeus);
        }
    } else if (throttle_timeus != 0) {
        AM_LOGV("invalid throttle time %" PRId64 " us, us since last %" PRId64 " us \n\n", throttle_timeus, us_since_last_write);
    }
    aml_out->us_used_last_write = us_since_last_write;
#endif
exit:
    aml_out->frame_write_sum += in_frames;
    aml_out->last_frames_postion = aml_out->frame_write_sum;
    clock_gettime(CLOCK_MONOTONIC, &aml_out->timestamp);
    aml_out->lasttimestamp.tv_sec = aml_out->timestamp.tv_sec;
    aml_out->lasttimestamp.tv_nsec = aml_out->timestamp.tv_nsec;
    AM_LOGV("frame write sum %" PRId64 "", aml_out->frame_write_sum);
    return bytes;
}

ssize_t mixer_mmap_buffer_write_sm(struct audio_stream_out *stream, const void *buffer, size_t bytes)
{
   struct aml_stream_out    *aml_out = (struct aml_stream_out *) stream;
   struct aml_audio_device  *adev = aml_out->dev;
   struct subMixing         *pstSubMixing = adev->sm;
   ssize_t                  bytes_written = 0;

   if (adev->debug_flag) {
       AM_LOGD("stream:%p, out_device:%#x, bytes:%zu",
           stream, aml_out->out_device, bytes);
   }

   if (adev->out_device != aml_out->out_device) {
       AM_LOGD("stream:%p, switch from device:%#x to device:%#x",
            stream, adev->out_device, aml_out->out_device);
       aml_out->out_device = adev->out_device;
       aml_out->stream.common.standby(&aml_out->stream.common);
       return bytes;
   } else if (aml_out->out_device == 0) {
       AM_LOGW("output device is none");
       return bytes;
   }

   if (aml_out->standby) {
       init_mixer_input_port(pstSubMixing->mixerData, &aml_out->audioCfg, aml_out->flags,
           on_notify_cbk, aml_out, on_input_avail_cbk, aml_out, NULL, NULL, 1.0);
       AM_LOGI("stream:%p, port_index:%s",
            aml_out, mixerInputType2Str(get_input_port_type(&aml_out->audioCfg, aml_out->flags)));
       aml_out->standby = false;
   }

   bytes_written = aml_out_write_to_mixer(stream, buffer, bytes);
   if (bytes_written != bytes) {
       AM_LOGW("write to mixer error, written:%zd, bytes:%zu", bytes_written, bytes);
   }

exit:
   return bytes_written;
}

/* must be called with hw device mutexes locked */
static int usecase_change_validate_l_sm(struct aml_stream_out *aml_out, bool is_standby)
{
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct subMixing *sm = aml_dev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    bool hw_mix;

    if (is_standby) {
        AM_LOGI("cur dev masks:%#x, delete out usecase:%s", aml_dev->usecase_masks, usecase2Str(aml_out->usecase));
        /**
         * If called by standby, reset out stream's usecase masks and clear the aml_dev usecase masks.
         * So other active streams could know that usecase have been changed.
         * But keep it's own usecase if out_write is called in the future to exit standby mode.
         */
        aml_out->dev_usecase_masks = 0;
        aml_out->write = NULL;
        aml_dev->usecase_cnt[aml_out->usecase]--;
        if (aml_dev->usecase_cnt[aml_out->usecase] <= 0) {
            AM_LOGI("standby unmask usecase %s", usecase2Str(aml_out->usecase));
            aml_dev->usecase_masks &= ~(1 << aml_out->usecase);
        }
        return 0;
    }

    /* No usecase changes, do nothing */
    if (((aml_dev->usecase_masks == aml_out->dev_usecase_masks) && aml_dev->usecase_masks) && (aml_dev->continuous_audio_mode == 0)) {
        if ((STREAM_PCM_NORMAL == aml_out->usecase) && (aml_out->write_func == PROCESS_BUFFER_WRITE)) {
            AM_LOGE("wrong write function reset it");
        } else {
            return 0;
        }
    }

    AM_LOGV("++dev masks:%#x, out masks:%#x, out usecase:%s",
           aml_dev->usecase_masks, aml_out->dev_usecase_masks, usecase2Str(aml_out->usecase));

    /* check the usecase validation */
    if (popcount(aml_dev->usecase_masks & SUBMIX_USECASE_MASK) > 1) {
        AM_LOGW("invalid dev masks:%#x, out usecase %s!",
              aml_dev->usecase_masks, usecase2Str(aml_out->usecase));
        //return -EINVAL;
    }

    if (((aml_dev->continuous_audio_mode == 1) && (aml_dev->debug_flag > 1)) || \
        (aml_dev->continuous_audio_mode == 0)) {
        AM_LOGI("++++continuous:%d dev masks:%#x, out masks:%#x, out usecase %s",
            aml_dev->continuous_audio_mode, aml_dev->usecase_masks,
            aml_out->dev_usecase_masks, usecase2Str(aml_out->usecase));
    }

    /* new output case entered, so no masks has been set to the out stream */
    if (!aml_out->dev_usecase_masks) {
        aml_dev->usecase_cnt[aml_out->usecase]++;
        AM_LOGI("add usecase %s, cnt %d", usecase2Str(aml_out->usecase),
                aml_dev->usecase_cnt[aml_out->usecase]);
        if ((1 << aml_out->usecase) & aml_dev->usecase_masks) {
            AM_LOGW("usecase: %s already exists!!", usecase2Str(aml_out->usecase) );
            //return -EINVAL;
        }

        if (popcount((aml_dev->usecase_masks | (1 << aml_out->usecase)) & SUBMIX_USECASE_MASK) > 1) {
            AM_LOGE("usecase masks:%#x, couldn't add new out usecase %s!",
                  aml_dev->usecase_masks, usecase2Str(aml_out->usecase));
            return -EINVAL;
        }
        if (aml_dev->usecase_cnt[aml_out->usecase] == 1) {
            AM_LOGD("cur dev masks:%#x, add out usecase:%s", aml_dev->usecase_masks, usecase2Str(aml_out->usecase));
            /* add the new output usecase to aml_dev usecase masks */
            aml_dev->usecase_masks |= 1 << aml_out->usecase;
        }
    }

    if (STREAM_PCM_NORMAL == aml_out->usecase) {
        if (aml_dev->audio_patching) {
            AM_LOGV("tv patching, mixer_aux_buffer_write!");
            aml_out->write = mixer_aux_buffer_write;
            aml_out->write_func = MIXER_AUX_BUFFER_WRITE;
        } else {
            aml_out->write = mixer_aux_buffer_write_sm;
            aml_out->write_func = MIXER_AUX_BUFFER_WRITE_SM;
            AM_LOGV("mixer_aux_buffer_write_sm !");
        }
    } else if (STREAM_PCM_MMAP == aml_out->usecase) {
        aml_out->write = mixer_mmap_buffer_write_sm;
        aml_out->write_func = MIXER_MMAP_BUFFER_WRITE_SM;
        AM_LOGV("mixer_mmap_buffer_write_sm !");
    } else {
        aml_out->write = mixer_main_buffer_write_sm;
        aml_out->write_func = MIXER_MAIN_BUFFER_WRITE_SM;
        AM_LOGV("mixer_main_buffer_write_sm !");
    }

    /* store the new usecase masks in the out stream */
    aml_out->dev_usecase_masks = aml_dev->usecase_masks;
    if (((aml_dev->continuous_audio_mode == 1) && (aml_dev->debug_flag > 1)) || \
        (aml_dev->continuous_audio_mode == 0))
        AM_LOGI("----continuous:%d dev masks:%#x, out masks:%#x, out usecase %s",
            aml_dev->continuous_audio_mode, aml_dev->usecase_masks, aml_out->dev_usecase_masks, usecase2Str(aml_out->usecase));
    return 0;
}

/* out_write_submixing entrance: every write to submixing goes in here. */
static ssize_t out_write_subMixingPCM(struct audio_stream_out *stream,
                      const void *buffer,
                      size_t bytes)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    ssize_t ret = 0;
    //write_func  write_func_p = NULL;

    AM_LOGV("out_stream(%p) position(%zu)", stream, bytes);
    aml_audio_trace_int("out_write_subMixingPCM", bytes);

    if (aml_audio_trace_debug_level() > 0) {
        if (false == aml_out->pause_status  &&  aml_out->write_count < 2) {
            aml_out->write_time = aml_audio_get_systime() / 1000; //us --> ms
            AM_LOGD("out_stream(%p) bytes(%zu), write_time:%llu, count:%d",
                       stream, bytes, aml_out->write_time, aml_out->write_count);
        }
        aml_out->write_count++;
    }

    /**
     * deal with the device output changes
     * pthread_mutex_lock(&aml_out->lock);
     * out_device_change_validate_l(aml_out);
     * pthread_mutex_unlock(&aml_out->lock);
     */
    pthread_mutex_lock(&adev->lock);
    ret = usecase_change_validate_l_sm(aml_out, false);
    if (ret < 0) {
        AM_LOGE("failed");
        pthread_mutex_unlock(&adev->lock);
        aml_audio_trace_int("out_write_subMixingPCM", 0);
        return ret;
    }
    adev->active_outputs[aml_out->usecase] = aml_out;
    pthread_mutex_unlock(&adev->lock);
    if (aml_out->write) {
        ret = aml_out->write(stream, buffer, bytes);
    } else {
        AM_LOGE("NULL write function");
    }
    if (ret > 0) {
        aml_out->total_write_size += ret;
    }
    if (adev->debug_flag > 1) {
        AM_LOGI("-ret %zd,%p %"PRIu64"\n", ret, stream, aml_out->total_write_size);
    }
    aml_audio_trace_int("out_write_subMixingPCM", 0);
    return ret;
}

int out_standby_subMixingPCM(struct audio_stream *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    ssize_t ret = 0;

    AM_LOGD("out_stream(%p) usecase: %s", stream, usecase2Str(aml_out->usecase));
    /**
     * deal with the device output changes
     * pthread_mutex_lock(&aml_out->lock);
     * out_device_change_validate_l(aml_out);
     * pthread_mutex_unlock(&aml_out->lock);
     */

    aml_audio_trace_int("out_standby_subMixingPCM", 1);
    pthread_mutex_lock(&adev->lock);
    if (aml_out->standby) {
        goto exit;
    }

    ret = usecase_change_validate_l_sm(aml_out, true);
    if (ret < 0) {
        AM_LOGE("failed");
        goto exit;
    }

    aml_out->status = STREAM_STANDBY;
    aml_out->standby = true;
#ifdef ENABLE_AEC_APP
    aec_set_spk_running(adev->aec, false);
#endif
    if (aml_out->inputPortID != -1) {
        delete_mixer_input_port(audio_mixer, aml_out->inputPortID);
        aml_out->inputPortID = -1;
    }

    if ((aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) && adev->a2dp_hal
        && get_mixer_inport_count(audio_mixer) == 0) {
        a2dp_out_standby(adev);
    }

    if (aml_out->hwsync_extractor) {
        delete_hw_avsync_header_extractor(aml_out->hwsync_extractor);
        aml_out->hwsync_extractor = NULL;
    }

    if (adev->debug_flag > 1) {
        AM_LOGI("-ret %zd,%p %"PRIu64"\n", ret, stream, aml_out->total_write_size);
    }
exit:
    pthread_mutex_unlock(&adev->lock);
    aml_audio_trace_int("out_standby_subMixingPCM", 0);
    return ret;
}

static int out_pause_subMixingPCM(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct subMixing *sm = aml_dev->sm;
    struct amlAudioMixer *audio_mixer = NULL;

    AM_LOGI("+stream %p, standby %d, pause status %d, usecase: %s",
            aml_out, aml_out->standby, aml_out->pause_status, usecase2Str(aml_out->usecase));

    aml_audio_trace_int("out_pause_subMixingPCM", 1);
    if (aml_audio_trace_debug_level() > 0)
    {
        aml_out->write_count = 0;
        aml_out->pause_time = aml_audio_get_systime() / 1000; //us --> ms
        if (aml_out->pause_time > aml_out->write_time && (aml_out->pause_time - aml_out->write_time < 5*1000)) { //continually write time less than 5s, audio gap
            AM_LOGD("out_stream(%p) AudioGap pause_time:%llu,  diff_time(pause - write):%llu ms",
                   stream, aml_out->pause_time, aml_out->pause_time - aml_out->write_time);
        } else {
            AM_LOGD("-------- pause ----------");
        }
    }

    if (aml_out->standby || aml_out->pause_status) {
        AM_LOGW("stream already paused");
        aml_audio_trace_int("out_pause_subMixingPCM", 0);
        return INVALID_STATE;
    }

    if (sm->type != MIXER_LPCM) {
        AM_LOGW("sub mixing type not pcm, type is %d", sm->type);
        aml_audio_trace_int("out_pause_subMixingPCM", 0);
        return 0;
    }

    audio_mixer = sm->mixerData;
    send_mixer_inport_message(audio_mixer, aml_out->inputPortID, MSG_PAUSE);

    aml_out->pause_status = true;
    if (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
        a2dp_out_standby(aml_dev);
    AM_LOGI("-");
    aml_audio_trace_int("out_pause_subMixingPCM", 0);
    return 0;
}

static int out_resume_subMixingPCM(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct subMixing *sm = aml_dev->sm;
    struct amlAudioMixer *audio_mixer = NULL;
    int ret = 0;

    AM_LOGI("+stream %p, standby %d, pause status %d, usecase: %s",
            aml_out, aml_out->standby,  aml_out->pause_status, usecase2Str(aml_out->usecase));
    aml_audio_trace_int("out_resume_subMixingPCM", 1);
    if (!aml_out->pause_status) {
        AM_LOGW("steam not in pause status");
        aml_audio_trace_int("out_resume_subMixingPCM", 0);
        return INVALID_STATE;
    }

    if (sm->type != MIXER_LPCM) {
        AM_LOGW("sub mixing type not pcm, type is %d", sm->type);
        aml_audio_trace_int("out_resume_subMixingPCM", 0);
        return 0;
    }

    audio_mixer = sm->mixerData;
    send_mixer_inport_message(audio_mixer, aml_out->inputPortID, MSG_RESUME);

    aml_out->pause_status = false;
    aml_out->need_first_sync = true;
    AM_LOGI("-");
    aml_audio_trace_int("out_resume_subMixingPCM", 0);
    return 0;
}

/* If supported, a stream should always succeed to flush */
static int out_flush_subMixingPCM(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct subMixing *sm = aml_dev->sm;
    struct amlAudioMixer *audio_mixer = NULL;
    int ret = 0;

    AM_LOGI("+stream %p, standby %d, pause status %d, usecase: %s",
            aml_out,  aml_out->standby, aml_out->pause_status, usecase2Str(aml_out->usecase));

    aml_audio_trace_int("out_flush_subMixingPCM", 1);
    if (sm->type != MIXER_LPCM) {
        AM_LOGW("sub mixing type not pcm, type is %d", sm->type);
        aml_audio_trace_int("out_flush_subMixingPCM", 0);
        return 0;
    }
    aml_out->frame_write_sum  = 0;
    aml_out->last_frames_postion = 0;
    aml_out->spdif_enc_init_frame_write_sum =  0;
    aml_out->frame_skip_sum = 0;
    aml_out->skip_frame = 0;
    aml_out->input_bytes_size = 0;
    //aml_out->pause_status = false;
    if (aml_out->pause_status) {
        struct meta_data_list *mdata_list;
        struct listnode *item;

        //mixer_flush_inport(audio_mixer, out->port_index);
        if (aml_out->hw_sync_mode) {
            pthread_mutex_lock(&aml_out->mdata_lock);
            while (!list_empty(&aml_out->mdata_list)) {
                item = list_head(&aml_out->mdata_list);
                mdata_list = node_to_item(item, struct meta_data_list, list);
                list_remove(item);
                aml_audio_free(mdata_list);
            }
            pthread_mutex_unlock(&aml_out->mdata_lock);
        }
        audio_mixer = sm->mixerData;
        send_mixer_inport_message(audio_mixer, aml_out->inputPortID, MSG_FLUSH);
        if (!aml_out->standby)
            flush_hw_avsync_header_extractor(aml_out->hwsync_extractor);
        //mixer_set_inport_state(audio_mixer, out->port_index, FLUSHING);
        aml_out->last_frames_postion = 0;
        aml_out->first_pts_set = false;
        aml_out->need_first_sync = false;
        aml_out->last_pts = 0;
        aml_out->last_payload_offset = 0;
        //aml_out->pause_status = false;
        //aml_out->standby = true;
    } else {
        AM_LOGW("Need check this case!");
        aml_audio_trace_int("out_flush_subMixingPCM", 0);
        return 0;
    }

    AM_LOGI("-");
    aml_audio_trace_int("out_flush_subMixingPCM", 0);
    return 0;
}

int subMixingOutputRestart(struct aml_audio_device *adev)
{
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;

    return mixer_outport_pcm_restart(audio_mixer);
}

int switchNormalStream(struct aml_stream_out *aml_out, bool on)
{
    AM_LOGI("+stream %p, on = %d", aml_out, on);
    R_CHECK_POINTER_LEGAL(-EINVAL, aml_out, "");
    if (!aml_out->is_normal_pcm) {
        AM_LOGE("not normal pcm stream");
        return -EINVAL;
    }
    if (on) {
        initSubMixingInputPcm(&aml_out->out_cfg, aml_out);
        aml_out->stream.write = out_write_subMixingPCM;
        aml_out->stream.common.standby = out_standby_subMixingPCM;
        out_standby_subMixingPCM((struct audio_stream *)aml_out);
    } else {
        aml_out->stream.write = out_write_new;
        aml_out->stream.common.standby = out_standby_new;
        deleteSubMixingInputPcm(aml_out);
        out_standby_new((struct audio_stream *)aml_out);
    }

    return 0;
}

void subMixingDump(int s32Fd, const struct aml_audio_device *pstAmlDev)
{
    if (NULL == pstAmlDev) {
        dprintf(s32Fd, "[AML_HAL] %s:%d device is NULL !\n", __func__, __LINE__);
        return;
    }
    dprintf(s32Fd, "[AML_HAL]\n");
    mixer_dump(s32Fd, pstAmlDev);
}

int subMixingSetKaraoke(struct aml_audio_device *adev, struct kara_manager *kara)
{
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;

    return mixer_set_karaoke(audio_mixer, kara);
}

