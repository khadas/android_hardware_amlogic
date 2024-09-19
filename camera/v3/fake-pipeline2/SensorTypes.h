

#ifndef HW_EMULATOR_SENSOR_TYPES_H
#define HW_EMULATOR_SENSOR_TYPES_H

#include <linux/videodev2.h>


namespace android {

typedef enum camera_mirror_flip_e {
    MF_NORMAL = 0,
    MF_MIRROR,
    MF_FLIP,
    MF_MIRROR_FLIP,
}camera_mirror_flip_t;


typedef enum camera_wb_flip_e {
    CAM_WB_AUTO = 0,
    CAM_WB_CLOUD,
    CAM_WB_DAYLIGHT,
    CAM_WB_INCANDESCENCE,
    CAM_WB_TUNGSTEN,
    CAM_WB_FLUORESCENT,
    CAM_WB_MANUAL,
    CAM_WB_SHADE,
    CAM_WB_TWILIGHT,
    CAM_WB_WARM_FLUORESCENT,
}camera_wb_flip_t;

typedef enum camera_effect_flip_e {
        CAM_EFFECT_ENC_NORMAL = 0,
        CAM_EFFECT_ENC_GRAYSCALE,
        CAM_EFFECT_ENC_SEPIA,
        CAM_EFFECT_ENC_SEPIAGREEN,
        CAM_EFFECT_ENC_SEPIABLUE,
        CAM_EFFECT_ENC_COLORINV,
}camera_effect_flip_t;

typedef enum camera_night_mode_flip_e {
    CAM_NM_AUTO = 0,
        CAM_NM_ENABLE,
}camera_night_mode_flip_t;

typedef enum camera_banding_mode_flip_e {
        CAM_ANTIBANDING_DISABLED= V4L2_CID_POWER_LINE_FREQUENCY_DISABLED,
        CAM_ANTIBANDING_50HZ    = V4L2_CID_POWER_LINE_FREQUENCY_50HZ,
        CAM_ANTIBANDING_60HZ    = V4L2_CID_POWER_LINE_FREQUENCY_60HZ,
        CAM_ANTIBANDING_AUTO,
        CAM_ANTIBANDING_OFF,
}camera_banding_mode_flip_t;

typedef enum camera_flashlight_status_e{
        FLASHLIGHT_AUTO = 0,
        FLASHLIGHT_ON,
        FLASHLIGHT_OFF,
        FLASHLIGHT_TORCH,
        FLASHLIGHT_RED_EYE,
}camera_flashlight_status_t;

typedef enum camera_focus_mode_e {
    CAM_FOCUS_MODE_RELEASE = 0,
    CAM_FOCUS_MODE_FIXED,
    CAM_FOCUS_MODE_INFINITY,
    CAM_FOCUS_MODE_AUTO,
    CAM_FOCUS_MODE_MACRO,
    CAM_FOCUS_MODE_EDOF,
    CAM_FOCUS_MODE_CONTI_VID,
    CAM_FOCUS_MODE_CONTI_PIC,
}camera_focus_mode_t;

typedef enum sensor_type_e{
    SENSOR_MMAP = 0,
    SENSOR_ION,
    SENSOR_ION_MPLANE,
    SENSOR_DMA,
    SENSOR_CANVAS_MODE,
    SENSOR_MIPI,
    SENSOR_USB,
    SENSOR_V4L2MEDIA,
    SENSOR_HDMI,
    SENSOR_SHARE_FD,
}sensor_type_t;

typedef enum sensor_face_type_e{
    SENSOR_FACE_NONE= 0,
    SENSOR_FACE_FRONT,
    SENSOR_FACE_BACK,
}sensor_face_type_t;

typedef struct usb_frmsize_discrete {
    uint32_t width;
    uint32_t height;
} usb_frmsize_discrete_t;

typedef struct usb_frmsize_whitelist{
    uint32_t width;
    uint32_t height;
    uint32_t actualWidth;
    uint32_t actualHeight;
} usb_frmsize_whitelist_t;

}


#endif

