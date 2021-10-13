/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_TAG "usb_audio_hw_primary"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/list.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/audio.h>
#include <hardware/audio_alsaops.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include <tinyalsa/asoundlib.h>

#include <audio_utils/channels.h>

#include "audio_usb_hal.h"
//#include "audio_hw.h"
#include "sub_mixing_factory.h"

static void stream_lock_init(struct stream_lock *lock) {
    pthread_mutex_init(&lock->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&lock->pre_lock, (const pthread_mutexattr_t *) NULL);
}

static void stream_lock(struct stream_lock *lock) {
    pthread_mutex_lock(&lock->pre_lock);
    pthread_mutex_lock(&lock->lock);
    pthread_mutex_unlock(&lock->pre_lock);
}

static void stream_unlock(struct stream_lock *lock) {
    pthread_mutex_unlock(&lock->lock);
}

static void device_lock(struct usb_audio_device *adev) {
    pthread_mutex_lock(&adev->lock);
}

static int device_try_lock(struct usb_audio_device *adev) {
    return pthread_mutex_trylock(&adev->lock);
}

static void device_unlock(struct usb_audio_device *adev) {
    pthread_mutex_unlock(&adev->lock);
}

/*
 * streams list management
 */
static void adev_add_stream_to_list(
    struct usb_audio_device* adev, struct listnode* list, struct listnode* stream_node) {
    device_lock(adev);

    list_add_tail(list, stream_node);

    device_unlock(adev);
}

static void adev_remove_stream_from_list(
    struct usb_audio_device* adev, struct listnode* stream_node) {
    device_lock(adev);

    list_remove(stream_node);

    device_unlock(adev);
}

/*
 * Extract the card and device numbers from the supplied key/value pairs.
 *   kvpairs    A null-terminated string containing the key/value pairs or card and device.
 *              i.e. "card=1;device=42"
 *   card   A pointer to a variable to receive the parsed-out card number.
 *   device A pointer to a variable to receive the parsed-out device number.
 * NOTE: The variables pointed to by card and device return -1 (undefined) if the
 *  associated key/value pair is not found in the provided string.
 *  Return true if the kvpairs string contain a card/device spec, false otherwise.
 */
static bool parse_card_device_params(const char *kvpairs, int *card, int *device)
{
    struct str_parms * parms = str_parms_create_str(kvpairs);
    char value[32];
    int param_val;

    // initialize to "undefined" state.
    *card = -1;
    *device = -1;

    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        *card = atoi(value);
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        *device = atoi(value);
    }

    str_parms_destroy(parms);

    return *card >= 0 && *device >= 0;
}

static char *device_get_parameters(const alsa_device_profile *profile, const char * keys)
{
    if (profile->card < 0 || profile->device < 0) {
        return strdup("");
    }

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    /* These keys are from hardware/libhardware/include/audio.h */
    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char* rates_list = profile_get_sample_rate_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_list);
        free(rates_list);
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char* channels_list = profile_get_channel_count_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_list);
        free(channels_list);
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char * format_params = profile_get_format_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          format_params);
        free(format_params);
    }
    str_parms_destroy(query);

    char* result_str = str_parms_to_str(result);
    str_parms_destroy(result);

    ALOGV("device_get_parameters = %s", result_str);

    return result_str;
}

/*
 * HAl Functions
 */
