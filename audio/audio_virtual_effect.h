#ifndef _AUDIO_HW_EFFECT_H_
#define _AUDIO_HW_EFFECT_H_

#ifdef __cplusplus
extern "C" {
#endif

int Virtualizer_init(void);
int Virtualizer_control(int enable, int EffectLevel);
int Virtualizer_process(int16_t *pInData, int16_t *pOutData, uint16_t NumSamples);
int Virtualizer_release(void);

#ifdef __cplusplus
}
#endif

#endif
