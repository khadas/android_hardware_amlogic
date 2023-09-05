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

#include <rkp_factory_extraction_lib.h>
#include <android/binder_manager.h>
#include <remote_prov/remote_prov_utils.h>

#include <android-base/properties.h>
#include <cutils/properties.h>
#include <tinyxml2.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>

#define KM_PROVISION_DEVID 0xFFFFA004

namespace keymaster {

using aidl::android::hardware::security::keymint::IRemotelyProvisionedComponent;
using aidl::android::hardware::security::keymint::remote_prov::jsonEncodeCsrWithBuild;

static bool initialize_flag = false;
void getCsrForInstance(void);

//static bool initialize_flag = false;

std::string wait_and_get_property(const char* prop) {
    std::string prop_value;
    /*while (!::android::base::WaitForPropertyCreation(prop))
        ;*/
    prop_value = ::android::base::GetProperty(prop, "" /* default */);
    return prop_value;
}

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

    getCsrForInstance();

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
    int error;
    if (initialize_flag == false) {
        initialize_flag = true;
        error = init_service_later();
    }

    keymaster_error_t err;
    err = aml_keymaster_send(&KM_session, command, req, rsp);
    if (err != KM_ERROR_OK) {
        ALOGE("Failed to send cmd %d err: %d", command, err);
        rsp->error = err;
    }
}

