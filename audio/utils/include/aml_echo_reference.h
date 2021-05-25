/*
** Copyright 2011, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef AML_ECHO_REFERENCE_H
#define AML_ECHO_REFERENCE_H

#include <audio_utils/echo_reference.h>

__BEGIN_DECLS

int aml_create_echo_reference(audio_format_t rdFormat,
                          uint32_t rdChannelCount,
                          uint32_t rdSamplingRate,
                          audio_format_t wrFormat,
                          uint32_t wrChannelCount,
                          uint32_t wrSamplingRate,
                          struct echo_reference_itfe **);

void aml_release_echo_reference(struct echo_reference_itfe *echo_reference);

__END_DECLS

#endif // AML_ECHO_REFERENCE_H