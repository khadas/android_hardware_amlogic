/*
 * Copyright (C) 2012 The Android Open Source Project
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

#pragma once

// clang-format off
/* This UUID is generated with uuidgen
   the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html */
#define TA_KEYMASTER_UUID {0x8efb1e1c, 0x37e5, 0x4326, \
    { 0xa5, 0xd6, 0x8c, 0x33, 0x72, 0x6c, 0x7d, 0x57} }
// Commands
enum amlogic_keymaster_command : uint32_t {
    KM_TA_INIT                       = (0x10000 << KEYMASTER_REQ_SHIFT),
    KM_TA_TERM                       = (0x10001 << KEYMASTER_REQ_SHIFT),
};
