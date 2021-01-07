/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "AmlogicKeymaster"

#include <log/log.h>
#include <keymaster/android_keymaster_messages.h>
#include <keymaster/keymaster_configuration.h>
#include <amlogic_keymaster/AmlogicKeymaster.h>
#include <amlogic_keymaster/ipc/amlogic_keymaster_ipc.h>

#if AMLOGIC_MODIFY
#include <amlogic_keymaster/amlogic_keymaster_messages.h>

#include <android-base/properties.h>
#include <sys/system_properties.h>
#endif

namespace keymaster {

int AmlogicKeymaster::Initialize() {
    int err;

#if AMLOGIC_MODIFY
    KM_context.fd = 0;
    KM_session.ctx = NULL;
    KM_session.session_id = 0;

	err = aml_keymaster_connect(&KM_context, &KM_session);
#else
    err = trusty_keymaster_connect();
#endif
    if (err) {
        ALOGE("Failed to connect to amlogic keymaster %d", err);
        return err;
    }

    ConfigureRequest req;
    req.os_version = GetOsVersion();
    req.os_patchlevel = GetOsPatchlevel();

    ConfigureResponse rsp;
    Configure(req, &rsp);

    if (rsp.error != KM_ERROR_OK) {
        ALOGE("Failed to configure keymaster %d", rsp.error);
        return -1;
    }

    return 0;
}

AmlogicKeymaster::AmlogicKeymaster() {
    KM_context.fd = 0;
    KM_session.ctx = NULL;
    KM_session.session_id = 0;
}

AmlogicKeymaster::~AmlogicKeymaster() {
#if AMLOGIC_MODIFY
   if (KM_session.ctx != NULL)
	   aml_keymaster_disconnect(&KM_context, &KM_session);
#else
    trusty_keymaster_disconnect();
#endif
}
#if AMLOGIC_MODIFY
/* Move this method into class */
void AmlogicKeymaster::ForwardCommand(enum keymaster_command command, const Serializable& req,
                           KeymasterResponse* rsp) {
    keymaster_error_t err;
    err = aml_keymaster_send(&KM_session, command, req, rsp);
    if (err != KM_ERROR_OK) {
        ALOGE("Failed to send cmd %d err: %d", command, err);
        rsp->error = err;
    }
}
#else
static void ForwardCommand(enum keymaster_command command, const Serializable& req,
                           KeymasterResponse* rsp) {
    keymaster_error_t err;
    err = trusty_keymaster_send(command, req, rsp);
    if (err != KM_ERROR_OK) {
        ALOGE("Failed to send cmd %d err: %d", command, err);
        rsp->error = err;
    }
}
#endif

void AmlogicKeymaster::GetVersion(const GetVersionRequest& request, GetVersionResponse* response) {
    ForwardCommand(KM_GET_VERSION, request, response);
}

void AmlogicKeymaster::SupportedAlgorithms(const SupportedAlgorithmsRequest& request,
                                          SupportedAlgorithmsResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_ALGORITHMS, request, response);
}

void AmlogicKeymaster::SupportedBlockModes(const SupportedBlockModesRequest& request,
                                          SupportedBlockModesResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_BLOCK_MODES, request, response);
}

void AmlogicKeymaster::SupportedPaddingModes(const SupportedPaddingModesRequest& request,
                                            SupportedPaddingModesResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_PADDING_MODES, request, response);
}

void AmlogicKeymaster::SupportedDigests(const SupportedDigestsRequest& request,
                                       SupportedDigestsResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_DIGESTS, request, response);
}

void AmlogicKeymaster::SupportedImportFormats(const SupportedImportFormatsRequest& request,
                                             SupportedImportFormatsResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_IMPORT_FORMATS, request, response);
}

void AmlogicKeymaster::SupportedExportFormats(const SupportedExportFormatsRequest& request,
                                             SupportedExportFormatsResponse* response) {
    ForwardCommand(KM_GET_SUPPORTED_EXPORT_FORMATS, request, response);
}

void AmlogicKeymaster::AddRngEntropy(const AddEntropyRequest& request,
                                    AddEntropyResponse* response) {
    ForwardCommand(KM_ADD_RNG_ENTROPY, request, response);
}

void AmlogicKeymaster::Configure(const ConfigureRequest& request, ConfigureResponse* response) {
    ForwardCommand(KM_CONFIGURE, request, response);
}

void AmlogicKeymaster::GenerateKey(const GenerateKeyRequest& request,
                                  GenerateKeyResponse* response) {
    GenerateKeyRequest datedRequest(request.message_version);
    datedRequest.key_description = request.key_description;

    if (!request.key_description.Contains(TAG_CREATION_DATETIME)) {
        datedRequest.key_description.push_back(TAG_CREATION_DATETIME, java_time(time(NULL)));
    }

    ForwardCommand(KM_GENERATE_KEY, datedRequest, response);
}

