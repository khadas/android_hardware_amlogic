/* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: C++ file
 */

#define LOG_TAG "hdmicecd"
#define LOG_CEE_TAG "HdmiCecControl"
#define LOG_UNIT_TAG "hdmicecd"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "HdmiCecControl.h"
#include "HdmiCecCompat.h"
#include "HdmiCecUtils.h"

namespace android {

HdmiCecControl::MsgHandler::MsgHandler(HdmiCecControl *hdmiControl)
{
    mControl = hdmiControl;
}

HdmiCecControl::MsgHandler::~MsgHandler(){}

void HdmiCecControl::MsgHandler::handleMessage (CMessage &msg)
{
    LOGD("msg handler handle message type = %x", msg.mType);
    unsigned char body[CEC_MESSAGE_BODY_MAX_LENGTH] = {0};
    switch (msg.mType) {
        case HdmiCecControl::MsgHandler::MSG_GIVE_PHYSICAL_ADDRESS:
            if (mControl->mCecDevice.is_tv) {
                cec_message_t message;
                message.initiator = CEC_ADDR_TV;
                message.destination = (cec_logical_address_t)(msg.mpPara[0]);
                message.body[0] = CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS;
                message.length = 1;
                mControl->send(&message);
            }
            break;
        case HdmiCecControl::MsgHandler::MSG_REPORT_PHYSICAL_ADDRESS:
            cec_message_t message;
            message.initiator = (cec_logical_address_t)(msg.mpPara[0]);;
            message.destination = CEC_ADDR_BROADCAST;
            message.body[0] = CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS;
            message.body[1] = (mControl->mCecDevice.phy_addr >> 8) & 0xFF;
            message.body[2] = mControl->mCecDevice.phy_addr & 0xFF;
            message.body[3] = msg.mpPara[1] & 0xFF;;
            message.length = 4;
            mControl->send(&message);
            break;
        case HdmiCecControl::MsgHandler::MSG_USER_CONTROL_PRESSED:
            if (mControl->mCecDevice.is_tv) {
                cec_message_t message;
                message.initiator = (cec_logical_address_t)((msg.mpPara[0]) & 0xff);
                message.destination = (cec_logical_address_t)((msg.mpPara[1]) & 0xff);
                message.body[0] = CEC_MESSAGE_USER_CONTROL_PRESSED;
                message.body[1] = (msg.mpPara[2]) & 0xff;
                message.length = 2;
                mControl->send(&message);
            }
            break;
        case HdmiCecControl::MsgHandler::MSG_ONE_TOUCH_PLAY:
            mControl->sendOneTouchPlay(msg.mpPara[0]);
            break;
        case HdmiCecControl::MsgHandler::MSG_GET_MENU_LANGUAGE:
            body[0] = CEC_MESSAGE_GET_MENU_LANGUAGE;
            mControl->sendMessage(mControl->mCecDevice.playback_logical_addr, CEC_ADDR_TV, 1, body);
            break;
        case HdmiCecControl::MsgHandler::MSG_GIVE_OSD_NAME:
            body[0] = CEC_MESSAGE_GIVE_OSD_NAME;
            mControl->sendMessage(mControl->mCecDevice.playback_logical_addr, CEC_ADDR_TV, 1, body);
            break;
        case HdmiCecControl::MsgHandler::MSG_GIVE_DEVICE_VENDOR_ID:
            body[0] = CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID;
            mControl->sendMessage(mControl->mCecDevice.playback_logical_addr, CEC_ADDR_TV, 1, body);
            break;
        case HdmiCecControl::MsgHandler::MSG_PROCESS_CEC_WAKEUP:
            mControl->processCecWakeup();
            break;
        case HdmiCecControl::MsgHandler::MSG_MAY_SEND_SET_STREAM_PATH:
            mControl->maySendSetStreamPath();
            break;
    }
}

HdmiCecControl::HdmiCecEventHandler::HdmiCecEventHandler(HdmiCecControl *hdmiControl)
{
    mControl = hdmiControl;
}

HdmiCecControl::HdmiCecEventHandler::~HdmiCecEventHandler()
{
}

void HdmiCecControl::HdmiCecEventHandler::onCecEvent(int type) {
    //LOGD("onCecEvent type :%d", type);
    if (HDMI_EVENT_HOT_PLUG == type
        && ((mControl->mCecEvent & HDMI_EVENT_HOT_PLUG) != 0)) {
        mControl->checkConnectStatus();
    } else if (HDMI_EVENT_CEC_MESSAGE == type
        && ((mControl->mCecEvent & HDMI_EVENT_CEC_MESSAGE) != 0)) {
        mControl->readCecMessage();
    }
}

HdmiCecControl::HdmiCecControl(int event)
{
    LOGI("%s event:%d start ", __FUNCTION__, event);
    mCecDevice.is_tv = false;
    mCecDevice.is_playback = false;
    mCecDevice.device_types = NULL;
    mCecDevice.added_phy_addr = NULL;
    mCecDevice.total_device = 0;
    mCecDevice.phy_addr = INVALID_PHYSICAL_ADDRESS;
    mCecDevice.arc_port = 0x2;
    mCecDevice.run = true;
    mCecDevice.exited = false;
    mCecDevice.total_port = 0;
    mCecDevice.cec_connect_status = 0;
    mCecDevice.port_data = NULL;
    mCecDevice.playback_logical_addr = CEC_ADDR_BROADCAST;
    mCecDevice.is_cec_enabled = true;
    mCecDevice.is_cec_controlled = true;
    mCecDevice.hdmi_cfg_init = false;
    mCecEvent = event;
    getDeviceTypes();
    mCachedRoutingEvent = NULL;
    mVendorEventListener = NULL;
    mWakeEnabled = 1;
    mIsFirstBoot = true;

    int index = 0;
    mCecDevice.added_phy_addr = new int[CEC_ADDR_BROADCAST];
    for (index = 0; index < CEC_ADDR_BROADCAST; index++) {
        mCecDevice.added_phy_addr[index] = 0;
    }

    mCecDevice.vendor_ids = new int[CEC_ADDR_BROADCAST];
    for (index = 0; index < CEC_ADDR_BROADCAST; index++) {
        mCecDevice.vendor_ids[index] = 0;
    }

    mCecDevice.driver_fd = open(CEC_FILE, O_RDWR);
    if (mCecDevice.driver_fd < 0) {
        LOGE("can't open device. fd < 0");
        return;
    }
    if (getPropertyBoolean(PROPERTY_CEC_DEBUG, false)) {
        LOGD("openCecDevice debug open!");
        ioctl(mCecDevice.driver_fd, CEC_IOC_SET_DEBUG_EN, 2);
    }

    for (index = 0; index < mCecDevice.total_device; index++) {
        int deviceType =  mCecDevice.device_types[index];
        LOGD("set device type index : %d, type: %d", index, deviceType);
        ioctl(mCecDevice.driver_fd, CEC_IOC_SET_DEV_TYPE, deviceType);
    }

    getBootConnectStatus();

    mHdmiCecEventHandler = new HdmiCecEventHandler(this);
    mMonitor = new HdmiCecBusMonitor(mHdmiCecEventHandler);
    mMonitor->run("HdmiCecBusMonitor");

    mMsgHandler = sp<MsgHandler>::make(this);
    mMsgHandler->startMsgQueue();
    // Boot one touch play logic for aml products.
    /* coverity[uninit_member] */
    bootOneTouchPlay();
}

HdmiCecControl::~HdmiCecControl()
{
    mCecDevice.run = false;
    while (!mCecDevice.exited) {
        usleep(100 * 1000);
    }

    close(mCecDevice.driver_fd);

    delete [] mCecDevice.port_data;
    delete [] mCecDevice.device_types;
    delete [] mCecDevice.added_phy_addr;
    delete [] mCecDevice.vendor_ids;
    LOGI("%s, cec has closed.", __FUNCTION__);
}

/**
 * close cec device, reset some cec flags, and recycle some resources.
 */
int HdmiCecControl::closeCecDevice()
{
    return 0;
}

/**
 * initialize all cec flags when open cec devices, get the {@code fd} of cec devices,
 * and create a thread for cec working.
 * {@Return}  fd of cec device.
 */
int HdmiCecControl::openCecDevice()
{
    return mCecDevice.driver_fd;
}

int HdmiCecControl::readCecMessage()
{
    unsigned char msgBuf[CEC_MESSAGE_BODY_MAX_LENGTH];
    int r = -1;

    memset(msgBuf, 0, sizeof(msgBuf));
    //try to get a message from dev.
    r = readMessage(msgBuf, CEC_MESSAGE_BODY_MAX_LENGTH);
    if (r <= 1) {
        //ignore received ping messages
        return 0;
    }

    printCecMsgBuf((const char*)msgBuf, r);

    hdmi_cec_event_t event;
    memset(event.cec.body, 0, sizeof(event.cec.body));
    memcpy(event.cec.body, msgBuf + 1, r - 1);
    event.eventType = 0;
    event.cec.initiator = cec_logical_address_t((msgBuf[0] >> 4) & 0xf);
    event.cec.destination = cec_logical_address_t((msgBuf[0] >> 0) & 0xf);
    event.cec.length = r - 1;

    if (mCecDevice.is_cec_controlled || transferableInSleep((char*)msgBuf)) {
         event.eventType |= HDMI_EVENT_CEC_MESSAGE;
    }

    messageValidateAndHandle(&event);
    handleOTPMsg(&event);

    if (!mCecDevice.is_cec_enabled) {
        return 0;
    }

    if (mEventListener != NULL && event.eventType != 0) {
        mEventListener->onEventUpdate(&event);
    }
    return 0;
}

int HdmiCecControl::readMessage(unsigned char *buf, int msgCount)
{
    if (msgCount <= 0 || !buf) {
        return 0;
    }

    int ret = -1;
    /* maybe blocked at driver */
    ret = read(mCecDevice.driver_fd, buf, msgCount);
    if (ret < 0) {
        LOGE("read :%s failed, ret:%d\n", CEC_FILE, ret);
        return -1;
    }

    return ret;
}

void HdmiCecControl::getPortInfos(hdmi_port_info_t* list[], int* total)
{
    if (assertHdmiCecDevice())
        return;

    ioctl(mCecDevice.driver_fd, CEC_IOC_GET_PORT_NUM, total);

    LOGD("total port:%d", *total);
    if (*total > MAX_PORT)
        *total = MAX_PORT;

    if (NULL != mCecDevice.port_data)
        delete [] mCecDevice.port_data;
    mCecDevice.port_data = new hdmi_port_info[*total];
    if (!mCecDevice.port_data) {
        LOGE("alloc port_data failed");
        *total = 0;
        return;
    }

    ioctl(mCecDevice.driver_fd, CEC_IOC_GET_PORT_INFO, mCecDevice.port_data);

    for (int i = 0; i < *total; i++) {
        LOGI("portId: %d, type:%s, cec support:%d, arc support:%d, physical address:%x",
                mCecDevice.port_data[i].port_id,
                mCecDevice.port_data[i].type ? "output" : "input",
                mCecDevice.port_data[i].cec_supported,
                mCecDevice.port_data[i].arc_supported,
                mCecDevice.port_data[i].physical_address);

        if (mCecDevice.port_data[i].arc_supported) {
            mCecDevice.arc_port = mCecDevice.port_data[i].port_id;
            LOGI("arc port:%d", mCecDevice.arc_port);
            char arcPort[128];
            sprintf(arcPort, "%d", mCecDevice.arc_port);
            setProperty(PROPERTY_ARC_PORT, arcPort);
        }
    }

    *list = mCecDevice.port_data;
    mCecDevice.total_port = *total;
}

int HdmiCecControl::addLogicalAddress(cec_logical_address_t address)
{
    if (assertHdmiCecDevice())
        return -EINVAL;

    if (isSourceDevice(address)) {
        mCecDevice.playback_logical_addr = address;
    }

    int res = ioctl(mCecDevice.driver_fd, CEC_IOC_ADD_LOGICAL_ADDR, address);
    LOGI("addr:%x, allocate result:%d\n", mCecDevice.playback_logical_addr, res);
    char logicalAddress[128];
    sprintf(logicalAddress, "%x", mCecDevice.playback_logical_addr);
    setProperty(PROPERTY_LOGICAL_ADDRESS, logicalAddress);

    onAddressAllocated(address);
    return res;
}

void HdmiCecControl::onAddressAllocated(int logicalAddress)
{
    LOGD("onAddressAllocated:%d", logicalAddress);
    #ifdef NO_USE_DROID_PATCH
    getDeviceExtraInfo(1);
    #endif
}

void HdmiCecControl::clearLogicaladdress()
{
    LOGI("%s", __FUNCTION__);
    if (assertHdmiCecDevice())
        return;

    ioctl(mCecDevice.driver_fd, CEC_IOC_CLR_LOGICAL_ADDR, 0);
}

void HdmiCecControl::setOption(int flag, int value)
{
    if (assertHdmiCecDevice())
        return;

    int ret = -1;
    switch (flag) {
        case HDMI_OPTION_ENABLE_CEC:
            mCecDevice.is_cec_enabled = (value == 1);
            if (mCecDevice.is_cec_enabled) {
                mCecDevice.is_cec_controlled = true;
            }
            if (handleCecEnabled(value)) {
                return;
            }
            ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_ENABLE_CEC, value);
            break;

        case HDMI_OPTION_WAKEUP:
            mWakeEnabled = value;
            ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_WAKEUP, value);
            break;

