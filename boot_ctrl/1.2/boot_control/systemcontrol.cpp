/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <systemcontrol.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>

//#include <utils/String16.h>


#define UNUSED(x) (void)x

#if PLATFORM_SDK_VERSION >=  26
#include <vendor/amlogic/hardware/systemcontrol/1.1/ISystemControl.h>
using ::vendor::amlogic::hardware::systemcontrol::V1_1::ISystemControl;
using ::vendor::amlogic::hardware::systemcontrol::V1_0::Result;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
#else
#include <ISystemControlService.h>
#include <binder/IServiceManager.h>
#endif

#define CHK_SC_PROXY() \
    if (gSC == NULL) { \
        load_sc_proxy(); \
        if (gSC == NULL) { \
            printf("Load systemcontrol service failed."); \
            return -EFAULT;\
        } \
    }

/*HIDL BASED SYSTEMCONTROL SERVICE PROXY.*/


static android::sp<ISystemControl> gSC = NULL;

struct SystemControlDeathRecipient : public android::hardware::hidl_death_recipient  {
    virtual void serviceDied(uint64_t cookie,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override {
        UNUSED(cookie);
        UNUSED(who);
        gSC = NULL;
    };
};
android::sp<SystemControlDeathRecipient> gSCDeathRecipient = NULL;

static void load_sc_proxy() {
    if (gSC != NULL)
        return;

    gSC = ISystemControl::tryGetService();
    while (!gSC) {
        printf("tryGet system control daemon Service failed, sleep to wait.");
        usleep(200*1000);//sleep 200ms
        gSC = ISystemControl::tryGetService();
    };

    gSCDeathRecipient = new SystemControlDeathRecipient();
    Return<bool> linked = gSC->linkToDeath(gSCDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        printf("Transaction error in linking to system service death: %s",
            linked.description().c_str());
    } else if (!linked) {
        printf("Unable to link to system service death notifications");
    } else {
        printf("Link to system service death notification successful");
    }
}


int sc_read_bootenv(const char * key, std::string & val) {
    CHK_SC_PROXY();

	char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, key);

    auto rtn = gSC->getBootEnv(ubootenv_name, [&val](
        const Result &ret, const hidl_string & retval) {
        if (Result::OK == ret) {
            val = retval.c_str();
        } else {
            val.clear();
        }
    });

    if (!rtn.isOk()) {
        printf("%s Fail", __func__);
        return -EFAULT;
    }
    if (val.empty()) {
        printf("sc_read_bootenv FAIL.");
        return -EFAULT;
    }

    return 0;
}

bool sc_set_bootenv(const char *key, const std::string &val) {
    CHK_SC_PROXY();

	char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, key);

    gSC->setBootEnv(ubootenv_name, val);
    return true;
}

