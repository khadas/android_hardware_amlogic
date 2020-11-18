/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _FILESYSTEMIO_H_
#define _FILESYSTEMIO_H_
#include <stdint.h>

#ifdef ANDROID
#include "SystemControlClient.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t FileSystem_create();

size_t FileSystem_readFile(const char *name, char *value);
size_t FileSystem_writeFile(const char *name, const char *value);

size_t FileSystem_setAudioParam(int param1, int param2, int param3);

size_t FileSystem_release();

#ifdef __cplusplus
}
#endif

#else
size_t FileSystem_create();

size_t FileSystem_readFile(const char *name, char *value);
size_t FileSystem_writeFile(const char *name, const char *value);

size_t FileSystem_setAudioParam(int param1, int param2, int param3);

size_t FileSystem_release();

#endif
#endif

