#ifndef __CAPTURE_IF__
#define __CAPTURE_IF__

#include "Base.h"
#include "IonIf.h"

namespace android {
    struct data_in {
        /*in*/
        uint8_t* src;
        /*in*/
        int src_fmt;
        /*in*/
        int share_fd;
        /*out*/
        int dmabuf_fd;
    };
    enum capture_status {
        ERROR_FRAME = -1,
        NEW_FRAME = 0,
        NO_NEW_FRAME = 1, //only used in video record
    };
    class ICapture {
        public:
             ICapture(){};
             virtual ~ICapture(){};
        public:
            virtual int getPicture(StreamBuffer b, struct data_in* in, IONInterface *ion)=0;
            virtual int captureYUYVframe(uint8_t *img, struct data_in* in)=0;
            virtual int captureNV21frame(StreamBuffer b, struct data_in* in)=0;
            virtual int captureYV12frame(StreamBuffer b, struct data_in* in)=0;
    };
}
#endif