        case HDMI_OPTION_SYSTEM_CEC_CONTROL:
            ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_SYS_CTRL, value);
            mCecDevice.is_cec_controlled = (value == 1);
            // Save the power state for hdmi connection service
            setProperty(PROPERTY_POWER_STATE,
                mCecDevice.is_cec_controlled ? CEC_STATE_ENABLED : CEC_STATE_UNABLED);
            if (!mCecDevice.hdmi_cfg_init && mCecDevice.is_cec_controlled) {
                LOGI("%s boot initialize hdmi cec config!", __FUNCTION__);
                ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_ENABLE_CEC, value);
                mCecDevice.hdmi_cfg_init = true;
            }
            /* removed for the tv compat logic has been moved to driver.
            if (mCecDevice.is_cec_controlled) {
                initCecWakeupInfo();
            }
            */

            updateActiveStateForFramework();
            break;

        case HDMI_OPTION_SET_LANG:
            ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_SET_LANG, value);
            break;

        default:
            break;
    }
    LOGD("%s, flag:0x%x, value:0x%x, ret:%d, is_cec_controlled:%x", __FUNCTION__,
                        flag, value, ret, mCecDevice.is_cec_controlled);
}

void HdmiCecControl::setAudioReturnChannel(int port, bool flag)
{
    if (assertHdmiCecDevice())
        return;

    int ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_ARC_ENABLE, flag);
    LOGD("%s, port id:%d, flag:%x, ret:%d\n", __FUNCTION__, port, flag, ret);
}

