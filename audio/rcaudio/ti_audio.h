#ifndef TI_AUDIO_H
#define TI_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif


void audio_ParseData(unsigned char *pdu, unsigned short len, short *decode_buf, unsigned short *decode_len);


#ifdef __cplusplus
}
#endif
#endif
