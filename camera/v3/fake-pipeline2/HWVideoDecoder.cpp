

#define LOG_TAG "HWVideoDecoder"

// std c libs
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include <mutex>
#include <list>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <sys/time.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <iostream>

#include <getopt.h>
#include <inttypes.h>

// android libs
#include <utils/Log.h>

// vendor libs
#include "AmVideoDecBase.h"

#include "HWVideoDecoder.h"

#include "CameraUtil.h"

#ifdef GE2D_ENABLE
#include "fake-pipeline2/ge2d_stream.h"
#endif

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

// use ion input buffer otherwise use malloc buffer.
//#define  USE_ION_INPUT_BUFFER          1

#define ALIGN(x, align) ((x) + (align -1) & (~(align -1)))

using namespace android;

typedef enum {
    VFORMAT_UNKNOWN = -1,
    VFORMAT_MPEG12 = 0,
    VFORMAT_MPEG4,
    VFORMAT_H264,
    VFORMAT_MJPEG,
    VFORMAT_REAL,
    VFORMAT_JPEG,
    VFORMAT_VC1,
    VFORMAT_AVS,
    VFORMAT_SW,
    VFORMAT_H264MVC,
    VFORMAT_H264_4K2K,
    VFORMAT_HEVC,
    VFORMAT_H264_ENC,
    VFORMAT_JPEG_ENC,
    VFORMAT_VP9,
    VFORMAT_AVS2,
    VFORMAT_AV1,
    VFORMAT_AVS3,
    VFORMAT_DVES_AVC,
    VFORMAT_DVES_HEVC,
    VFORMAT_MPEG2TS,
    VFORMAT_UNSUPPORT,
    VFORMAT_MAX
} vformat_t;

class HWVideoDecoderImpl final {
    // construction & destruction
public:
    HWVideoDecoderImpl(HWVideoDecoder * interfaceObj);
    ~HWVideoDecoderImpl();

    // interface functions inherited from HWVideoDecoder
public:

    virtual bool initialize(uint32_t dec_type, uint32_t bitstream_width, uint32_t bitstream_height, uint32_t framerate, HWVideoDecoder::DecoderMode workMode);
    virtual void deinitialize();

    virtual HWVideoDecoder::DecoderStatus getDecoderStatus();

    // async decode methods
    virtual int asyncDecodeQueueInput(int in_fd, uint8_t*in_src, uint32_t in_size);
    virtual int asyncDecodeDequeueOutput( Vector<StreamBuffer>& b, bool isJpegRequest);

    // sync decode method
    int syncDecode(int in_fd, uint8_t*in_src, uint32_t in_size, Vector<StreamBuffer>& b, bool isJpegRequest);

    // implemented video decode callback functions
public:
    void onOutputFormatChanged(uint32_t requestedNumOfBuffers,
                               int32_t width, uint32_t height);
    void onOutputBufferDone(int32_t pictureBufferId, int64_t bitstreamId,
                            uint32_t width, uint32_t height);
    void onInputBufferDone(int32_t bitstream_buffer_id);
    void onUpdateDecInfo(const uint8_t* info, uint32_t isize);
    void onFlushDone();
    void onResetDone();
    void onError(int32_t error);
    void onEvent(uint32_t event, void* param, uint32_t paramSize);

private:
    int checkMjpegData(uint8_t* in_src, uint32_t in_size, bool check_mjpeg_wh);
    bool findSOI(uint8_t* in_src, uint32_t in_size, int& offset);
    bool findEOI(uint8_t* in_src, uint32_t in_size);

    bool checkAndwaitForOutBuf(uint32_t ms);
    int queueInputBufferInternal(int fd, uint8_t * data, int size);
    int  queueInputBufferNoBlock(int in_fd, uint8_t* in_src, uint32_t in_size);
    int  queueInputBufferMayBlock(int in_fd, uint8_t* in_src, uint32_t in_size);
    void dumpInputTofile(uint8_t* in_src, uint32_t in_size);
    bool isIDR(uint8_t* in_src, uint32_t in_size);

    int preAllocOutputBufferLocked(uint32_t NumOfBuffers,
                int32_t width, uint32_t height);
    int queueOutputBuffersLocked();
    int reconfigOutputBuffersLocked(uint32_t requestedMinNumOfBuffers_t,
                int32_t width_t, uint32_t height_t);

    // per object members
private:
    HWVideoDecoder * const mInterfaceObj;
    inline HWVideoDecoder* get_interface() { return static_cast<HWVideoDecoder *>(mInterfaceObj); }
    friend class HWVideoDecoder;

private:

    HWVideoDecoder::DecoderStatus                   mStatus;
    HWVideoDecoder::DecoderMode     mWorkMode;

    AmVideoDecCallback* mAmVideoDecCallback = nullptr;
    AmVideoDecBase*     mAmVideoDec;

    uint32_t mDefaultInputQueueCount;
    uint32_t mDefaultOutputQueueCount;

    init_param_t mVideoDecConfig;
    CameraUtil* mDump = NULL;
    int dumpIndex[4] = {0};
    bool mEnableDewarp;
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    dewarpInfo mPreDewarpInfo[ISP_PORT_NUM];
#endif

    struct mapInfo {
        uint8_t*   vaddr;
        uint32_t   size;
        int        pictureId;
        int        fd;
        int        fdHasSetToPictureId;
        mapInfo(): vaddr(0), size(0), pictureId(-1), fd(-1), fdHasSetToPictureId(0){}
    };

    uint64_t mBitStreamId;
    std::map<int32_t, uint8_t*> mInputBuffer;

    uint32_t mOutputBufferNum;
    std::vector<struct mapInfo> mOutputBufs;

    uint32_t mDqWidth;
    uint32_t mDqHeight;
    uint32_t mFormatWidth;
    uint32_t mFormatHeight;

    uint32_t mQueuedInputBufCountBeforeOutBufDone;
    uint32_t mInputDoneCount;
    uint32_t mOutputDoneCount;

    std::mutex mInputLock;
    std::mutex mOutputLock;
    std::mutex mFlushedLock;

    std::condition_variable mOutBufReadyCondition;
    std::queue<uint32_t> mReadyOutBufQueue;
    bool mDropOutBuf;

    std::condition_variable mFlushedCondition;
    bool mFlushed;
    bool mCheckMjpegWH;
#ifdef GE2D_ENABLE
        ge2dTransform* mGE2D;
#endif
    IONInterface* mION;
    int mWaitOutBufDurationMs;

    FILE* mInputDumpFile;

};


class videoDecCallbackImpl final : public AmVideoDecCallback {
public:
    videoDecCallbackImpl(HWVideoDecoderImpl * decImpl);
    virtual ~videoDecCallbackImpl();

    virtual void onOutputFormatChanged(uint32_t requestedNumOfBuffers,
            int32_t width, uint32_t height) override;

    virtual void onOutputBufferDone(int32_t pictureBufferId, int64_t bitstreamId,
            uint32_t width, uint32_t height) override;

    virtual void onInputBufferDone(int32_t bitstream_buffer_id) override ;

    virtual void onUserdataReady(const uint8_t* userdata, uint32_t usize) override ;
    virtual void onUpdateDecInfo(const uint8_t* info, uint32_t isize) override;
    virtual void onFlushDone() override ;
    virtual void onResetDone() override ;
    virtual void onError(int32_t error) override;
    virtual void onEvent(uint32_t event, void* param, uint32_t paramSize) override ;

private:
    HWVideoDecoderImpl *mHWVideoDecImpl;
};


// =========  end HWVideoDecoderImpl ============================


// ========= begin  internal  helper functions ================================================

std::mutex mediaHalLibHandleLock;
static void * mediaHalLibHandle_g = nullptr;

static bool loadMediaHalLibrary(void)
{
    std::lock_guard<std::mutex> lock(mediaHalLibHandleLock);

    if (NULL == mediaHalLibHandle_g) {
        mediaHalLibHandle_g = dlopen("libmediahal_videodec.so", RTLD_NOW);
        if (NULL == mediaHalLibHandle_g) {
            CAMHAL_LOGE("unable to dlopen libmediahal_videodec.so: %s", dlerror());
            return false;
        }
    }
    return true;
}


static void releaseMediaHalLibrary(void)
{
    std::lock_guard<std::mutex> lock(mediaHalLibHandleLock);

    if (mediaHalLibHandle_g) {
        dlclose(mediaHalLibHandle_g);
        mediaHalLibHandle_g = NULL;
    } else {
        CAMHAL_LOGE("mediaHalLibHandle_g is already closed" );
    }
}