bool HdmiCecControl::isConnected(int port)
{
    if (assertHdmiCecDevice())
        return false;

    int status = -1, ret;
    /* use status pass port id */
    status = port;
    ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_CONNECT_STATUS, &status);
    if (ret)
        return false;
    LOGD("%s, port:%d, connected:%s", __FUNCTION__, port, status ? "yes" : "no");
    return status;
}

int HdmiCecControl::getVersion(int* version)
{
    if (assertHdmiCecDevice())
        return -1;

    int ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_VERSION, version);
    LOGD("%s, version:%x, ret = %d", __FUNCTION__, *version, ret);
    return ret;
}

int HdmiCecControl::getVendorId(uint32_t* vendorId)
{
    if (assertHdmiCecDevice())
        return -1;

    /*int ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_VENDOR_ID, vendorId);
    LOGD("%s, vendorId: %x, ret = %d", __FUNCTION__, *vendorId, ret);
    if (*vendorId == 0 || *vendorId == VENDOR_ID_DEFAULT) {
        LOGD("use amlogic vendor id");
        *vendorId = VENDOR_ID_AML;
    }
    return ret;*/
    char value[PROPERTY_VALUE_MAX] = {0};
    // "1877008" is decimal value of the amlogic vendor id
    getProperty(PROPERTY_VENDOR_ID, value, "1877008");
    char *endptr;
    long int vendorIdValue = strtol(value, &endptr, 10);
    if (*endptr != '\0') {
        // Conversion was not successful
        LOGD("Conversion failed");
        return -1;
    }
    *vendorId = (unsigned int)vendorIdValue;
    //*vendorId = atoi(value);
    LOGD("%s, vendorId: 0x%X", __FUNCTION__, *vendorId);
    return 0;
}

int HdmiCecControl::getPhysicalAddress(uint16_t* addr)
{
    if (assertHdmiCecDevice())
        return -EINVAL;

    if (mCecDevice.is_tv) {
        // no need to update tv's address, it's persistent 0.
        *addr = 0;
        return 0;
    }

    int ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_PHYSICAL_ADDR, addr);
    LOGD("%s, physical addr: %x, last pa: %x, ret = %d", __FUNCTION__,
        *addr, mCecDevice.phy_addr, ret);

    // In cases in which device sleeps the physical address could be 0 for a playback.
    // It's not acceptable. We can use the last valid value in this case.
    if (*addr != 0 && *addr != INVALID_PHYSICAL_ADDRESS) {
        // Save the valid one
        mCecDevice.phy_addr = *addr;
    } else {
        *addr = mCecDevice.phy_addr;
    }
    return ret;
}

int HdmiCecControl::sendMessage(const cec_message_t* message)
{
    int ret = -1;
    int retry = 0;
    if (assertHdmiCecDevice()) {
        LOGE("sendMessage not valid cec device!");
        return HDMI_RESULT_FAIL;
    }

    if (!mCecDevice.is_cec_enabled) {
        LOGE("sendMessage cec not enabled!");
        return HDMI_RESULT_FAIL;
    }

    if (preHandleOfSend(message) < 0) {
        return HDMI_RESULT_SUCCESS;
    }
    // As there is no retry in driver and the limit of retry count is 5, we could retry
    // twice in hal and there could be in total 4 times plus android's retry twice.
    do {
        ret = send(message);
    } while(message->length != 0
            && (ret != HDMI_RESULT_SUCCESS)
            && (++retry < SEND_MESSAGE_RETRY_HAL));

    postHandleOfSend(message, ret);
    return ret;
}

