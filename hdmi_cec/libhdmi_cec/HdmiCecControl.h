/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef _HDMI_CEC_CONTROL_CPP_
#define _HDMI_CEC_CONTROL_CPP_

#include <pthread.h>
#include <utils/StrongPointer.h>

#include "CMsgQueue.h"
#include "HdmiCecBase.h"
#include "HdmiCecBusMonitor.h"
#include "HdmiCecUtils.h"

#define CEC_FILE        "/dev/cec"
#define MAX_PORT        32
#define MESSAGE_SET_MENU_LANGUAGE 0x32

#define CEC_IOC_MAGIC                   'C'
#define CEC_IOC_GET_PHYSICAL_ADDR       _IOR(CEC_IOC_MAGIC, 0x00, uint16_t)
#define CEC_IOC_GET_VERSION             _IOR(CEC_IOC_MAGIC, 0x01, int)
#define CEC_IOC_GET_VENDOR_ID           _IOR(CEC_IOC_MAGIC, 0x02, uint32_t)
#define CEC_IOC_GET_PORT_INFO           _IOR(CEC_IOC_MAGIC, 0x03, int)
#define CEC_IOC_GET_PORT_NUM            _IOR(CEC_IOC_MAGIC, 0x04, int)
#define CEC_IOC_GET_SEND_FAIL_REASON    _IOR(CEC_IOC_MAGIC, 0x05, uint32_t)
#define CEC_IOC_SET_OPTION_WAKEUP       _IOW(CEC_IOC_MAGIC, 0x06, uint32_t)
#define CEC_IOC_SET_OPTION_ENABLE_CEC   _IOW(CEC_IOC_MAGIC, 0x07, uint32_t)
#define CEC_IOC_SET_OPTION_SYS_CTRL     _IOW(CEC_IOC_MAGIC, 0x08, uint32_t)
#define CEC_IOC_SET_OPTION_SET_LANG     _IOW(CEC_IOC_MAGIC, 0x09, uint32_t)
#define CEC_IOC_GET_CONNECT_STATUS      _IOR(CEC_IOC_MAGIC, 0x0A, uint32_t)
#define CEC_IOC_ADD_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0B, uint32_t)
#define CEC_IOC_CLR_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0C, uint32_t)
#define CEC_IOC_SET_DEV_TYPE            _IOW(CEC_IOC_MAGIC, 0x0D, uint32_t)
#define CEC_IOC_SET_ARC_ENABLE          _IOW(CEC_IOC_MAGIC, 0x0E, uint32_t)
#define CEC_IOC_SET_AUTO_DEVICE_OFF     _IOW(CEC_IOC_MAGIC, 0x0F, uint32_t)
#define CEC_IOC_GET_BOOT_ADDR           _IOW(CEC_IOC_MAGIC, 0x10, uint32_t)
#define CEC_IOC_GET_BOOT_REASON         _IOW(CEC_IOC_MAGIC, 0x11, uint32_t)
#define CEC_IOC_GET_BOOT_PORT           _IOW(CEC_IOC_MAGIC, 0x13, uint32_t)
#define CEC_IOC_SET_DEBUG_EN            _IOW(CEC_IOC_MAGIC, 0x14, uint32_t)

#define CEC_WAKEUP      8

#define DEV_TYPE_TV                     0
#define DEV_TYPE_RECORDER               1
#define DEV_TYPE_RESERVED               2
#define DEV_TYPE_TUNER                  3
#define DEV_TYPE_PLAYBACK               4
#define DEV_TYPE_AUDIO_SYSTEM           5
#define DEV_TYPE_PURE_CEC_SWITCH        6
#define DEV_TYPE_VIDEO_PROCESSOR        7

#define INVALID_PHYSICAL_ADDRESS        0xFFFF

#define VENDOR_ID_DEFAULT               0xFFFFFF
#define VENDOR_ID_AML                   0x1CA410

#define DELAY_TIMEOUT_MS                5000
#define DELAY_TRANSMISSION_TIMEOUT      2000
#define BOOT_OTP_RETRY_COUNT            7

