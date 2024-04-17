#ifndef __HW_JPEG_ENC__
#define __HW_JPEG_ENC__

#include <jpegenc_api.h>

namespace android {

class HwJpegEnc {
    private:
        HwJpegEnc();
        ~HwJpegEnc();
    public:
        static HwJpegEnc* mInstance;
        static HwJpegEnc* getInstance();
        static void releaseInstance();
        int encode(int in_width, int in_height, int quality,
                               jpegenc_frame_fmt_e format,
                                      uint8_t*src, uint8_t*dst, int* p_len);
    public:
        int mWidth;
        int mHeight;
        int mQuality;
        jpegenc_frame_fmt_e mInFormat;
    private:
        int mStride_w = 32;
        int mStride_h = 32;
        const jpegenc_frame_fmt_e mOutFormat = FMT_YUV420;
        // using memcpy defaultly
        const jpegenc_mem_type_e mMem_type = JPEGENC_LOCAL_BUFF;
        jpegenc_handle_t mHandle;
};


}
#endif