int HdmiCecControl::send(const cec_message_t* message)
{
    unsigned char msgBuf[CEC_MESSAGE_BODY_MAX_LENGTH];
    int ret = -1;
    memset(msgBuf, 0, sizeof(msgBuf));
    msgBuf[0] = ((message->initiator & 0xf) << 4) | (message->destination & 0xf);
    memcpy(msgBuf + 1, message->body, message->length);
    ret = write(mCecDevice.driver_fd, msgBuf, message->length + 1);
    printCecMessage(message, ret);
    return ret;
}

bool HdmiCecControl::assertHdmiCecDevice()
{
    return mCecDevice.driver_fd < 0;
}

void HdmiCecControl::setEventObserver(const sp<HdmiCecEventListener> &eventListener)
{
    mEventListener = eventListener;
}

void HdmiCecControl::setVendorEventObserver(const sp<HdmiCecEventListener> &eventListener)
{
    LOGD("%s", __FUNCTION__);
    mVendorEventListener = eventListener;
    if (mCachedRoutingEvent != NULL) {
        LOGD("Send cached routing message");
        mVendorEventListener->onEventUpdate(mCachedRoutingEvent);
    }
    if (!mCecDevice.is_cec_enabled) {
        handleCecEnabled(0);
        int ret = ioctl(mCecDevice.driver_fd, CEC_IOC_SET_OPTION_ENABLE_CEC, 1);
        LOGD("Enable cec driver again with vendor callback is set. ret:%d", ret);
    }
}

int HdmiCecControl::sendMessage(int source, int destination, int length, unsigned char body[])
{
    cec_message_t message;
    message.initiator = (cec_logical_address_t)(source & 0xf);
    message.destination = (cec_logical_address_t)(destination & 0xf);
    message.length = length;
    memcpy(message.body, body, length);
    return send(&message);
}

void HdmiCecControl::getDeviceTypes() {
    int index = 0;
    char value[PROPERTY_VALUE_MAX] = {0};
    const char * split = ",";
    char * type;
    mCecDevice.device_types = new int[DEV_TYPE_VIDEO_PROCESSOR];
    getProperty(PROPERTY_DEVICE_TYPE, value, "4");
    type = strtok(value, split);
    //mCecDevice.device_types[index] = atoi(type);
    char *endptr;
    if (type == NULL) {
        return;
    }
    long int deviceTypeValue = strtol(type, &endptr, 10);
    if (*endptr != '\0') {
        // Conversion was not successful
        LOGD("Conversion failed");
        return;
    }
    mCecDevice.device_types[index] = (unsigned int)deviceTypeValue;
    while (type != NULL) {
        type = strtok(NULL,split);
        if (type != NULL) {
            //mCecDevice.device_types[++index] = atoi(type);
            deviceTypeValue = strtol(type, &endptr, 10);
            if (*endptr != '\0') {
                // Conversion was not successful
                LOGD("Conversion failed");
                return;
            }
            mCecDevice.device_types[++index] = (unsigned int)deviceTypeValue;
        }
    }
    mCecDevice.total_device = index + 1;
    //index = 0;
    for (index = 0; index < mCecDevice.total_device; index++) {
        if (mCecDevice.device_types[index] == DEV_TYPE_TV) {
            mCecDevice.is_tv = true;
        } else if (mCecDevice.device_types[index] == DEV_TYPE_PLAYBACK) {
            mCecDevice.is_playback = true;
        }
        LOGI("mCecDevice.device_types[%d]: %d", index, mCecDevice.device_types[index]);
    }
}

/**
 * get connect status when boot
 */
void HdmiCecControl::getBootConnectStatus()
{
    unsigned int total, i;
    int port, connect, ret;

    ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_PORT_NUM, &total);
    LOGD("total port:%d, ret:%d", total, ret);
    if (ret < 0)
        return ;

    if (total > MAX_PORT)
        total = MAX_PORT;
    hdmi_port_info_t  *portData = NULL;
    portData = new hdmi_port_info[total];
    if (!portData) {
        LOGE("alloc port_data failed");
        return;
    }
    ioctl(mCecDevice.driver_fd, CEC_IOC_GET_PORT_INFO, portData);
    mCecDevice.cec_connect_status = 0;
    for (i = 0; i < total; i++) {
        port = portData[i].port_id;
        connect = port;
        ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_CONNECT_STATUS, &connect);
        if (ret) {
            LOGD("get port %d connected status failed, ret:%d", i, ret);
            continue;
        }
        mCecDevice.cec_connect_status |= ((connect ? 1 : 0) << port);
    }
    delete [] portData;
    LOGD("cec_connect_status: %d", mCecDevice.cec_connect_status);
}

void* HdmiCecControl::__threadLoop(void *user)
{
    HdmiCecControl *const self = static_cast<HdmiCecControl *>(user);
    self->threadLoop();
    return 0;
}

void HdmiCecControl::threadLoop()
{
    unsigned char msgBuf[CEC_MESSAGE_BODY_MAX_LENGTH];
    hdmi_cec_event_t event;
    int r = -1;

    while (mCecDevice.driver_fd < 0) {
        usleep(1000 * 1000);
        mCecDevice.driver_fd = open(CEC_FILE, O_RDWR);
    }
    LOGI("file open ok, fd = %d.", mCecDevice.driver_fd);

    while (mCecDevice.run) {
        if (!mCecDevice.is_cec_enabled) {
            usleep(1000 * 1000);
            continue;
        }
        checkConnectStatus();

        memset(msgBuf, 0, sizeof(msgBuf));
        //try to get a message from dev.
        r = readMessage(msgBuf, CEC_MESSAGE_BODY_MAX_LENGTH);
        if (r <= 1)//ignore received ping messages
            continue;

        printCecMsgBuf((const char*)msgBuf, r);

        memset(event.cec.body, 0, sizeof(event.cec.body));
        memcpy(event.cec.body, msgBuf + 1, r - 1);
        event.eventType = 0;
        event.cec.initiator = cec_logical_address_t((msgBuf[0] >> 4) & 0xf);
        event.cec.destination = cec_logical_address_t((msgBuf[0] >> 0) & 0xf);
        event.cec.length = r - 1;

        if (mCecDevice.is_cec_controlled || transferableInSleep((char*)msgBuf)) {
             event.eventType |= HDMI_EVENT_CEC_MESSAGE;
        }

        messageValidateAndHandle(&event);
        handleOTPMsg(&event);
        if (mEventListener != NULL && event.eventType != 0) {
            mEventListener->onEventUpdate(&event);
        }
    }
    //LOGE("thread end.");
    mCecDevice.exited = true;
}

