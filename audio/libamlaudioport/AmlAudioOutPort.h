/*
 * Copyright (C) 2007 The Android Open Source Project
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
#ifndef AML_AUDIO_OUT_PORT_H
#define AML_AUDIO_OUT_PORT_H

//#include <media/AudioSystem.h>
#include <system/audio.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/String8.h>


namespace android
{
class DevicesFactoryHalInterface;
class DeviceHalInterface;
class StreamOutHalInterface;

class AmlAudioOutPort : public RefBase
{
    public:
    sp<DevicesFactoryHalInterface> mDevicesFactoryHal;
    sp<DeviceHalInterface> hwDevice;
    sp<StreamOutHalInterface> outStream;
    const char address= {};
    struct audio_config config;
    audio_io_handle_t handle;
    audio_devices_t  devices = AUDIO_DEVICE_OUT_AUX_DIGITAL;
	audio_patch_handle_t patch;
    /*
    * Parameters:
     *
     * streamType:         Select the type of audio stream this track is attached to
     *                     (e.g. AUDIO_STREAM_MUSIC).
     * sampleRate:         Data source sampling rate in Hz.  Zero means to use the sink sample rate.
     *                     A non-zero value must be specified if AUDIO_OUTPUT_FLAG_DIRECT is set.
     *                     0 will not work with current policy implementation for direct output
     *                     selection where an exact match is needed for sampling rate.
     * format:             Audio format. For mixed tracks, any PCM format supported by server is OK.
     *                     For direct and offloaded tracks, the possible format(s) depends on the
     *                     output sink.
     * channelMask:        Channel mask, such that audio_is_output_channel(channelMask) is true.
     * flags:              See comments on audio_output_flags_t in <system/audio.h>.
     */
     AmlAudioOutPort(audio_stream_type_t streamType,
                        uint32_t sampleRate,
                        audio_format_t format,
                        audio_channel_mask_t channelMask,
                        audio_output_flags_t flags);

     //add for to get parament info
     AmlAudioOutPort();

     status_t    standby();
    /* After it's created the track is not active. Call start() to
     * make it active. If set, the callback will start being called.
     * If the track was previously paused, volume is ramped up over the first mix buffer.
     */
     status_t    start();

    /* Stop a track.
     * In static buffer mode, the track is stopped immediately.
     * In streaming mode, the callback will cease being called.  Note that obtainBuffer() still
     * works and will fill up buffers until the pool is exhausted, and then will return WOULD_BLOCK.
     * In streaming mode the stop does not occur immediately: any data remaining in the buffer
     * is first drained, mixed, and output, and only then is the track marked as stopped.
     */
     void        stop();

    /* Flush a stopped or paused track. All previously buffered data is discarded immediately.
     * This has the effect of draining the buffers without mixing or output.
     * Flush is intended for streaming mode, for example before switching to non-contiguous content.
     * This function is a no-op if the track is not stopped or paused, or uses a static buffer.
     */
    void        flush();

    /* Pause a track. After pause, the callback will cease being called and
     * obtainBuffer returns WOULD_BLOCK. Note that obtainBuffer() still works
     * and will fill up buffers until the pool is exhausted.
     * Volume is ramped down over the next mix buffer following the pause request,
     * and then the track is marked as paused.  It can be resumed with ramp up by start().
     */
    void        pause();

    /* Set volume for this track, mostly used for games' sound effects
     * left and right volumes. Levels must be >= 0.0 and <= 1.0.
     * This is the older API.  New applications should use setVolume(float) when possible.
     */
    status_t    setVolume(float left, float right);

    /* Set volume for all channels.  This is the preferred API for new applications,
     * especially for multi-channel content.
     */
    status_t    setVolume(float volume);
    /* Return the total number of frames played since playback start.
     * The counter will wrap (overflow) periodically, e.g. every ~27 hours at 44.1 kHz.
     * It is reset to zero by flush(), reload(), and stop().
     *
     * Parameters:
     *
     *  position:  Address where to return play head position.
     *
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful operation
     *  - BAD_VALUE:  position is NULL
     */
    status_t    getPosition(uint32_t *position);
    /* As a convenience we provide a write() interface to the audio buffer.
     * Input parameter 'size' is in byte units.
     * performance use callbacks. Returns actual number of bytes written >= 0,
     * or one of the following negative status codes:
     *      INVALID_OPERATION   AudioTrack is configured for static buffer or streaming mode
     *      BAD_VALUE           size is invalid
     *      WOULD_BLOCK         when obtainBuffer() returns same, or
     *                          AudioTrack was stopped during the write
     *      DEAD_OBJECT         when AudioFlinger dies or the output device changes and
     *                          the track cannot be automatically restored.
     *                          The application needs to recreate the AudioTrack
     *                          because the audio device changed or AudioFlinger died.
     *                          This typically occurs for direct or offload tracks
     *                          or if mDoNotReconnect is true.
     *      or any other error code returned by IAudioTrack::start() or restoreTrack_l().
     * Default behavior is to only return when all data has been transferred. Set 'blocking' to
     * false for the method to return immediately without waiting to try multiple times to write
     * the full content of the buffer.
     */
    ssize_t     write(const void* buffer, size_t size, bool blocking);

    status_t getHwDevice();

    status_t setParameters(const String8& keyValuePairs);
    String8  getParameters(const String8& keys);
    status_t createAudioPatch();
    status_t releaseAudioPatch();

    private:

};
}
#endif
