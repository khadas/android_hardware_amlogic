//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0

#define LOG_TAG "vicp_stream"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>


#define RATIO_SCALE
#include "amlogic_camera.h"
#include <CamHalDebugLog.h>
#include "vicp_stream.h"

namespace android {
vicpTransform::vicpTransform() {
    memset(&m_amlvicpinfo, 0, sizeof(aml_vicp_info_t));
    m_vicpinitinfo = aml_vicp_init();

}

vicpTransform::~vicpTransform() {
    aml_vicp_uninit(m_vicpinitinfo);
    m_vicpinitinfo = nullptr;
}

int vicpTransform::vicp_scale(int dst_fd, vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h)
{
    m_amlvicpinfo.dst_data_info.buf_fd = dst_fd;
    m_amlvicpinfo.dst_data_info.buf_width = dst_w;
    m_amlvicpinfo.dst_data_info.buf_height = dst_h;
    m_amlvicpinfo.dst_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.dst_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.dst_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.dst_data_info.axis_x = 0;
    m_amlvicpinfo.dst_data_info.axis_y = 0;
    m_amlvicpinfo.dst_data_info.axis_w = dst_w;
    m_amlvicpinfo.dst_data_info.axis_h = dst_h;

    m_amlvicpinfo.src_data_info.buf_fd = src_fd;
    m_amlvicpinfo.src_data_info.buf_align_w = src_w;
    m_amlvicpinfo.src_data_info.buf_align_h = src_h;
    m_amlvicpinfo.src_data_info.data_width = src_h;
    m_amlvicpinfo.src_data_info.data_height = src_w;
    m_amlvicpinfo.src_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.src_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.src_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.src_data_info.crop_x = 0;
    m_amlvicpinfo.src_data_info.crop_y = 0;
    m_amlvicpinfo.src_data_info.crop_w = src_w;
    m_amlvicpinfo.src_data_info.crop_h = src_h;

    int ret = aml_vicp_process(m_vicpinitinfo, &m_amlvicpinfo);
    if (ret < 0) {
        CAMHAL_LOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }

    return 0;
}

int vicpTransform::vicp_keep_ration_scale(int dst_fd, vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h) {
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    ATRACE_CALL();
    m_amlvicpinfo.dst_data_info.buf_fd = dst_fd;
    m_amlvicpinfo.dst_data_info.buf_width = dst_w;
    m_amlvicpinfo.dst_data_info.buf_height = dst_h;
    m_amlvicpinfo.dst_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.dst_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.dst_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.dst_data_info.axis_x = 0;
    m_amlvicpinfo.dst_data_info.axis_y = 0;
    m_amlvicpinfo.dst_data_info.axis_w = dst_w;
    m_amlvicpinfo.dst_data_info.axis_h = dst_h;

    m_amlvicpinfo.src_data_info.buf_fd = src_fd;

    m_amlvicpinfo.src_data_info.buf_align_w = src_w;
    m_amlvicpinfo.src_data_info.buf_align_h = src_h;

    m_amlvicpinfo.src_data_info.data_width = src_w;
    m_amlvicpinfo.src_data_info.data_height = src_h;
    m_amlvicpinfo.src_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.src_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.src_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.src_data_info.crop_x = 0;
    m_amlvicpinfo.src_data_info.crop_y = 0;
    m_amlvicpinfo.src_data_info.crop_w = src_w;
    m_amlvicpinfo.src_data_info.crop_h = src_h;

    m_amlvicpinfo.rotation_mode = VICP_ROTATION_0;

    int ret = aml_vicp_process(m_vicpinitinfo, &m_amlvicpinfo);
    if (ret < 0) {
        CAMHAL_LOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    return 0;
}


int vicpTransform::vicp_keep_ration_scale(int dst_fd, vicp_color_format_t dst_fmt, size_t dst_w, size_t dst_h,
                       int src_fd, size_t src_w, size_t src_h, size_t format_w, size_t format_h) {
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    ATRACE_CALL();
    int src_rect_start_row = 0;
    int src_rect_start_col = 0;
    int src_rect_width     = format_w;
    int src_rect_height    = format_h;

    if (format_w * dst_h != format_h * dst_w) {
        // int & out not the same ration.
        if (dst_w * format_h < dst_h * format_w) {
            // eg: src 16:9  dst 4:3.
            src_rect_width     = format_h * dst_w / dst_h;
            src_rect_start_col = (format_w - src_rect_width) / 2;
        } else {
            src_rect_height    = format_w * dst_h / dst_w;
            src_rect_start_row = (format_h - src_rect_height) / 2;
        }
    }

    m_amlvicpinfo.dst_data_info.buf_fd = dst_fd;
    m_amlvicpinfo.dst_data_info.buf_width = dst_w;
    m_amlvicpinfo.dst_data_info.buf_height = dst_h;
    m_amlvicpinfo.dst_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.dst_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.dst_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.dst_data_info.axis_x = 0;
    m_amlvicpinfo.dst_data_info.axis_y = 0;
    m_amlvicpinfo.dst_data_info.axis_w = dst_w;
    m_amlvicpinfo.dst_data_info.axis_h = dst_h;

    m_amlvicpinfo.src_data_info.buf_fd = src_fd;

    m_amlvicpinfo.src_data_info.buf_align_w = src_w;
    m_amlvicpinfo.src_data_info.buf_align_h = src_h;

    m_amlvicpinfo.src_data_info.data_width = format_w;
    m_amlvicpinfo.src_data_info.data_height = format_h;
    m_amlvicpinfo.src_data_info.color_fmt = dst_fmt;
    m_amlvicpinfo.src_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.src_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.src_data_info.crop_x = src_rect_start_row;
    m_amlvicpinfo.src_data_info.crop_y = src_rect_start_col;
    m_amlvicpinfo.src_data_info.crop_w = src_rect_width;
    m_amlvicpinfo.src_data_info.crop_h = src_rect_height;

    m_amlvicpinfo.rotation_mode = VICP_ROTATION_0;

    int ret = aml_vicp_process(m_vicpinitinfo, &m_amlvicpinfo);
    if (ret < 0) {
        CAMHAL_LOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }
    return 0;
}

int vicpTransform::vicp_copy(int dst_fd, int src_fd, size_t width, size_t height, vicp_color_format_t fmt) {
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    ATRACE_CALL();
    CAMHAL_LOGV("vicp copy begin width %zu, height %zu\n", width, height);
    m_amlvicpinfo.dst_data_info.buf_fd = dst_fd;
    m_amlvicpinfo.dst_data_info.buf_width = width;
    m_amlvicpinfo.dst_data_info.buf_height = height;
    m_amlvicpinfo.dst_data_info.color_fmt = fmt;
    m_amlvicpinfo.dst_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.dst_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.dst_data_info.axis_x = 0;
    m_amlvicpinfo.dst_data_info.axis_y = 0;
    m_amlvicpinfo.dst_data_info.axis_w = width;
    m_amlvicpinfo.dst_data_info.axis_h = height;

    m_amlvicpinfo.src_data_info.buf_fd = src_fd;
    m_amlvicpinfo.src_data_info.buf_align_w = width;
    m_amlvicpinfo.src_data_info.buf_align_h = height;

    m_amlvicpinfo.src_data_info.data_width = width;
    m_amlvicpinfo.src_data_info.data_height = height;
    m_amlvicpinfo.src_data_info.color_fmt = fmt;
    m_amlvicpinfo.src_data_info.color_depth = VICP_COLOR_DEPTH_8;
    m_amlvicpinfo.src_data_info.endian = VICP_ENDIAN_LITTLE;
    m_amlvicpinfo.src_data_info.crop_x = 0;
    m_amlvicpinfo.src_data_info.crop_y = 0;
    m_amlvicpinfo.src_data_info.crop_w = width;
    m_amlvicpinfo.src_data_info.crop_h = height;

    m_amlvicpinfo.rotation_mode = VICP_ROTATION_0;
    int ret = aml_vicp_process(m_vicpinitinfo, &m_amlvicpinfo);
    if (ret < 0) {
        CAMHAL_LOGE("%s: %s", __FUNCTION__,strerror(errno));
        return ret;
    }

    return 0;
}
}
