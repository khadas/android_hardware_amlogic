#ifndef NEUSDK_AI_MODULE_COMMON_VSI_DEFINE_H
#define NEUSDK_AI_MODULE_COMMON_VSI_DEFINE_H

// this file include part of the define of VSI, it is used if  we don't use VSI NNA
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint8_t*                data_ptr;
	int                     width = 512;
	int                     height = 288;
} nn_rgb_frame_t;

#ifdef __cplusplus
}
#endif

#endif