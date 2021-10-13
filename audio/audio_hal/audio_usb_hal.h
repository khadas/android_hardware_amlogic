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

#ifndef _AMLOGIC_USB_AUDIO_HW_H_
#define _AMLOGIC_USB_AUDIO_HW_H_

#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"
#include "karaoke_manager.h"

 struct usb_audio_device {
     pthread_mutex_t lock; /* see note below on mutex acquisition order */

     /* input */
     alsa_device_profile in_profile;
     struct listnode input_stream_list;
     struct audio_stream_in *stream;

     bool mic_muted;
     int32_t inputs_open; /* number of input streams currently open. */

     /* for karaoke mixer*/
     struct kara_manager karaoke;

     void *adev_primary;
 };

 struct stream_lock {
     pthread_mutex_t lock;               /* see note below on mutex acquisition order */
     pthread_mutex_t pre_lock;           /* acquire before lock to avoid DOS by playback thread */
 };

 struct stream_in {
     struct audio_stream_in stream;

     struct stream_lock  lock;

     bool standby;

     audio_devices_t device;

     struct usb_audio_device *adev;      /* hardware information - only using this for the lock */

     const alsa_device_profile *profile; /* Points to the alsa_device_profile in the audio_device.
                                          * Const, so modifications go through adev->out_profile
                                          * and thus should have the hardware lock and ensure
                                          * stream is not active and no other open input streams.
                                          */

     alsa_device_proxy proxy;            /* state of the stream */

     unsigned hal_channel_count;         /* channel count exposed to AudioFlinger.
                                          * This may differ from the device channel count when
                                          * the device is not compatible with AudioFlinger
                                          * capabilities, e.g. exposes too many channels or
                                          * too few channels. */
     audio_channel_mask_t hal_channel_mask;  /* USB devices deal in channel counts, not masks
                                              * so the proxy doesn't have a channel_mask, but
                                              * audio HALs need to talk about channel masks
                                              * so expose the one calculated by
                                              * adev_open_input_stream */

     struct listnode list_node;

     /* We may need to read more data from the device in order to data reduce to 16bit, 4chan */
     void * conversion_buffer;           /* any conversions are put into here
                                          * they could come from here too if
                                          * there was a previous conversion */
     size_t conversion_buffer_size;      /* in bytes */

     struct echo_reference_itfe *echo_reference;
 };

int adev_open_usb_input_stream(struct usb_audio_device *hw_dev,
                               audio_devices_t devices,
                               struct audio_config *config,
                               struct audio_stream_in **stream_in,
                               const char *address);

void adev_close_usb_input_stream(struct audio_stream_in *stream);

#endif