void AmlogicKeymaster::GetKeyCharacteristics(const GetKeyCharacteristicsRequest& request,
                                            GetKeyCharacteristicsResponse* response) {
    ForwardCommand(KM_GET_KEY_CHARACTERISTICS, request, response);
}

void AmlogicKeymaster::ImportKey(const ImportKeyRequest& request, ImportKeyResponse* response) {
    ForwardCommand(KM_IMPORT_KEY, request, response);
}

void AmlogicKeymaster::ImportWrappedKey(const ImportWrappedKeyRequest& request,
                                       ImportWrappedKeyResponse* response) {
    ForwardCommand(KM_IMPORT_WRAPPED_KEY, request, response);
}

void AmlogicKeymaster::ExportKey(const ExportKeyRequest& request, ExportKeyResponse* response) {
    ForwardCommand(KM_EXPORT_KEY, request, response);
}

void AmlogicKeymaster::AttestKey(const AttestKeyRequest& request, AttestKeyResponse* response) {
    ForwardCommand(KM_ATTEST_KEY, request, response);
}

void AmlogicKeymaster::UpgradeKey(const UpgradeKeyRequest& request, UpgradeKeyResponse* response) {
    ForwardCommand(KM_UPGRADE_KEY, request, response);
}

void AmlogicKeymaster::DeleteKey(const DeleteKeyRequest& request, DeleteKeyResponse* response) {
    ForwardCommand(KM_DELETE_KEY, request, response);
}

void AmlogicKeymaster::DeleteAllKeys(const DeleteAllKeysRequest& request,
                                    DeleteAllKeysResponse* response) {
    ForwardCommand(KM_DELETE_ALL_KEYS, request, response);
}

void AmlogicKeymaster::BeginOperation(const BeginOperationRequest& request,
                                     BeginOperationResponse* response) {
    ForwardCommand(KM_BEGIN_OPERATION, request, response);
}

void AmlogicKeymaster::UpdateOperation(const UpdateOperationRequest& request,
                                      UpdateOperationResponse* response) {
    ForwardCommand(KM_UPDATE_OPERATION, request, response);
}

void AmlogicKeymaster::FinishOperation(const FinishOperationRequest& request,
                                      FinishOperationResponse* response) {
    uint32_t req_size = request.SerializedSize();

    if (req_size > AMLOGIC_KEYMASTER_SEND_BUF_SIZE) {
        /* abort the operation, if req is oversize and final */
        AbortOperationRequest abort_req;
        AbortOperationResponse abort_rsp;

        abort_req.op_handle = request.op_handle;
        ForwardCommand(KM_ABORT_OPERATION, abort_req, &abort_rsp);
        response->error = KM_ERROR_INVALID_INPUT_LENGTH;
    } else {
        ForwardCommand(KM_FINISH_OPERATION, request, response);
    }
}

void AmlogicKeymaster::AbortOperation(const AbortOperationRequest& request,
                                     AbortOperationResponse* response) {
    ForwardCommand(KM_ABORT_OPERATION, request, response);
}

GetHmacSharingParametersResponse AmlogicKeymaster::GetHmacSharingParameters() {
    // Dummy empty buffer to allow ForwardCommand to have something to serialize
    Buffer request;
    GetHmacSharingParametersResponse response;
    ForwardCommand(KM_GET_HMAC_SHARING_PARAMETERS, request, &response);
    return response;
}

ComputeSharedHmacResponse AmlogicKeymaster::ComputeSharedHmac(
        const ComputeSharedHmacRequest& request) {
    ComputeSharedHmacResponse response;
    ForwardCommand(KM_COMPUTE_SHARED_HMAC, request, &response);
    return response;
}

VerifyAuthorizationResponse AmlogicKeymaster::VerifyAuthorization(
        const VerifyAuthorizationRequest& request) {
    VerifyAuthorizationResponse response;
    ForwardCommand(KM_VERIFY_AUTHORIZATION, request, &response);
    return response;
}
#if AMLOGIC_MODIFY
	DeviceLockedResponse AmlogicKeymaster::DeviceLocked(__attribute__((unused))const DeviceLockedRequest& request) {
		//TODO: Replace fake implementation here
		return DeviceLockedResponse(KM_ERROR_OK);
	}
	EarlyBootEndedResponse AmlogicKeymaster::EarlyBootEnded() {
		//TODO: Replace fake implementation here
		return EarlyBootEndedResponse(KM_ERROR_OK);
	}
#endif
}  // namespace keymaster
