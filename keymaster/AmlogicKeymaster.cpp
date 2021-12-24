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

namespace keymaster {

int AmlogicKeymaster::Initialize(KmVersion version) {
    int err;

    KM_context.fd = 0;
    KM_session.ctx = NULL;
    KM_session.session_id = 0;

    err = aml_keymaster_connect(&KM_context, &KM_session);
    if (err) {
        ALOGE("Failed to connect to amlogic keymaster %d", err);
        return err;
    }

    // Try GetVersion2 first.
    GetVersion2Request versionReq;
    versionReq.max_message_version = MessageVersion(version);
    GetVersion2Response versionRsp = GetVersion2(versionReq);
    if (versionRsp.error != KM_ERROR_OK) {
        ALOGW("TA appears not to support GetVersion2, falling back (err = %d)", versionRsp.error);
        GetVersionRequest versionReq;
        GetVersionResponse versionRsp;
        GetVersion(versionReq, &versionRsp);
        if (versionRsp.error != KM_ERROR_OK) {
            ALOGE("Failed to get TA version %d", versionRsp.error);
            return -1;
        } else {
            keymaster_error_t error;
            message_version_ = NegotiateMessageVersion(versionRsp, &error);
            if (error != KM_ERROR_OK) {
                ALOGE("Failed to negotiate message version %d", error);
                return -1;
            }
        }
    } else {
        message_version_ = NegotiateMessageVersion(versionReq, versionRsp);
    }

    ConfigureRequest req(message_version());
    req.os_version = GetOsVersion();
    req.os_patchlevel = GetOsPatchlevel();

    ConfigureResponse rsp(message_version());
    Configure(req, &rsp);

    if (rsp.error != KM_ERROR_OK) {
        ALOGE("Failed to configure keymaster %d", rsp.error);
        return -1;
    }

    // Set the vendor patchlevel to value retrieved from system property (which
    // requires SELinux permission).
    ConfigureVendorPatchlevelRequest vendor_req(message_version());
    vendor_req.vendor_patchlevel = GetVendorPatchlevel();
    ConfigureVendorPatchlevelResponse vendor_rsp = ConfigureVendorPatchlevel(vendor_req);
    if (vendor_rsp.error != KM_ERROR_OK) {
        ALOGE("Failed to configure keymaster vendor patchlevel: %d", vendor_rsp.error);
        // Don't fail if this message isn't understood.
    }

    return 0;
}

AmlogicKeymaster::AmlogicKeymaster() {
    KM_context.fd = 0;
    KM_session.ctx = NULL;
    KM_session.session_id = 0;
}

AmlogicKeymaster::~AmlogicKeymaster() {
   if (KM_session.ctx != NULL)
       aml_keymaster_disconnect(&KM_context, &KM_session);
}

/* Move this method into class */
void AmlogicKeymaster::ForwardCommand(enum keymaster_command command, const KeymasterMessage& req,
                           KeymasterResponse* rsp) {
    keymaster_error_t err;
    err = aml_keymaster_send(&KM_session, command, req, rsp);
    if (err != KM_ERROR_OK) {
        ALOGE("Failed to send cmd %d err: %d", command, err);
        rsp->error = err;
    }
}

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
    if (message_version_ < 4) {
        // Pre-KeyMint we need to add TAG_CREATION_DATETIME if not provided by the caller.
        GenerateKeyRequest datedRequest(request.message_version);
        datedRequest.key_description = request.key_description;

        if (!request.key_description.Contains(TAG_CREATION_DATETIME)) {
            datedRequest.key_description.push_back(TAG_CREATION_DATETIME, java_time(time(NULL)));
        }

        ForwardCommand(KM_GENERATE_KEY, datedRequest, response);
    } else {
        ForwardCommand(KM_GENERATE_KEY, request, response);
    }
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
        AbortOperationRequest abort_req(message_version());
        AbortOperationResponse abort_rsp(message_version());

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
    GetHmacSharingParametersRequest request(message_version());
    GetHmacSharingParametersResponse response(message_version());
    ForwardCommand(KM_GET_HMAC_SHARING_PARAMETERS, request, &response);
    return response;
}

ComputeSharedHmacResponse AmlogicKeymaster::ComputeSharedHmac(
        const ComputeSharedHmacRequest& request) {
    ComputeSharedHmacResponse response(message_version());
    ForwardCommand(KM_COMPUTE_SHARED_HMAC, request, &response);
    return response;
}

VerifyAuthorizationResponse AmlogicKeymaster::VerifyAuthorization(
        const VerifyAuthorizationRequest& request) {
    VerifyAuthorizationResponse response(message_version());
    ForwardCommand(KM_VERIFY_AUTHORIZATION, request, &response);
    return response;
}

GetVersion2Response AmlogicKeymaster::GetVersion2(const GetVersion2Request& request) {
    GetVersion2Response response(message_version());
    ForwardCommand(KM_GET_VERSION_2, request, &response);
    return response;
}

DeviceLockedResponse AmlogicKeymaster::DeviceLocked(const DeviceLockedRequest& request) {
    DeviceLockedResponse response(message_version());
    ForwardCommand(KM_DEVICE_LOCKED, request, &response);
    return response;
}

EarlyBootEndedResponse AmlogicKeymaster::EarlyBootEnded() {
    EarlyBootEndedResponse response(message_version());
    ForwardCommand(KM_EARLY_BOOT_ENDED, EarlyBootEndedRequest(message_version()), &response);
    return response;
}

ConfigureVendorPatchlevelResponse AmlogicKeymaster::ConfigureVendorPatchlevel(
    const ConfigureVendorPatchlevelRequest& request) {
    ConfigureVendorPatchlevelResponse response(message_version());
    ForwardCommand(KM_CONFIGURE_VENDOR_PATCHLEVEL, request, &response);
    return response;
}

}  // namespace keymaster