void AmlogicKeymaster::ForwardCommand2(enum keymaster_command command, const KeymasterMessage& req,
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
    ForwardCommand2(KM_CONFIGURE, request, response);
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

void AmlogicKeymaster::GenerateRkpKey(const GenerateRkpKeyRequest& request,
                                     GenerateRkpKeyResponse* response) {
    ForwardCommand(KM_GENERATE_RKP_KEY, request, response);
}

void AmlogicKeymaster::GenerateCsr(const GenerateCsrRequest& request,
                                  GenerateCsrResponse* response) {
    ForwardCommand(KM_GENERATE_CSR, request, response);
}

void AmlogicKeymaster::GenerateCsrV2(const GenerateCsrV2Request& request,
                                    GenerateCsrV2Response* response) {
    ForwardCommand(KM_GENERATE_CSR_V2, request, response);
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
    /* coverity[uninit_use:SUPPRESS] */
    ForwardCommand2(KM_GET_VERSION_2, request, &response);
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

GetRootOfTrustResponse AmlogicKeymaster::GetRootOfTrust(const GetRootOfTrustRequest& request) {
    GetRootOfTrustResponse response(message_version());
    ForwardCommand2(KM_GET_ROOT_OF_TRUST, request, &response);
    return response;
}

GetHwInfoResponse AmlogicKeymaster::GetHwInfo() {
    GetHwInfoResponse response(message_version());
    /* coverity[uninit_use:SUPPRESS] */
    ForwardCommand(KM_GET_HW_INFO, GetHwInfoRequest(message_version()), &response);
    return response;
}

void getCsrForInstance(void) {
    const std::vector<uint8_t> challenge = generateChallenge();
    constexpr char fullName[] =
        "android.hardware.security.keymint.IRemotelyProvisionedComponent/default";
    AIBinder* rkpAiBinder = AServiceManager_getService(fullName);
    ::ndk::SpAIBinder rkp_binder(rkpAiBinder);
    auto rkp_service = IRemotelyProvisionedComponent::fromBinder(rkp_binder);
    if (!rkp_service) {
        ALOGE("Unable to get binder object for: %s", fullName);
        return;
    }

    auto [request, errMsg] = getCsr("default", rkp_service.get(), false);
    if (!request) {
        ALOGE("Unable to build CSR for for: %s", fullName);
        return;
    }

    auto [json, error] = jsonEncodeCsrWithBuild("default", *request);
    if (!error.empty()) {
        ALOGE("Error JSON encoding");
        return;
    }

    ALOGD("getCsrForInstance output: %s", json.c_str());

    FILE *file = NULL;
    file = fopen("/mnt/vendor/factory/csrs/csrs.json", "w");
    if (file == NULL) {
        ALOGE("open/write /mnt/vendor/factory/csrs/csrs.json failed.");
        return;
    } else {
        ALOGD("open/write /mnt/vendor/factory/csrs/csrs.json success.");
    }
    fprintf(file, "%s",json.c_str());
    fclose(file);
    ALOGD("write /mnt/vendor/factory/csrs/csrs.json finish.");
}

TEEC_Result AmlogicKeymaster::ProvisionDevidBox(
                                    const uint8_t *key_buff,
                                    uint32_t key_size,
                                    bool *is_locked) {
    TEEC_Result res = TEEC_SUCCESS;
    TEEC_Operation op;
    uint32_t ret_orig;
    uint8_t send_buf[AMLOGIC_KEYMASTER_SEND_BUF_SIZE];
    int error;

    error = aml_keymaster_connect(&KM_context, &KM_session);
    if (error) {
        ALOGE("Failed to connect to amlogic keymaster %d", error);
        return error;
    }

    /*if (initialize_flag == false) {
        initialize_flag = true;
        error = init_service_later();
    }*/

    memcpy(send_buf, key_buff, key_size);
    memset(&op, 0, sizeof(op));

    op.params[1].tmpref.buffer = (void *)send_buf;
    op.params[1].tmpref.size = key_size;

    op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_VALUE_OUTPUT,
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_NONE,
            TEEC_NONE);

    res = TEEC_InvokeCommand(&KM_session, KM_PROVISION_DEVID, &op, &ret_orig);
    if (res != TEEC_SUCCESS) {
        ALOGE("Invoke cmd: %u failed with res(%x), ret_orig(%x), return(%d)\n",
                KM_PROVISION_DEVID, res, ret_orig, op.params[0].value.a);
    } else {
        *is_locked = op.params[0].value.a;
        ALOGE("ProvisionDevidBox already locked : %d", *is_locked);
    }
    return res;
}

keymaster_error_t AmlogicKeymaster::CreateIdAttestationXml(bool lock_xml) {
    AmlogicKeymaster obj;
    TEEC_Result res = TEEC_SUCCESS;
    bool is_locked;
    tinyxml2::XMLDocument doc;

    //root element
    tinyxml2::XMLElement* rootElement = doc.NewElement("ATTESTATION_ID");
    doc.InsertEndChild(rootElement);

    //child element SERIAL
    std::string serialno = wait_and_get_property("ro.serialno");
    ALOGI("serialno:%s", serialno.c_str());
    tinyxml2::XMLElement* serialElement = doc.NewElement("SERIAL");
    rootElement->InsertEndChild(serialElement);
    tinyxml2::XMLElement* serialValueElement = doc.NewElement("val");
    serialValueElement->SetText(serialno.c_str());
    serialElement->InsertEndChild(serialValueElement);

    //child element BRAND
    std::string brand = wait_and_get_property("ro.product.brand");
    ALOGI("brand:%s", brand.c_str());
    tinyxml2::XMLElement* brandElement = doc.NewElement("BRAND");
    rootElement->InsertEndChild(brandElement);
    tinyxml2::XMLElement* brandValueElement = doc.NewElement("val");
    brandValueElement->SetText(brand.c_str());
    brandElement->InsertEndChild(brandValueElement);

    //child element DEVICE
    std::string device = wait_and_get_property("ro.product.device");
    ALOGI("device:%s", device.c_str());
    tinyxml2::XMLElement* deviceElement = doc.NewElement("DEVICE");
    rootElement->InsertEndChild(deviceElement);
    tinyxml2::XMLElement* deviceValueElement = doc.NewElement("val");
    deviceValueElement->SetText(device.c_str());
    deviceElement->InsertEndChild(deviceValueElement);

    //child element PRODUCT
    std::string productname = wait_and_get_property("ro.product.name");
    ALOGI("productname:%s", productname.c_str());
    tinyxml2::XMLElement* productElement = doc.NewElement("PRODUCT");
    rootElement->InsertEndChild(productElement);
    tinyxml2::XMLElement* productValueElement = doc.NewElement("val");
    productValueElement->SetText(productname.c_str());
    productElement->InsertEndChild(productValueElement);

    //child element MANUFACTURER
    std::string manufacturer = wait_and_get_property("ro.product.manufacturer");
    ALOGI("manufacturer:%s", manufacturer.c_str());
    tinyxml2::XMLElement* manufacturerElement = doc.NewElement("MANUFACTURER");
    rootElement->InsertEndChild(manufacturerElement);
    tinyxml2::XMLElement* manufacturerValueElement = doc.NewElement("val");
    manufacturerValueElement->SetText(manufacturer.c_str());
    manufacturerElement->InsertEndChild(manufacturerValueElement);

    //child element MODEL
    std::string model = wait_and_get_property("ro.product.model");
    ALOGI("model:%s", model.c_str());
    tinyxml2::XMLElement* modelElement = doc.NewElement("MODEL");
    rootElement->InsertEndChild(modelElement);
    tinyxml2::XMLElement* modelValueElement = doc.NewElement("val");
    modelValueElement->SetText(model.c_str());
    modelElement->InsertEndChild(modelValueElement);

    if (lock_xml) {
        tinyxml2::XMLElement* lockElement = doc.NewElement("LOCK");
        rootElement->InsertEndChild(lockElement);
    }

    // save xml to string
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    std::string xmlString = printer.CStr();

    res = obj.ProvisionDevidBox(
                reinterpret_cast<const uint8_t*>(xmlString.c_str()),
                strlen(xmlString.c_str()), &is_locked);

    if (res != TEEC_SUCCESS)
        return KM_ERROR_ATTESTATION_IDS_NOT_PROVISIONED;
    return KM_ERROR_OK;
}

}  // namespace keymaster