bool HdmiCecControl::isSourceDevice(int logicalAddress)
{
    bool res = false;
    switch (logicalAddress) {
        case CEC_ADDR_RECORDER_1:
        case CEC_ADDR_RECORDER_2:
        case CEC_ADDR_TUNER_1:
        case CEC_ADDR_PLAYBACK_1:
        case CEC_ADDR_TUNER_2:
        case CEC_ADDR_TUNER_3:
        case CEC_ADDR_PLAYBACK_2:
        case CEC_ADDR_RECORDER_3:
        case CEC_ADDR_TUNER_4:
        case CEC_ADDR_PLAYBACK_3:{
            res = true;
            break;
        }
        default:
            break;
    }
    return res;
}

/**
 * Check if still transfer it when mCecDevice.is_cec_controlled is false
*/

bool HdmiCecControl::transferableInSleep(char *msgBuf)
{
    switch (msgBuf[1]) {
        case CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID:
        case CEC_MESSAGE_GIVE_OSD_NAME:
        case CEC_MESSAGE_GIVE_DEVICE_POWER_STATUS:
        case CEC_MESSAGE_REPORT_POWER_STATUS:
        case CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS:
        case CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS:{
            return true;
        }
    }
    // Allow all messages to be sent to android in sleep.
    return true;
}

/**
 * Check if received a valid message.
* @param msgBuf is a message Buf
*   msgBuf[1]: message type
*   msgBuf[2]-msgBuf[n]: message para
* @param len is message length
* @param deviceType is type of device
*/

void HdmiCecControl::messageValidateAndHandle(hdmi_cec_event_t* event)
{
    CMessage msg;
    int initiator = (int)(event->cec.initiator);
    int destination = (int)(event->cec.destination);
    int devPhyAddr = 0;
    int opcode = event->cec.body[0];

    switch (opcode) {
        case CEC_MESSAGE_DEVICE_VENDOR_ID:
            mCecDevice.vendor_ids[initiator] = ((event->cec.body[1] & 0xff) << 16)
                + ((event->cec.body[2] & 0xff) << 8) +  (event->cec.body[3] & 0xff);
            break;
        default:
            break;
    }
    if (mCecDevice.is_tv) {
        switch (opcode) {
            case CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS:
                devPhyAddr = ((event->cec.body[1] & 0xff) << 8) +  (event->cec.body[2] & 0xff);
                // Compat code: not accept a device other than tv takes physical address 0.
                if (event->cec.body[1] == 0) {
                    msg.mType = HdmiCecControl::MsgHandler::MSG_GIVE_PHYSICAL_ADDRESS;
                    msg.mDelayMs = DELAY_TIMEOUT_MS/5;
                    msg.mpPara[0] = initiator;
                    mMsgHandler->removeMsg(msg);
                    mMsgHandler->sendMsg(msg);
                    event->eventType = 0;
                    LOGE("received message: %02x validate fail and drop", event->cec.body[0]);
                } else {
                    // Compat code: no transfer the same <Report Physical Address> if it has been done.
                    // Preserve this code for projects like amazon fireos.
                    // Check if we have added the specific address, we should not allow the same
                    // devices reports too many messages so that tv might trigger so many NewDeviceActions.
                    // This works mostly in senarios where the connected box or audio system might trigger
                    // a hotplug out and in event when it goes to sleep.
                    AutoMutex _l(mLock);
                    if (mCecDevice.added_phy_addr[initiator] == devPhyAddr) {
                        LOGE("receive report physical address message but we have already added it.");
                        event->eventType = 0;
                        return;
                    }

                    mCecDevice.added_phy_addr[initiator] = devPhyAddr;
                    /*
                    // Compat code: Process uboot cec wake up and forge missed otp messages.
                    // aml cec driver forging messages may have height risk.
                    if (mCecDevice.cec_wake_status.processed
                        && mCecDevice.cec_wake_status.wake_device_logical_addr == initiator
                        && mCecDevice.cec_wake_status.wake_device_phy_addr == devPhyAddr) {
                        LOGI("cec wake up device!");
                        msg.mType = HdmiCecControl::MsgHandler::MSG_PROCESS_CEC_WAKEUP;
                        mMsgHandler->sendMsg(msg);
                    }
                    */
                }
                break;
            case CEC_MESSAGE_ROUTING_CHANGE:
                [[fallthrough]];
            case CEC_MESSAGE_ROUTING_INFORMATION:
                if (destination != CEC_ADDR_BROADCAST) {
                    LOGD("received message: %02x validate fail and drop", opcode);
                    event->eventType = 0;
                }
                break;
            default:
                break;
        }
    } else if (mCecDevice.is_playback) {
        switch (opcode) {
            #ifdef NO_USE_DROID_PATCH
            case CEC_MESSAGE_ROUTING_CHANGE:
                [[fallthrough]];
            case CEC_MESSAGE_ROUTING_INFORMATION:
                // build <set stream path> message for routing change message
                // in case java framwork dos not process this message
                event->cec.body[0] = CEC_MESSAGE_SET_STREAM_PATH;
                event->cec.body[1] = event->cec.body[event->cec.length - 2];
                event->cec.body[2] = event->cec.body[event->cec.length - 1];
                event->cec.length = event->cec.length - 2;
                printCecEvent(event);
                LOGD("replace <Routing Change> with <Set Stream Path>");
                break;
            #endif
            case CEC_MESSAGE_SET_MENU_LANGUAGE:
                handleSetMenuLanguage(event);
                break;
        }
    }
}

