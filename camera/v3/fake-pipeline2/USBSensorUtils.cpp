
#define LOG_TAG "USBSensorUtils"

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "CameraIO.h"

#include "USBSensorUtils.h"

#include "SensorTypes.h"

#include "CamHalDebugLog.h"

namespace android {

static const int MAX_LEVEL_FOR_EXPOSURE = 16;
static const int MIN_LEVEL_FOR_EXPOSURE = 3;


int USBSensorUtils::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    int ret = 0;
    struct v4l2_queryctrl qc;
    memset(&qc, 0, sizeof(qc));
    qc.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ((qc.flags == V4L2_CTRL_FLAG_DISABLED) || ( ret < 0)
        || (qc.type != V4L2_CTRL_TYPE_INTEGER)) {
        ret = -1;
        *zoomMin = 0;
        *zoomMax = 0;
        *zoomStep = 1;
        CAMHAL_LOGD("%s: Can't get zoom level!\n", __FUNCTION__);
    } else {
        if ((qc.step != 0) && (qc.minimum != 0) &&
            ((qc.minimum/qc.step) > (qc.maximum/qc.minimum))) {
                CAMHAL_LOGD("adjust zoom step. \n");
                qc.step = (qc.minimum * qc.step);
            }
        *zoomMin = qc.minimum;
        *zoomMax = qc.maximum;
        *zoomStep = qc.step;
        CAMHAL_LOGD("zoomMin:%dzoomMax:%dzoomStep:%d\n", *zoomMin, *zoomMax, *zoomStep);
    }
    return ret ;
}

int USBSensorUtils::setZoom(int zoomValue) {
    int ret = 0;
    struct v4l2_control ctl;
    memset( &ctl, 0, sizeof(ctl));
    ctl.value = zoomValue;
    ctl.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        CAMHAL_LOGE("%s: Set zoom level failed!\n", __FUNCTION__);
        }
    return ret ;
}

status_t USBSensorUtils::setEffect(uint8_t effect) {
   int ret = 0;
   struct v4l2_control ctl;
   ctl.id = V4L2_CID_COLORFX;
   switch (effect) {
       case ANDROID_CONTROL_EFFECT_MODE_OFF:
       ctl.value= CAM_EFFECT_ENC_NORMAL;
       break;
       case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
       ctl.value= CAM_EFFECT_ENC_COLORINV;
       break;
       case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
       ctl.value= CAM_EFFECT_ENC_SEPIA;
       break;
       default:
       CAMHAL_LOGE("%s: Doesn't support effect mode %d",
       __FUNCTION__, effect);
       return BAD_VALUE;
   }
   CAMHAL_LOGD("set effect mode:%d", effect);
   ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
   if (ret < 0)
       CAMHAL_LOGD("Set effect fail: %s. ret=%d", strerror(errno),ret);
   return ret ;
}



int USBSensorUtils::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step)
{
    struct v4l2_queryctrl qc;
    int ret=0;
    int level = 0;
    int middle = 0;

    memset( &qc, 0, sizeof(qc));

    CAMHAL_LOGD("getExposure\n");
    qc.id = V4L2_CID_EXPOSURE;
    ret = ioctl(mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if (ret < 0) {
        CAMHAL_LOGD("QUERYCTRL failed, errno=%d\n", errno);
        *minExp = -4;
        *maxExp = 4;
        *def = 0;
        step->numerator = 1;
        step->denominator = 1;
        return ret;
    }

    if (0 < qc.step)
        level = ( qc.maximum - qc.minimum + 1 )/qc.step;

    if ((level > MAX_LEVEL_FOR_EXPOSURE)
      || (level < MIN_LEVEL_FOR_EXPOSURE)) {
        *minExp = -4;
        *maxExp = 4;
        *def = 0;
        step->numerator = 1;
        step->denominator = 1;
        CAMHAL_LOGD("not in[min,max], min=%d, max=%d, def=%d\n",
                                        *minExp, *maxExp, *def);
        return true;
    }

    middle = (qc.minimum+qc.maximum)/2;
    *minExp = qc.minimum - middle;
    *maxExp = qc.maximum - middle;
    *def = qc.default_value - middle;
    step->numerator = 1;
    step->denominator = 2;//qc.step;
    CAMHAL_LOGD("min=%d, max=%d, step=%d\n", qc.minimum, qc.maximum, qc.step);
    return ret;
}

status_t USBSensorUtils::setExposure(int expCmp)
{
    int ret = 0;
    struct v4l2_control ctl;
    struct v4l2_queryctrl qc;

    if (mEV == expCmp) {
        return 0;
    }else{
        mEV = expCmp;
    }
    memset(&ctl, 0, sizeof(ctl));
    memset(&qc, 0, sizeof(qc));

    qc.id = V4L2_CID_EXPOSURE;

    ret = ioctl(mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if (ret < 0) {
        CAMHAL_LOGD("AMLOGIC CAMERA get Exposure fail: %s. ret=%d", strerror(errno),ret);
    }

    ctl.id = V4L2_CID_EXPOSURE;
    ctl.value = expCmp + (qc.maximum - qc.minimum) / 2;

    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        CAMHAL_LOGD("AMLOGIC CAMERA Set Exposure fail: %s. ret=%d", strerror(errno),ret);
    }
    CAMHAL_LOGD("setExposure value%d mEVmin%d mEVmax%d\n",ctl.value, qc.minimum, qc.maximum);
    return ret ;
}

int USBSensorUtils::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_POWER_LINE_FREQUENCY;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        CAMHAL_LOGD("camera handle %d can't support this ctrl",mVinfo->fd);
    } else if ( qc.type != V4L2_CTRL_TYPE_INTEGER) {
        CAMHAL_LOGD("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        antiBanding[0] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCont)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_POWER_LINE_FREQUENCY;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"50hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"60hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"auto") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensorUtils::setAntiBanding(uint8_t antiBanding)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_POWER_LINE_FREQUENCY;

    switch (antiBanding) {
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
        ctl.value= CAM_ANTIBANDING_OFF;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
        ctl.value= CAM_ANTIBANDING_50HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
        ctl.value= CAM_ANTIBANDING_60HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
        ctl.value= CAM_ANTIBANDING_AUTO;
        break;
    default:
            CAMHAL_LOGE("%s: Doesn't support ANTIBANDING mode %d",
                    __FUNCTION__, antiBanding);
            return BAD_VALUE;
    }

    CAMHAL_LOGD("anti banding mode:%d", antiBanding);
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if ( ret < 0) {
        CAMHAL_LOGD("failed to set anti banding mode!\n");
        return BAD_VALUE;
    }
    return ret;
}

status_t USBSensorUtils::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctl.value = ((x0 + x1) / 2 + 1000) << 16;
    ctl.value |= ((y0 + y1) / 2 + 1000) & 0xffff;

    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    return ret;
}


