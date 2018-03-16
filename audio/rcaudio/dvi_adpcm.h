#ifndef _dvi_adpcm_h
#define _dvi_adpcm_h

#include <stdint.h>

struct __attribute__ ((__packed__)) dvi_adpcm_state {
  int16_t valpred;    /* Previous predicted value */
  uint8_t index;     /* Index into stepsize table */
};

typedef struct dvi_adpcm_state dvi_adpcm_state_t;

void *dvi_adpcm_init(void *, double);
int dvi_adpcm_decode(void *in_buf, int in_size, void *out_buf, int *out_size, void *state);

#endif