void HdmiCecControl::handleOTPMsg(hdmi_cec_event_t* event)
{
    if (event->cec.body[0] == CEC_MESSAGE_ACTIVE_SOURCE) {
        AutoMutex _l(mLock);
        mCecDevice.active_logical_addr = (int)(event->cec.initiator);
        mCecDevice.active_routing_path = ((event->cec.body[1] & 0xff) << 8) + (event->cec.body[2] & 0xff);
    }

    // Update active state in the case of receiving routing messages.
    updateActiveState(&(event->cec), true);
}

void HdmiCecControl::handleSetMenuLanguage(hdmi_cec_event_t* event)
{
    if (event->cec.initiator != CEC_ADDR_TV) {
        event->eventType = 0;
        LOGE("handleSetMenuLanguage message from no tv is not accepted");
        return;
    }

    int language = ((event->cec.body[1] & 0xff) << 16) + ((event->cec.body[2] & 0xff) << 8)
                    + (event->cec.body[3] & 0xff);
    LOGD("handleSetMenuLanguage tv vendorId %x lang %x", mCecDevice.vendor_ids[CEC_ADDR_TV], language);
    // Compat for the vendors which use different code for "zho"
    compatLangIfneeded(mCecDevice.vendor_ids[CEC_ADDR_TV], language, event);
}

void HdmiCecControl::getDeviceExtraInfo(int flag)
{
    LOGD("get info from tv");
    CMessage msg;
    // Get tv osd name
    msg.mType = HdmiCecControl::MsgHandler::MSG_GIVE_OSD_NAME;
    msg.mDelayMs = DELAY_TIMEOUT_MS * flag;
    mMsgHandler->removeMsg(msg);
    mMsgHandler->sendMsg(msg);
    // Get tv vendor id
    msg.mType = HdmiCecControl::MsgHandler::MSG_GIVE_DEVICE_VENDOR_ID;
    msg.mDelayMs = DELAY_TIMEOUT_MS * flag;
    mMsgHandler->removeMsg(msg);
    mMsgHandler->sendMsg(msg);
    // Get tv menu language
    msg.mType = HdmiCecControl::MsgHandler::MSG_GET_MENU_LANGUAGE;
    msg.mDelayMs = DELAY_TIMEOUT_MS * flag;
    mMsgHandler->removeMsg(msg);
    mMsgHandler->sendMsg (msg);
}

void HdmiCecControl::bootOneTouchPlay() {
    if ((mCecEvent & HDMI_EVENT_CEC_MESSAGE) == 0) {
        // Hdmi connection hal could also use this cpp.
        return;
    }
    if (!mCecDevice.is_playback || mCecDevice.is_audio_system) {
        // only do this for a single playback device.
        return;
    }
    if (!getPropertyBoolean(PROPERTY_BOOT_OTP, false)
            || !getPropertyBoolean(PROPERTY_ONE_TOUCH_PLAY, true)) {
        LOGD("No need for boot one touch play as switch is disabled");
        return;
    }
    char bootReason[PROPERTY_VALUE_MAX] = {0};
    memset(bootReason, '\0', PROPERTY_VALUE_MAX);
    if (property_get(PROPERTY_BOOT_REASON, bootReason, "") > 0) {
        LOGD("%s with prop:%s boot reason:%s", __FUNCTION__, PROPERTY_BOOT_REASON, bootReason);
    } else {
        LOGE("%s failed to get boot reason", __FUNCTION__);
    }

    if (strcmp(bootReason, BOOT_REASON_COLD) != 0
        && strcmp(bootReason, BOOT_REASON_SHUTDOWN) != 0) {
        // Don't do this in any reboot scenarios except cold boot.
        // It will make sure that no one touch play is started in cts and ota cases.
        LOGD("It's not cold or shutdown boot");
        return;
    }

    mBootOtpCount = 0;
    getPhysicalAddress(&mCecDevice.phy_addr);

    char address[PROPERTY_VALUE_MAX] = {0};
    memset(address, '\0', PROPERTY_VALUE_MAX);
    getProperty(PROPERTY_LOGICAL_ADDRESS, address, "4");
    int logicalAddress = atoi(address);
    if (logicalAddress == 0) {
        logicalAddress = 4;
    }
    LOGD("%s with logical address:%x physical address:%x", __FUNCTION__,
            logicalAddress, mCecDevice.phy_addr);
    // allocate logical address first.
    addLogicalAddress(cec_logical_address_t(logicalAddress));

    // report physical address
    CMessage reportPhysicalAddress;
    reportPhysicalAddress.mType = HdmiCecControl::MsgHandler::MSG_REPORT_PHYSICAL_ADDRESS;
    reportPhysicalAddress.mDelayMs = 0;
    reportPhysicalAddress.mpPara[0] = logicalAddress;
    reportPhysicalAddress.mpPara[1] = CEC_DEVICE_PLAYBACK;
    mMsgHandler->sendMsg(reportPhysicalAddress);


    CMessage message;
    message.mType = HdmiCecControl::MsgHandler::MSG_ONE_TOUCH_PLAY;
    message.mDelayMs = 0;
    message.mpPara[0] = logicalAddress;
    mMsgHandler->removeMsg(message);
    mMsgHandler->sendMsg(message);
}

void HdmiCecControl::sendOneTouchPlay(int logicalAddress) {
    LOGD("send one touch play message %x count:%x", logicalAddress, mBootOtpCount);

    cec_message_t message;
    message.initiator = (cec_logical_address_t)logicalAddress;
    message.destination = CEC_ADDR_TV;
    message.length = 1;
    message.body[0] = CEC_MESSAGE_TEXT_VIEW_ON;
    send(&message);

    uint16_t physicalAddress = mCecDevice.phy_addr;
    if (physicalAddress == INVALID_PHYSICAL_ADDRESS
        || physicalAddress == 0) {
        physicalAddress = 0x1000;
    }

    message.destination = CEC_ADDR_BROADCAST;
    message.length = 4;
    message.body[0] = CEC_MESSAGE_ACTIVE_SOURCE;
    message.body[1] = (physicalAddress >> 8) & 0xff;
    message.body[2] = physicalAddress & 0xff;
    message.body[3] = logicalAddress & 0xff;
    send(&message);

    // Update active state in case of boot one touch play.
    updateActiveState(&message, false);

    if (++mBootOtpCount < BOOT_OTP_RETRY_COUNT) {
        CMessage message;
        message.mType = HdmiCecControl::MsgHandler::MSG_ONE_TOUCH_PLAY;
        message.mDelayMs = DELAY_TRANSMISSION_TIMEOUT;
        message.mpPara[0] = logicalAddress;
        mMsgHandler->removeMsg(message);
        mMsgHandler->sendMsg(message);
    }

    AutoMutex _l(mLock);
    mCecDevice.active_logical_addr = logicalAddress;
    mCecDevice.active_routing_path = physicalAddress;
}