void * getMediaHalLibrary(void)
{
    std::lock_guard<std::mutex> lock(mediaHalLibHandleLock);

    if (nullptr == mediaHalLibHandle_g) {
        CAMHAL_LOGE("mediaHalLibHandle_g is invalid. please loadMediaHalLibrary first" );
    }
    return mediaHalLibHandle_g;
}

static AmVideoDecBase*  getAmVideoDec(AmVideoDecCallback * callback)
{
    uint32_t versionM = 1;
    uint32_t versionL = 0;

    if (!loadMediaHalLibrary()) {
        return nullptr;
    }

    CAMHAL_LOGI("open libmediahal_videodec.so ok\n");

    typedef uint32_t (*getVersionFunc)(uint32_t* versionM, uint32_t* versionL);
    typedef AmVideoDecBase *(*createAmVideoDecFunc)(AmVideoDecCallback* callback);

    getVersionFunc getVersion = (getVersionFunc)dlsym(getMediaHalLibrary(), "AmVideoDec_getVersion");
    if (NULL == getVersion) {
        releaseMediaHalLibrary();
        CAMHAL_LOGE("can not find function AmVideoDec_getVersion in library\n");
        return nullptr;
    }

    getVersion(&versionM, &versionL);

    createAmVideoDecFunc AmVideoDec_create = nullptr;

    if ((versionM == 1) && (versionL == 0)) {
        CAMHAL_LOGI("version 1.0 use create AmMediaHal\n");
        AmVideoDec_create =
            (createAmVideoDecFunc)dlsym(getMediaHalLibrary(), "createAmMediaHal");
    } else if ((versionM == 1) && (versionL == 1)){
        CAMHAL_LOGI( "version 1.1 use create AmVideoDec_create\n");
        AmVideoDec_create =
            (createAmVideoDecFunc)dlsym(getMediaHalLibrary(), "AmVideoDec_create");
    } else {
        CAMHAL_LOGE("Mediahal version do not right\n");
        releaseMediaHalLibrary();
        return nullptr;
    }

    if (NULL == AmVideoDec_create) {
        releaseMediaHalLibrary();
        CAMHAL_LOGE("can not find function AmVideoDec_create in library\n");
        return nullptr;
    }

    AmVideoDecBase* halHanle = AmVideoDec_create(callback);
    CAMHAL_LOGI("AmVideoDec_create ok\n");
    return halHanle;
}


static const char* vformat_to_mime(uint32_t vformat) {
    switch (vformat) {
        case 3:
            return "video/mjpeg";
        case 2:
            return "video/avc";
        case 11:
            return "video/hevc";
        case 14:
            return "video/x-vnd.on2.vp9";
        case 0:
            return "video/mpeg2";
        case 1:
            return "video/mp4v-es";
        case 6:
            return "video/vc1";
        case 7:
            return "video/avs";
        case 15:
            return "video/avs2";
        default:
            return "";
    }
}


static uint64_t getTimeUs(void) {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1e6 + time.tv_usec;
}


// --------- begin   videoDecCallbackImpl ---------------

videoDecCallbackImpl::videoDecCallbackImpl(HWVideoDecoderImpl * decImpl)
{
    mHWVideoDecImpl = decImpl;
}

videoDecCallbackImpl::~videoDecCallbackImpl()
{
}

void videoDecCallbackImpl::onOutputFormatChanged(uint32_t requestedNumOfBuffers,
            int32_t width, uint32_t height)
{
    mHWVideoDecImpl->onOutputFormatChanged(requestedNumOfBuffers, width, height);
}

void videoDecCallbackImpl::onOutputBufferDone(int32_t pictureBufferId, int64_t bitstreamId,
        uint32_t width, uint32_t height)
{
    mHWVideoDecImpl->onOutputBufferDone(pictureBufferId, bitstreamId, width, height);
}

void videoDecCallbackImpl::onInputBufferDone(int32_t bitstream_buffer_id)
{
    mHWVideoDecImpl->onInputBufferDone(bitstream_buffer_id);
}

void videoDecCallbackImpl::onUserdataReady(const uint8_t* userdata, uint32_t usize)
{
    UNUSED(userdata);
    UNUSED(usize);
}

void videoDecCallbackImpl::onUpdateDecInfo(const uint8_t* info, uint32_t isize)
{
    mHWVideoDecImpl->onUpdateDecInfo(info, isize);
}


void videoDecCallbackImpl::onFlushDone()
{
    mHWVideoDecImpl->onFlushDone();
}


void videoDecCallbackImpl::onResetDone()
{
    mHWVideoDecImpl->onResetDone();
}


void videoDecCallbackImpl::onError(int32_t error)
{
    mHWVideoDecImpl->onError(error);
}


void videoDecCallbackImpl::onEvent(uint32_t event, void* param, uint32_t paramSize)
{
    mHWVideoDecImpl->onEvent(event, param, paramSize);
}

// --------- end   videoDecCallbackImpl ---------------




// --------- begin   HWVideoDecoderImpl ---------------


//  ============  input buffer queue ======================================

// brief introduction FOR input buffer queue:

// for deocder:
//        decoder will hold input buffer until this buffer has beed decoded.
//        after a input buffer has beed used, decoder will return it back by onInputBufferDone

// for queueInputBuffer(int in_fd, uint8_t* in_src, uint32_t in_size)
//        on return, we lost the ownership of buffer pointed by in_src.
//         usually it will be queue back to v4l2 driver.

// the problem is: decoder must hold the ownership of input before it has be decoded,
//   while we can't hold input buffer after queueInputBuffer function finished.

// solution 1: expand async callback of onInputBufferDone to the user of HWVideoDecoder.
// That is, user of HWVideoDecoder(USBSensorHWDec) must provide a callback function (namely, onBufferrRelease),
//    which will be called by onInputBufferDone
// USBSensorHWDec lost ownership of buffer on queueInputBuffer, get ownership of buffer on callback function (onBufferrRelease)
// USBSensorHWDec handles complicated async ownerships of buffers.
//    ownered by v4l2 driver, ownered by USBSensorHWDec, ownered by decoder.


// solution 2: copy & store input buffer in HWVideoDecoder. terminate async callback in HWVideoDecoder,
//                      simplify the usage of HWVideoDecoder.
// this is the solution we chose.

// brief:
// input buffer queue - free_input_buffer_list - has INPUT_QUEUE_BUFFER_NUM elements.
//     2 interfaces: request_input_buffer & release_input_buffer


// request_input_buffer
// used to request a free buffer. used by queueInputBuffer.

// on success, return a free buffer.
//          queueInputBufferNoBlock will copy input data to this buffer and queue this buffer to mediahalsdk.

// on fail (that is all buffers has been queued to mediahalsdk), return nullptr.
//          queueInputBufferNoBlock will return fail, instead of calling mAmVideoDec->queueInputBuffer, which may block.

// release_input_buffer
// called by onInputBufferDone
//  return buffer to free_input_buffer_list.


// std::map<int32_t, uint8_t*> mInputBuffer; record input buffers hold by mediahalsdk
// when nearly all buffers has been hold by mediahalsdk,
//         That is mInputBuffer.size() >= (INPUT_QUEUE_BUFFER_NUM - 2)
// queueInputBufferNoBlock will select I frame, and only copy I frames, and queue I frames to mediahalsdk
//


enum BUFFER_STATE{
    FREE,
    IN_USE,
};

// default value.


// INPUT_QUEUE_BUFFER_NUM
// the max number of hold input buffers for mediahalsdk video-decoder.

// this value is set to mediahalsdk via mAmVideoDec->setQueueCount

// mAmVideoDec->queueInputBuffer will be blocked,
//   which is a bad unexpected behavior for async decode mode used for h264 bitstream,
//       when caller try to queue more buffer on INPUT_QUEUE_BUFFER_NUM buffers has been hold by mediahalsdk.

#define INPUT_QUEUE_BUFFER_NUM   8

// max input data size. should be the max value of v4l2 buffer's bytesused;
// for 1920x1080 mjpeg stream or h264 stream.
#define INPUT_BUFFER_SIZE        (1024*1024)

#ifndef USE_ION_INPUT_BUFFER
// for malloc input buffer
static uint8_t * input_buffer_start = 0;
#endif

typedef struct buffer_item {
    uint8_t    *buffer;
    uint32_t    state;
    uint32_t    size;
    uint32_t    serial;
    uint32_t    used;
    int         fd;
} buffer_item_t;

// buffer_item_t obj array.
static buffer_item_t input_buffers[INPUT_QUEUE_BUFFER_NUM];

// hold pointers which point to obj in input_buffers.
static std::list<buffer_item_t *> free_input_buffer_list;

