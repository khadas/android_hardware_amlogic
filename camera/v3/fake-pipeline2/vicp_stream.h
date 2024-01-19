#ifndef __VICP_STREAM_H__
#define __VICP_STREAM_H__

#include <linux/videodev2.h>
#include "Base.h"
#include "camera_hw.h"
#include <aml_vicp.h>
#include <cutils/properties.h>
#include "IonIf.h"
namespace android {
class vicpTransform {

private:
        aml_vicp_info_t m_amlvicpinfo;
        vicp_init_info_t *m_vicpinitinfo;
public:
    vicpTransform();
    ~vicpTransform();

    int vicp_scale(int dst_fd,vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h);
    int vicp_keep_ration_scale(int dst_fd, vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h);
    int vicp_keep_ration_scale(int dst_fd, vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h, size_t format_w, size_t format_h);
    int vicp_copy(int dst_fd, int src_fd, size_t width, size_t height,vicp_color_format_t fmt);
};
}
#endif