void HdmiCecControl::checkConnectStatus()
{
    unsigned int prevStatus, bit;
    int i, port, connect, ret;
    hdmi_cec_event_t event;

    prevStatus = mCecDevice.cec_connect_status;
    for (i = 0; i < mCecDevice.total_port && mCecDevice.port_data != NULL; i++) {
        port = mCecDevice.port_data[i].port_id;
        if (mCecDevice.total_port == 1 && mCecDevice.device_types[0] == DEV_TYPE_PLAYBACK) {
            //playback for tx hotplug para is always 0
            port = 0;
        }
        connect = port;
        ret = ioctl(mCecDevice.driver_fd, CEC_IOC_GET_CONNECT_STATUS, &connect);
        if (ret) {
            LOGE("get port %d connected status failed, ret:%d\n", mCecDevice.port_data[i].port_id, ret);
            continue;
        }
        bit = prevStatus & (1 << port);
        if (bit ^ ((connect ? 1 : 0) << port)) {//connect status has changed
            bool isWake = getPropertyBoolean(PROPERTY_POWER_STATE, true);
            LOGI("Hotplug event port:%x, now:%x, prevStatus:%x power:%d",
                    mCecDevice.port_data[i].port_id, connect, prevStatus, isWake);
            if (mEventListener != NULL && mCecDevice.is_cec_enabled && isWake) {
                event.eventType = HDMI_EVENT_HOT_PLUG;
                event.hotplug.connected = connect;
                event.hotplug.port_id = mCecDevice.port_data[i].port_id;
                mEventListener->onEventUpdate(&event);
            }
            prevStatus &= ~(bit);
            prevStatus |= ((connect ? 1 : 0) << port);
        }
    }
    mCecDevice.cec_connect_status = prevStatus;
}

int HdmiCecControl::preHandleOfSend(const cec_message_t* message)
{
    int ret = 0;
    //int para = 0;
    int dest = message->destination;
    int opcode = message->body[0] & 0xff;
    switch (opcode) {
        case CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID: {
            mCecDevice.vendor_ids[dest] = 0;
            break;
        }
        case CEC_MESSAGE_ROUTING_CHANGE:
        case CEC_MESSAGE_SET_STREAM_PATH: {
            //para = ((message->body[message->length - 2] & 0xff) << 8) + (message->body[message->length - 1] & 0xff);
            /*
            // Filter all routing messages if cec wake up work is not finished.
            if (mCecDevice.cec_wake_status.processed && para != mCecDevice.cec_wake_status.wake_device_phy_addr) {
                LOGE("%s filter routing message during cec wake up:0x%x para:0x%2x", __FUNCTION__, opcode, para);
                return -1;
            }
            */
            break;
        }
        case CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS: {
            // TV needs to reset the saved address if we are going to check it again. Or else if the device has
            // reported its address before tv tries to query it in DeviceDiscoveryAction, the filter logic may
            // filter it and cause the DeviceDiscoveryAction fails.
            AutoMutex _l(mLock);
            mCecDevice.added_phy_addr[dest] = 0;
            break;
        }
        case CEC_MESSAGE_TEXT_VIEW_ON:
        case CEC_MESSAGE_IMAGE_VIEW_ON:
        case CEC_MESSAGE_ACTIVE_SOURCE: {
            if (opcode == CEC_MESSAGE_ACTIVE_SOURCE) {
                // Update active state in case of common one touch play.
                updateActiveState(message, false);
            }
            // The android framework has not taken this senario into consideration, we have to do the supplement
            // filter work in hal. It works when the playback powers down just after it wakes up.
            if (mCecDevice.is_playback && !mCecDevice.is_cec_controlled) {
                LOGD("filter One Touch Play message when playback goes to sleep.");
                ret = -1;
            }
            break;
        }
        default:
            break;
    }
    return ret;
}

int HdmiCecControl::postHandleOfSend(const cec_message_t* message, int result)
{
    int ret = 0;
    int dest, len;
    len = message->length;
    dest = message->destination;
    AutoMutex _l(mLock);
    if (len == 0 && result == NACK && mCecDevice.added_phy_addr[dest] != 0) {
        mCecDevice.added_phy_addr[dest] = 0;
        LOGE("Polling %d fail, reset mCecDevice.added_phy_addr[%d].", dest, dest);
    }
    return ret;
}

void HdmiCecControl::turnOnDevice(int logicalAddress)
{
    cec_message_t message;
    message.initiator = CEC_ADDR_TV;
    message.destination = (cec_logical_address_t)logicalAddress;
    // Power
    message.body[0] = CEC_MESSAGE_USER_CONTROL_PRESSED;
    message.body[1] = (CEC_KEYCODE_POWER_ON_FUNCTION & 0xff);
    message.length = 2;
    send(&message);
    message.body[0] = CEC_MESSAGE_USER_CONTROL_RELEASED;
    message.length = 1;
    send(&message);
    LOGD("send wakeUp message.");
}