// alloc data space and fill list with pointers.
static int init_input_buffer_queue()
{
    int ret = 0;
    int i = 0;

#ifndef USE_ION_INPUT_BUFFER
    // for malloc buffer
    int total_frame_bytes = INPUT_BUFFER_SIZE * INPUT_QUEUE_BUFFER_NUM;

    if (0 == input_buffer_start) {
        input_buffer_start = (uint8_t*) malloc( total_frame_bytes );
        if (NULL == input_buffer_start) {
            CAMHAL_LOGE("frame buffers malloc failed\n");
            return -1;
        }
    }

    uint8_t*  start_addr = input_buffer_start;
#endif

    // clear

    if (!free_input_buffer_list.empty()) {
        free_input_buffer_list.clear();
    }

    memset(&input_buffers[0], 0, sizeof(struct buffer_item) * INPUT_QUEUE_BUFFER_NUM);
    for (i = 0; i < INPUT_QUEUE_BUFFER_NUM; ++i) {
        input_buffers[i].fd = -1;
    }

    // allocate buffers.
    for (i = 0; i < INPUT_QUEUE_BUFFER_NUM; ++i) {
        uint8_t* vaddr = nullptr;
        int fd = -1;

#ifdef USE_ION_INPUT_BUFFER
        // ion input buffer, using ion alloc
        auto ion = IONInterface::get_instance();
        vaddr = ion->alloc_buffer(INPUT_BUFFER_SIZE, &fd);
        if (!vaddr) {
            CAMHAL_LOGE("alloc ion input buffer fail, free already allocated input buffers.");
            for (int jj = i; jj >= 0; jj--) {
                if (input_buffers[jj].fd > 0 )
                    ion->free_buffer(input_buffers[jj].fd);
            }
            ion->put_instance();
            ret = -1;
            return ret;
        }
        ion->put_instance();
#else
        // malloc input buffer.
        vaddr = start_addr + i * INPUT_BUFFER_SIZE;
#endif
        input_buffers[i].buffer = vaddr;
        input_buffers[i].state = FREE;
        input_buffers[i].size = INPUT_BUFFER_SIZE;
        input_buffers[i].serial = i;
        input_buffers[i].used = 0;
        input_buffers[i].fd = fd;
        free_input_buffer_list.push_back(&input_buffers[i]);

        CAMHAL_LOGI("input frame buffer %d, obj %p, buffer %p", i, &input_buffers[i], input_buffers[i].buffer );

    }
    return ret;
}

// free list. free data space.
static int deinit_input_buffer_queue()
{
    if (!free_input_buffer_list.empty()) {
        free_input_buffer_list.clear();
    }

#ifdef USE_ION_INPUT_BUFFER
    // ion input buffer - free using for loop
    for (int jj = 0; jj < INPUT_QUEUE_BUFFER_NUM; jj++) {
        if (input_buffers[jj].fd > 0 )
            IONInterface::get_instance()->free_buffer(input_buffers[jj].fd);
    }

#else
    // malloc input buffer
    if (input_buffer_start) {
        free(input_buffer_start);
        input_buffer_start = nullptr;
    }
#endif

    return 0;
}

// return 0 on success,
// return -1 on fail.
static int try_request_input_buffer(buffer_item_t** item)
{

    if (free_input_buffer_list.empty()) {
        CAMHAL_LOGE("line %d, no free buffers", __LINE__);
        return -1;
    }

    *item = free_input_buffer_list.front();
    free_input_buffer_list.pop_front();

    (*item)->state = IN_USE;

    return 0;
}

// return 0 on success.
// return -1 on fail.
static int release_input_buffer(buffer_item_t* item) {
    if (item->state != IN_USE) {
        CAMHAL_LOGE("line %d, item %p , bad state, not in-use", __LINE__, item);
    }

    // push_back on free_input_buffer_list has not contains this pointer

    item->state = FREE;

    // push back when list not contains this item.
    auto it = std::find(free_input_buffer_list.begin(), free_input_buffer_list.end(), item);

    if (it == free_input_buffer_list.end() ) {
        free_input_buffer_list.push_back(item);
    } else {
        CAMHAL_LOGE("item %p was in free_input_buffer_list", item);
    }

    return 0;
}

//  ============ end input buffer queue ======================================

// ======== beg pre-alloc output buffer queue =========================

// brief introduction FOR pre-alloc output buffer queue:

// normal sequence:
//1. queue first frame bitstream to mediahalsdk, mediahalsdk need first frame's bitstream to determin
//          output frame width & height & count.
//2. provide these info to HWVideoDecoder via callback onOutputFormatChanged
//3. HWVideoDecoder alloc output buffers with these info, and queue output buffers to mediahalsdk
//4. mediahalsdk decode to output buffers and callback onOutputBufferDone

// problem: the interval from step 1 ~ step 4 for first frame is too big.
//

// solution: pre-alloc output buffers using ion before queue first frame bitstream to mediahalsdk;
//          [can not use mediahalsdk to alloc output buffers]
//          queue these output buffers to mediahalsdk in onOutputFormatChanged;

// related variables:
// uint32_t mOutputBufferNum;
// std::vector<struct mapInfo> mOutputBufs;

// ====== end pre-alloc output buffer queue ===========================

// ======== begin ready output buffer queue ============================


//std::condition_variable mOutBufReadyCondition;
//std::queue<uint32_t> mReadyOutBufQueue;

//  save output buffer idx to mReadyOutBufQueue in onOutputBufferDone

// =========== end ready output buffer queue=====================

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
static bool isNeedDestroyDewarp (dewarpInfo &info_exist, dewarpInfo &info) {
    if (info_exist.o_width && info_exist.o_height && info_exist.i_width && info_exist.i_height
            && info.i_width && info.i_height && info.o_width && info.o_height) {
        if ((info_exist.o_width != info.o_width) || (info_exist.o_height != info.o_height)
                || (info_exist.i_width != info.i_width) || (info_exist.i_height != info.i_height)) {
            return true;
        }
    }
    return false;
}
#endif
HWVideoDecoderImpl::HWVideoDecoderImpl(HWVideoDecoder * interfaceObj)
    : mInterfaceObj (interfaceObj), mStatus(HWVideoDecoder::NOT_CONSTRUCTED)
{
    mDefaultInputQueueCount = INPUT_QUEUE_BUFFER_NUM;
    mDefaultOutputQueueCount = 12;

    init_param_t defConfig = {
    /* video */
    .vpid = 256, .nVideoWidth = 1920, .nVideoHeight = 1080, .nFrameRate = 30, .vFmt = VFORMAT_MJPEG, .drmMode = 0,
    /* audio */
    .apid = 0,   .nChannels = 2,      .nSampleRate = 44100, .aFmt = 2,
    /*pcrid */
    .pcrid = 0,
    /* display */
    .dispMode = 1, .nSidebandType = 0, .nSidebandId = 0, .nAvsyncMode = 0,
    .subtitleFlg = 2, .mDemuxType = 0, .dmx_dev_id = 0, .dmx_player_id = 0, .stbuf_start = 0, .stbuf_size = 0, .nDecType = 0, .nVideoRecoveryValue = 1
    };
    mVideoDecConfig = defConfig;

    mEnableDewarp = false;
    mFlushed = false;
    mCheckMjpegWH = false;
    mWaitOutBufDurationMs = 20;

    mInputDumpFile = nullptr;
    mStatus = HWVideoDecoder::CONSTRUCTED;
    if (property_get_bool("vendor.camhal.usbsensor.use.dewarp", false)) {
        mEnableDewarp = true;
    }
    if (property_get_bool("camera.debug.dump.decoder", false)) {
        if (nullptr == mDump) {
            mDump = new CameraUtil();
        }
    }
    mWorkMode = HWVideoDecoder::SYNC_DECODE_MODE;
    mAmVideoDec = nullptr;
    mBitStreamId = 0;
    mOutputBufferNum = 0;
    mDqWidth = 0;
    mDqHeight = 0;
    mFormatWidth = 0;
    mFormatHeight = 0;
    mQueuedInputBufCountBeforeOutBufDone = 0;
    mInputDoneCount = 0;
    mOutputDoneCount = 0;
    mDropOutBuf = false;
    mGE2D = nullptr;
    mION = nullptr;
}


HWVideoDecoderImpl::~HWVideoDecoderImpl()
{

    if (mAmVideoDecCallback) {
        delete mAmVideoDecCallback;
        mAmVideoDecCallback = nullptr;
    }
    if (mAmVideoDec) {
        delete mAmVideoDec;
        mAmVideoDec = nullptr;
    }
#ifdef GE2D_ENABLE
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#endif
    if (mION) {
        mION->put_instance();
    }
    if (property_get_bool("camera.debug.dump.decoder", false)) {
        if (mDump) {
            delete mDump;
            mDump = NULL;
        }
    }
}


