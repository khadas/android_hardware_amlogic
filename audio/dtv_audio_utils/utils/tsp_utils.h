/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRMP_UTILS_H
#define DRMP_UTILS_H

#include "tsp_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRMPTRACE
#define DRMPTRACE ALOGE
#endif
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FFERRTAG(a, b, c, d) (-(int)MKTAG(a, b, c, d))
#define ERROR_EOF                FFERRTAG( 'E','O','F',' ') ///< End of file

#define EEIO (-5)
enum DRMTYPE {
    DRMTYPE_NONE,

    DRMTYPE_DCLR,/*clear ts , just for test*/
    DRMTYPE_DSTB,/*for drmplayer test, payload is encrypted*/
    DRMTYPE_DHLS,/*for drmplayer test, whole mpeg ts is encrypted*/
    DRMTYPE_VSTB,/*verimatrix IPTV*/
    DRMTYPE_VWCH,/*verimatrix WEB Client HLS*/
    DRMTYPE_WCAS,/*widevine cas Client*/

    /*add new drmtype here*/

	DRMTYPE_INVD,/*invalid type, need to add support*/
 };
int strstart(const char *str, const char *pfx, const char **ptr);
enum DRMTYPE drmtype_convert(char * prefix);
int is_ultra();
uint32_t time_ms(void);
#ifdef __cplusplus
}
#endif

#endif
