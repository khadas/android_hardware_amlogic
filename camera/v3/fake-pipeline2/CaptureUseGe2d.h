#ifndef __CAPTUREUSEGE2D_H__
#define __CAPTUREUSEGE2D_H__
#include "Base.h"
#include "ICapture.h"
#include "MIPICameraIO.h"
#include "CameraUtil.h"
#ifdef CAM_DPTZ
#include <centerface_5.16/centerface_network.h>
#endif
#ifdef GE2D_ENABLE
#include "ge2d_stream.h"
#endif
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

namespace android {
    class CaptureUseGe2d:public ICapture {
        protected:
            MIPIVideoInfo* mInfo;
            CameraUtil* mCameraUtil;
#ifdef GE2D_ENABLE
            ge2dTransform* mGE2D;
#endif
            ssize_t  mPrevCenter_x    = -1, mPrevCenter_y    = -1;
            ssize_t  mPrevCenter_dstx = -1, mPrevCenter_dsty = -1;
            ssize_t  mSmoothing_x     = -1, mSmoothing_y     = -1;
            ssize_t  mPrevCrop_x      = -1, mPrevCrop_y      = -1;
            ssize_t  mPrevCrop_w      = -1, mPrevCrop_h      = -1;
            size_t   mDectNum = 0;
            bool     mSmoothing = false;
            int      mRGBIonFd  = -1;
            uint8_t* mRGBIonVa  = NULL;

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
            dewarpInfo mPreDewarpInfo[ISP_PORT_NUM];
#endif
        public:
            CaptureUseGe2d(MIPIVideoInfo* info);
            virtual ~CaptureUseGe2d();
            int getPicture(StreamBuffer b, struct data_in* in, IONInterface *ion) override;
            int captureYUYVframe(uint8_t *img, struct data_in* in) override;
            int captureNV21frame(StreamBuffer b, struct data_in* in) override;
            int captureYV12frame(StreamBuffer b, struct data_in* in) override;
            int captureRGBAframe(StreamBuffer b, struct data_in * in) override;
            int captureDPTZframe(StreamBuffer b, struct data_in * in) override;
    };
}
#endif