bool HWVideoDecoderImpl::initialize(uint32_t streamType, uint32_t bitstream_width, uint32_t bitstream_height, uint32_t framerate, HWVideoDecoder::DecoderMode workMode)
{
    uint32_t vFmt = VFORMAT_MJPEG;
    int ret = -1;
    const char * mime = NULL;

#ifdef GE2D_ENABLE
    mGE2D = new ge2dTransform();
#endif
    mION = IONInterface::get_instance();

    mBitStreamId = 0;
    mWorkMode = workMode;
    if (property_get_bool("vendor.media.camera.dec.checkmjpegwh", true))
        mCheckMjpegWH = true;

    mInputDoneCount = 0;
    mOutputDoneCount = 0;
    mOutputBufferNum = 0;

    mDropOutBuf = false;
    mQueuedInputBufCountBeforeOutBufDone = 0;

    mVideoDecConfig.nVideoWidth = bitstream_width;
    mVideoDecConfig.nVideoHeight = bitstream_height;
    mVideoDecConfig.nFrameRate = framerate;

    mDqWidth = 0;
    mDqHeight = 0;
    mFormatWidth = 0;
    mFormatHeight = 0;

    switch (streamType) {
        case HWVideoDecoder::H264_STREAM:
            vFmt = VFORMAT_H264;
        break;

        case HWVideoDecoder::MJPEG_STREAM:
            vFmt = VFORMAT_MJPEG;
        break;

        case HWVideoDecoder::HEVC_STREAM:
            vFmt = VFORMAT_HEVC;
        break;

        default:
            vFmt = VFORMAT_MJPEG;
        break;
    }

    mVideoDecConfig.vFmt = vFmt;
    mime = vformat_to_mime(mVideoDecConfig.vFmt);

    CAMHAL_LOGI("%s in, vFmt %d, mime: %s ", __func__, vFmt, mime);


    mAmVideoDecCallback = new videoDecCallbackImpl(this);

    if (mAmVideoDecCallback && !mAmVideoDec) {
        mAmVideoDec = getAmVideoDec(mAmVideoDecCallback );
    }

    if (nullptr == mAmVideoDec) {
        CAMHAL_LOGE("line %d mAmVideoDec is NULL, getAmVideoDec  failed", __LINE__);
        return false;
    }

    ret = mAmVideoDec->setQueueCount(mDefaultInputQueueCount);
    if (ret) {
        CAMHAL_LOGE("setQueueCount failed!, ret=%d\n", ret);
        delete mAmVideoDec;
        mAmVideoDec = NULL;
        return false;
    }

    ret = mAmVideoDec->initialize(mime, (uint8_t*)&mVideoDecConfig, sizeof(init_param_t),
                               false /*secureMode*/,
                               true  /*usev4l2*/,
                               AM_VIDEO_DEC_INIT_FLAG_DEFAULT /*flags*/);

    if (ret) {
        CAMHAL_LOGE("init failed!, ret=%d\n", ret);
        delete mAmVideoDec;
        mAmVideoDec = NULL;
        return false;
    }


    init_input_buffer_queue();

    // ======== begin just for debug dump input ==================================
    {
        char inputDumpFilename[256];
        memset(&inputDumpFilename[0], 0, sizeof(inputDumpFilename) );

        if (VFORMAT_MJPEG == vFmt) {
            snprintf(inputDumpFilename, 256, "/data/vendor/camera/video_dec_input_%dx%d.mjpeg", bitstream_width, bitstream_height);
        } else if (VFORMAT_HEVC == vFmt) {
            snprintf(inputDumpFilename, 256, "/data/vendor/camera/video_dec_input.h265");
        } else {
            snprintf(inputDumpFilename, 256, "/data/vendor/camera/video_dec_input.h264");
        }

        if (mInputDumpFile == nullptr && inputDumpFilename[0] != 0) {
            mInputDumpFile = fopen(inputDumpFilename, "w+b");
        }

        if (nullptr == mInputDumpFile) {
            CAMHAL_LOGW("open input dump file %s failed", inputDumpFilename);
        }
    }

    CAMHAL_LOGI("init success!, useV4L2, inputQueueCount %d, outputQueueCount %d\n", mDefaultInputQueueCount, mDefaultOutputQueueCount);

    // ======== end just for debug dump input ==================================
    if (1) {
        // pre-allocate output buffer.
        std::lock_guard<std::mutex> lock(mOutputLock);
        preAllocOutputBufferLocked(mDefaultOutputQueueCount, bitstream_width, bitstream_height);

        CAMHAL_LOGI("alloc outputbuf  success ");
    }

    mStatus = HWVideoDecoder::INITED;

    return true;
}



void HWVideoDecoderImpl::deinitialize()
{
    CAMHAL_LOGI("%s in", __func__);

    if (nullptr == mAmVideoDec) {
        CAMHAL_LOGE("%d mAmVideoDec is null", __LINE__);
        return ;
    }

    // set drop flag.
    {
        std::lock_guard<std::mutex> lock(mOutputLock);
        mDropOutBuf = true;
        while (!mReadyOutBufQueue.empty()) {
            int outputBufIdx = mReadyOutBufQueue.front();
            mReadyOutBufQueue.pop();
            mAmVideoDec->queueOutputBuffer(outputBufIdx);
            CAMHAL_LOGD("return back outBufferIdx %d", outputBufIdx);
        }
    }

    mAmVideoDec->flush();

    if (mStatus == HWVideoDecoder::DECODE_FAIL_AND_INPUT_FULL) {
        usleep(50*1000);
        CAMHAL_LOGE("bad status. do not wait flush done signal");
    } else {
        std::unique_lock <std::mutex> lck(mFlushedLock);
        while (!mFlushed) {
            if (std::cv_status::timeout == mFlushedCondition.wait_for(lck, std::chrono::milliseconds(1000) ) ) {
                CAMHAL_LOGE("flush mediahal timeout");
                break;
            }
        }
    }

    CAMHAL_LOGI("flush success, sleep 50ms, let decoder internal thread finish\n" );
    usleep(50*1000);

    if (mOutputBufferNum > 0) {
        CAMHAL_LOGI("free all buffer, mOutputBufferNum %d\n", mOutputBufferNum);
        for (int i = 0; i < mOutputBufs.size(); i++) {
            if (mOutputBufs[i].fd > 0)
                mION->free_buffer(mOutputBufs[i].fd);
        }
        mOutputBufferNum = 0;
    }

    CAMHAL_LOGI("before destroy " );

    mAmVideoDec->destroy();

    CAMHAL_LOGI("destroy success" );

    std::map<int32_t, uint8_t*>::iterator iter;
    iter = mInputBuffer.begin();
    while (iter != mInputBuffer.end()) {

        buffer_item_t* item = (buffer_item_t*)iter->second;
        release_input_buffer(item);

        iter = mInputBuffer.erase(iter);
    }

    deinit_input_buffer_queue();

    if (nullptr != mInputDumpFile) {
        fclose(mInputDumpFile);
    }

    mStatus = HWVideoDecoder::CONSTRUCTED;
    mCheckMjpegWH = false;
    CAMHAL_LOGI("%s leave", __func__);
}

HWVideoDecoder::DecoderStatus HWVideoDecoderImpl::getDecoderStatus()
{
    return mStatus;
}

void HWVideoDecoderImpl::dumpInputTofile(uint8_t* in_src, uint32_t in_size)
{
    static int dumped_frames = 0;
    // ====== begin debug dump ========================================================
    if (nullptr != mInputDumpFile) {
        dumped_frames++;
        CAMHAL_LOGI("line %d, write to dump file, dump frames %d ", __LINE__, dumped_frames);
        int written_bytes = fwrite(in_src, 1, in_size, mInputDumpFile);
        if (in_size != written_bytes ) {
            // todo: try again ?. now just log error and skip.
            CAMHAL_LOGE("line %d, write dump file ret %d, expected %d bytes", __LINE__, written_bytes, in_size);
        }

        // for mjpeg, padding to size w*h
        if (VFORMAT_MJPEG == mVideoDecConfig.vFmt) {
            if (written_bytes < INPUT_BUFFER_SIZE) {
                int pad_bytes = INPUT_BUFFER_SIZE - written_bytes;
                int ret = fseek(mInputDumpFile, pad_bytes, SEEK_CUR);
                if (!ret)
                    CAMHAL_LOGE("fseek error");
            }
        }
        fflush(mInputDumpFile);
    }

    // ====== end debug dump ========================================================

}

