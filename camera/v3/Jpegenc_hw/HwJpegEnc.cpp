#define LOG_TAG "HwJpegEnc"

#include "HwJpegEnc.h"
#include <utils/Log.h>
#include <string.h>
namespace android {

    HwJpegEnc* HwJpegEnc::mInstance = nullptr;


    HwJpegEnc* HwJpegEnc::getInstance(void) {
        ALOGE("%s: E",__FUNCTION__);
        if (mInstance == nullptr) {
            mInstance = new HwJpegEnc();
            return mInstance;
        }
        else
            return mInstance;
    }

    HwJpegEnc::HwJpegEnc() {
        mHandle = 0;
        mWidth = 0;
        mHeight = 0;
        mQuality = 0;
        mInFormat = FMT_RGB888;
        ALOGE("%s: E",__FUNCTION__);
    }


    HwJpegEnc:: ~HwJpegEnc() {
        ALOGE("%s: E",__FUNCTION__);
        //jpegenc_destroy(mHandle);
    }

    void HwJpegEnc::releaseInstance() {
        ALOGE("%s: E",__FUNCTION__);
        if (mInstance) {
            delete mInstance;
            mInstance = nullptr;
        }
    }

    int HwJpegEnc::encode(int in_width, int in_height, int quality,
                            jpegenc_frame_fmt_e format,
                            uint8_t*src, uint8_t*dst, int* p_len) {
        mHandle = jpegenc_init();
        if (!mHandle) {
            ALOGE("%s:jpeg is not inited,this=%p, handle=0x%lx",
                __FUNCTION__,this,mHandle);
            return -1;
        }
        ALOGE("%s:call encode,src=%p, dst=%p",
                __FUNCTION__,src,dst);
        mStride_w = ((in_width + 31) / 32) * 32;
        mStride_h = ((in_height + 31) / 32) * 32;
        jpegenc_frame_info_t frame_info = {
            .width = in_width,
            .height = in_height,
            .w_stride = mStride_w,
            .h_stride = mStride_h,
            .quality = quality,
            .iformat = format,
            .oformat = mOutFormat,
            .mem_type = mMem_type,
            .plane_num = 2,
        };
        frame_info.YCbCr[0] = (unsigned long)src;
        frame_info.YCbCr[1] = 0;
        frame_info.YCbCr[2] = 0;
        jpegenc_result_e res = jpegenc_encode(mHandle, frame_info, dst, p_len);
        if (mHandle) {
            jpegenc_destroy(mHandle);
            mHandle = 0;
        }
        if (!res)
            ALOGE("jpeg encode fail");

        return res;
    }
}

