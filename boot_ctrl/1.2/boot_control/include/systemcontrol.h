/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H
#include <errno.h>
#include <string>

int sc_read_bootenv(const char * key, std::string & val);
bool sc_set_bootenv(const char *key, const std::string &val);

#endif/*SYSTEM_CONTROL_H*/
