

#define FIFO "/data/HBG/blehidfifo"
#define MAX_NUM_CALLBACK 10

#define LOGH_TAG "audio_so_hv_306"

#ifndef am8
struct stub_stream_in {
    struct audio_stream_in stream;
    int hbg_channel; //huajianwu
};
#endif
extern unsigned char hbg_bmic_used;
void startReceiveAudioData();
void stopReceiveAudioData();
int regist_callBack_stream();
void unregist_callBack_stream(int channel);
//in function
  uint32_t in_hbg_get_sample_rate(const struct audio_stream *stream);

 int in_hbg_set_sample_rate(struct audio_stream *stream, uint32_t rate);
 size_t in_hbg_get_buffer_size(const struct audio_stream *stream);

 audio_format_t in_hbg_get_format(const struct audio_stream *stream);
 uint32_t in_hbg_get_channels(const struct audio_stream *stream);
 int in_hbg_set_format(struct audio_stream *stream, audio_format_t format);
 int in_hbg_standby(struct audio_stream *stream);
 int in_hbg_dump(const struct audio_stream *stream, int fd);
 int in_hbg_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect);
 int in_hbg_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect);
 int in_hbg_set_parameters(struct audio_stream *stream, const char *kvpairs);
 char * in_hbg_get_parameters(const struct audio_stream *stream,const char *keys);
 int in_hbg_set_gain(struct audio_stream_in *stream, float gain);
 ssize_t in_hbg_read(struct audio_stream_in *stream, void* buffer,size_t bytes);
 uint32_t in_hbg_get_input_frames_lost(struct audio_stream_in *stream);
 int in_hbg_get_hbg_capture_position(const struct audio_stream_in *stream,int64_t *frames, int64_t *time);
 int is_hbg_hidraw();



