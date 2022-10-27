#define __STDC_FORMAT_MACROS
#include "OMXDecoder.h"
#include <linux/ion.h>
#include <ion/ion.h>
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
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "OMXDecoder"
#endif


extern "C" {
#include "amuvm.h"
}
#include "ion_4.12.h"

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
#endif
    mTimeOut = false;
    mOutWidth = 0;
    mOutHeight = 0;
    mFormat = 0;
    mStride = 0;
    mIonFd = -1;
    memset(&mVideoInputPortParam, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
}

OMXDecoder::~OMXDecoder() {
    ALOGD("%s\n", __FUNCTION__);
#ifdef GE2D_ENABLE
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#endif
}

//Please don't use saveNativeBufferHdr() again if you want to use setParameters().
bool OMXDecoder::setParameters(uint32_t in_width, uint32_t in_height,
                               uint32_t out_width, uint32_t out_height,
                               uint32_t out_buffer_count) {
    if (!out_width || !out_height || !out_buffer_count) {
        ALOGE("Error parameters!!! in_width %u  in_height %u out_height %u  out_height %u  out_buffer_count %u",
                in_width, in_height, out_width, out_height, out_buffer_count);
        return false;
    }
    mInWidth = in_width;
    mInHeight = in_height;
    mOutWidth = out_width;
    mOutHeight = out_height;
    mOutBufferCount = out_buffer_count;
    ALOGD("in_width %u  in_height %u out_height %u  out_height %u  out_buffer_count %u",
                in_width, in_height, out_width, out_height, out_buffer_count);
    return true;
}

