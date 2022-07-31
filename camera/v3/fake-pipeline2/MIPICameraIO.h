#ifndef __MIPI_CAMERA_IO_H__
#define __MIPI_CAMERA_IO_H__
#if 0
#include "CameraIO.h"
#endif
#include "MIPIBaseIO.h"
#include "MIPIBaseIO2.h"
#include "MIPIBaseIO3.h"
//#include "KeyEvent.h"

enum {
    ONE_FD = 1,
    TWO_FD,
    PIC_SCALER,
};

namespace android {
#if 0
class MIPIVideoInfo:public CVideoInfo {

public:
    MIPIVideoInfo(int preview, int snapshot);
    ~MIPIVideoInfo();
public:
    int mPreviewFd;
    int mSnapFd;
public:
    int start_capturing(void) override;
    int stop_capturing(void) override;
    int releasebuf_and_stop_capturing(void) override;
    uintptr_t get_frame_phys(void) override;
    int putback_frame() override;
    int putback_picture_frame() override;
    int start_picture(int rotate) override;
    void stop_picture() override;
    void releasebuf_and_stop_picture() override;
    int get_picture_fd();
    int get_frame_buffer(struct VideoInfoBuffer* b);
protected:
    int get_frame_index_by_fd(FrameV4L2Info& info,int fd);
    int export_dmabuf_fd(int v4lfd, int index, int* dmafd);
};
#endif
class MIPIVideoInfo {
public:
    int mWorkMode;
public:
    void set_fds(std::vector<int>& fds);
    int get_fd(void);
    void set_index(int idx);
    uint32_t get_index(void);
    int camera_init(void);
    int setBuffersFormat(void);
    int start_capturing(void);
    int start_recording();
    int start_picture(int rotate);
    void stop_picture();
    void releasebuf_and_stop_picture();
    int stop_capturing();
    int stop_recording();
    int releasebuf_and_stop_capturing();
    uintptr_t get_frame_phys();
    void set_device_status();
    int get_device_status();
    void *get_frame();
    void *get_picture();
    int get_frame_buffer(struct VideoInfoBuffer* b);
    int get_record_buffer(struct VideoInfoBuffer* b);
    int putback_frame();
    int putback_record_frame();
    int putback_picture_frame();
    int EnumerateFormat(uint32_t pixelformat);
    bool IsSupportRotation();
    void set_buffer_numbers(int io_buffer);
    uint32_t get_preview_pixelformat();
    uint32_t get_preview_width();
    uint32_t get_preview_height();
    void set_preview_format(uint32_t width, uint32_t height, uint32_t pixelformat);
    uint32_t get_preview_buf_length();
    uint32_t get_preview_buf_bytesused();
    uint32_t get_record_pixelformat();
    uint32_t get_record_width();
    uint32_t get_record_height();
    void set_record_format(uint32_t width, uint32_t height, uint32_t pixelformat);
    uint32_t get_record_buf_length();
    uint32_t get_record_buf_bytesused();
    uint32_t get_picture_pixelformat();
    uint32_t get_picture_width();
    uint32_t get_picture_height();
    void set_picture_format(uint32_t width, uint32_t height, uint32_t pixelformat);
    uint32_t get_picture_buf_length();
    uint32_t get_picture_buf_bytesused();
    bool Stream_status();
    bool Stream_record_status();
    bool Picture_status();
    int get_picture_buffer(struct VideoInfoBuffer* b);
    //int onMuteKeyChanged(KeyEvent keyStatus);
private:
    VideoInfoUseOneFd mOneFd;
    VideoInfoUseTowFd mTwoFd;
    VideoInfoUsePictureScaler mPicScaler;
};
}
#endif
