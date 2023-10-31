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
        mHandle = jpegenc_init();
        if (!mHandle)
            ALOGE("%s:jpeg init fail:this=%p,handle=0x%lx",
            __FUNCTION__,this,mHandle);
        else
            ALOGE("jpegenc this=%p handle:0x%lx \n",this, mHandle);
    }


    HwJpegEnc:: ~HwJpegEnc() {
        ALOGE("%s: E",__FUNCTION__);
        jpegenc_destroy(mHandle);
    }

    void HwJpegEnc::releaseInstance() {
        ALOGE("%s: E",__FUNCTION__);
        if (mInstance) {
            delete mInstance;
            mInstance = nullptr;
        }
    }

    int HwJpegEnc::encode(int in_width, int in_height, int quality,
                            enum jpegenc_frame_fmt_e format,
                            uint8_t*src, uint8_t*dst) {
        if (!mHandle) {
            ALOGE("%s:jpeg is not inited,this=%p, handle=0x%lx",
                __FUNCTION__,this,mHandle);
            return -1;
        }
        ALOGE("%s:call encode,src=%p, dst=%p",
                __FUNCTION__,src,dst);
        mStride_w = ((in_width + 31) / 32) * 32;
        mStride_h = ((in_height + 31) / 32) * 32;
        int len = jpegenc_encode(mHandle,
                                in_width,
                                in_height,
                                mStride_w,
                                mStride_h,
                                quality,
                                format,
                                mOutFormat,
                                mMem_type,
                                mSharedFd,
                                src,
                                dst
                                );

        if (!len)
            ALOGE("jpeg encode fail");

        return len;
    }
}

