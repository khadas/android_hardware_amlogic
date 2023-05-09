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

#include "SystemControlClient.h"

#define UNUSED(x) (void)x

/*HIDL BASED SYSTEMCONTROL SERVICE PROXY.*/

static ::android::sp<::android::SystemControlClient> mSysCtrl = NULL;

static void load_sc_proxy() {
    if (mSysCtrl != NULL)
        return;

    mSysCtrl = android::SystemControlClient::getInstance();
    while (!mSysCtrl) {
        printf("tryGet system control daemon Service failed, sleep to wait.");
        usleep(200*1000);//sleep 200ms
        mSysCtrl = android::SystemControlClient::getInstance();
    };

}


int sc_read_bootenv(const char * key, std::string & val) {
    if (mSysCtrl == NULL) {
        load_sc_proxy();
        if (mSysCtrl == NULL) {
            printf("Load systemcontrol service failed.");
            return -EFAULT;
        }
    }


    char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, key);

    mSysCtrl->getBootEnv(ubootenv_name, val);

    if (val.empty()) {
        printf("sc_read_bootenv FAIL.");
        return -EFAULT;
    }

    return 0;
}

bool sc_set_bootenv(const char *key, const std::string &val) {

    if (mSysCtrl == NULL) {
        load_sc_proxy();
        if (mSysCtrl == NULL) {
            printf("Load systemcontrol service failed.");
            return false;
        }
    }

    char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, key);

    mSysCtrl->setBootEnv(ubootenv_name, val);
    return true;
}


