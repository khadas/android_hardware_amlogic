#ifndef __SPDIFENC_WRAP_H__
#define __SPDIFENC_WRAP_H__

#ifdef __cplusplus
extern "C" {
#endif

int spdifenc_init(struct pcm *mypcm, audio_format_t format);
int spdifenc_write(const void *buffer, size_t numBytes);
uint64_t  spdifenc_get_total(void);

#ifdef __cplusplus
}
#endif

#endif  //__SPDIFENC_WRAP_H__
