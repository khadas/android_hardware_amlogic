/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef TRUSTY_KEYMASTER_AMLOGIC_KEYMASTER_IPC_H_
#define TRUSTY_KEYMASTER_AMLOGIC_KEYMASTER_IPC_H_

#include <keymaster/android_keymaster_messages.h>
#include <amlogic_keymaster/ipc/keymaster_ipc.h>

extern "C" {
#include <tee_client_api.h>
}
__BEGIN_DECLS

const uint32_t AMLOGIC_KEYMASTER_RECV_BUF_SIZE = 2 * PAGE_SIZE;
const uint32_t AMLOGIC_KEYMASTER_SEND_BUF_SIZE =
        (PAGE_SIZE - sizeof(struct keymaster_message) - 16 /* tipc header */);
#if !AMLOGIC_MODIFY
int trusty_keymaster_connect(void);
int trusty_keymaster_call(uint32_t cmd, void* in, uint32_t in_size, uint8_t* out,
                          uint32_t* out_size);
void trusty_keymaster_disconnect(void);

keymaster_error_t translate_error(int err);
keymaster_error_t trusty_keymaster_send(uint32_t command, const keymaster::Serializable& req,
                                        keymaster::KeymasterResponse* rsp);
#else
TEEC_Result aml_keymaster_connect(TEEC_Context *c, TEEC_Session *s);
TEEC_Result aml_keymaster_call(TEEC_Session *s, uint32_t cmd, void* in, uint32_t in_size, uint8_t* out,
                       uint32_t* out_size);
TEEC_Result aml_keymaster_disconnect(TEEC_Context *c, TEEC_Session *s);
keymaster_error_t translate_error(int err);
keymaster_error_t aml_keymaster_send(TEEC_Session *s, uint32_t command, const keymaster::Serializable& req,
                                        keymaster::KeymasterResponse* rsp);
#endif

__END_DECLS

#endif  // TRUSTY_KEYMASTER_AMLOGIC_KEYMASTER_IPC_H_
