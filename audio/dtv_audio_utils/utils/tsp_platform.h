/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRMP_PLATFORM_H_
#define DRMP_PLATFORM_H_

#ifdef ANDROID
#include "log/log.h"
#include <cutils/properties.h>
#else

#ifdef __cplusplus
#include  <limits>
#endif

#include "tsplinux.h"
#endif


#endif /* DRMP_PLATFORM_H_ */