int USBSensorUtils::getAutoFocus(uint8_t *afMode, uint8_t maxCount)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_FOCUS_AUTO;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        CAMHAL_LOGD("camera handle %d can't support this ctrl",mVinfo->fd);
    }else if( qc.type != V4L2_CTRL_TYPE_MENU) {
        CAMHAL_LOGD("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    }else{
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        afMode[0] = ANDROID_CONTROL_AF_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_FOCUS_AUTO;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-video") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-picture") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensorUtils::setAutoFocus(uint8_t afMode)
{
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_AUTO;

    switch (afMode) {
        case ANDROID_CONTROL_AF_MODE_AUTO:
            ctl.value = CAM_FOCUS_MODE_AUTO;
            break;
        case ANDROID_CONTROL_AF_MODE_MACRO:
            ctl.value = CAM_FOCUS_MODE_MACRO;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            ctl.value = CAM_FOCUS_MODE_CONTI_VID;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            ctl.value = CAM_FOCUS_MODE_CONTI_PIC;
            break;
        default:
            CAMHAL_LOGE("%s: Emulator doesn't support AF mode %d",
                    __FUNCTION__, afMode);
            return BAD_VALUE;
    }

    if (ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl) < 0) {
        CAMHAL_LOGD("failed to set camera focus mode!\n");
        return BAD_VALUE;
    }

    return OK;
}

int USBSensorUtils::getAWB(uint8_t *awbMode, uint8_t maxCount)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_DO_WHITE_BALANCE;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        CAMHAL_LOGD("camera handle %d can't support this ctrl",mVinfo->fd);
    } else if( qc.type != V4L2_CTRL_TYPE_MENU) {
        CAMHAL_LOGD("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        awbMode[0] = ANDROID_CONTROL_AWB_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_DO_WHITE_BALANCE;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"incandescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"warm-fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"cloudy-daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"twilight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_TWILIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"shade") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_SHADE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensorUtils::setAWB(uint8_t awbMode)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_DO_WHITE_BALANCE;

    switch (awbMode) {
        case ANDROID_CONTROL_AWB_MODE_AUTO:
            ctl.value = CAM_WB_AUTO;
            break;
        case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
            ctl.value = CAM_WB_INCANDESCENCE;
            break;
        case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
            ctl.value = CAM_WB_FLUORESCENT;
            break;
        case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
            ctl.value = CAM_WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_SHADE:
            ctl.value = CAM_WB_SHADE;
            break;
        default:
            CAMHAL_LOGE("%s: Emulator doesn't support AWB mode %d",
                    __FUNCTION__, awbMode);
            return BAD_VALUE;
    }
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    return ret;
}

const char* USBSensorUtils::getformtStr(int id) {
    static char str[128];
    memset(str,0,sizeof(str));

    switch (id) {
        case V4L2_PIX_FMT_MJPEG:
            sprintf(str,"%s","V4L2_PIX_FMT_MJPEG");
            break;
        case V4L2_PIX_FMT_H264:
            sprintf(str,"%s","V4L2_PIX_FMT_H264");
            break;
        case V4L2_PIX_FMT_RGB24:
            sprintf(str,"%s","V4L2_PIX_FMT_RGB24");
            break;
        case V4L2_PIX_FMT_YUYV:
            sprintf(str,"%s","V4L2_PIX_FMT_YUYV");
            break;
        default:
            sprintf(str,"%s","not support");
            break;
    };
    return str;
}

}