bool HWVideoDecoderImpl::isIDR(uint8_t* in_src, uint32_t in_size)
{
    if (in_size < 8) {
        CAMHAL_LOGE("%s leave, bad len %d", __FUNCTION__, in_size );
        return false;
    }

    if ( in_src[0] != 0x00 ||  in_src[1] != 0x00 ||  in_src[2] != 0x00 || in_src[3] != 0x01) {
        CAMHAL_LOGE("%s leave, not start with 00 00 00 01", __FUNCTION__ );
        return false;
    }

    if (mVideoDecConfig.vFmt == VFORMAT_H264) {
        // nal_ref_idc bit 6:5  nal_unit_type bit 4:0
        int nal_ref_idc_shifted = in_src[4] & 0x60;
        int nal_unit_type = in_src[4] & 0x1f;
        if (nal_ref_idc_shifted > 0)
            if (nal_unit_type >= 5 && nal_unit_type <= 8)
                return true;
    } else {
        if (in_src[4] == 0x40 || in_src[4] == 0x42 || in_src[4] == 0x44 || in_src[4] == 0x4E || in_src[4] == 0x26) {
            return true;
        }
    }

    return false;
}

int HWVideoDecoderImpl::asyncDecodeQueueInput(int in_fd, uint8_t* in_src, uint32_t in_size)
{
    int ret = 0;

    if (HWVideoDecoder::ASYNC_DECODE_MODE != mWorkMode ) {
        CAMHAL_LOGE("line %d not in async decode mode", __LINE__);
        return -1;
    }

    if (mStatus >= HWVideoDecoder::OUTPUT_BUFFER_DONE) {
        return queueInputBufferNoBlock(-1, in_src, in_size);
    }

    // must queue first input buffer.
    if (mQueuedInputBufCountBeforeOutBufDone < 1) {
        mQueuedInputBufCountBeforeOutBufDone++;
        return queueInputBufferNoBlock(-1, in_src, in_size);
    }

    if (in_size >= INPUT_BUFFER_SIZE) {
        CAMHAL_LOGE("%s leave, src_len %d, too big. should be less than %d", __FUNCTION__, in_size, INPUT_BUFFER_SIZE);
        return -1;
    }

    // after INPUT_QUEUE_BUFFER_NUM/2 input buffers, before OUTPUT_BUFFER_DONE, h264 only queue IDR frame.
    if ((mVideoDecConfig.vFmt == VFORMAT_H264 || mVideoDecConfig.vFmt == VFORMAT_HEVC) &&
        ( isIDR(in_src, in_size) || mQueuedInputBufCountBeforeOutBufDone < INPUT_QUEUE_BUFFER_NUM/2 ) ) {
        mQueuedInputBufCountBeforeOutBufDone++;
        return queueInputBufferNoBlock(-1, in_src, in_size);
    }

    // before OUTPUT_BUFFER_DONE, mjpeg only queue INPUT_QUEUE_BUFFER_NUM/2 frames;
    if (mVideoDecConfig.vFmt == VFORMAT_MJPEG && mQueuedInputBufCountBeforeOutBufDone < INPUT_QUEUE_BUFFER_NUM/2) {
        mQueuedInputBufCountBeforeOutBufDone++;
        return queueInputBufferNoBlock(-1, in_src, in_size);
    }
    return ret;
}


int HWVideoDecoderImpl::queueInputBufferInternal(int fd, uint8_t * data, int size)
{

    if (mInputBuffer.size() >= INPUT_QUEUE_BUFFER_NUM) {
        mStatus = HWVideoDecoder::DECODE_FAIL_AND_INPUT_FULL;
    }

    if (mStatus != HWVideoDecoder::RUNTIME_ERROR && mStatus >= HWVideoDecoder::INITED) {
        if (fd == -1)
            return mAmVideoDec->queueInputBuffer(mBitStreamId, data, 0 /*offset*/, size /*bytesUsed*/, 0 /*timestamp*/ );
        else if (fd > 0)
            return mAmVideoDec->queueInputBuffer(mBitStreamId, fd, 0 /*offset*/, size /*bytesUsed*/, 0 /*timestamp*/ );
    }

    CAMHAL_LOGE("bad status %d, skip queue", mStatus);
    return -1;
}

// data ok,  return 0;
// otherwise return -1
bool HWVideoDecoderImpl::findSOI(uint8_t* in_src, uint32_t in_size, int& offset) {
    offset = 0;
    if (in_size != 0) {
        while (offset < in_size - 1) {
            if (in_src[offset] == 0xFF && in_src[offset + 1] == 0xD8)
                return true;
        offset++;
        }
    }
    CAMHAL_LOGD("%s: not find SOI", __FUNCTION__);
    return false;
}

bool HWVideoDecoderImpl::findEOI(uint8_t* in_src, uint32_t in_size) {
    uint8_t EOI[] = {0xff, 0xd9};
    if (in_size != 0) {
        for (size_t i = 0; i <= in_size - sizeof(EOI); i++) {
            if (memcmp(in_src + i, EOI, sizeof(EOI)) == 0) {
                return true;
            }
        }
    }
    CAMHAL_LOGD("%s: not find EOI", __FUNCTION__);
    return false;
}

int HWVideoDecoderImpl::checkMjpegData(uint8_t* in_src, uint32_t in_size, bool check_mjpeg_wh)
{
    int offset = 0;
    if (findSOI(in_src, in_size, offset) && findEOI(in_src, in_size)) {
        if (check_mjpeg_wh) {
            int width = 0;
            int height = 0;
            while (offset < in_size - 1)
            {
                if (in_src[offset] == 0xFF)
                {
                    switch (in_src[offset + 1])
                    {
                    case 0xC0: // SOF0 (baseline JPEG)
                    case 0xC1: // SOF1 (extended sequential JPEG)
                    case 0xC2: // SOF2 (progressive JPEG)
                        height = in_src[offset + 5] * 256 + in_src[offset + 6];
                        width = in_src[offset + 7] * 256 + in_src[offset + 8];
                        if (height != mFormatHeight || width != mFormatWidth) {
                            CAMHAL_LOGW("this frame size is error");
                            return -1;
                        }
                        return 0;
                    }
                }
                offset++;
            }
        }
        return 0;
    }

    CAMHAL_LOGW("this frame is not standard mjpg data");
    return -1;
}

// todo: alloc buffer using ion and use hw copy.
int HWVideoDecoderImpl::queueInputBufferNoBlock(int in_fd, uint8_t* in_src, uint32_t in_size)
{
    int32_t err = 0;

    CAMHAL_LOGVV("%s E, fd %d, src %p, src_len %d", __FUNCTION__, in_fd, in_src, in_size);



    if (in_size >= INPUT_BUFFER_SIZE) {
        CAMHAL_LOGE("%s leave, src_len %d, too big. should be less than %d", __FUNCTION__, in_size, INPUT_BUFFER_SIZE);
        return -1;
    }

    std::lock_guard<std::mutex> lock(mInputLock);

    // h264 stream. near input buffer full, if not IDR, drop it;
    if ((mVideoDecConfig.vFmt == VFORMAT_H264 || mVideoDecConfig.vFmt == VFORMAT_HEVC) &&
        (mInputBuffer.size() > INPUT_QUEUE_BUFFER_NUM - 2) && false == isIDR( in_src, in_size)) {
        CAMHAL_LOGW("%s leave, not IDR & input buffer queue near full. drop it", __FUNCTION__);
        return -1;
    }

    // mjpeg stream. check header 0xffd8 & tail 0xffd9
    // if mCheckMjpegWH is true, also check width & height in 0xffc0
    if (mVideoDecConfig.vFmt == VFORMAT_MJPEG) {
        if ( 0 != checkMjpegData(in_src, in_size, mCheckMjpegWH) ) {
            // check fail.
            CAMHAL_LOGW("nonstandard mjpg data do not queue decoder");
            return -1;
        }
        mCheckMjpegWH = false;
    }

    buffer_item_t *buf_item;

    // case 1: no more free input buffers. should not happen
    if ( 0 != try_request_input_buffer(&buf_item) ) {
        CAMHAL_LOGE("%s %d leave, request input buffer fail", __FUNCTION__, __LINE__);
        return -1;
    }

    // case 2
    {

        memcpy( buf_item->buffer , in_src, in_size);

        //dumpInputTofile( buf_item->buffer, in_size);

        mInputBuffer[mBitStreamId] = (uint8_t*)buf_item;

        err = queueInputBufferInternal(buf_item->fd, buf_item->buffer, in_size);

        CAMHAL_LOGD("%s %d leave, err %d  bitstreamId %" PRId64 ", obj %p, buffer %p, buffer_size %d", __FUNCTION__, __LINE__,
                           err, mBitStreamId, buf_item, buf_item->buffer, in_size);

        mBitStreamId++;

        return err;
    }
}