void HdmiCecControl::processCecWakeup()
{
    LOGD("%s logical addr:%x, physical addr:%x", __FUNCTION__,
            mCecDevice.cec_wake_status.wake_device_logical_addr,
            mCecDevice.cec_wake_status.wake_device_phy_addr);
    if (isSourceDevice(mCecDevice.cec_wake_status.wake_device_logical_addr)) {
        hdmi_cec_event_t event;
        event.eventType = HDMI_EVENT_CEC_MESSAGE;
        event.cec.initiator = (cec_logical_address_t)mCecDevice.cec_wake_status.wake_device_logical_addr;
        event.cec.destination = CEC_ADDR_BROADCAST;
        event.cec.length = 3;
        event.cec.body[0] = CEC_MESSAGE_ACTIVE_SOURCE;
        event.cec.body[1] = (mCecDevice.cec_wake_status.wake_device_phy_addr >> 8) & 0xff;
        event.cec.body[2] = mCecDevice.cec_wake_status.wake_device_phy_addr & 0xff;
        if (mEventListener != NULL) {
            LOGD("%s receives delayed active source message from uboot.", __FUNCTION__);
            mEventListener->onEventUpdate(&event);
        }
    }
    mCecDevice.cec_wake_status.processed = false;
}

void HdmiCecControl::initCecWakeupInfo()
{
    if (!mCecDevice.is_tv) {
        return;
    }

    int wakeupReason = 0;
    ioctl(mCecDevice.driver_fd, CEC_IOC_GET_BOOT_REASON, &wakeupReason);
    mCecDevice.cec_wake_status.processed = (wakeupReason & 0xff) == CEC_WAKEUP;

    LOGD("%s wakeup reason:%d, cec wake up:%d", __FUNCTION__, wakeupReason,
                                        mCecDevice.cec_wake_status.processed);

    if (!mCecDevice.cec_wake_status.processed) {
        return;
    }

    int wakeAddr = 0;
    // wakeup address is 0xb1000.
    ioctl(mCecDevice.driver_fd, CEC_IOC_GET_BOOT_ADDR, &wakeAddr);
    mCecDevice.cec_wake_status.wake_device_logical_addr = (wakeAddr >> 16) & 0xf;
    mCecDevice.cec_wake_status.wake_device_phy_addr = (wakeAddr & 0xffff);
    LOGD("%s wakeup %x logical addr:%x, physical addr:%x", __FUNCTION__, wakeAddr,
            mCecDevice.cec_wake_status.wake_device_logical_addr,
            mCecDevice.cec_wake_status.wake_device_phy_addr);
}

bool HdmiCecControl::handleCecEnabled(int enabled) {
    if (!mCecDevice.is_playback || mVendorEventListener == NULL) {
        return false;
    }
    if (enabled == 1) {
        // cec enabled
        setOption(HDMI_OPTION_WAKEUP, mWakeEnabled);
        return false;
    }
    // cec disabled
    setOption(HDMI_OPTION_WAKEUP, 0);
    LOGI("abort notify driver cec disabled");
    return true;
}

void HdmiCecControl::updateActiveState(const cec_message_t* message, bool received) {
    if (!mCecDevice.is_playback) {
        return;
    }

    int path = 0;
    int size = message->length;
    int opcode = message->body[0];
    bool updated  = false;
    int value = 0;

    if (received) {
        // handle received routing messages
        switch (opcode) {
            case CEC_MESSAGE_SET_STREAM_PATH:
            case CEC_MESSAGE_ROUTING_CHANGE:
            case CEC_MESSAGE_ACTIVE_SOURCE: {
                path = ((message->body[size - 2] & 0xff) << 8) + (message->body[size - 1] & 0xff);
                value = path == mCecDevice.phy_addr ? ACTIVENESS_STATE_ON : ACTIVENESS_STATE_OFF;
                LOGD("%s received messag:%x active state:%d", __FUNCTION__, opcode, value);
                updated = true;
                break;
            case CEC_MESSAGE_STANDBY:
                value = ACTIVENESS_STATE_OFF;
                LOGD("%s received standby message!", __FUNCTION__);
                updated = true;
                break;
            }
        }
    } else {
        switch (opcode) {
            case CEC_MESSAGE_ACTIVE_SOURCE: {
                value = ACTIVENESS_STATE_ON;
                LOGD("%s sending messag:%x active state:%d", __FUNCTION__, opcode, value);
                updated = true;
                break;
            }
        }
    }

    if (updated) {
        hdmi_cec_event_t event;
        event.eventType = HDMI_EVENT_VENDOR_MESSAGE;
        event.cec.initiator = CEC_ADDR_RESERVED_1;
        event.cec.destination = CEC_ADDR_BROADCAST;
        event.cec.length = 3;
        event.cec.body[0] = CEC_MESSAGE_VENDOR_COMMAND;
        event.cec.body[1] = VENDOR_CMD_ACTIVENESS;
        event.cec.body[2] = value;

        if (mVendorEventListener != NULL) {
            mVendorEventListener->onEventUpdate(&event);
        } else {
            mCachedRoutingEvent = &event;
        }
    }
}


void HdmiCecControl::updateActiveStateForFramework() {
    if (mIsFirstBoot) {
        mIsFirstBoot = false;
        CMessage message;
        message.mType = HdmiCecControl::MsgHandler::MSG_MAY_SEND_SET_STREAM_PATH;
        message.mDelayMs = 0;
        mMsgHandler->removeMsg(message);
        mMsgHandler->sendMsg(message);
    }
}


void HdmiCecControl::maySendSetStreamPath() {
    if ((mCecEvent & HDMI_EVENT_CEC_MESSAGE) == 0) {
        // Hdmi connection hal could also use this cpp.
        return;
    }
    if (!mCecDevice.is_playback || mCecDevice.is_audio_system) {
        // only do this for a single playback device.
        return;
    }
    if (!getPropertyBoolean(PROPERTY_BOOT_OTP, false)
            || !getPropertyBoolean(PROPERTY_ONE_TOUCH_PLAY, true)) {
        return;
    }

    int physicalAddress = mCecDevice.active_routing_path;
    hdmi_cec_event_t event;
    event.eventType = HDMI_EVENT_CEC_MESSAGE;
    event.cec.initiator = CEC_ADDR_TV;
    event.cec.destination = CEC_ADDR_BROADCAST;
    event.cec.length = 3;
    event.cec.body[0] = CEC_MESSAGE_SET_STREAM_PATH;
    event.cec.body[1] = (physicalAddress >> 8) & 0xff;;
    event.cec.body[2] = physicalAddress & 0xff;
    if (mEventListener != NULL) {
        LOGD("%s send set stream path message to framework from hal.", __FUNCTION__);
        mEventListener->onEventUpdate(&event);
    }
}
};//namespace android