/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/*
 * IN functions
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    uint32_t rate = proxy_get_sample_rate(&((const struct stream_in *)stream)->proxy);
    ALOGV("in_get_sample_rate() = %d", rate);
    return rate;
}

static int in_set_sample_rate(struct audio_stream *stream __unused, uint32_t rate)
{
    ALOGV("in_set_sample_rate(%d) - NOPE", rate);
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stream_in * in = ((const struct stream_in*)stream);
    return proxy_get_period_size(&in->proxy) * audio_stream_in_frame_size(&(in->stream));
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    const struct stream_in *in = (const struct stream_in*)stream;
    return in->hal_channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
     alsa_device_proxy *proxy = &((struct stream_in*)stream)->proxy;
     audio_format_t format = audio_format_from_pcm_format(proxy_get_format(proxy));
     return format;
}

static int in_set_format(struct audio_stream *stream __unused, audio_format_t format)
{
    ALOGV("in_set_format(%d) - NOPE", format);

    return -ENOSYS;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct usb_audio_device *usb_device = in->adev;
    struct kara_manager *karaoke = &usb_device->karaoke;

    if (in->echo_reference) {
        ALOGD("%s() - stop echo_reference", __func__);
        /* stop reading from echo reference */
        in->echo_reference->read(in->echo_reference, NULL);
        put_echo_reference(karaoke, in->echo_reference);
        in->echo_reference = NULL;
    }

    if (karaoke->karaoke_on || karaoke->karaoke_start) {
        ALOGI("%s stop karaoke record!", __func__);
        return 0;
    }

    stream_lock(&in->lock);
    if (!in->standby) {
        device_lock(in->adev);
        proxy_close(&in->proxy);
        device_unlock(in->adev);
        in->standby = true;
    }

    stream_unlock(&in->lock);

    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
  const struct stream_in* in_stream = (const struct stream_in*)stream;
  if (in_stream != NULL) {
      dprintf(fd, "Input Profile:\n");
      profile_dump(in_stream->profile, fd);

      dprintf(fd, "Input Proxy:\n");
      proxy_dump(&in_stream->proxy, fd);
  }

  return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("++in_set_parameters() keys:%s", kvpairs);

    struct stream_in *in = (struct stream_in *)stream;

    int ret_value = 0;
    int card = -1;
    int device = -1;

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        // nothing to do
        return ret_value;
    }

    stream_lock(&in->lock);
    device_lock(in->adev);

    if (card >= 0 && device >= 0 && !profile_is_cached_for(in->profile, card, device)) {
        /* cannot read pcm device info if playback is active, or more than one open stream */
        if (!in->standby || in->adev->inputs_open > 1)
            ret_value = -ENOSYS;
        else {
            int saved_card = in->profile->card;
            int saved_device = in->profile->device;
            in->adev->in_profile.card = card;
            in->adev->in_profile.device = device;
            ret_value = profile_read_device_info(&in->adev->in_profile) ? 0 : -EINVAL;
            if (ret_value != 0) {
                in->adev->in_profile.card = saved_card;
                in->adev->in_profile.device = saved_device;
            }
        }
    }

    device_unlock(in->adev);
    stream_unlock(&in->lock);

    ALOGV("--in_set_parameters() ret_value:%d", ret_value);
    return ret_value;
}

static char * in_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_in *in = (struct stream_in *)stream;
    //struct usb_audio_device *usb_device = in->adev;
    //struct kara_manager *karaoke = &usb_device->karaoke;

    ALOGV("++in_get_parameters() keys:%s", keys);

    stream_lock(&in->lock);
    device_lock(in->adev);

    char * params_str =  device_get_parameters(in->profile, keys);

    device_unlock(in->adev);
    stream_unlock(&in->lock);

    ALOGV("--in_get_parameters() keys:%s", keys);

    return params_str;
}

static int read_from_kara_buffer(struct audio_stream_in *stream, void *buffer, size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct usb_audio_device *usb_device = in->adev;
    struct kara_manager *kara = &usb_device->karaoke;
    int rate_req = proxy_get_sample_rate(&in->proxy);
    int channels_req = proxy_get_channel_count(&in->proxy);
    int frames = bytes / channels_req / 2; // suppose 16bit
    int ret;

    if (!in->echo_reference) {
        in->echo_reference = get_echo_reference(kara,
                AUDIO_FORMAT_PCM_16_BIT,
                channels_req, rate_req);
    }

    if (in->echo_reference) {
        struct echo_reference_buffer b;
        memset(&b, 0, sizeof(b));

        b.raw = buffer;
        b.frame_count = frames;

        in->echo_reference->read(in->echo_reference, &b);
    } else {
        memset(buffer, 0, bytes);
    }

    return bytes;
}

/* must be called with hw device and output stream mutexes locked */
static int start_input_stream(struct stream_in *in)
{
    ALOGV("start_input_stream(card:%d device:%d)", in->profile->card, in->profile->device);

    return proxy_open(&in->proxy);
}