#define HDMIRX_SYSFS                    "/sys/class/hdmirx/hdmirx0/cec"
#define CEC_STATE_BOOT_ENABLED          "2"
#define CEC_STATE_ENABLED               "1"
#define CEC_STATE_UNABLED               "0"

//#define PROPERTY_DEVICE_TYPE            "ro.vendor.platform.hdmi.device_type"
#define PROPERTY_DEVICE_TYPE            "ro.hdmi.device_type"
#define PROPERTY_VENDOR_ID              "ro.vendor.platform.hdmi.vendor_id"
#define PROPERTY_BOOT_OTP               "ro.vendor.hdmi.auto_otp"
#define PROPERTY_ONE_TOUCH_PLAY         "persist.vendor.sys.cec.onetouchplay"
#define PROPERTY_SET_MENU_LANGUAGE      "persist.vendor.sys.cec.set_menu_language"
#define PROPERTY_DEVICE_AUTO_POWEROFF   "persist.vendor.sys.cec.deviceautopoweroff"
#define PROPERTY_LOGICAL_ADDRESS        "persist.vendor.sys.cec.logicaladdress"
#define PROPERTY_POWER_STATE            "persist.vendor.sys.cec.power_state"
#define PROPERTY_CEC_ENABLED            "persist.vendor.sys.cec.cec_enabled"

//#define PROPERTY_BOOT_REASON            "sys.boot.reason"
#define PROPERTY_BOOT_REASON            "sys.boot.reason"

#define BOOT_REASON_COLD                "cold"
#define BOOT_REASON_SHUTDOWN            "shutdown"

#define PROPERTY_ARC_PORT               "persist.vendor.sys.arc_port"
#define PROPERTY_CEC_DEBUG              "persist.vendor.sys.cec_debug"


#define PROPERTY_EARC_SUPPORTED         "ro.vendor.media.support_earc"

#define SEND_MESSAGE_RETRY_HAL          2

#define ACTIVENESS_STATE_ON             1
#define ACTIVENESS_STATE_OFF            0
#define VENDOR_CMD_ACTIVENESS           1
#define LANG_VENDOR_CALLBACK            "aml"


namespace android {

/*
 * HDMI CEC messages para value
 */
enum cec_message_para_value{
    CEC_KEYCODE_UP = 0x01,
    CEC_KEYCODE_DOWN = 0x02,
    CEC_KEYCODE_LEFT = 0x03,
    CEC_KEYCODE_RIGHT = 0x04,
    CEC_KEYCODE_POWER = 0x40,
    CEC_KEYCODE_PLAY = 0x44,
    CEC_KEYCODE_POWER_TOGGLE_FUNCTION = 0x6B,
    CEC_KEYCODE_POWER_ON_FUNCTION = 0x6D,
};

enum send_message_result{
    SUCCESS = 0,
    NACK = 1, // not acknowledged
    BUSY = 2, // bus is busy
    FAIL = 3,
};

enum power_status{
    POWER_STATUS_UNKNOWN = -1,
    POWER_STATUS_ON = 0,
    POWER_STATUS_STANDBY = 1,
    POWER_STATUS_TRANSIENT_TO_ON = 2,
    POWER_STATUS_TRANSIENT_TO_STANDBY = 3,
};

typedef struct cec_wake {
    bool                        processed;
    int                         wake_device_logical_addr;
    int                         wake_device_phy_addr;
} cec_wake_t;

typedef struct hdmi_device {
    int                         driver_fd;
    int                         *device_types;
    int                         total_device;
    int                         total_port;
    hdmi_port_info_t            *port_data;
    uint16_t                    phy_addr;
    int                         arc_port;

    bool                        is_tv;
    bool                        is_playback;
    bool                        is_audio_system;
    bool                        is_cec_enabled;
    bool                        is_cec_controlled;
    bool                        hdmi_cfg_init;
    unsigned int                cec_connect_status;
    bool                        run;
    bool                        exited;
    int                         playback_logical_addr;

    int                         active_logical_addr;
    int                         active_routing_path;
    int                         *added_phy_addr;
    int                         *vendor_ids;

    cec_wake_t                  cec_wake_status;
} hdmi_device_t;

class HdmiCecControl : public HdmiCecBase {
public:
    HdmiCecControl(int eventType);
    ~HdmiCecControl();

