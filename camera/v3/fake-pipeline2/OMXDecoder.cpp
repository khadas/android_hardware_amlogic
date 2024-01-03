#define __STDC_FORMAT_MACROS
#include "OMXDecoder.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <sys/time.h>

#include <ui/GraphicBuffer.h>
#include <media/hardware/HardwareAPI.h>

#include <binder/IPCThreadState.h>
#ifdef GE2D_ENABLE
#include "fake-pipeline2/ge2d_stream.h"
#endif
#ifdef VICP_ENABLE
#include "fake-pipeline2/vicp_stream.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "OMXDecoder"
#endif


extern "C" {
#include "amuvm.h"
}
#include "IonIf.h"

#define OMX2_OUTPUT_BUFS_ALIGN_64 (64)

typedef enum MemType {
    VMALLOC_BUFFER = 0,
    ION_BUFFER,
    UVM_BUFFER,
    SHARED_FD,
} MemType;

static MemType mem_type = ION_BUFFER;

using namespace android;
OMX_CALLBACKTYPE OMXDecoder::kCallbacks = {
    &OnEvent, &OnEmptyBufferDone, &OnFillBufferDone
};

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

OMXDecoder::OMXDecoder(bool useDMABuffer, bool keepOriginalSize) {
    LOG_LINE("useDMABuffer=%d", useDMABuffer);
    mUseDMABuffer        = useDMABuffer;
    mKeepOriginalSize    = keepOriginalSize;
    mOutBufferNative = NULL;
    mLibHandle = NULL;
    mFreeHandle = NULL;
    mDeinit = NULL;
    mVDecoderHandle = NULL;
    mDequeueFailNum = 0;
    mContinuousVsyncFailNum = 0;
    memset(&mVideoOutputPortParam,0,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    memset(&mInOutPutBufferParam,0,sizeof(OMX_BUFFERHEADERTYPE));
    mOutBuffer = NULL;
    mNoFreeFlag = 0;
    mppBuffer = NULL;
    mOutBufferCount = 0;
    mInit = NULL;
    mGetHandle = NULL;
    mDecoderComponentName = NULL;
    memset(&mTempFrame,0,sizeof(mTempFrame));
    mUvmFd = -1;
#ifdef GE2D_ENABLE
    mGE2D = new ge2dTransform();
#ifdef VICP_ENABLE
    mVICP = new vicpTransform();
#endif
#endif
    mTimeOut = false;
    mOutWidth = 0;
    mOutHeight = 0;
    mFormat = 0;
    mStride = 0;
    memset(&mVideoInputPortParam, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    decoderType = DEC_NONE;
    mWaitVsyncDuration = 0;
    mInHeight = 0;
    mInWidth = 0;
    mEnableDewarp = false;
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.camhal.usbsensor.use.dewarp", property, "false");
    if (strstr(property, "true")) {
        mEnableDewarp = true;
    }
    VICPEnable = false;
}

OMXDecoder::~OMXDecoder() {
    CAMHAL_LOGD("%s\n", __FUNCTION__);
#ifdef GE2D_ENABLE
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#ifdef VICP_ENABLE
    if (mVICP) {
        delete mVICP;
        mVICP = nullptr;
    }
#endif
#endif

}

//Please don't use saveNativeBufferHdr() again if you want to use setParameters().
bool OMXDecoder::setParameters(uint32_t in_width, uint32_t in_height,
                               uint32_t out_width, uint32_t out_height,
                               uint32_t out_buffer_count) {
    if (!out_width || !out_height || !out_buffer_count) {
        CAMHAL_LOGE("Error parameters!!! in_width %u  in_height %u out_height %u  out_height %u  out_buffer_count %u",
                in_width, in_height, out_width, out_height, out_buffer_count);
        return false;
    }
    mInWidth = in_width;
    mInHeight = in_height;
    mOutWidth = out_width;
    mOutHeight = out_height;
    mOutBufferCount = out_buffer_count;
    CAMHAL_LOGD("in_width %u  in_height %u out_height %u  out_height %u  out_buffer_count %u",
                in_width, in_height, out_width, out_height, out_buffer_count);
    return true;
}

bool OMXDecoder::initialize(const char* name) {
    LOG_LINE();
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    mDequeueFailNum = 0;
    mContinuousVsyncFailNum = 0;
    mTimeOut = false;
    /*for (int i = 0; i < TempBufferNum; i++)
        mTempFrame[i] = (uint8_t*)malloc(mOutWidth*mOutHeight*3/2);
    */
    if (0 == strcmp(name,"mjpeg")) {
        decoderType = DEC_MJPEG;
        mDecoderComponentName = (char *)"OMX.amlogic.mjpeg.decoder.awesome2";
        if (mOutWidth != mInWidth || mOutHeight != mInHeight) {
            mOutWidth = mInWidth;
            mOutHeight = mInHeight;
            CAMHAL_LOGD("dec out size changed to w=%d, h=%d, using ge2d resize output", mOutWidth, mOutHeight);
        }
    } else if (0 == strcmp(name,"h264")) {
        decoderType = DEC_H264;
        mDecoderComponentName = (char *)"OMX.amlogic.avc.decoder.awesome2";
        if (mOutWidth != mInWidth || mOutHeight != mInHeight) {
            mOutWidth = mInWidth;
            mOutHeight = mInHeight;
            CAMHAL_LOGD("dec out size changed to w=%d, h=%d, using ge2d resize output", mOutWidth, mOutHeight);
        }
    } else {
        CAMHAL_LOGE("cannot support this format");
    }

    if (decoderType == DEC_H264)
        mWaitVsyncDuration = 30;
    else
        mWaitVsyncDuration = 200;

    mLibHandle = dlopen("libOmxCore.so", RTLD_NOW);
    if (mLibHandle != NULL) {
        mInit         =     (InitFunc) dlsym(mLibHandle, "OMX_Init");
        mDeinit     =     (DeinitFunc) dlsym(mLibHandle, "OMX_Deinit");
        mGetHandle  =     (GetHandleFunc) dlsym(mLibHandle, "OMX_GetHandle");
        mFreeHandle =     (FreeHandleFunc) dlsym(mLibHandle, "OMX_FreeHandle");
    } else {
        CAMHAL_LOGE("cannot open libOmxCore.so\n");
        return false;
    }


    if (OMX_ErrorNone != (*mInit)()) {
        CAMHAL_LOGE("OMX_Init fail!\n");
        return false;
    } else {
        CAMHAL_LOGD("OMX_Init success!\n");
    }

    eRet = (*mGetHandle)(&mVDecoderHandle, mDecoderComponentName, this, &kCallbacks);

    if (OMX_ErrorNone != eRet) {
        CAMHAL_LOGE("OMX_GetHandle fail!, eRet = %#x\n", eRet);
        return false;
    } else {
        CAMHAL_LOGD("OMX_GetHandle success!\n");
    }

    OMX_UUIDTYPE componentUUID;
    char pComponentName[128];
    OMX_VERSIONTYPE componentVersion;

    eRet = OMX_GetComponentVersion(mVDecoderHandle, pComponentName,
            &componentVersion, &mSpecVersion,
            &componentUUID);
    if (eRet != OMX_ErrorNone)
        CAMHAL_LOGE("OMX_GetComponentVersion failed!, eRet = %#x\n", eRet);
    else
        CAMHAL_LOGD("OMX_GetComponentVersion success!\n");

    OMX_PORT_PARAM_TYPE mPortParam;
    mPortParam.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    mPortParam.nVersion = mSpecVersion;

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamVideoInit,
            &mPortParam);
    if (eRet != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_GetParameter failed!\n");
        return false;
    }
    else
        CAMHAL_LOGD("OMX_GetParameter success!\n");

    /*configure input port*/
    mVideoInputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    mVideoInputPortParam.nVersion = mSpecVersion;
    mVideoInputPortParam.nPortIndex = mPortParam.nStartPortNumber;
    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition,
            &mVideoInputPortParam);
    if (eRet != OMX_ErrorNone) {
        CAMHAL_LOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition failed!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    CAMHAL_LOGD("[%s:%d]OMX_GetParameter mVideoInputPortParam.nBufferSize = %u, mVideoInputPortParam.format.video.eColorFormat =%d\n",
            __FUNCTION__, __LINE__,
            mVideoInputPortParam.nBufferSize, mVideoInputPortParam.format.video.eColorFormat);

    mVideoInputPortParam.format.video.nFrameWidth = mInWidth;
    mVideoInputPortParam.format.video.nFrameHeight = mInHeight;
    if (strcmp(name,"mjpeg") == 0)
        mVideoInputPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
    else if (strcmp(name,"h264") == 0)
        mVideoInputPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    mVideoInputPortParam.format.video.xFramerate = (15 << 16);
    mVideoInputPortParam.nBufferCountActual = 6;
    eRet = OMX_SetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoInputPortParam);
    if (OMX_ErrorNone != eRet) {
        CAMHAL_LOGE("[%s:%d]OMX_SetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    CAMHAL_LOGD("[%s:%d]OMX_SetParameter mVideoInputPortParam.nBufferSize = %u\n",
            __FUNCTION__, __LINE__,
            mVideoInputPortParam.nBufferSize);

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoInputPortParam);
    if (OMX_ErrorNone != eRet) {
        CAMHAL_LOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    CAMHAL_LOGD("[%s:%d]mVideoInputPortParam.nBufferCountActual = %u, mVideoInputPortParam.nBufferSize = %u, mVideoInputPortParam.format.video.eColorFormat =%d\n",
            __FUNCTION__, __LINE__,
            mVideoInputPortParam.nBufferCountActual,
            mVideoInputPortParam.nBufferSize,
            mVideoInputPortParam.format.video.eColorFormat);

    /*configure output port*/
    if (mKeepOriginalSize) {
        OMX_INDEXTYPE index;
        OMX_BOOL keepOriginalSize = OMX_TRUE;
        eRet = OMX_GetExtensionIndex(mVDecoderHandle, (char *)"OMX.amlogic.android.index.KeepOriginalSize", &index);
        if (eRet == OMX_ErrorNone) {
            CAMHAL_LOGD("OMX_GetExtensionIndex returned %#x", index);
            eRet = OMX_SetParameter(mVDecoderHandle, index, &keepOriginalSize);
            if (eRet != OMX_ErrorNone)
                CAMHAL_LOGW("setting keeporiginalsize returned error: %#x", eRet);
        } else
            CAMHAL_LOGW("OMX_GetExtensionIndex returned error: %#x", eRet);
    }

    if (mUseDMABuffer) {
        /*Enable DMA Buffer*/
        OMX_INDEXTYPE index;
        OMX_BOOL useDMABuffers = OMX_TRUE;
        eRet = OMX_GetExtensionIndex(mVDecoderHandle, (char *)"OMX.amlogic.android.index.EnableDMABuffers", &index);
        if (eRet == OMX_ErrorNone) {
            CAMHAL_LOGD("OMX_GetExtensionIndex returned %#x", index);
            eRet = OMX_SetParameter(mVDecoderHandle, index, &useDMABuffers);
            if (eRet != OMX_ErrorNone)
                CAMHAL_LOGW("setting enableDMABuffers returned error: %#x", eRet);
        } else
            CAMHAL_LOGW("OMX_GetExtensionIndex returned error: %#x", eRet);
    }

    mVideoOutputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    mVideoOutputPortParam.nVersion = mSpecVersion;
    mVideoOutputPortParam.nPortIndex = mPortParam.nStartPortNumber + 1;
    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
    CAMHAL_LOGD("[%s:%d]mVideoOutputPortParam.format.video.eCompressionFormat = %d\n",
            __FUNCTION__, __LINE__,
            mVideoOutputPortParam.format.video.eCompressionFormat);

    if (OMX_ErrorNone != eRet)
        CAMHAL_LOGE("[%s:%d]mVDecoderHandle OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);

    mVideoOutputPortParam.nBufferCountActual = mOutBufferCount;
    mVideoOutputPortParam.format.video.nFrameWidth = mOutWidth;
    mVideoOutputPortParam.format.video.nFrameHeight = mOutHeight;
    mVideoOutputPortParam.format.video.nStride = ROUND_16(mOutWidth);
    mVideoOutputPortParam.format.video.nSliceHeight = ROUND_16(mOutHeight);
    mVideoOutputPortParam.format.video.eColorFormat = static_cast<OMX_COLOR_FORMATTYPE>(HAL_PIXEL_FORMAT_YCrCb_420_SP);//OMX_COLOR_FormatYUV420SemiPlanar;
    mVideoOutputPortParam.format.video.xFramerate = (15 << 16);
    mVideoOutputPortParam.nBufferSize = YUV_SIZE(mVideoOutputPortParam.format.video.nStride,
            mVideoOutputPortParam.format.video.nSliceHeight);

    eRet = OMX_SetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
    if (OMX_ErrorNone != eRet) {
        CAMHAL_LOGE("[%s:%d]OMX_SetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
    if (OMX_ErrorNone != eRet) {
        CAMHAL_LOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    CAMHAL_LOGD("[%s:%d]mVideoOutputPortParam.nBufferCountActual = %u, mVideoOutputPortParam.nBufferSize = %u\n",
            __FUNCTION__, __LINE__, mVideoOutputPortParam.nBufferCountActual, mVideoOutputPortParam.nBufferSize);

    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

    return true;
}

template<class T>
void OMXDecoder::InitOMXParams(T *params) {
    memset(params, 0, sizeof(T));
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

void OMXDecoder::start()
{
    LOG_LINE();
    if (mem_type != SHARED_FD) {
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        AutoMutex l(mOutputBufferLock);
        while (!mListOfOutputBufferHeader.empty()) {
            pBufferHdr = *mListOfOutputBufferHeader.begin();
            OMX_FillThisBuffer(mVDecoderHandle, pBufferHdr);
            mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
        }
    }
}

void OMXDecoder::deinitialize()
{
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    OMX_STATETYPE eState1, eState2;
    LOG_LINE();
    /*for (int i = 0; i < TempBufferNum; i++)
            free(mTempFrame[i]);
    */
    mNoFreeFlag = 1;
    if (mVDecoderHandle == NULL) {
        CAMHAL_LOGD("mVDecoderHandle is NULL, alread deinitialized or not initialized at all");
        return;
    }

    OMX_SendCommand(mVDecoderHandle, OMX_CommandFlush, OMX_ALL, NULL);

    usleep(100 * 1000);

    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

    do {
        eRet = OMX_GetState(mVDecoderHandle, &eState1);
        usleep(5*1000);
    } while (OMX_StateIdle != eState1 && OMX_StateInvalid != eState1);

    if (eRet != OMX_ErrorNone) {
        CAMHAL_LOGE("Switch to StateIdle failed");
    }
    CAMHAL_LOGD("Switch to StateIdle successful");

    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

    while (mListOfInputBufferHeader.size() != mVideoInputPortParam.nBufferCountActual
            || mListOfOutputBufferHeader.size() != mVideoOutputPortParam.nBufferCountActual) {
        CAMHAL_LOGD("Input: %zu/%u  Output: %zu/%u",
                mListOfInputBufferHeader.size(), mVideoInputPortParam.nBufferCountActual,
                mListOfOutputBufferHeader.size(), mVideoOutputPortParam.nBufferCountActual);
        usleep(5000);
    }

    freeBuffers();

    do {
        eRet = OMX_GetState(mVDecoderHandle, &eState2);
        usleep(5*1000);
    } while (OMX_StateLoaded != eState2 && OMX_StateInvalid != eState2);

    if (eRet != OMX_ErrorNone) {
        CAMHAL_LOGE("Switch to StateLoaded failed");
    }
    CAMHAL_LOGD("Switch to StateLoaded successful");

    (*mFreeHandle)(static_cast<OMX_HANDLETYPE *>(mVDecoderHandle));
    (*mDeinit)();
    if (mLibHandle != NULL) {
        dlclose(mLibHandle);
        mLibHandle = NULL;
        CAMHAL_LOGD("dlclose lib handle at %p and null it", mLibHandle);
    }

}

OMX_BUFFERHEADERTYPE* OMXDecoder::dequeueInputBuffer()
{
    AutoMutex l(mInputBufferLock);
    OMX_BUFFERHEADERTYPE *ret = NULL;
    if (!mListOfInputBufferHeader.empty()) {
        ret = *mListOfInputBufferHeader.begin();
        mListOfInputBufferHeader.erase(mListOfInputBufferHeader.begin());
    }
    return ret;
}

void OMXDecoder::queueInputBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    if (pBufferHdr != NULL) {
        if (mNoFreeFlag) {
            CAMHAL_LOGD("exiting!! return to input queue.");
            AutoMutex l(mInputBufferLock);
            mListOfInputBufferHeader.push_back(pBufferHdr);
        } else {
            OMX_EmptyThisBuffer(mVDecoderHandle, pBufferHdr);
        }
    } else {
        CAMHAL_LOGD("queueInputBuffer invalid pBufferHdr(NULL)\n");
    }
}

OMX_BUFFERHEADERTYPE* OMXDecoder::dequeueOutputBuffer()
{
    AutoMutex l(mOutputBufferLock);
    OMX_BUFFERHEADERTYPE *ret = NULL;
    if (!mListOfOutputBufferHeader.empty()) {
        ret = *mListOfOutputBufferHeader.begin();
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
    return ret;
}

bool OMXDecoder::hasReadyOutputBuffer()
{
    AutoMutex l(mOutputBufferLock);
    if (mListOfOutputBufferHeader.empty()) {
        return false;
    }
    return true;
}

void OMXDecoder::releaseOutputBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    if (pBufferHdr != NULL)
        if (mNoFreeFlag) {
            CAMHAL_LOGD("exiting!! return to output queue.");
            AutoMutex l(mOutputBufferLock);
            mListOfOutputBufferHeader.push_back(pBufferHdr);
        } else
            OMX_FillThisBuffer(mVDecoderHandle, pBufferHdr);
        else
            CAMHAL_LOGD("releaseOutputBuffer can't find pBufferHdr .\n");
}

bool OMXDecoder::uvm_buffer_init() {
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    if (mUvmFd <= 0) {
        mUvmFd = amuvm_open();
        if (mUvmFd < 0) {
            CAMHAL_LOGE("open uvm device fail");
            return -1;
        }
    }

    int i = 0;
    uint32_t width = mOutWidth;
    uint32_t height = mOutHeight;
    //if (mDoubleWriteMode == 0x3) {
    width = (width  + (OMX2_OUTPUT_BUFS_ALIGN_64 - 1)) & (~(OMX2_OUTPUT_BUFS_ALIGN_64 - 1));
    height = (height + (OMX2_OUTPUT_BUFS_ALIGN_64 - 1)) & (~(OMX2_OUTPUT_BUFS_ALIGN_64 - 1));
    //}
    CAMHAL_LOGI("AllocDmaBuffers uvm mDecOutWidth:%d mDecOutHeight:%d, %dx%d", mOutWidth, mOutHeight, width, height);
    while (i < mOutBufferCount) {
        int shared_fd = -1;
        int buffer_size = width * height * 3 / 2;
        int ret =  amuvm_allocate(mUvmFd, buffer_size,
                    width, height, UVM_IMM_ALLOC,&shared_fd);
        if (ret < 0) {
            CAMHAL_LOGE("uvm device alloc fail");
            return -1;
        }

        /*uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0);
        if (MAP_FAILED == cpu_ptr) {
            CAMHAL_LOGE("uvm mmap error!\n");
            amuvm_free(shared_fd);
            return -1;
        }

        LOG_LINE("amuvm_allocate shared fd=%d, vaddr=%p", shared_fd, cpu_ptr);
        munmap(cpu_ptr, mOutWidth * mOutHeight * 3 / 2);*/

        //mDmaBufferAlloced = true;
        OMX_BUFFERHEADERTYPE* bufferHdr;
        eRet = OMX_UseBuffer(mVDecoderHandle,
                    &bufferHdr,
                    mVideoOutputPortParam.nPortIndex,
                    (OMX_PTR)(long)shared_fd,
                    mVideoOutputPortParam.nBufferSize,
                    (OMX_U8*)0xFFFF);//(OMX_U8*)cpu_ptr);
        if (OMX_ErrorNone != eRet) {
            CAMHAL_LOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
            return false;
        }
        bufferHdr->pAppPrivate = (OMX_PTR)NULL;
        mListOfOutputBufferHeader.push_back(bufferHdr);
        i++;
    }
    return 0;
}

bool OMXDecoder::normal_buffer_init(int buffer_size){
    OMX_ERRORTYPE eRet = OMX_ErrorNone;

    for (uint32_t i = 0; i < mOutBufferCount; i++) {
        if (mUseDMABuffer) {
            OMX_BUFFERHEADERTYPE* bufferHdr;
            OMX_U8 *ptr = (OMX_U8 *)(malloc(buffer_size * sizeof(OMX_U8)));
            if (!ptr) {
                CAMHAL_LOGE("out of memory when allocation output buffers");
                return false;
            }
            eRet = OMX_UseBuffer(mVDecoderHandle, &bufferHdr,
                    mVideoOutputPortParam.nPortIndex, NULL,
                    buffer_size, ptr);
            if (OMX_ErrorNone != eRet) {
                CAMHAL_LOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
                return false;
            }
            CAMHAL_LOGD("OMX_UseBuffer output %p", bufferHdr);
            mListOfOutputBufferHeader.push_back(bufferHdr);
        } else {
            sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(mOutBufferNative[i].handle,
                        GraphicBuffer::TAKE_HANDLE,
                        mOutWidth,
                        mOutHeight,
                        mFormat,
                        1,
                        GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK,
                        mStride));
            OMX_STRING nameEnable = const_cast<OMX_STRING>(
                    "OMX.google.android.index.enableAndroidNativeBuffers");
            OMX_INDEXTYPE indexEnable;
            OMX_ERRORTYPE err = OMX_GetExtensionIndex(mVDecoderHandle, nameEnable, &indexEnable);
            if (err == OMX_ErrorNone) {
                EnableAndroidNativeBuffersParams params;
                InitOMXParams(&params);
                params.nPortIndex = mVideoOutputPortParam.nPortIndex;
                params.enable = OMX_TRUE;

                err = OMX_SetParameter(mVDecoderHandle, indexEnable, &params);
                if (err != OMX_ErrorNone)
                    CAMHAL_LOGE("setParameter error 1.");
            } else {
                CAMHAL_LOGE("getExtensionIndex failed 1.");
            }

            OMX_STRING name = const_cast<OMX_STRING>(
                    "OMX.google.android.index.useAndroidNativeBuffer");
            OMX_INDEXTYPE index;
            err = OMX_GetExtensionIndex(mVDecoderHandle, name, &index);
            if (err != OMX_ErrorNone) {
                CAMHAL_LOGE("getExtensionIndex failed 2.");
            }
            OMX_BUFFERHEADERTYPE* header;
            OMX_VERSIONTYPE ver;
            ver.s.nVersionMajor = 1;
            ver.s.nVersionMinor = 0;
            ver.s.nRevision = 0;
            ver.s.nStep = 0;
            UseAndroidNativeBufferParams params = {
                sizeof(UseAndroidNativeBufferParams), ver, mVideoOutputPortParam.nPortIndex, NULL,
                &header, graphicBuffer
            };

            err = OMX_SetParameter(mVDecoderHandle, index, &params);
            if (err != OMX_ErrorNone)
                CAMHAL_LOGE("setParameter error 2.");

            mOutBufferNative[i].pBuffer = header;
            if (mOutBufferNative[i].isQueued)
                mListOfOutputBufferHeader.push_back(header);
        }
    }
    return true;
}

bool OMXDecoder::ion_buffer_init() {
    int shared_fd = -1;
    int buffer_size = mOutWidth * mOutHeight * 3 / 2 ;
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    IONInterface* ion = IONInterface::get_instance();
    OMX_U32 uAlignedBytes = (((mVideoOutputPortParam.nBufferSize
                    + ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1)
                & ~(ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1)));
    for (uint32_t i = 0; i < mVideoOutputPortParam.nBufferCountActual; i++) {
        OMX_BUFFERHEADERTYPE* bufferHdr;
        OMX_U8 *cpu_ptr;
        if (mUseDMABuffer) {
            CAMHAL_LOGD("try to allocate dma buffer %d", i);
            cpu_ptr = ion->alloc_buffer(buffer_size, &shared_fd);
            if (!cpu_ptr) {
                CAMHAL_LOGE("allocate dma buffer %d failed", i);
                return false;
            }
            CAMHAL_LOGD("AllocDmaBuffers shared_fd=%d, cpu_ptr=%p\n", shared_fd, cpu_ptr);
        } else {
            cpu_ptr = (OMX_U8 *)(malloc(uAlignedBytes * sizeof(OMX_U8)));
            if (!cpu_ptr) {
                CAMHAL_LOGE("out of memory when allocation output buffers");
                return false;
            }
        }
        eRet = OMX_UseBuffer(mVDecoderHandle,
                &bufferHdr,
                mVideoOutputPortParam.nPortIndex,
                (OMX_PTR)(long)shared_fd,
                mVideoOutputPortParam.nBufferSize,
                (OMX_U8*)cpu_ptr);
        if (OMX_ErrorNone != eRet) {
            CAMHAL_LOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
            return false;
        }
        bufferHdr->pAppPrivate = (OMX_PTR)0xff; //fake data
        mListOfOutputBufferHeader.push_back(bufferHdr);
    }
    return true;
}

bool OMXDecoder::do_buffer_init() {
    int shared_fd;
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    for (uint32_t i = 0; i < mVideoOutputPortParam.nBufferCountActual; i++) {
        OMX_BUFFERHEADERTYPE* bufferHdr;
        OMX_U8 *cpu_ptr = (OMX_U8 *)0xFFFFFFFF;  //fake address
        shared_fd = -1; //fake fd
        eRet = OMX_UseBuffer(mVDecoderHandle,
                &bufferHdr,
                mVideoOutputPortParam.nPortIndex,
                (OMX_PTR)(long)shared_fd,
                mVideoOutputPortParam.nBufferSize,
                (OMX_U8*)cpu_ptr);
        if (OMX_ErrorNone != eRet) {
            CAMHAL_LOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
            return false;
        }
        bufferHdr->pAppPrivate = (OMX_PTR)0xff; //fake data
        mListOfOutputBufferHeader.push_back(bufferHdr);
        CAMHAL_LOGD("%s: bufferHdr = %p\n", __FUNCTION__, bufferHdr);
    }
    return true;
}

void OMXDecoder::do_buffer_free(void) {
#if 0
    while (!mListOfOutputBufferHeader.empty()) {
        CAMHAL_LOGD("do_free_buffer: erase mListOfOutputBufferHeader");
        OMX_BUFFERHEADERTYPE *pBufferHdr = *mListOfOutputBufferHeader.begin();
        pBufferHdr->pPlatformPrivate = NULL;
        OMX_FreeBuffer(mVDecoderHandle, mVideoOutputPortParam.nPortIndex, pBufferHdr);
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
#else
    OMX_ERRORTYPE eRet = OMX_ErrorNone;

    for (uint32_t i = 0; i < mVideoOutputPortParam.nBufferCountActual; i++) {
        OMX_BUFFERHEADERTYPE *bufferHdr = (OMX_BUFFERHEADERTYPE *)malloc(sizeof(OMX_BUFFERHEADERTYPE));
        bufferHdr->pPlatformPrivate = NULL;

#if 0
        eRet = OMX_FreeBuffer(mVDecoderHandle,
                &bufferHdr,
                mVideoOutputPortParam.nPortIndex,
                (OMX_PTR)shared_fd,
                mVideoOutputPortParam.nBufferSize,
                (OMX_U8*)cpu_ptr);
#else
        eRet = OMX_FreeBuffer(mVDecoderHandle, mVideoOutputPortParam.nPortIndex, bufferHdr);
#endif

        if (OMX_ErrorNone != eRet) {
            CAMHAL_LOGE("OMX_FreeBuffer on output port failed! eRet = %#x\n", eRet);
        }
    }
#endif
}

bool OMXDecoder::prepareBuffers()
{
    LOG_LINE();
    OMX_U32 uAlignedBytes = (((mVideoInputPortParam.nBufferSize + ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1) & ~(ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1)));
    mNoFreeFlag = 0;
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    for (uint32_t i = 0; i < mVideoInputPortParam.nBufferCountActual; i++) {
        OMX_BUFFERHEADERTYPE* bufferHdr;
        OMX_U8 *ptr = (OMX_U8 *)(malloc(uAlignedBytes * sizeof(OMX_U8)));
        if (!ptr) {
            CAMHAL_LOGE("out of memory when allocation input buffers");
            return false;
        }
        eRet = OMX_UseBuffer(mVDecoderHandle, &bufferHdr,
                mVideoInputPortParam.nPortIndex, NULL,
                mVideoInputPortParam.nBufferSize, ptr);

        if (OMX_ErrorNone != eRet) {
            CAMHAL_LOGE("OMX_UseBuffer on input port failed! eRet = %#x\n", eRet);
            return false;
        }
        CAMHAL_LOGD("OMX_UseBuffer input %p", bufferHdr);
        mListOfInputBufferHeader.push_back(bufferHdr);
    }

    OMX_STATETYPE eState1, eState2;
    int buffer_size = mOutWidth * mOutHeight * 3 / 2 ;
    CAMHAL_LOGD("Allocating %u buffers from a native window of size %u on "
            "output port", mOutBufferCount, buffer_size);

    switch (mem_type) {
    case VMALLOC_BUFFER:
        LOG_LINE();
        normal_buffer_init(buffer_size);
        break;
    case ION_BUFFER:
        LOG_LINE();
        ion_buffer_init();
        break;
    case UVM_BUFFER:
        LOG_LINE();
        uvm_buffer_init();
        break;
    case SHARED_FD:
        do_buffer_init();
        break;
    default:
        LOG_LINE();
        normal_buffer_init(buffer_size);
        break;
    }

    do {
        OMX_GetState(mVDecoderHandle, &eState1);
        usleep(10*1000);
    } while (OMX_StateIdle != eState1 && OMX_StateInvalid != eState1);
    CAMHAL_LOGD("STATETRANS FROM LOADED TO IDLE COMPLETED, eRet =%x\n", eRet);

    //swith to Excuting state
    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

    do {
        OMX_GetState(mVDecoderHandle, &eState2);
        usleep(10*1000);
    } while (OMX_StateExecuting != eState2 && OMX_StateInvalid != eState2);

    CAMHAL_LOGD("STATETRANS FROM IDLE TO EXECUTING COMPLETED, eRet =%x\n", eRet);

    return true;
}

void OMXDecoder::free_ion_buffer(void) {
    IONInterface* ion = IONInterface::get_instance();
    while (!mListOfOutputBufferHeader.empty()) {
        OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfOutputBufferHeader.begin());
        OMX_ERRORTYPE err;
        if (bufferHdr != NULL) {
            if (mUseDMABuffer) {
                ion->free_buffer((int)(long)bufferHdr->pPlatformPrivate);
                err = OMX_FreeBuffer(mVDecoderHandle,mVideoOutputPortParam.nPortIndex,bufferHdr);
                if (OMX_ErrorNone != err) {
                    CAMHAL_LOGE("%d, OutPortIndex: %d\n",__LINE__,mVideoOutputPortParam.nPortIndex);
                }
            } else if (bufferHdr->pBuffer != NULL)
                free(bufferHdr->pBuffer);
        }
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
}

void OMXDecoder::free_normal_buffer(void) {
    OMX_ERRORTYPE err;
    while (!mListOfOutputBufferHeader.empty()) {
        OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfOutputBufferHeader.begin());
        if (bufferHdr != NULL) {
            OMX_U8 *pOut = bufferHdr->pBuffer;
            CAMHAL_LOGD("OMX_FreeBuffer output %p", bufferHdr);
            err = OMX_FreeBuffer(mVDecoderHandle, mVideoOutputPortParam.nPortIndex, bufferHdr);
            if (OMX_ErrorNone != err) {
                CAMHAL_LOGE("%d, OutPortIndex: %d\n", __LINE__, mVideoOutputPortParam.nPortIndex);
            }
            if (pOut != NULL) {
                free(pOut);
                pOut = NULL;
            }
        }
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
}

void OMXDecoder::free_uvm_buffer() {
    while (!mListOfOutputBufferHeader.empty()) {
           OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfOutputBufferHeader.begin());
           OMX_ERRORTYPE err;
           if (bufferHdr != NULL) {
               if (mUseDMABuffer) {
                   LOG_LINE("try to unmap uvm vaddr %p, fd: %d", bufferHdr->pBuffer, (int)(long)(bufferHdr->pPlatformPrivate));

                   //munmap(bufferHdr->pBuffer, mOutWidth * mOutHeight * 3 / 2);

                   amuvm_free((int)(long)(bufferHdr->pPlatformPrivate));

                   CAMHAL_LOGD("bufferHdr->pAppPrivate: %p", bufferHdr->pAppPrivate);

                   err = OMX_FreeBuffer(mVDecoderHandle,mVideoOutputPortParam.nPortIndex,bufferHdr);
                   if (OMX_ErrorNone != err) {
                       CAMHAL_LOGE("%d, OutPortIndex: %d\n",__LINE__,mVideoOutputPortParam.nPortIndex);
                   }
               } else if (bufferHdr->pBuffer != NULL)
                   free(bufferHdr->pBuffer);
           }
           mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
    close(mUvmFd);
}



void OMXDecoder::freeBuffers() {
    OMX_ERRORTYPE err;
    unsigned int i;
    while (!mListOfInputBufferHeader.empty()) {
        OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfInputBufferHeader.begin());
        if (bufferHdr != NULL) {
            OMX_U8 *pIn = bufferHdr->pBuffer;
            CAMHAL_LOGD("OMX_FreeBuffer input %p", bufferHdr);
            err = OMX_FreeBuffer(mVDecoderHandle, mVideoInputPortParam.nPortIndex, bufferHdr);
            if (OMX_ErrorNone != err) {
                CAMHAL_LOGE("%d, InPortIndex: %d\n", __LINE__, mVideoInputPortParam.nPortIndex);
            }
            if (pIn != NULL) {
                free(pIn);
                pIn = NULL;
            }
        }
        mListOfInputBufferHeader.erase(mListOfInputBufferHeader.begin());
    }

    if (mUseDMABuffer) {
        switch (mem_type) {
        case VMALLOC_BUFFER:
            LOG_LINE();
            free_normal_buffer();
            break;
        case ION_BUFFER:
            LOG_LINE();
            free_ion_buffer();
            break;
        case UVM_BUFFER:
            LOG_LINE();
            free_uvm_buffer();
            break;
        case SHARED_FD:
            do_buffer_free();
            break;
        default:
            LOG_LINE();
            free_normal_buffer();
            break;
        }
    } else {
        for (i = 0; i < mOutBufferCount; i++) {
            OMX_BUFFERHEADERTYPE* bufferHdr = mOutBufferNative[i].pBuffer;
            if (bufferHdr != NULL) {
                err = OMX_FreeBuffer(mVDecoderHandle, mVideoOutputPortParam.nPortIndex, bufferHdr);
                if (OMX_ErrorNone != err) {
                    CAMHAL_LOGE("%d, OutPortIndex: %d\n", __LINE__, mVideoOutputPortParam.nPortIndex);
                }
            }
        }

        if (mOutBufferNative != NULL) {
            free(mOutBufferNative);
            mOutBufferNative = NULL;
        }
    }
}

OMX_ERRORTYPE OMXDecoder::WaitForState(OMX_HANDLETYPE hComponent, OMX_STATETYPE eTestState, OMX_STATETYPE eTestState2)
{
    LOG_LINE();
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STATETYPE eState;

    eError = OMX_GetState(hComponent, &eState);

    while (eState != eTestState && eState != eTestState2)
    {
        sleep(1);
        eError = OMX_GetState(hComponent, &eState);
    }
    return eError;
}

OMX_ERRORTYPE OMXDecoder::OnEvent(
        OMX_IN OMX_EVENTTYPE eEvent,
        OMX_IN OMX_U32 nData1,
        OMX_IN OMX_U32 nData2,
        OMX_IN OMX_PTR)
{
    LOG_LINE();
    CAMHAL_LOGD("data1 = %u, data2 = %u, event = %d\n", nData1, nData2, eEvent);

    if (eEvent == OMX_EventBufferFlag)
    {
        CAMHAL_LOGD("Got OMX_EventBufferFlag event\n");
    }
    else if (eEvent == OMX_EventError)
    {
        CAMHAL_LOGD("Got OMX_EventError event\n");
    }
    else if (eEvent == OMX_EventPortSettingsChanged)
    {
        OMX_ERRORTYPE omx_error_type = OMX_ErrorNone;
        CAMHAL_LOGD("Got OMX_EventPortSettingsChanged event\n");
        if (OMX_IndexParamPortDefinition == (OMX_INDEXTYPE) nData2)
        {
            mVideoOutputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
            mVideoOutputPortParam.nVersion = mSpecVersion;
            mVideoOutputPortParam.nPortIndex = nData1;
            omx_error_type = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
            if (omx_error_type != OMX_ErrorNone)
            {
                CAMHAL_LOGD("OMX_GetParameter FAILED");
            }
            CAMHAL_LOGD("w= %u, h= %u\n", mVideoOutputPortParam.format.video.nFrameWidth, mVideoOutputPortParam.format.video.nFrameHeight);
            if (mOutWidth != mVideoOutputPortParam.format.video.nFrameWidth || mOutHeight != mVideoOutputPortParam.format.video.nFrameHeight)
            {
                CAMHAL_LOGD("Dynamic resolution changes triggered");
            }
        }
    }
    else
    {
        CAMHAL_LOGD("not support event!\n");
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::emptyBufferDone(OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    //CAMHAL_LOGD("%s ++", __func__);
    AutoMutex l(mInputBufferLock);
    mListOfInputBufferHeader.push_back(pBuffer);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::fillBufferDone(OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    //CAMHAL_LOGD("%s ++", __func__);
    AutoMutex l(mOutputBufferLock);
    mListOfOutputBufferHeader.push_back(pBuffer);
    //send signal
    Mutex::Autolock lock(mOMXControlMutex);
    mOMXVSync.signal();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::OnEvent(
        OMX_IN OMX_HANDLETYPE,/*omit hComponent to avoid unused variable warning*/
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_EVENTTYPE eEvent,
        OMX_IN OMX_U32 nData1,
        OMX_IN OMX_U32 nData2,
        OMX_IN OMX_PTR pEventData)
{
    OMXDecoder *instance = static_cast<OMXDecoder *>(pAppData);
    return instance->OnEvent(eEvent, nData1, nData2, pEventData);
}

OMX_ERRORTYPE OMXDecoder::OnEmptyBufferDone(
        OMX_IN OMX_HANDLETYPE,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMXDecoder *instance = static_cast<OMXDecoder *>(pAppData);
    return instance->emptyBufferDone(pBuffer);
}

OMX_ERRORTYPE OMXDecoder::OnFillBufferDone(
        OMX_IN OMX_HANDLETYPE,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMXDecoder *instance = static_cast<OMXDecoder *>(pAppData);
    return instance->fillBufferDone(pBuffer);
}

void OMXDecoder::QueueBuffer(uint8_t* src, size_t size) {
    static OMX_TICKS timeStamp = 0;
    OMX_BUFFERHEADERTYPE *pInPutBufferHdr = NULL;
    pInPutBufferHdr = dequeueInputBuffer();

    if (pInPutBufferHdr && pInPutBufferHdr->pBuffer) {
        //CAMHAL_LOGD("omx queue input buf %p \n", pInPutBufferHdr);
        memcpy(pInPutBufferHdr->pBuffer, src, size);
        pInPutBufferHdr->nFilledLen = size;
        pInPutBufferHdr->nOffset = 0;
        pInPutBufferHdr->nTimeStamp = timeStamp;
        pInPutBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
        queueInputBuffer(pInPutBufferHdr);
        timeStamp += 33 * 1000; //44
    } else {
        CAMHAL_LOGE("no more input bufs");
    }
}

void OMXDecoder::SetOutputBuffer(int share_fd, uint8_t* addr) {
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
    AutoMutex l(mOutputBufferLock);

    if (!mListOfOutputBufferHeader.empty()) {
        pBufferHdr = *mListOfOutputBufferHeader.begin();
        pBufferHdr->pAppPrivate = (void *)(long)share_fd;
        pBufferHdr->pPlatformPrivate = (void *)(long)share_fd;
        pBufferHdr->pBuffer = addr;

        CAMHAL_LOGV("SetOutputBuffer %p, OMX_FillThisBuffer, share_fd=%d, pAppPrivate=%d, pPlatformPrivate=%d",
                pBufferHdr,
                share_fd,
                (int)(long)(pBufferHdr->pAppPrivate),
                (int)(long)(pBufferHdr->pPlatformPrivate));

        OMX_FillThisBuffer(mVDecoderHandle, pBufferHdr);
        CAMHAL_LOGV("SetOutputBuffer: erase mListOfOutputBufferHeader");
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
}


int OMXDecoder::DequeueBufferAndClean(Vector<StreamBuffer>& b , bool isJpegRequest) {

    OMX_BUFFERHEADERTYPE *pOutPutBufferHdr = NULL;
    {
        AutoMutex l(mOutputBufferLock);
        while (mListOfOutputBufferHeader.size() > 1) {
            pOutPutBufferHdr = *mListOfOutputBufferHeader.begin();
            mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
            if (pOutPutBufferHdr != NULL) {
                if (mNoFreeFlag) {
                    CAMHAL_LOGD("exiting!! return to output queue.");
                    mListOfOutputBufferHeader.push_back(pOutPutBufferHdr);
                } else
                    OMX_FillThisBuffer(mVDecoderHandle, pOutPutBufferHdr);
            } else
                CAMHAL_LOGD("releaseOutputBuffer can't find pBufferHdr .\n");
        }
    }

    int ret = DequeueBuffer(b, isJpegRequest);
    return ret;
}
int OMXDecoder::DequeueBuffer(Vector<StreamBuffer>& b, bool isJpegRequest) {
        int ret = 0;

        OMX_BUFFERHEADERTYPE *pOutPutBufferHdr = NULL;
        pOutPutBufferHdr = dequeueOutputBuffer();
        if (pOutPutBufferHdr == NULL) {
            //dequeue fail
            CAMHAL_LOGE("%s:dequeue fail",__FUNCTION__);
            ret = 0;
        } else {
            //CAMHAL_LOGD("omx pOutPutBufferHdr = %p\n", pOutPutBufferHdr);
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
            int index = 0;
#endif
            int omx_share_fd = (int)(long)pOutPutBufferHdr->pPlatformPrivate;
            for (size_t i = 0; i < b.size(); i++) {
                uint32_t dst_w = b[i].width;
                uint32_t dst_h = b[i].height;
                uint32_t src_w = mInWidth;
                uint32_t src_h = mInHeight;
                int dst_fd =  b[i].share_fd;
                uint8_t *dst_buf = b[i].img;

                if (b[i].format == HAL_PIXEL_FORMAT_BLOB) {
                    CAMHAL_LOGD("%s:blob buffer bypass",__FUNCTION__);
                } else {
#ifdef GE2D_ENABLE
                    if (dst_fd != -1) {
                        if (VICPEnable) {
#ifdef VICP_ENABLE
                            if (mVICP) {
                                if (src_w == dst_w && src_h == dst_h) {
                                    mVICP->vicp_copy(dst_fd, omx_share_fd, dst_w, dst_h, VICP_COLOR_FMT_YCrCb_420_SP_NV21);
                                } else {
                                    mVICP->vicp_keep_ration_scale(dst_fd, VICP_COLOR_FMT_YCrCb_420_SP_NV21, dst_w, dst_h,
                                                  omx_share_fd, src_w, src_h);
                                }
                            } else {
                                 CAMHAL_LOGE("%s:vicp object is null",__FUNCTION__);
                            }
#endif
                        } else if (mEnableDewarp) {
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
                            dewarpInfo dewarpInfo;
                            DeWarp* GDCObj = nullptr;
                                //  fill dewarp info for check dewarp config
                                {
                                    dewarpInfo.i_width = mInWidth;
                                    dewarpInfo.i_height = mInHeight;
                                    dewarpInfo.o_width = b[i].width;
                                    dewarpInfo.o_height = b[i].height;
                                }
                                dewarpcam2port port;
                                switch (index) {
                                    case 0:
                                        port = DEWARP_CAM2PORT_PREVIEW;
                                        break;
                                    case 1:
                                        port = DEWARP_CAM2PORT_CAPTURE;
                                        break;
                                    case 2:
                                        port = DEWARP_CAM2PORT_RECORD;
                                        break;
                                    default:
                                        port = DEWARP_CAM2PORT_PREVIEW;
                                        break;
                                }
                                bool needDestroy = isNeedDestroyDewarp(mPreDewarpInfo[port], dewarpInfo);
                                if (needDestroy) {
                                    DeWarp::putInstance(port);
                                }
                                ALOGD("buffer index %d, dewarp port %d, isNeedDestroyDewarp %d", index, port, needDestroy);
                                CameraConfig* config = CameraConfig::getInstance(port);
                                config->setInputWidth(mInWidth);
                                config->setInputHeight(mInHeight);
                                config->setWidth(b[i].width);
                                config->setHeight(b[i].height);
                                GDCObj = DeWarp::getInstance(port, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
                                if (GDCObj) {
                                    GDCObj->mInput_fd = omx_share_fd;
                                    GDCObj->mOutput_fd = dst_fd;
                                    GDCObj->gdc_do_fisheye_correction();
                                }
                                index++;
                                mPreDewarpInfo[port].o_width = b[i].width;
                                mPreDewarpInfo[port].o_height = b[i].height;
                                mPreDewarpInfo[port].i_width = mInWidth;
                                mPreDewarpInfo[port].i_height = mInHeight;
#endif
                            }else {
                            if (mGE2D) {
                                if (src_w == dst_w && src_h == dst_h) {
                                    //CAMHAL_LOGD("%s ge2d copy");
                                    mGE2D->ge2d_copy(dst_fd, omx_share_fd, dst_w, dst_h, ge2dTransform::NV12);
                                } else {
                                    // scale & crop
                                    //CAMHAL_LOGD("%s ge2d scale to dst size", __FUNCTION__);
                                    mGE2D->ge2d_keep_ration_scale(dst_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, dst_w, dst_h,
                                                  omx_share_fd, src_w, src_h);
                                }
                            } else {
                                CAMHAL_LOGE("%s:ge2d object is null",__FUNCTION__);
                            }
                        }
                    } else if (src_w == dst_w && src_h == dst_h) {
                        if (mem_type == UVM_BUFFER) {
                            uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, pOutPutBufferHdr->nFilledLen, PROT_READ | PROT_WRITE, MAP_SHARED, (int)(long)pOutPutBufferHdr->pPlatformPrivate, 0);
                            memcpy(dst_buf, cpu_ptr, pOutPutBufferHdr->nFilledLen);
                            munmap(cpu_ptr, pOutPutBufferHdr->nFilledLen);
                        } else {
                            //CAMHAL_LOGD("%s ge2d. no dst_fd, using sw memcpy");
                            memcpy(dst_buf, pOutPutBufferHdr->pBuffer, pOutPutBufferHdr->nFilledLen);
                        }
                    } else {
                        CAMHAL_LOGE(" ge2d src w&h not equal dst w&h. hw dec not supported");
                    }
                    if (mGE2D) {
                        mGE2D->doRotationAndMirror(b[i]);
                    }
#else
                    //no ge2d support
                    if (src_w == dst_w && src_h == dst_h) {
                        if (mem_type == UVM_BUFFER) {
                            uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, pOutPutBufferHdr->nFilledLen, PROT_READ | PROT_WRITE, MAP_SHARED, (int)(long)pOutPutBufferHdr->pPlatformPrivate, 0);
                            memcpy(dst_buf, cpu_ptr, pOutPutBufferHdr->nFilledLen);
                            if (munmap(cpu_ptr, pOutPutBufferHdr->nFilledLen) < 0)
                                CAMHAL_LOGE("%s:%d munmap failed errno=%d", __FUNCTION__,__LINE__,errno);
                        } else {
                            memcpy(dst_buf, pOutPutBufferHdr->pBuffer, pOutPutBufferHdr->nFilledLen);
                        }
                    } else {
                        CAMHAL_LOGE("src w&h not equal dst w&h. hw dec not supported");
                    }
#endif
                }
            }
            releaseOutputBuffer(pOutPutBufferHdr);
            ret = 1;
        }
        return ret;
}

bool OMXDecoder::OMXWaitForVSync(nsecs_t reltime) {
    //ATRACE_CALL();
    int res;
    Mutex::Autolock lock(mOMXControlMutex);
    res = mOMXVSync.waitRelative(mOMXControlMutex, reltime);
    if (res != OK) {
        CAMHAL_LOGE("%s: Error waiting for VSync signal: %d", __FUNCTION__, res);
        return false;
    }
    return true;
}

int OMXDecoder::Decode(uint8_t*src, size_t src_size, Vector<StreamBuffer>& b, bool isJpegRequest) {
    int ret = 0;

//    if (dst_buf == NULL) {
//        CAMHAL_LOGE("%s: dst_fd=%d, dst_buf=%p", __FUNCTION__, dst_fd, dst_buf);
//        return ret;
//    }
//
//    if (dst_fd > 0 && mem_type == SHARED_FD) {
//        SetOutputBuffer(dst_fd, dst_buf);
//        QueueBuffer(src, src_size);
//
//        if (OMXWaitForVSync(mWaitVsyncDuration*1000*1000) == false) {
//            mDequeueFailNum ++;
//            CAMHAL_LOGD("decode failed %d", mDequeueFailNum);
//            return ret;
//        }
//
//        ret = 1;
//
//        return ret;
//    }

    QueueBuffer(src, src_size);

    bool state = true;
    if ( false == hasReadyOutputBuffer() ) {
        // no ready output buf. wait
        state = OMXWaitForVSync(mWaitVsyncDuration*1000*1000);
    } else {
        // has ready output buf. state should be true.
        state = true;
    }

    if (state) {
        mContinuousVsyncFailNum = 0;
        ret = DequeueBuffer(b, isJpegRequest);
        if (!ret) {
            if (mDequeueFailNum ++ > MAX_POLLING_COUNT) {
                mTimeOut = true;
            }

            CAMHAL_LOGD("%s:Polling number=%d",__FUNCTION__,mDequeueFailNum);
        }
    } else {
        if ( mContinuousVsyncFailNum++ > MAX_CONTINUE_VSYNC_FAIL_COUNT) {
            mTimeOut = true;
        }
        CAMHAL_LOGD("%s: OMX Vsync error num = %d",__FUNCTION__, mContinuousVsyncFailNum);
        ret = 0;
    }

    return ret;
}

int OMXDecoder::DecodeAsync(uint8_t*src, size_t src_size, Vector<StreamBuffer>& b, bool isJpegRequest) {
    int ret = 0;
    bool state = true;
    if ( false == hasReadyOutputBuffer() ) {
        // no ready output buf. wait
        state = OMXWaitForVSync(mWaitVsyncDuration*1000*1000);
    } else {
        // has ready output buf. state should be true.
        state = true;
    }

    if (state) {
        mContinuousVsyncFailNum = 0;
        ret = DequeueBufferAndClean(b, isJpegRequest);
        if (!ret) {
           if (mDequeueFailNum ++ > MAX_POLLING_COUNT) {
                mTimeOut = true;
            }
            CAMHAL_LOGD("%s:Polling number=%d",__FUNCTION__,mDequeueFailNum);
        }
    } else {
        if (mContinuousVsyncFailNum++ > MAX_CONTINUE_VSYNC_FAIL_COUNT) {
            mTimeOut = true;
        }
        CAMHAL_LOGD("%s: OMX Vsync error num = %d",__FUNCTION__, mContinuousVsyncFailNum);
        ret = 0;
    }
    return ret;
}

void OMXDecoder::PutInBuffer(uint8_t* src, size_t size){
    return QueueBuffer(src, size);
}