/* TODO mutex stuff here (see out_write) */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    size_t num_read_buff_bytes = 0;
    void * read_buff = buffer;
    void * out_buff = buffer;
    int ret = 0;
    struct stream_in * in = (struct stream_in *)stream;
    struct usb_audio_device *usb_device = in->adev;
    struct aml_audio_device *adev = (struct aml_audio_device *)usb_device->adev_primary;
    struct kara_manager *karaoke = &usb_device->karaoke;

    if (karaoke->karaoke_on || karaoke->karaoke_start) {
        if (!in->standby) {
            //device_lock(in->adev);
            proxy_close(&in->proxy);
            //device_unlock(in->adev);
            in->standby = true;
            karaoke->karaoke_enable = true;
            ALOGD("karaoke is on, audio record steam do standby!");
        }

        return read_from_kara_buffer(stream, buffer, bytes);
    }

    stream_lock(&in->lock);
    if (in->standby) {
        device_lock(in->adev);
        ret = start_input_stream(in);
        device_unlock(in->adev);
        if (ret != 0) {
            goto err;
        }
        in->standby = false;
    }

    /*
     * OK, we need to figure out how much data to read to be able to output the requested
     * number of bytes in the HAL format (16-bit, stereo).
     */
    num_read_buff_bytes = bytes;
    int num_device_channels = proxy_get_channel_count(&in->proxy); /* what we told Alsa */
    int num_req_channels = in->hal_channel_count; /* what we told AudioFlinger */

    if (num_device_channels != num_req_channels) {
        num_read_buff_bytes = (num_device_channels * num_read_buff_bytes) / num_req_channels;
    }

    /* Setup/Realloc the conversion buffer (if necessary). */
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            /*TODO Remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
              (and do these conversions themselves) */
            in->conversion_buffer_size = num_read_buff_bytes;
            in->conversion_buffer = realloc(in->conversion_buffer, in->conversion_buffer_size);
        }
        read_buff = in->conversion_buffer;
    }

    ret = proxy_read(&in->proxy, read_buff, num_read_buff_bytes);
    if (ret == 0) {
        if (num_device_channels != num_req_channels) {
            // ALOGV("chans dev:%d req:%d", num_device_channels, num_req_channels);

            out_buff = buffer;
            /* Num Channels conversion */
            if (num_device_channels != num_req_channels) {
                audio_format_t audio_format = in_get_format(&(in->stream.common));
                unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);

                num_read_buff_bytes =
                    adjust_channels(read_buff, num_device_channels,
                                    out_buff, num_req_channels,
                                    sample_size_in_bytes, num_read_buff_bytes);
            }
        }

        /* no need to acquire in->adev->lock to read mic_muted here as we don't change its state */
        if (num_read_buff_bytes > 0 && in->adev->mic_muted)
            memset(buffer, 0, num_read_buff_bytes);
    } else {
        num_read_buff_bytes = 0; // reset the value after USB headset is unplugged
    }

err:
    stream_unlock(&in->lock);
    return num_read_buff_bytes;
}

