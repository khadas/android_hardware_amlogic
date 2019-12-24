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
#if AMLOGIC_MODIFY
    // Set boot parameters before configure
    SetBootParamsRequest setBootParamReq;
    SetBootParamsResponse setBootParamRsp;
    SetBootParams(setBootParamReq, &setBootParamRsp);
    if (setBootParamRsp.error != KM_ERROR_OK) {
        ALOGE("Failed to set boot params to keymaster %d", setBootParamRsp.error);
        //return -1;
    }
#endif
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
void AmlogicKeymaster::SetBootParams(SetBootParamsRequest& req, SetBootParamsResponse *rsp) {
#if 0
    std::string prop_val;
    // SHA256
    uint8_t bootkey_hash[32];
    uint8_t vbmeta_digest[32];

    const uint8_t empty_hash_bin[32] = {0x0};
    std::string empty_hash_hex_str(64, '0');

    req.os_version = GetOsVersion();
    req.os_patchlevel = GetOsPatchlevel();

    // device_locked
    prop_val = android::base::GetProperty("ro.boot.vbmeta.device_state", "unlocked");
    req.device_locked = !prop_val.compare("locked")? 1: 0;

    // verified_boot_state
    prop_val = android::base::GetProperty("ro.boot.verifiedbootstate", "red");
    req.verified_boot_state = KM_VERIFIED_BOOT_FAILED;
    if (!prop_val.compare("green"))
        req.verified_boot_state = KM_VERIFIED_BOOT_VERIFIED;
    else if (!prop_val.compare("yellow"))
        req.verified_boot_state = KM_VERIFIED_BOOT_SELF_SIGNED;
    else if (!prop_val.compare("orange"))
        req.verified_boot_state = KM_VERIFIED_BOOT_UNVERIFIED;
    else if (!prop_val.compare("red"))
        req.verified_boot_state = KM_VERIFIED_BOOT_FAILED;

    // verified_boot_key
    prop_val = android::base::GetProperty("ro.boot.vbmeta.bootkey_hash", empty_hash_hex_str);
    //ALOGE("bootkey_hash = %s", prop_val.c_str());
    //bootkey_hash = hex2bin(prop_val);
    if (HexToBytes(bootkey_hash, sizeof(bootkey_hash), prop_val))
        req.verified_boot_key.Reinitialize(bootkey_hash, sizeof(bootkey_hash));
    else
        req.verified_boot_key.Reinitialize(empty_hash_bin, sizeof(empty_hash_bin));

    // verified_boot_hash
    prop_val = android::base::GetProperty("ro.boot.vbmeta.digest", empty_hash_hex_str);
    //ALOGE("vbmeta.digest = %s",  prop_val.c_str());

    //vbmeta_digest = hex2bin(prop_val);
    if (HexToBytes(vbmeta_digest, sizeof(vbmeta_digest), prop_val))
        req.verified_boot_hash.Reinitialize(vbmeta_digest, sizeof(vbmeta_digest));
    else
        req.verified_boot_hash.Reinitialize(empty_hash_bin, sizeof(empty_hash_bin));
#endif
    ALOGE("send empty boot params");

    req.os_version = GetOsVersion();
    req.os_patchlevel = GetOsPatchlevel();

    ForwardCommand(KM_SET_BOOT_PARAMS, req, rsp);
}
#if 0
bool AmlogicKeymaster::NibbleValue(const char& c, uint8_t* value) {
    //CHECK(value != nullptr);
    switch (c) {
        case '0' ... '9':
            *value = c - '0';
            break;
        case 'a' ... 'f':
            *value = c - 'a' + 10;
            break;
        case 'A' ... 'F':
            *value = c - 'A' + 10;
            break;
        default:
            return false;
    }

    return true;
}

bool AmlogicKeymaster::HexToBytes(uint8_t* bytes, size_t bytes_len, const std::string& hex) {
    //CHECK(bytes != nullptr);

    if (hex.size() % 2 != 0) {
        return false;
    }
    if (hex.size() / 2 > bytes_len) {
        return false;
    }
    for (size_t i = 0, j = 0, n = hex.size(); i < n; i += 2, ++j) {
        uint8_t high;
        if (!NibbleValue(hex[i], &high)) {
            return false;
        }
        uint8_t low;
        if (!NibbleValue(hex[i + 1], &low)) {
            return false;
        }
        bytes[j] = (high << 4) | low;
    }
    return true;
}
std::string AmlogicKeymaster::hex2bin(std::string const& s) {
	//assert(s.length() % 2 == 0);
	std::string sOut;
	sOut.reserve(s.length()/2);

	std::string extract;
	for (std::string::const_iterator pos = s.begin(); pos<s.end(); pos += 2)
	{
		extract.assign(pos, pos+2);
		sOut.push_back(std::stoi(extract, nullptr, 16));
	}
	return sOut;
}
#endif
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