bool HWVideoDecoderImpl::checkAndwaitForOutBuf(uint32_t ms) {

    std::unique_lock <std::mutex> lck(mOutputLock);

    if (mReadyOutBufQueue.size() > 0) {
        return true;
    }

    while (mReadyOutBufQueue.size() == 0) {
        auto ret = mOutBufReadyCondition.wait_for(lck, std::chrono::milliseconds(ms));
        if (ret == std::cv_status::timeout) {
            CAMHAL_LOGE("%s: Error waiting timeout %d ms for output buf", __FUNCTION__, ms);
            return false;
        }
    }
    return true;
}


// sync decode method
int HWVideoDecoderImpl::syncDecode(int in_fd, uint8_t*in_src, uint32_t in_size, Vector<StreamBuffer>& b, bool isJpegRequest)
{
    int ret = -1;
    int queue_input_ret = 0;
    bool outputBufReady = false;

    if (mStatus < HWVideoDecoder::INITED) {
        CAMHAL_LOGE("not initialized yet, status %d", mStatus);
        return -1;
    }

    // queueInputBufferNoBlock will copy in_src to input buffer queue
    // then, queue input buffer to decoder.
    queue_input_ret = queueInputBufferNoBlock(in_fd, in_src, in_size);
    if (0 != queue_input_ret) {
        CAMHAL_LOGE("line %d, queueInputBuffer failed, ret %d", __LINE__, queue_input_ret);
        return ret;
    }

    mWaitOutBufDurationMs = 30;
    outputBufReady = checkAndwaitForOutBuf(mWaitOutBufDurationMs);

    if (outputBufReady) {
        std::unique_lock <std::mutex> lck(mOutputLock);

        while (mReadyOutBufQueue.size() > 1) {
            // just keep only the latest buffer. queue back older buffers back.
            int outputBufIdx = mReadyOutBufQueue.front();
            mReadyOutBufQueue.pop();
            mAmVideoDec->queueOutputBuffer(outputBufIdx);
            CAMHAL_LOGVV("return back outBufferIdx %d", outputBufIdx);
        }

        int outputBufIdx = mReadyOutBufQueue.front();
        mReadyOutBufQueue.pop();

        if (outputBufIdx >= 0 && outputBufIdx < mOutputBufferNum) {
            struct mapInfo outputBufInfo = mOutputBufs[outputBufIdx];
#ifdef GE2D_ENABLE
            int dec_out_fd = outputBufInfo.fd;
            if (dec_out_fd > 0) {
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
                int index = 0;
#endif
                for (size_t i = 0; i < b.size(); i++) {
                    if (b[i].format == HAL_PIXEL_FORMAT_BLOB) {
                        CAMHAL_LOGE("%s:blob buffer bypass",__FUNCTION__);
                    } else {
                        if (b[i].share_fd != -1) {
                            if (isJpegRequest || !mEnableDewarp) {
                                mGE2D->ge2d_keep_ration_scale(b[i].share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b[i].width, b[i].height,
                                                       dec_out_fd, mDqWidth, mDqHeight, mFormatWidth, mFormatHeight);
                                mGE2D->doRotationAndMirror(b[i]);
                            } else {
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
                                dewarpInfo dewarpInfo;
                                DeWarp* GDCObj = nullptr;
                                CropInfo inputInfo;
                                //  fill dewarp info for check dewarp config
                                {
                                    dewarpInfo.i_width = mFormatWidth;
                                    dewarpInfo.i_height = mFormatHeight;
                                    dewarpInfo.o_width = b[i].width;
                                    dewarpInfo.o_height = b[i].height;
                                }
                                //  fill crop info for crop
                                {
                                    inputInfo.srcWidth = mDqWidth;
                                    inputInfo.srcHeight = mDqHeight;
                                    inputInfo.width = mFormatWidth;
                                    inputInfo.height = mFormatHeight;
                                }
                                dewarpcam2port port;
                                switch (index) {
                                    case 0:
                                        port = DEWARP_CAM2PORT_USB_PREVIEW;
                                        break;
                                    case 1:
                                        port = DEWARP_CAM2PORT_USB_RECORD;
                                        break;
                                    case 2:
                                        port = DEWARP_CAM2PORT_USB_CAPTURE;
                                        break;
                                    default:
                                        port = DEWARP_CAM2PORT_USB_PREVIEW;
                                        break;
                                }
                                bool needDestroy = isNeedDestroyDewarp(mPreDewarpInfo[port], dewarpInfo);
                                if (needDestroy) {
                                    DeWarp::putInstance(port);
                                }
                                CAMHAL_LOGD("buffer index %d, dewarp port %d, isNeedDestroyDewarp %d", index, port, needDestroy);
                                CameraConfig* config = CameraConfig::getInstance(port);
                                config->setCropInfo(inputInfo);
                                config->setInputWidth(mDqWidth);
                                config->setInputHeight(mDqHeight);
                                config->setOutputWidth(b[i].width);
                                config->setOutputHeight(b[i].height);
                                config->setOutputStride(b[i].stride);
                                GDCObj = DeWarp::getInstance(port, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
                                if (GDCObj) {
                                    GDCObj->mInput_fd = dec_out_fd;
                                    GDCObj->mOutput_fd = b[i].share_fd;
                                    GDCObj->gdc_do_fisheye_correction();
                                }
                                index++;
                                mPreDewarpInfo[port].o_width = b[i].width;
                                mPreDewarpInfo[port].o_height = b[i].height;
                                mPreDewarpInfo[port].i_width = mFormatWidth;
                                mPreDewarpInfo[port].i_height = mFormatHeight;
#endif
                            }
                            ret = 0;
                            if (property_get_bool("camera.debug.dump.decoder", false)) {
                                char dumpOutPath[256];
                                char dumpDecodePath[256];

                                /*=== dump yuv data after dewarp or ge2d ===*/
                                memset(&dumpOutPath[0], 0, sizeof(dumpOutPath));
                                snprintf(dumpOutPath, 256, "/data/vendor/camera/dst_%zu_%dx%d.yuv", i, b[i].width, b[i].height);
                                mDump -> dump(dumpIndex[i], b[i].img, (b[i].width * b[i].height * 3 / 2), dumpOutPath);

                                /*=== dump yuv data after decode ===*/
                                memset(&dumpDecodePath[0], 0, sizeof(dumpDecodePath));
                                snprintf(dumpDecodePath, 256, "/data/vendor/camera/decode/dst_%dx%d.yuv", mDqWidth, mDqHeight);
                                mDump -> dump(dumpIndex[i], outputBufInfo.vaddr, (mDqWidth * mDqHeight * 3 / 2), dumpDecodePath);
                                dumpIndex[i]++;
                            }
                       } else
                           CAMHAL_LOGE("%s:request buffer invalid fd",__FUNCTION__);
                   }
               }
           } else
               CAMHAL_LOGE("invalid decode out fd");
#endif
            mAmVideoDec->queueOutputBuffer(outputBufIdx);
            CAMHAL_LOGVV("return back outBufferIdx %d", outputBufIdx);
        } else {
            CAMHAL_LOGE("abnormal outputBufIdx %d", outputBufIdx);
        }
    } else {
         //dumpInputTofile(in_src, in_size);
    }

    return ret;
}


// async decode method
int HWVideoDecoderImpl::asyncDecodeDequeueOutput( Vector<StreamBuffer>& b, bool isJpegRequest)
{
    int ret = -1;
    bool outputBufReady = false;

    if (mStatus < HWVideoDecoder::INITED) {
        CAMHAL_LOGE("not initialized yet, status %d", mStatus);
        return -1;
    }

    mWaitOutBufDurationMs = 30;
    outputBufReady = checkAndwaitForOutBuf(mWaitOutBufDurationMs);

    if (outputBufReady) {
        std::unique_lock <std::mutex> lck(mOutputLock);

        while (mReadyOutBufQueue.size() > 1) {
            // just keep only the latest buffer. queue back older buffers back.
            int outputBufIdx = mReadyOutBufQueue.front();
            mReadyOutBufQueue.pop();
            mAmVideoDec->queueOutputBuffer(outputBufIdx);
            CAMHAL_LOGVV("return back outBufferIdx %d", outputBufIdx);
        }

        int outputBufIdx = mReadyOutBufQueue.front();
        mReadyOutBufQueue.pop();

        if (outputBufIdx >= 0 && outputBufIdx < mOutputBufferNum) {
            struct mapInfo outputBufInfo = mOutputBufs[outputBufIdx];
#ifdef GE2D_ENABLE
           int dec_out_fd = outputBufInfo.fd;
           if (dec_out_fd > 0) {
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
               int index = 0;
#endif
               for (size_t i = 0; i < b.size(); i++) {
                   if (b[i].format == HAL_PIXEL_FORMAT_BLOB) {
                       CAMHAL_LOGE("%s:blob buffer bypass",__FUNCTION__);
                   } else {
                       if (b[i].share_fd != -1) {
                           if (isJpegRequest || !mEnableDewarp) {
                                mGE2D->ge2d_keep_ration_scale(b[i].share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b[i].width, b[i].height,
                                                       dec_out_fd, mDqWidth, mDqHeight, mFormatWidth, mFormatHeight);
                                mGE2D->doRotationAndMirror(b[i]);
                           } else {
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
                                dewarpInfo dewarpInfo;
                                DeWarp* GDCObj = nullptr;
                                CropInfo inputInfo;
                                //  fill dewarp info for check dewarp config
                                {
                                    dewarpInfo.i_width = mFormatWidth;
                                    dewarpInfo.i_height = mFormatHeight;
                                    dewarpInfo.o_width = b[i].width;
                                    dewarpInfo.o_height = b[i].height;
                                }
                                //  fill crop info for crop
                                {
                                    inputInfo.srcWidth = mDqWidth;
                                    inputInfo.srcHeight = mDqHeight;
                                    inputInfo.width = mFormatWidth;
                                    inputInfo.height = mFormatHeight;
                                }
                                dewarpcam2port port;
                                switch (index) {
                                    case 0:
                                        port = DEWARP_CAM2PORT_USB_PREVIEW;
                                        break;
                                    case 1:
                                        port = DEWARP_CAM2PORT_USB_RECORD;
                                        break;
                                    case 2:
                                        port = DEWARP_CAM2PORT_USB_CAPTURE;
                                        break;
                                    default:
                                        port = DEWARP_CAM2PORT_USB_PREVIEW;
                                        break;
                                }
                                bool needDestroy = isNeedDestroyDewarp(mPreDewarpInfo[port], dewarpInfo);
                                if (needDestroy) {
                                    DeWarp::putInstance(port);
                                }
                                CAMHAL_LOGD("buffer index %d, dewarp port %d, isNeedDestroyDewarp %d", index, port, needDestroy);
                                CameraConfig* config = CameraConfig::getInstance(port);
                                config->setCropInfo(inputInfo);
                                config->setInputWidth(mDqWidth);
                                config->setInputHeight(mDqHeight);
                                config->setOutputWidth(b[i].width);
                                config->setOutputHeight(b[i].height);
                                config->setOutputStride(b[i].stride);
                                GDCObj = DeWarp::getInstance(port, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
                                if (GDCObj) {
                                    GDCObj->mInput_fd = dec_out_fd;
                                    GDCObj->mOutput_fd = b[i].share_fd;
                                    GDCObj->gdc_do_fisheye_correction();
                                }
                                index ++;
                                mPreDewarpInfo[port].o_width = b[i].width;
                                mPreDewarpInfo[port].o_height = b[i].height;
                                mPreDewarpInfo[port].i_width = mFormatWidth;
                                mPreDewarpInfo[port].i_height = mFormatHeight;
#endif
                           }
                           ret = 0;
                           if (property_get_bool("camera.debug.dump.decoder", false)) {
                                char dumpOutPath[256];
                                char dumpDecodePath[256];

                                /*=== dump yuv data after dewarp or ge2d ===*/
                                memset(&dumpOutPath[0], 0, sizeof(dumpOutPath));
                                snprintf(dumpOutPath, 256, "/data/vendor/camera/dst_%zu_%dx%d.yuv", i, b[i].width, b[i].height);
                                mDump -> dump(dumpIndex[i], b[i].img, (b[i].width * b[i].height * 3 / 2), dumpOutPath);

                                /*=== dump yuv data after decode ===*/
                                memset(&dumpDecodePath[0], 0, sizeof(dumpDecodePath));
                                snprintf(dumpDecodePath, 256, "/data/vendor/camera/decode/dst_%dx%d.yuv", mDqWidth, mDqHeight);
                                mDump -> dump(dumpIndex[i], outputBufInfo.vaddr, (mDqWidth * mDqHeight * 3 / 2), dumpDecodePath);
                                dumpIndex[i]++;
                            }
                       } else
                           CAMHAL_LOGE("%s:request buffer invalid fd",__FUNCTION__);
                   }
               }
           } else
               CAMHAL_LOGE("invalid decode out fd");
#endif
            mAmVideoDec->queueOutputBuffer(outputBufIdx);
            CAMHAL_LOGVV("return back outBufferIdx %d", outputBufIdx);

        } else {
            CAMHAL_LOGE("abnormal outputBufIdx %d", outputBufIdx);
        }

    } else {
         //dumpInputTofile(in_src, in_size);
    }

    return ret;
}


// return 0 on success.
// return -1 on fail
int HWVideoDecoderImpl::preAllocOutputBufferLocked(uint32_t requestedNumOfBuffers,
            int32_t width, uint32_t height)
{
    int ret = 0;
    mOutputBufferNum = requestedNumOfBuffers;
    mDqWidth = ALIGN(width, 64);
    mDqHeight = ALIGN(height, 64);
    mFormatWidth = width;
    mFormatHeight = height;
    uint32_t imagesize = (mDqWidth * mDqHeight * 3) / 2;

    mOutputBufs.resize(mOutputBufferNum);

    for (uint32_t i = 0; i < mOutputBufferNum; i++) {
        uint8_t* vaddr;
        int fd;


        vaddr = mION->alloc_buffer(imagesize, &fd);
        if (!vaddr) {
            mStatus = HWVideoDecoder::RUNTIME_ERROR;
            ret = -1;
            CAMHAL_LOGE("alloc buffer fail");
        }

        if (ret) {
            mStatus = HWVideoDecoder::RUNTIME_ERROR;
            CAMHAL_LOGE("alloc %d output Buffer fail", i);
            if (i > 0) {
                // free allocated output buffers
                for (int i = 0; i < mOutputBufs.size(); i++) {
                    if (mOutputBufs[i].fd > 0)
                        mION->free_buffer(mOutputBufs[i].fd);
                }


                CAMHAL_LOGE("free all allocated output buffers.");

            }
            return -1;
        }

        mapInfo outputBufInfo;
        outputBufInfo.vaddr = vaddr;
        outputBufInfo.size = imagesize;
        outputBufInfo.fd = fd;
        outputBufInfo.pictureId = i;

        mOutputBufs[i] = (outputBufInfo);

        CAMHAL_LOGD("alloc output Buffer idx %d, fd %d, size = %d, vaddr 0x%p \n", i, fd, imagesize, vaddr);
    }

    return 0;

}

int HWVideoDecoderImpl::queueOutputBuffersLocked()
{
    int ret = 0;

    mOutputBufferNum = mOutputBufs.size();
    if (mOutputBufferNum <= 0) {
        CAMHAL_LOGE("%s line %d error, output buffer num %d is invalid", __FUNCTION__, __LINE__, mOutputBufferNum);
        return -1;
    }

    if (0 != mAmVideoDec->setupOutputBufferNum(mOutputBufferNum) ) {
        CAMHAL_LOGE("setupOutputBufferNum %d, failed, ret %d", mOutputBufferNum, ret);
        return ret;
    }

    for (int ii = 0; ii < mOutputBufs.size(); ++ii) {
        if (mOutputBufs[ii].fdHasSetToPictureId == 0) {
            if (mOutputBufs[ii].pictureId >=0 && mOutputBufs[ii].fd >= 0) {
                ret = mAmVideoDec->createOutputBuffer(mOutputBufs[ii].pictureId, mOutputBufs[ii].fd);

                if (0 == ret) {
                    mOutputBufs[ii].fdHasSetToPictureId = 1;
                    CAMHAL_LOGI(" createOutputBuffer index %d, pictureid %d, fd %d success", ii, mOutputBufs[ii].pictureId, mOutputBufs[ii].fd);
                } else {
                    // exit on create fail.
                    CAMHAL_LOGE("createOutputBuffer fail, with pictureid %d, fd %d",  mOutputBufs[ii].pictureId, mOutputBufs[ii].fd);
                    ret = -1;
                    break;
                }

            } else {
                // exit on invalid picture or fd.
                CAMHAL_LOGE("invalid pictureid %d or fd %d", mOutputBufs[ii].pictureId, mOutputBufs[ii].fd);
                ret = -1;
                break;
            }
        }
    }
    return ret;
}

int HWVideoDecoderImpl::reconfigOutputBuffersLocked(uint32_t requestedMinNumOfBuffers_t,
            int32_t width_t, uint32_t height_t)
{
    int ret = 0;
    if (width_t != mFormatWidth || height_t != mFormatHeight) {
        //size_not_match
        CAMHAL_LOGE("initialize size %dx%d not match size from bitstream %dx%d", mFormatWidth, mFormatHeight, width_t, height_t);
        CAMHAL_LOGE("release pre-allocated output buffers and re-allocate");

        // clear all pre-allocated.
        for (int i = 0; i < mOutputBufs.size(); i++) {
            if (mOutputBufs[i].fd > 0)
                mION->free_buffer(mOutputBufs[i].fd);
        }

        // free objs stored in mOutputBufs.
        mOutputBufs.clear();

        if (0 != preAllocOutputBufferLocked(requestedMinNumOfBuffers_t, width_t, height_t) ) {
            return -1;
        }

        CAMHAL_LOGI("success allocated output buffers.");
        return 0;
    }

    // here size matched. requested min buf count bigger than pre-allocated.
    if (requestedMinNumOfBuffers_t > mOutputBufs.size()) {
        // need more_output_buffer
        int hasOutputBufCount = mOutputBufs.size();
        int appendOutputBufCount = requestedMinNumOfBuffers_t - hasOutputBufCount;
        mOutputBufferNum = requestedMinNumOfBuffers_t;

        uint32_t imagesize = (mDqWidth * mDqHeight * 3) / 2;

        // resize will keep old elements.
        mOutputBufs.resize(mOutputBufferNum);

        CAMHAL_LOGI("%d buffers, append %d buffers. total %d output buffers", hasOutputBufCount, appendOutputBufCount, requestedMinNumOfBuffers_t);


        for (uint32_t i = hasOutputBufCount; i < requestedMinNumOfBuffers_t; i++) {
            uint8_t* vaddr;
            int fd;

            vaddr = mION->alloc_buffer(imagesize, &fd);
            if (!vaddr) {
                mStatus = HWVideoDecoder::RUNTIME_ERROR;
                ret = -1;
                CAMHAL_LOGE("alloc buffer fail");
            }

            if (ret) {
                mStatus = HWVideoDecoder::RUNTIME_ERROR;
                CAMHAL_LOGE("alloc %d output Buffer fail", i);
                if (i > 0) {
                    // free allocated output buffers

                    for (int i = 0; i < mOutputBufs.size(); i++) {
                        if (mOutputBufs[i].fd > 0) {
                            mION->free_buffer(mOutputBufs[i].fd);
                            mOutputBufs[i].fd = -1;
                            mOutputBufs[i].vaddr = 0;
                            mOutputBufs[i].size = 0;
                        }
                    }

                    CAMHAL_LOGE("free all allocated output buffers.");

                }
                // free objs stored in mOutputBufs.
                mOutputBufs.clear();
                return -1;
            }

            mapInfo outputBufInfo;
            outputBufInfo.vaddr = vaddr;
            outputBufInfo.size = imagesize;
            outputBufInfo.fd = fd;
            outputBufInfo.pictureId = i;

            mOutputBufs[i] = (outputBufInfo);

            CAMHAL_LOGD("alloc output Buffer idx %d, fd %d, size = %d, vaddr 0x%p \n", i, fd, imagesize, vaddr);
        }
    } else {
        CAMHAL_LOGI("use pre allocated output buffers.");
    }

    return 0;
}

void HWVideoDecoderImpl::onOutputFormatChanged(uint32_t requestedNumOfBuffers,
            int32_t width, uint32_t height)
{

    CAMHAL_LOGD("onOutputFormatChanged bufnum %d, width %d, height %d\n",
                requestedNumOfBuffers,
                width,
                height);

    mStatus = HWVideoDecoder::OUTPUT_FORMAT_CHANGED;

    std::lock_guard<std::mutex> lock(mOutputLock);

    reconfigOutputBuffersLocked( requestedNumOfBuffers ,  width,  height);


    if ( 0 != queueOutputBuffersLocked() ) {
        CAMHAL_LOGE("queue(create) output buffers failed");
        return ;
    }

    if (mStatus == HWVideoDecoder::OUTPUT_FORMAT_CHANGED) {
        mStatus = HWVideoDecoder::OUTPUT_BUFFER_CREATED;
    }
    CAMHAL_LOGI("onOutputFormatChanged out timeUs %" PRId64 "\n", getTimeUs() );
}


/*
callback by decoder on decoded buffer ready.
case 1: no one want decoded buffer. just queue back output buffer.
case 2: async mode. notify and queue back.
case 3: sync mode. record latest output buffer to mReadyOutBufQueue. If there has been output buffers in mReadyOutBufQueue.
        queue older output buffer back, only keep the latest one.
*/
void HWVideoDecoderImpl::onOutputBufferDone(int32_t outBufferIdx, int64_t bitstreamId,
        uint32_t width, uint32_t height)
{

    CAMHAL_LOGD("onOutputBufferDone this %p, outBufferIdx %d, bitstreamId %" PRId64 ", output done %d\n",
                        this, outBufferIdx, bitstreamId, mOutputDoneCount);

    mStatus = HWVideoDecoder::OUTPUT_BUFFER_DONE;

    std::lock_guard<std::mutex> lock(mOutputLock);

    if (mDropOutBuf) {
        mAmVideoDec->queueOutputBuffer(outBufferIdx);
        CAMHAL_LOGD("directly queue back output buffer %d", outBufferIdx);
        return ;
    }
    // both sync & async decode using this mReadyOutBufQueue
    mReadyOutBufQueue.push(outBufferIdx);
    mOutBufReadyCondition.notify_all();

    mOutputDoneCount++;
}

void HWVideoDecoderImpl::onInputBufferDone(int32_t bitstreamId)
{
    std::lock_guard<std::mutex> lock(mInputLock);

    if (mInputBuffer.size() > 0) {

        buffer_item_t* item = (buffer_item_t*)mInputBuffer[bitstreamId];
        CAMHAL_LOGD("%s line %d, bitstreamId:%d, obj %p, buffer  %p, input done %d\n", __FUNCTION__, __LINE__, bitstreamId, item, item->buffer, mInputDoneCount);
        release_input_buffer(item);

        mInputBuffer.erase(bitstreamId);
    }
    mInputDoneCount++;
}

void HWVideoDecoderImpl::onUpdateDecInfo(const uint8_t* info, uint32_t isize)
{
    CAMHAL_LOGI("%s info:%s isize:%d\n", __func__, info, isize);
}


void HWVideoDecoderImpl::onFlushDone()
{
    CAMHAL_LOGI("onFlushDone\n");
    std::unique_lock <std::mutex> lck(mFlushedLock);
    mFlushed = true;
    mFlushedCondition.notify_all();
}


void HWVideoDecoderImpl::onResetDone()
{
    CAMHAL_LOGI("onResetDone\n");
}


void HWVideoDecoderImpl::onError(int32_t error)
{
    CAMHAL_LOGW("%s error %d\n", __func__, error);
    mStatus = HWVideoDecoder::RUNTIME_ERROR;
}


void HWVideoDecoderImpl::onEvent(uint32_t event, void* param, uint32_t paramSize)
{
    UNUSED(param);
    UNUSED(paramSize);
    CAMHAL_LOGI("%s event:%x, %s %d\n", __func__, event, (char*)param, paramSize);
}

// --------- end   HWVideoDecoderImpl ---------------

// ========= end  internal  helper functions ================================================



// ========= begin  interface functions ================================================

HWVideoDecoder::HWVideoDecoder()
{
    mPrivateImpl = new HWVideoDecoderImpl(this);
}


HWVideoDecoder::~HWVideoDecoder()
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        delete impl;
        mPrivateImpl = nullptr;
    }
}

