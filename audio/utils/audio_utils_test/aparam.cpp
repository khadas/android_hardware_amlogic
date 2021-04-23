/*
 This file is used for tuning audio.
 */

/*
 * Copyright (C) 2017 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system/audio.h>
#include <media/AudioSystem.h>
#include <media/AudioParameter.h>

using namespace android;

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: param_set <io_handle> <cmd>\n");
        return -1;
    }

    return AudioSystem::setParameters(atoi(argv[1]), String8(argv[2]));
}
