
#ifndef HWVIDEO_DECODER_H
#define HWVIDEO_DECODER_H 1

#include <stdint.h>
#include "Base.h"


using namespace android;

class HWVideoDecoderImpl;

class HWVideoDecoder
{
public:
    enum BitstreamType{
        UNKNOWN_STREAM,
        MJPEG_STREAM,
        H264_STREAM,
        HEVC_STREAM,
    };

    enum DecoderMode {
        SYNC_DECODE_MODE = 0,
        ASYNC_DECODE_MODE
    };

    enum DecoderStatus {
        NOT_CONSTRUCTED,
        CONSTRUCTED,
        INITED,
        OUTPUT_FORMAT_CHANGED,
        OUTPUT_BUFFER_CREATED,
        OUTPUT_BUFFER_DONE,
        RUNTIME_ERROR,
        DECODE_FAIL_AND_INPUT_FULL
    };

    HWVideoDecoder(int cameraId);
    virtual ~HWVideoDecoder();

    virtual bool initialize(uint32_t streamType, uint32_t bitstream_width, uint32_t bitstream_height, uint32_t framerate, DecoderMode workMode, int _dataspace);
    virtual void deinitialize();

    virtual DecoderStatus getDecoderStatus();

    // async decode methods
    virtual int asyncDecodeQueueInput(int in_fd, uint8_t*in_src, uint32_t in_size);
    virtual int asyncDecodeDequeueOutput( Vector<StreamBuffer>& b, bool isJpegRequest);

    // sync decode method
    virtual int syncDecode(int in_fd, uint8_t* in_src, uint32_t in_size, Vector<StreamBuffer>& b, bool isJpegRequest);


private:
    HWVideoDecoderImpl * mPrivateImpl;
    int mCameraId;
    inline HWVideoDecoderImpl* get_impl() { return reinterpret_cast<HWVideoDecoderImpl *>( mPrivateImpl ); }
    friend class HWVideoDecoderImpl;
};

#endif