bool HWVideoDecoder::initialize(uint32_t stream_type, uint32_t bitstream_width, uint32_t bitstream_height, uint32_t framerate, DecoderMode workMode)
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        return impl->initialize( stream_type,  bitstream_width,  bitstream_height, framerate, workMode);
    }
    return false;
}


void HWVideoDecoder::deinitialize()
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        impl->deinitialize();
    }
}

// async decode methods
int HWVideoDecoder::asyncDecodeQueueInput(int fd, uint8_t* src, uint32_t size)
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        return impl->asyncDecodeQueueInput(fd, src, size);
    }
    return -1;
}

int HWVideoDecoder::asyncDecodeDequeueOutput(Vector<StreamBuffer>& b, bool isJpegRequest)
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        return impl->asyncDecodeDequeueOutput(b, isJpegRequest);
    }
    return -1;
}


// sync decode method
int HWVideoDecoder::syncDecode(int in_fd, uint8_t* in_src, uint32_t in_size, Vector<StreamBuffer>& b, bool isJpegRequest)
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        return impl->syncDecode(in_fd, in_src, in_size, b, isJpegRequest);
    }
    return -1;

}
HWVideoDecoder::DecoderStatus HWVideoDecoder::getDecoderStatus()
{
    if (mPrivateImpl) {
        HWVideoDecoderImpl * impl = static_cast<HWVideoDecoderImpl *> (mPrivateImpl);
        return impl->getDecoderStatus();
    }
    return HWVideoDecoder::NOT_CONSTRUCTED;
}