    virtual int openCecDevice();
    virtual int closeCecDevice();

    virtual int getVersion(int* version);
    virtual int getVendorId(uint32_t* vendorId);
    virtual int getPhysicalAddress(uint16_t* addr);
    virtual int sendMessage(const cec_message_t* message);

    virtual void getPortInfos(hdmi_port_info_t* list[], int* total);
    virtual int addLogicalAddress(cec_logical_address_t address);
    virtual void clearLogicaladdress();
    virtual void setOption(int flag, int value);
    virtual void setAudioReturnChannel(int port, bool flag);
    virtual bool isConnected(int port);

    void setEventObserver(const sp<HdmiCecEventListener> &eventListener);
    void setVendorEventObserver(const sp<HdmiCecEventListener> &eventListener);

protected:
    class MsgHandler: public CMsgQueueThread {
    public:
        static const int MSG_GET_MENU_LANGUAGE =        1;
        static const int MSG_GIVE_OSD_NAME =            2;
        static const int MSG_GIVE_DEVICE_VENDOR_ID =    3;
        static const int MSG_GIVE_PHYSICAL_ADDRESS =    4;
        static const int MSG_ONE_TOUCH_PLAY =           5;
        static const int MSG_PROCESS_CEC_WAKEUP =       6;
        static const int MSG_USER_CONTROL_PRESSED =     7;
        static const int MSG_REPORT_PHYSICAL_ADDRESS =  8;
        static const int MSG_MAY_SEND_SET_STREAM_PATH = 9;

        MsgHandler(HdmiCecControl *hdmiControl);
        ~MsgHandler();
    private:
        virtual void handleMessage (CMessage &msg);
        HdmiCecControl *mControl;
    };

    class HdmiCecEventHandler: public HdmiCecListener {
    public:
        HdmiCecEventHandler(HdmiCecControl *hdmiControl);
        virtual ~HdmiCecEventHandler();
        virtual void onCecEvent(int type);

    private:
        HdmiCecControl *mControl;
    };

private:
    void getDeviceTypes();
    void getBootConnectStatus();
    static void *__threadLoop(void *data);
    void threadLoop();
    int sendMessage(int initiator, int destination, int length, unsigned char body[]);
    int send(const cec_message_t* message);
    int readMessage(unsigned char *buf, int msgCount);
    int readCecMessage();
    void checkConnectStatus();
    bool assertHdmiCecDevice();
    int preHandleOfSend(const cec_message_t* message);
    int postHandleOfSend(const cec_message_t* message, int result);
    bool transferableInSleep(char *msgBuf);
    void messageValidateAndHandle(hdmi_cec_event_t* event);
    void handleOTPMsg(hdmi_cec_event_t* event);
    void handleSetMenuLanguage(hdmi_cec_event_t* event);
    void onAddressAllocated(int logicalAddress);
    void turnOnDevice(int logicalAddress);
    void getDeviceExtraInfo(int flag);
    void sendOneTouchPlay(int logicalAddress);
    void processCecWakeup();
    void initCecWakeupInfo();
    bool isSourceDevice(int logicalAddress);
    void bootOneTouchPlay();

    bool handleCecEnabled(int enabled);
        // Return the physical address in routing message or 0.
    void updateActiveState(const cec_message_t* message, bool received);

    void updateActiveStateForFramework();
    void maySendSetStreamPath();

    hdmi_device_t mCecDevice;
    sp<HdmiCecBusMonitor> mMonitor;
    sp<HdmiCecEventHandler> mHdmiCecEventHandler;
    sp<HdmiCecEventListener> mEventListener;
    sp<HdmiCecEventListener> mVendorEventListener;
    sp<MsgHandler> mMsgHandler;
    mutable Mutex mLock;
    int mCecEvent;
    int mBootOtpCount;
    int mWakeEnabled;
    hdmi_cec_event_t* mCachedRoutingEvent;
    bool mIsFirstBoot;
};


}; //namespace android

#endif /* _HDMI_CEC_CONTROL_CPP_ */
