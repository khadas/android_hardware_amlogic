#ifndef SPEECH_AUDIO_THIRD_PARTY_AUDIO_EARS_ECHO_GOOGLE_AEC_H_
#define SPEECH_AUDIO_THIRD_PARTY_AUDIO_EARS_ECHO_GOOGLE_AEC_H_

#include <stdint.h>

namespace audio_ears {
class EchoCanceller;
struct GoogleAecInternal;

// Multi-channel Acoustic Echo Canceller.
//
// It is capable of processing multi-channel playout and multi-channel
// captured signals. Samples of playout and captured signals may be
// fed in asynchronous fashion. The AEC estimates the relative delay
// of microphone signal with respect to playout signal. The delay
// should not exceed 200 milliseconds, and causality is assumed when
// the delay is estimated, i.e., captured signal is behind the playout
// signal.
//
// It is expected that the playout DAC and the capture ADC to be in
// sync, i.e., both are driven by the same master clock. There is no
// clock drift compensation.
//
// After the initial delay estimation, AEC does not monitor whether
// playout and capture signals stay aligned, hence, no more
// realignment occurs after the initial delay estimation, unless
// Reset() is called. Therefore, it is the responsibility of the
// client of this module to monitor for audio drops and compensate for
// them.
//
// This class is NOT thread-safe.
class GoogleAec {
 public:
  struct AudioBufferInfo {
    explicit AudioBufferInfo(int num_samples_per_channel)
        : samples_per_channel(num_samples_per_channel),
          timestamp_microseconds(0),
          valid_timestamp(false) {}

    int samples_per_channel;
    uint64_t timestamp_microseconds;
    bool valid_timestamp;
  };

  // sample_rate_hz: sampling rate of loudspeaker and microphone signals in
  //                 Hertz.
  //
  // num_loudspeaker_feed: number of audio channels feeds to loudspeakers.
  //
  // num_microphone_channels: number of microphones signals to be processed.
  //
  // aec_mode: a NULL terminated char array specifying the operational mode of
  //           AEC, different modes differ by complexity and performance.
  //           Currently 3 operational modes are supported:
  //
  //           GoogleAecMode1: more appropriate for cases with shorter
  //                           reverb and relatively low complexity.
  //
  //           GoogleAecMode2: more appropriate for medium length reverb and
  //                           have relatively medium complexity.
  //
  //           GoogleAecMode3: more suitable for heavy tailed reverb cases and
  //                           has higher complexity compared to GoogleAecMode0
  //                           and GoogleAecMode1.
  //
  //           GoogleAecMode4: this mode is the same as GoogleAecMode3 for mono
  //                           playout. However, for stereo playout it uses a
  //                           more stable method to compute filter
  //                           coefficients, hence, might be somewhat more
  //                           complex.
  //
  // align_by_timestamp: indicates that valid timestamps are provided to be used
  //     for alignment. Timestamps are given in microseconds and indicate the
  //     time of the playout or the capture of the first sample in a block,
  //     according to a wall clock. Timestamps are expected to be accurate
  //     within 100 microseconds.
  GoogleAec(int sample_rate_hz, int num_loudspeaker_feeds,
            int num_microphone_channels, const char* aec_mode,
            bool align_by_timestamp);
  ~GoogleAec();

  GoogleAec(const GoogleAec&) = delete;
  GoogleAec& operator=(const GoogleAec&) = delete;