int adev_open_usb_input_stream(struct usb_audio_device *hw_dev,
                               audio_devices_t devices,
                               struct audio_config *config,
                               struct audio_stream_in **stream_in,
                               const char *address)
{
    ALOGV("++adev_open_usb_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);

    /* Pull out the card/device pair */
    int32_t card, device;
    struct aml_audio_device *adev = (struct aml_audio_device *)hw_dev->adev_primary;
    struct kara_manager *karaoke = &hw_dev->karaoke;

    if (!parse_card_device_params(address, &card, &device)) {
        ALOGW("%s fail - invalid address %s", __func__, address);
        *stream_in = NULL;
        return -EINVAL;
    }

    /* if karaoke is enable, free hardware for usb hal */
    if (karaoke->karaoke_enable) {
        karaoke->karaoke_enable = false;
        if (karaoke->close)
            karaoke->close(karaoke);
    }

    struct stream_in * const in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL) {
        *stream_in = NULL;
        return -ENOMEM;
    }

    /* setup function pointers */
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;

    in->stream.read = in_read;

    stream_lock_init(&in->lock);

    in->adev = (struct usb_audio_device *)hw_dev;
    device_lock(in->adev);

    in->profile = &in->adev->in_profile;
    in->device = devices;

    struct pcm_config proxy_config;
    memset(&proxy_config, 0, sizeof(proxy_config));

    int ret = 0;
    /* Check if an input stream is already open */
    if (in->adev->inputs_open > 0) {
        if (!profile_is_cached_for(in->profile, card, device)) {
            ALOGW("%s fail - address card:%d device:%d doesn't match existing profile",
                    __func__, card, device);
            ret = -EINVAL;
        }
    } else {
        /* Read input profile only if necessary */
        in->adev->in_profile.card = card;
        in->adev->in_profile.device = device;
        if (!profile_read_device_info(&in->adev->in_profile)) {
            ALOGW("%s fail - cannot read profile", __func__);
            ret = -EINVAL;
        }
    }
    if (ret != 0) {
        device_unlock(in->adev);
        free(in);
        *stream_in = NULL;
        return ret;
    }

    /* Rate */
    if (config->sample_rate == 0) {
        config->sample_rate = profile_get_default_sample_rate(in->profile);
    }

    if (profile_is_sample_rate_valid(in->profile, config->sample_rate)) {
        proxy_config.rate = config->sample_rate;
    } else {
        proxy_config.rate = config->sample_rate = profile_get_default_sample_rate(in->profile);
        ret = -EINVAL;
    }
    device_unlock(in->adev);

    /* Format */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        proxy_config.format = profile_get_default_format(in->profile);
        config->format = audio_format_from_pcm_format(proxy_config.format);
    } else {
        enum pcm_format fmt = pcm_format_from_audio_format(config->format);
        if (profile_is_format_valid(in->profile, fmt)) {
            proxy_config.format = fmt;
        } else {
            proxy_config.format = profile_get_default_format(in->profile);
            config->format = audio_format_from_pcm_format(proxy_config.format);
            ret = -EINVAL;
        }
    }

    /* set profile to karaoke*/
    if (profile_is_valid(&hw_dev->in_profile))
        karaoke_init(&hw_dev->karaoke, &hw_dev->in_profile);

    /* Channels */
    bool calc_mask = false;
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* query case */
        in->hal_channel_count = profile_get_default_channel_count(in->profile);
        calc_mask = true;
    } else {
        /* explicit case */
        in->hal_channel_count = audio_channel_count_from_in_mask(config->channel_mask);
    }

    /* The Framework is currently limited to no more than this number of channels */
    if (in->hal_channel_count > FCC_8) {
        in->hal_channel_count = FCC_8;
        calc_mask = true;
    }

    if (calc_mask) {
        /* need to calculate the mask from channel count either because this is the query case
         * or the specified mask isn't valid for this device, or is more then the FW can handle */
        in->hal_channel_mask = in->hal_channel_count <= FCC_2
            /* position mask for mono & stereo */
            ? audio_channel_in_mask_from_count(in->hal_channel_count)
            /* otherwise indexed */
            : audio_channel_mask_for_index_assignment_from_count(in->hal_channel_count);

        // if we change the mask...
        if (in->hal_channel_mask != config->channel_mask &&
            config->channel_mask != AUDIO_CHANNEL_NONE) {
            config->channel_mask = in->hal_channel_mask;
            ret = -EINVAL;
        }
    } else {
        in->hal_channel_mask = config->channel_mask;
    }

    if (ret == 0) {
        // Validate the "logical" channel count against support in the "actual" profile.
        // if they differ, choose the "actual" number of channels *closest* to the "logical".
        // and store THAT in proxy_config.channels
        proxy_config.channels =
                profile_get_closest_channel_count(in->profile, in->hal_channel_count);
        ret = proxy_prepare(&in->proxy, in->profile, &proxy_config);
        if (ret == 0) {
            in->standby = true;

            in->conversion_buffer = NULL;
            in->conversion_buffer_size = 0;

            *stream_in = &in->stream;

            /* Save this for adev_dump() */
            //adev_add_stream_to_list(in->adev, &in->adev->input_stream_list, &in->list_node);
        } else {
            ALOGW("proxy_prepare error %d", ret);
            unsigned channel_count = proxy_get_channel_count(&in->proxy);
            config->channel_mask = channel_count <= FCC_2
                ? audio_channel_in_mask_from_count(channel_count)
                : audio_channel_mask_for_index_assignment_from_count(channel_count);
            config->format = audio_format_from_pcm_format(proxy_get_format(&in->proxy));
            config->sample_rate = proxy_get_sample_rate(&in->proxy);
        }
    }

    if (ret != 0) {
        // Deallocate this stream on error, because AudioFlinger won't call
        // adev_close_input_stream() in this case.
        *stream_in = NULL;
        free(in);
        goto Exit;
    }

    device_lock(in->adev);
    ++in->adev->inputs_open;
    device_unlock(in->adev);

    karaoke->karaoke_enable = true;

    in->adev->stream = (struct audio_stream_in *)in;

Exit:
    ALOGV("--adev_open_usb_input_stream() exit, stream = %p, ret = %d", in->adev->stream, ret);
    return ret;
}

void adev_close_usb_input_stream(struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    ALOGV("++adev_close_input_stream, stream = %p", in);

    //adev_remove_stream_from_list(in->adev, &in->list_node);

    device_lock(in->adev);
    --in->adev->inputs_open;
    LOG_ALWAYS_FATAL_IF(in->adev->inputs_open < 0,
            "invalid inputs_open: %d", in->adev->inputs_open);
    device_unlock(in->adev);

    /* Close the pcm device */
    in_standby(&stream->common);

    free(in->conversion_buffer);

    in->adev->stream = NULL;

    free(stream);

    ALOGV("--adev_close_usb_input_stream exit");

    return;
}

