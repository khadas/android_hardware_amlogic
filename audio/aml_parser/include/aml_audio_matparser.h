/*
 * Copyright (C) 2020 Amlogic Corporation.
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

#ifndef _AML_MAT_PARSER_H_
#define _AML_MAT_PARSER_H_

#define IS_MAT_FORMAT_LSB_SYNC(a,b) (((a) == 0x07) && ((b) == 0x9e))
#define IS_MAT_FORMAT_MSB_SYNC(a,b) (((a) == 0x9e) && ((b) == 0x07))
#define MLP_WITHIN_MAT_PROFILE                  1
#define OBJECT_PCM_WITHIN_MAT_PROFILE           2
#define CHANNEL_BASED_PCM_WITHIN_MAT_PROFILE    3

#define IS_AVAILABLE_MAT_STREAM_PROFILE(profile) \
    ((MLP_WITHIN_MAT_PROFILE == profile) || (OBJECT_PCM_WITHIN_MAT_PROFILE == profile) || (CHANNEL_BASED_PCM_WITHIN_MAT_PROFILE == profile))
/*
 *@brief get stream profile from dolby mat frame
 *   1:Meridian Lossless Packing (MLP) profile
 *   2:Object audio PCM + metadata profile
 *   3:Channel-based PCM + metadata profile
 */
int get_stream_profile_from_dolby_mat_frame(const char *audio_buffer, size_t audio_bytes);

#endif