  // Processes zero or more loudspeaker samples and zero or more microphone
  // samples and returns zero or more cleaned samples.  We restrict to process
  // audio in chunks which are shorter than 50 ms.
  //
  //
  // Inputs:
  //
  // loudspeaker_samples: an array of pointers to loudspeaker signals. Each
  //     element points to one channel, hence, it is expected to have as many
  //     valid pointers as the number of loudspeaker feeds that is specified at
  //     the construction.
  //
  // num_loudspeaker_samples_per_channel: number of samples per loudspeaker
  //     channels.
  //
  // loudspeaker_timestamp_microsec: the time, in microseconds, when the first
  //     sample of the given block of loudspeaker samples is played out.
  //
  // microphone_samples: an array of pointers to microphone signals. Each
  //     element points to one channel, hence, it is expected to have as many
  //     valid pointers as the number of microphone channels which is specified
  //     at the construction.
  //
  // num_microphone_samples_per_channel: number of samples per microphone
  //     channels.
  //
  // microphone_timestamp_microsec: the time, in microseconds, when the first
  //     sample of the given block of microphone samples is captured.
  //
  // Outputs:
  //
  // num_cleaned_samples_per_channel: it is set to the number of cleaned samples
  //     per channel.
  //
  //
  // Return value:
  //
  // An array of pointers where each element is pointing to erased samples of
  // a microphone channel. The return value is NULL if no clean sample is
  // produced. The buffers which hold erased samples are owned by GoogleAec.
  const int32_t* const* ProcessInt32PlanarAudio(
      const int32_t* const* loudspeaker_samples,
      const AudioBufferInfo& loudspeaker_buffer_info,
      const int32_t* const* microphone_samples,
      const AudioBufferInfo& microphone_buffer_info,
      int* num_cleaned_samples_per_channel);

  // Similarly to ProcessInt32PlanarAudio() processes zero or some loudspeaker
  // samples and zero or some microphone samples, producing zero or some cleaned
  // samples. However, the audio samples are of type int16_t.
  const int16_t* const* ProcessInt16PlanarAudio(
      const int16_t* const* loudspeaker_samples,
      const AudioBufferInfo& loudspeaker_buffer_info,
      const int16_t* const* microphone_samples,
      const AudioBufferInfo& microphone_buffer_info,
      int* num_cleaned_samples_per_channel);

  // Similarly to ProcessInt32PlanarAudio(), processes zero or more (but shorter
  // than 50 millisecond per channel) of loudspeaker and/or microphone samples,
  // and outputs zero or some cleaned samples. however, all audio streams
  // (loudspeaker, microphone and cleaned) are in interleaved format. Therefore,
  //
  //
  // Inputs
  //
  // loudspeaker_samples: pointer to interleaved loudspeaker samples.
  //
  // num_loudspeaker_samples_per_channel: number of loudspeaker samples per
  // channel.
  //
  // microphone_samples:  pointer to interleaved microphone samples.
  //
  // loudspeaker_timestamp_microsec: the time, in microseconds, when the first
  //     sample of the given block of loudspeaker samples is played out.
  //
  // num_microphone_samples_per_channel: number of microphone samples per
  // channel.
  //
  // microphone_timestamp_microsec: the time, in microseconds, when the first
  //     sample of the given block of microphone samples is captured.
  //
  // Outputs
  //
  // cleaned_samples: if any clean sample is generated it will be set to an
  //     internal buffer of AEC where clean samples are stored, in interleaved
  //     format, and the type of int32_t.
  //
  // Return Value:
  //
  // A pointer to a buffer that stores the cleaned samples in interleaved
  // format. The return value is NULL if no clean sample is produced. The
  // buffer which holds erased samples is owned by GoogleAec.
  const int32_t* ProcessInt32InterleavedAudio(
      const int32_t* loudspeaker_samples,
      const AudioBufferInfo& loudspeaker_buffer_info,
      const int32_t* microphone_samples,
      const AudioBufferInfo& microphone_buffer_info,
      int* num_cleaned_samples_per_channel);

  // Similar to ProcessInt32InterleavedAudio() but the input and the output
  // samples are of type int16_t.
  const int16_t* ProcessInt16InterleavedAudio(
      const int16_t* loudspeaker_samples,
      const AudioBufferInfo& loudspeaker_buffer_info,
      const int16_t* microphone_samples,
      const AudioBufferInfo& microphone_buffer_info,
      int* num_cleaned_samples_per_channel);

  // Resets the state of the AEC to right after creation, and discards
  // all buffered microphone samples.
  void Reset();

 private:
  GoogleAecInternal* state_;
};

}  // namespace audio_ears

#endif  // SPEECH_AUDIO_THIRD_PARTY_AUDIO_EARS_ECHO_GOOGLE_ECHO_CANCELLER_H_