bool OMXDecoder::initialize(const char* name) {
    LOG_LINE();
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    mDequeueFailNum = 0;
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
            ALOGD("dec out size changed to w=%d, h=%d, using ge2d resize output", mOutWidth, mOutHeight);
        }
    } else if (0 == strcmp(name,"h264")) {
        decoderType = DEC_H264;
        mDecoderComponentName = (char *)"OMX.amlogic.avc.decoder.awesome2";
    } else {
        ALOGE("cannot support this format");
    }

    mLibHandle = dlopen("libOmxCore.so", RTLD_NOW);
    if (mLibHandle != NULL) {
        mInit         =     (InitFunc) dlsym(mLibHandle, "OMX_Init");
        mDeinit     =     (DeinitFunc) dlsym(mLibHandle, "OMX_Deinit");
        mGetHandle  =     (GetHandleFunc) dlsym(mLibHandle, "OMX_GetHandle");
        mFreeHandle =     (FreeHandleFunc) dlsym(mLibHandle, "OMX_FreeHandle");
    } else {
        ALOGE("cannot open libOmxCore.so\n");
        return false;
    }


    if (OMX_ErrorNone != (*mInit)()) {
        ALOGE("OMX_Init fail!\n");
        return false;
    } else {
        ALOGD("OMX_Init success!\n");
    }

    eRet = (*mGetHandle)(&mVDecoderHandle, mDecoderComponentName, this, &kCallbacks);

    if (OMX_ErrorNone != eRet) {
        ALOGE("OMX_GetHandle fail!, eRet = %#x\n", eRet);
        return false;
    } else {
        ALOGD("OMX_GetHandle success!\n");
    }

    OMX_UUIDTYPE componentUUID;
    char pComponentName[128];
    OMX_VERSIONTYPE componentVersion;

    eRet = OMX_GetComponentVersion(mVDecoderHandle, pComponentName,
            &componentVersion, &mSpecVersion,
            &componentUUID);
    if (eRet != OMX_ErrorNone)
        ALOGE("OMX_GetComponentVersion failed!, eRet = %#x\n", eRet);
    else
        ALOGD("OMX_GetComponentVersion success!\n");

    OMX_PORT_PARAM_TYPE mPortParam;
    mPortParam.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    mPortParam.nVersion = mSpecVersion;

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamVideoInit,
            &mPortParam);
    if (eRet != OMX_ErrorNone) {
        ALOGE("OMX_GetParameter failed!\n");
        return false;
    }
    else
        ALOGD("OMX_GetParameter succes!\n");

    /*configure input port*/
    mVideoInputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    mVideoInputPortParam.nVersion = mSpecVersion;
    mVideoInputPortParam.nPortIndex = mPortParam.nStartPortNumber;
    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition,
            &mVideoInputPortParam);
    if (eRet != OMX_ErrorNone) {
        ALOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition failed!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    ALOGD("[%s:%d]OMX_GetParameter mVideoInputPortParam.nBufferSize = %zu, mVideoInputPortParam.format.video.eColorFormat =%d\n",
            __FUNCTION__, __LINE__,
            mVideoInputPortParam.nBufferSize, mVideoInputPortParam.format.video.eColorFormat);

    mVideoInputPortParam.format.video.nFrameWidth = mInWidth;
    mVideoInputPortParam.format.video.nFrameHeight = mInHeight;
    if (strcmp(name,"mjpeg") == 0)
        mVideoInputPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
    else if (strcmp(name,"h264") == 0)
        mVideoInputPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    mVideoInputPortParam.format.video.xFramerate = (15 << 16);
    eRet = OMX_SetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoInputPortParam);
    if (OMX_ErrorNone != eRet) {
        ALOGE("[%s:%d]OMX_SetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    ALOGD("[%s:%d]OMX_SetParameter mVideoInputPortParam.nBufferSize = %zu\n",
            __FUNCTION__, __LINE__,
            mVideoInputPortParam.nBufferSize);

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoInputPortParam);
    if (OMX_ErrorNone != eRet) {
        ALOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    ALOGD("[%s:%d]mVideoInputPortParam.nBufferCountActual = %zu, mVideoInputPortParam.nBufferSize = %zu, mVideoInputPortParam.format.video.eColorFormat =%d\n",
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
            ALOGD("OMX_GetExtensionIndex returned %#x", index);
            eRet = OMX_SetParameter(mVDecoderHandle, index, &keepOriginalSize);
            if (eRet != OMX_ErrorNone)
                ALOGW("setting keeporiginalsize returned error: %#x", eRet);
        } else
            ALOGW("OMX_GetExtensionIndex returned error: %#x", eRet);
    }

    if (mUseDMABuffer) {
        /*Enable DMA Buffer*/
        OMX_INDEXTYPE index;
        OMX_BOOL useDMABuffers = OMX_TRUE;
        eRet = OMX_GetExtensionIndex(mVDecoderHandle, (char *)"OMX.amlogic.android.index.EnableDMABuffers", &index);
        if (eRet == OMX_ErrorNone) {
            ALOGD("OMX_GetExtensionIndex returned %#x", index);
            eRet = OMX_SetParameter(mVDecoderHandle, index, &useDMABuffers);
            if (eRet != OMX_ErrorNone)
                ALOGW("setting enableDMABuffers returned error: %#x", eRet);
        } else
            ALOGW("OMX_GetExtensionIndex returned error: %#x", eRet);
    }

    mVideoOutputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    mVideoOutputPortParam.nVersion = mSpecVersion;
    mVideoOutputPortParam.nPortIndex = mPortParam.nStartPortNumber + 1;
    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
    ALOGD("[%s:%d]mVideoOutputPortParam.format.video.eCompressionFormat = %d\n",
            __FUNCTION__, __LINE__,
            mVideoOutputPortParam.format.video.eCompressionFormat);

    if (OMX_ErrorNone != eRet)
        ALOGE("[%s:%d]mVDecoderHandle OMX_IndexParamPortDefinition error!! eRet = %#x\n",
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
        ALOGE("[%s:%d]OMX_SetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    eRet = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
    if (OMX_ErrorNone != eRet) {
        ALOGE("[%s:%d]OMX_GetParameter OMX_IndexParamPortDefinition error!! eRet = %#x\n",
                __FUNCTION__, __LINE__, eRet);
        return false;
    }

    ALOGD("[%s:%d]mVideoOutputPortParam.nBufferCountActual = %zu, mVideoOutputPortParam.nBufferSize = %zu\n",
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
        ALOGD("mVDecoderHandle is NULL, alread deinitialized or not initialized at all");
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
        ALOGE("Switch to StateIdle failed");
    }
    ALOGD("Switch to StateIdle successful");

    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

    while (mListOfInputBufferHeader.size() != mVideoInputPortParam.nBufferCountActual
            || mListOfOutputBufferHeader.size() != mVideoOutputPortParam.nBufferCountActual) {
        ALOGD("Input: %u/%u  Output: %u/%u",
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
        ALOGE("Switch to StateLoaded failed");
    }
    ALOGD("Switch to StateLoaded successful");

    (*mFreeHandle)(static_cast<OMX_HANDLETYPE *>(mVDecoderHandle));
    (*mDeinit)();
    if (mLibHandle != NULL) {
        dlclose(mLibHandle);
        mLibHandle = NULL;
        ALOGD("dlclose lib handle at %p and null it", mLibHandle);
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
    if (pBufferHdr != NULL)
        if (mNoFreeFlag) {
            ALOGD("exiting!! return to input queue.");
            AutoMutex l(mInputBufferLock);
            mListOfInputBufferHeader.push_back(pBufferHdr);
        } else
            OMX_EmptyThisBuffer(mVDecoderHandle, pBufferHdr);
        else
            ALOGD("queueInputBuffer can't find pBufferHdr .\n");
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

void OMXDecoder::releaseOutputBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    if (pBufferHdr != NULL)
        if (mNoFreeFlag) {
            ALOGD("exiting!! return to output queue.");
            AutoMutex l(mOutputBufferLock);
            mListOfOutputBufferHeader.push_back(pBufferHdr);
        } else
            OMX_FillThisBuffer(mVDecoderHandle, pBufferHdr);
        else
            ALOGD("releaseOutputBuffer can't find pBufferHdr .\n");
}

bool OMXDecoder::uvm_buffer_init() {
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    if (mUvmFd <= 0) {
        mUvmFd = amuvm_open();
        if (mUvmFd < 0) {
            ALOGE("open uvm device fail");
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
    ALOGI("AllocDmaBuffers uvm mDecOutWidth:%d mDecOutHeight:%d, %dx%d", mOutWidth, mOutHeight, width, height);
    while (i < mOutBufferCount) {
        int shared_fd = -1;
        int buffer_size = width * height * 3 / 2;
        int ret =  amuvm_allocate(mUvmFd, buffer_size,
                    width, height, UVM_IMM_ALLOC,&shared_fd);
        if (ret < 0) {
            ALOGE("uvm device alloc fail");
            return -1;
        }

        /*uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0);
        if (MAP_FAILED == cpu_ptr) {
            ALOGE("uvm mmap error!\n");
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
            ALOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
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
                ALOGE("out of memory when allocation output buffers");
                return false;
            }
            eRet = OMX_UseBuffer(mVDecoderHandle, &bufferHdr,
                    mVideoOutputPortParam.nPortIndex, NULL,
                    buffer_size, ptr);
            if (OMX_ErrorNone != eRet) {
                ALOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
                return false;
            }
            ALOGD("OMX_UseBuffer output %p", bufferHdr);
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
                    ALOGE("setParameter error 1.");
            } else {
                ALOGE("getExtensionIndex failed 1.");
            }

            OMX_STRING name = const_cast<OMX_STRING>(
                    "OMX.google.android.index.useAndroidNativeBuffer");
            OMX_INDEXTYPE index;
            err = OMX_GetExtensionIndex(mVDecoderHandle, name, &index);
            if (err != OMX_ErrorNone) {
                ALOGE("getExtensionIndex failed 2.");
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
                ALOGE("setParameter error 2.");

            mOutBufferNative[i].pBuffer = header;
            if (mOutBufferNative[i].isQueued)
                mListOfOutputBufferHeader.push_back(header);
        }
    }
    return true;
}

int OMXDecoder::ion_alloc_buffer(int ion_fd, size_t size,
                        int* pShareFd, unsigned int flag, unsigned int alloc_hmask) {
    int ret = -1;
    int num_heaps = 0;
    unsigned int heap_mask = 0;

    if (ion_query_heap_cnt(ion_fd, &num_heaps) >= 0) {
        ALOGD("num_heaps=%d\n", num_heaps);
        struct ion_heap_data *const heaps =
                (struct ion_heap_data *)malloc(num_heaps * sizeof(struct ion_heap_data));
        if (heaps != NULL && num_heaps) {
            if (ion_query_get_heaps(ion_fd, num_heaps, heaps) >= 0) {
                for (int i = 0; i != num_heaps; ++i) {
                    ALOGD("match name heaps[%d].type=%d, heap_id=%d, name=%s\n", i, heaps[i].type, heaps[i].heap_id, heaps[i].name);
                    if ((1 << heaps[i].type) == alloc_hmask && 0 == strcmp(heaps[i].name, "ion-dev")) {
                        heap_mask = 1 << heaps[i].heap_id;
                        ALOGD("%d, m=%x, 1<<heap_id=%x, heap_mask=%x, name=%s, alloc_hmask=%x\n",
                                heaps[i].type, 1<<heaps[i].type,
                                heaps[i].heap_id, heap_mask,
                                heaps[i].name, alloc_hmask);
                        break;
                    }
                }
            }
            free(heaps);
            if (heap_mask)
                ret = ion_alloc_fd(ion_fd, size, 0, heap_mask, flag, pShareFd);
            else
                ALOGE("don't find match heap!!\n");
        } else {
            if (heaps != NULL)
                free(heaps);
            ALOGE("heaps is NULL or no heaps,num_heaps=%d\n", num_heaps);
        }
    } else {
        ALOGE("query_heap_cnt fail! no ion heaps for alloc!!!\n");
    }
    if (ret < 0) {
        ALOGE("ion_alloc failed, %s\n", strerror(errno));
        return -ENOMEM;
    }
    return ret;
}

bool OMXDecoder::ion_buffer_init() {

    ion_user_handle_t ion_hnd = 0;
    int shared_fd = -1;
    int ret = 0;
    int buffer_size = mOutWidth * mOutHeight * 3 / 2 ;
    OMX_ERRORTYPE eRet = OMX_ErrorNone;
    unsigned int ion_flag = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    mIonFd = ion_open();
    if (mIonFd < 0) {
        ALOGE("ion open failed! fd= %d \n", mIonFd);
        return false;
    }

    int is_legacy = ion_is_legacy(mIonFd);
    OMX_U32 uAlignedBytes = (((mVideoOutputPortParam.nBufferSize
                    + ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1)
                & ~(ZTE_BUF_ADDR_ALIGNMENT_VALUE - 1)));
    for (uint32_t i = 0; i < mVideoOutputPortParam.nBufferCountActual; i++) {
        OMX_BUFFERHEADERTYPE* bufferHdr;
        OMX_U8 *cpu_ptr;

        if (mUseDMABuffer) {
            ALOGD("try to allocate dma buffer %d", i);
            if (is_legacy == 1) {
                ret = ion_alloc(mIonFd, buffer_size, 0, 1 << ION_HEAP_TYPE_CUSTOM, ion_flag, &ion_hnd);
                if (ret) {
                    ALOGE("ion alloc error, errno=%d",ret);
                    ion_close(mIonFd);
                    return false;
                } else
                    ALOGD("allocating dma buffer %d success, handle %d", i, ion_hnd);
                ret = ion_share(mIonFd, ion_hnd, &shared_fd);
                if (ret) {
                    ALOGE("ion share error!, errno=%d\n",ret);
                    ion_free(mIonFd, ion_hnd);
                    ion_close(mIonFd);
                    return false;
                }
            } else {
                ret = ion_alloc_buffer(mIonFd, buffer_size, &shared_fd, (1<<30), ION_HEAP_TYPE_DMA_MASK);
                if (ret < 0) {
                    ALOGE("%s:ion try alloc memory error!\n",__FUNCTION__);
                    return false;
                }
            }
            cpu_ptr = (OMX_U8*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    shared_fd, 0);
            if (MAP_FAILED == cpu_ptr) {
                ALOGE("ion mmap error!\n");
                close(shared_fd);
                ion_free(mIonFd, ion_hnd);
                ion_close(mIonFd);
                return false;
            }
            if (cpu_ptr == NULL)
                ALOGD("cpu_ptr is NULL");
            ALOGD("AllocDmaBuffers ion_hnd=%d, shared_fd=%d, cpu_ptr=%p\n", ion_hnd, shared_fd, cpu_ptr);
        } else {
            cpu_ptr = (OMX_U8 *)(malloc(uAlignedBytes * sizeof(OMX_U8)));
            if (!cpu_ptr) {
                ALOGE("out of memory when allocation output buffers");
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
            ALOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
            return false;
        }
        bufferHdr->pAppPrivate = (OMX_PTR)(long)ion_hnd;
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
            ALOGE("OMX_UseBuffer on output port failed! eRet = %#x\n", eRet);
            return false;
        }
        bufferHdr->pAppPrivate = (OMX_PTR)0xff; //fake data
        mListOfOutputBufferHeader.push_back(bufferHdr);
        ALOGD("%s: bufferHdr = %p\n", __FUNCTION__, bufferHdr);
    }
    return true;
}

void OMXDecoder::do_buffer_free(void) {
#if 0
    while (!mListOfOutputBufferHeader.empty()) {
        ALOGD("do_free_buffer: erase mListOfOutputBufferHeader");
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
            ALOGE("OMX_FreeBuffer on output port failed! eRet = %#x\n", eRet);
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
            ALOGE("out of memory when allocation input buffers");
            return false;
        }
        eRet = OMX_UseBuffer(mVDecoderHandle, &bufferHdr,
                mVideoInputPortParam.nPortIndex, NULL,
                mVideoInputPortParam.nBufferSize, ptr);

        if (OMX_ErrorNone != eRet) {
            ALOGE("OMX_UseBuffer on input port failed! eRet = %#x\n", eRet);
            return false;
        }
        ALOGD("OMX_UseBuffer input %p", bufferHdr);
        mListOfInputBufferHeader.push_back(bufferHdr);
    }

    OMX_STATETYPE eState1, eState2;
    int buffer_size = mOutWidth * mOutHeight * 3 / 2 ;
    ALOGD("Allocating %u buffers from a native window of size %u on "
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
    ALOGD("STATETRANS FROM LOADED TO IDLE COMPLETED, eRet =%x\n", eRet);

    //swith to Excuting state
    OMX_SendCommand(mVDecoderHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

    do {
        OMX_GetState(mVDecoderHandle, &eState2);
        usleep(10*1000);
    } while (OMX_StateExecuting != eState2 && OMX_StateInvalid != eState2);

    ALOGD("STATETRANS FROM IDLE TO EXECUTING COMPLETED, eRet =%x\n", eRet);

    return true;
}

void OMXDecoder::free_ion_buffer(void) {
    while (!mListOfOutputBufferHeader.empty()) {
        OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfOutputBufferHeader.begin());
        OMX_ERRORTYPE err;
        if (bufferHdr != NULL) {
            if (mUseDMABuffer) {
                if (munmap(bufferHdr->pBuffer, mOutWidth * mOutHeight * 3 / 2) < 0) {
                    ALOGE("munmap failed errno=%d", errno);
                }
                int ret = close((int)(long)bufferHdr->pPlatformPrivate);
                if (ret != 0) {
                    ALOGD("close ion shared fd failed for reason %s",strerror(errno));
                }
                ALOGD("bufferHdr->pAppPrivate: %p", bufferHdr->pAppPrivate);
                ret = ion_free(mIonFd, (ion_user_handle_t)(long)(bufferHdr->pAppPrivate));
                if (ret != 0) {
                    ALOGD("ion_free failed for reason %s",strerror(errno));
                }
                err = OMX_FreeBuffer(mVDecoderHandle,mVideoOutputPortParam.nPortIndex,bufferHdr);
                if (OMX_ErrorNone != err) {
                    ALOGE("%d, OutPortIndex: %d\n",__LINE__,mVideoOutputPortParam.nPortIndex);
                }
            } else if (bufferHdr->pBuffer != NULL)
                free(bufferHdr->pBuffer);
        }
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
    if (mIonFd != -1) {
        int ret = ion_close(mIonFd);
        ALOGD("free_ion_buffer:close ion device fd %s",ret==0 ? "sucess":strerror(errno));
        mIonFd = -1;
    }
}

void OMXDecoder::free_normal_buffer(void) {
    OMX_ERRORTYPE err;
    while (!mListOfOutputBufferHeader.empty()) {
        OMX_BUFFERHEADERTYPE* bufferHdr = *(mListOfOutputBufferHeader.begin());
        if (bufferHdr != NULL) {
            OMX_U8 *pOut = bufferHdr->pBuffer;
            ALOGD("OMX_FreeBuffer output %p", bufferHdr);
            err = OMX_FreeBuffer(mVDecoderHandle, mVideoOutputPortParam.nPortIndex, bufferHdr);
            if (OMX_ErrorNone != err) {
                ALOGE("%d, OutPortIndex: %d\n", __LINE__, mVideoOutputPortParam.nPortIndex);
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

                   ALOGD("bufferHdr->pAppPrivate: %p", bufferHdr->pAppPrivate);

                   err = OMX_FreeBuffer(mVDecoderHandle,mVideoOutputPortParam.nPortIndex,bufferHdr);
                   if (OMX_ErrorNone != err) {
                       ALOGE("%d, OutPortIndex: %d\n",__LINE__,mVideoOutputPortParam.nPortIndex);
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
            ALOGD("OMX_FreeBuffer input %p", bufferHdr);
            err = OMX_FreeBuffer(mVDecoderHandle, mVideoInputPortParam.nPortIndex, bufferHdr);
            if (OMX_ErrorNone != err) {
                ALOGE("%d, InPortIndex: %d\n", __LINE__, mVideoInputPortParam.nPortIndex);
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
                    ALOGE("%d, OutPortIndex: %d\n", __LINE__, mVideoOutputPortParam.nPortIndex);
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
    ALOGD("data1 = %zu, data2 = %zu, event = %d\n", nData1, nData2, eEvent);

    if (eEvent == OMX_EventBufferFlag)
    {
        ALOGD("Got OMX_EventBufferFlag event\n");
    }
    else if (eEvent == OMX_EventError)
    {
        ALOGD("Got OMX_EventError event\n");
    }
    else if (eEvent == OMX_EventPortSettingsChanged)
    {
        OMX_ERRORTYPE omx_error_type = OMX_ErrorNone;
        ALOGD("Got OMX_EventPortSettingsChanged event\n");
        if (OMX_IndexParamPortDefinition == (OMX_INDEXTYPE) nData2)
        {
            mVideoOutputPortParam.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
            mVideoOutputPortParam.nVersion = mSpecVersion;
            mVideoOutputPortParam.nPortIndex = nData1;
            omx_error_type = OMX_GetParameter(mVDecoderHandle, OMX_IndexParamPortDefinition, &mVideoOutputPortParam);
            if (omx_error_type != OMX_ErrorNone)
            {
                ALOGD("OMX_GetParameter FAILED");
            }
            ALOGD("w= %zu, h= %zu\n", mVideoOutputPortParam.format.video.nFrameWidth, mVideoOutputPortParam.format.video.nFrameHeight);
            if (mOutWidth != mVideoOutputPortParam.format.video.nFrameWidth || mOutHeight != mVideoOutputPortParam.format.video.nFrameHeight)
            {
                ALOGD("Dynamic resolution changes triggered");
            }
        }
    }
    else
    {
        ALOGD("not support event!\n");
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::emptyBufferDone(OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    AutoMutex l(mInputBufferLock);
    mListOfInputBufferHeader.push_back(pBuffer);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::fillBufferDone(OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    AutoMutex l(mOutputBufferLock);
    mListOfOutputBufferHeader.push_back(pBuffer);
    //send signal
    Mutex::Autolock lock(mOMXControlMutex);
    mOMXVSync.signal();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXDecoder::OnEvent(
        OMX_IN OMX_HANDLETYPE,/*ommit hComponent to avoid unused variable warning*/
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
    //ALOGD("omx pInPutBufferHdr = %p\n", pInPutBufferHdr);
    if (pInPutBufferHdr && pInPutBufferHdr->pBuffer) {
        memcpy(pInPutBufferHdr->pBuffer, src, size);
        pInPutBufferHdr->nFilledLen = size;
        pInPutBufferHdr->nOffset = 0;
        pInPutBufferHdr->nTimeStamp = timeStamp;
        pInPutBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
        queueInputBuffer(pInPutBufferHdr);
        timeStamp += 33 * 1000; //44
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

        ALOGV("SetOutputBuffer %p, OMX_FillThisBuffer, share_fd=%d, pAppPrivate=%d, pPlatformPrivate=%d",
                pBufferHdr,
                share_fd,
                (int)(long)(pBufferHdr->pAppPrivate),
                (int)(long)(pBufferHdr->pPlatformPrivate));

        OMX_FillThisBuffer(mVDecoderHandle, pBufferHdr);
        ALOGV("SetOutputBuffer: erase mListOfOutputBufferHeader");
        mListOfOutputBufferHeader.erase(mListOfOutputBufferHeader.begin());
    }
}

int OMXDecoder::DequeueBuffer(int dst_fd ,uint8_t* dst_buf,
                              size_t src_w, size_t src_h,
                              size_t dst_w, size_t dst_h) {

        int ret = 0;
        ALOGD("%s:Enter, src_w=%d, dst_w=%d", __FUNCTION__, src_w, dst_w);

        OMX_BUFFERHEADERTYPE *pOutPutBufferHdr = NULL;
        pOutPutBufferHdr = dequeueOutputBuffer();
        if (pOutPutBufferHdr == NULL) {
            //dequeue fail
            ALOGE("%s:dequeue fail",__FUNCTION__);
            ret = 0;
        } else {
            //ALOGD("omx pOutPutBufferHdr = %p\n", pOutPutBufferHdr);
#ifdef GE2D_ENABLE
           if (dst_fd != -1) {
                //copy data using ge2d
                int omx_share_fd = (int)(long)pOutPutBufferHdr->pPlatformPrivate;
                if (mGE2D) {
                    if (src_w == dst_w && src_h == dst_h) {
                        //ALOGD("%s ge2d copy");
                        mGE2D->ge2d_copy(dst_fd, omx_share_fd, dst_w, dst_h, ge2dTransform::NV12);
                    } else {
                        // scale & crop
                        //ALOGD("%s ge2d scale to dst size", __FUNCTION__);
                        mGE2D->ge2d_keep_ration_scale(dst_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, dst_w, dst_h,
                                          omx_share_fd, src_w, src_h);
                    }
                } else {
                    ALOGE("%s:ge2d object is null",__FUNCTION__);
                }
            } else if (src_w == dst_w && src_h == dst_h) {
                if (mem_type == UVM_BUFFER) {
                    uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, pOutPutBufferHdr->nFilledLen, PROT_READ | PROT_WRITE, MAP_SHARED, (int)(long)pOutPutBufferHdr->pPlatformPrivate, 0);
                    memcpy(dst_buf, cpu_ptr, pOutPutBufferHdr->nFilledLen);
                    munmap(cpu_ptr, pOutPutBufferHdr->nFilledLen);
                } else {
                    //ALOGD("%s ge2d. no dst_fd, using sw memcpy");
                    memcpy(dst_buf, pOutPutBufferHdr->pBuffer, pOutPutBufferHdr->nFilledLen);
                }
            } else {
                ALOGE(" ge2d src w&h not equal dst w&h. hw dec not supported");
            }
#else
            //no ge2d support
            if (src_w == dst_w && src_h == dst_h) {
                if (mem_type == UVM_BUFFER) {
                    uint8_t* cpu_ptr = (uint8_t*)mmap(NULL, pOutPutBufferHdr->nFilledLen, PROT_READ | PROT_WRITE, MAP_SHARED, (int)(long)pOutPutBufferHdr->pPlatformPrivate, 0);
                    memcpy(dst_buf, cpu_ptr, pOutPutBufferHdr->nFilledLen);
                    if (munmap(cpu_ptr, pOutPutBufferHdr->nFilledLen) < 0)
                        ALOGE("%s:%d munmap failed errno=%d", __FUNCTION__,__LINE__,errno);
                } else {
                    memcpy(dst_buf, pOutPutBufferHdr->pBuffer, pOutPutBufferHdr->nFilledLen);
                }
            } else {
                ALOGE("src w&h not equal dst w&h. hw dec not supported");
            }
#endif
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
        ALOGE("%s: Error waiting for VSync signal: %d", __FUNCTION__, res);
        return false;
    }
    return true;
}

int OMXDecoder::Decode(uint8_t*src, size_t src_size,
                          int dst_fd,uint8_t *dst_buf,
                          size_t src_w, size_t src_h,
                          size_t dst_w, size_t dst_h) {
    int ret = 0;
    if (dst_buf == NULL) {
        ALOGE("%s: dst_fd=%d, dst_buf=%p", __FUNCTION__, dst_fd, dst_buf);
        return ret;
    }

    if (dst_fd > 0 && mem_type == SHARED_FD) {
        SetOutputBuffer(dst_fd, dst_buf);
        QueueBuffer(src, src_size);

        if (OMXWaitForVSync(200*1000*1000) == false) { //wait timeout 200ms
            mDequeueFailNum ++;
            ALOGD("Decoderss failed %d", mDequeueFailNum);
            return ret;
        }

        ret = 1;

        return ret;
    }

    QueueBuffer(src, src_size);

    bool state = OMXWaitForVSync(200*1000*1000); //200ms

    if (state) {
        ret = DequeueBuffer(dst_fd, dst_buf, src_w, src_h, dst_w, dst_h);
        if (!ret) {
            if (mDequeueFailNum ++ > MAX_POLLING_COUNT) {
                mTimeOut = true;
            }

            ALOGD("%s:Polling number=%d",__FUNCTION__,mDequeueFailNum);
        }
    } else {
        ALOGD("%s: OMX Vsync error",__FUNCTION__);
        ret = 0;
    }

    return ret;
}

