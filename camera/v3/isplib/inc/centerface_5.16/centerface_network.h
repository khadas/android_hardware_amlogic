#ifndef NEUSDK_CENTER_FACE_INCLUDE_NETWORK_H
#define NEUSDK_CENTER_FACE_INCLUDE_NETWORK_H

#include <malloc.h>
#include "aml_nn_image.hpp"
#include "ai_module_common_nnsdk.h"


#define _CHECK_PTR( ptr, lbl )      do {\
    if( NULL == ptr ) {\
    printf("%s CHECK PTR %d\n", __FUNCTION__, __LINE__);\
    goto lbl;\
    }\
} while(0)

#define _CHECK_STATUS( stat, lbl )  do {\
    if( VX_SUCCESS != stat ) {\
    printf("%s CHECK STATUS %d\n", __FUNCTION__, __LINE__);\
    goto lbl;\
    }\
} while(0)

#define _CHECK_INVALID(stat, value, lbl) do {\
    if (value == stat) { \
    printf("%s CHECK INVALID %d\n", __FUNCTION__, __LINE__); \
    goto lbl; \
    }\
} while(0)

#ifdef __cplusplus
extern "C"{
#endif

#define CENTER_FACE_WIDTH   512
#define CENTER_FACE_HEIGHT  288
#define CENTER_FACE_CHANNEL 3

#define CENTER_FACE_WIDTH_768   768
#define CENTER_FACE_HEIGHT_768  448
#define CENTER_FACE_CHANNEL_768 3

#define CENTER_FACE_WIDTH_1024   1024
#define CENTER_FACE_HEIGHT_1024  576
#define CENTER_FACE_CHANNEL_1024 3


/**
 * 人脸检测类型
 */
typedef enum {
    NEU_IVA_CENTER_FACE = 0,        //轻型人脸检测算法标志位,速度最快,检出率一般
    NEU_IVA_CENTER_FACE_768 = 1,    //中型人脸检测算法标志位,速度中等,检出率较好
    NEU_IVA_CENTER_FACE_1024 = 2,   //重型人脸检测算法标志位,速度最慢,检出率最好
} FACE_DETECT_NN_METHOD;


typedef struct
{
    float f32X1;
    float f32Y1;
    float f32X2;
    float f32Y2;
}CFaceRect;

typedef struct
{
    float f32X[5];
    float f32Y[5];
}CFacePts;

typedef struct
{
    CFaceRect cBox;
    CFacePts  cPts;
    float     f32Score;
} CFaceInfo;

typedef struct {
    CFaceRect cBox;
    CFacePts  cPts;
    float f32Score;
    int sort_class;
} FaceInfo;

typedef struct {
    // vsi_nn_tensor_t            *tensor_input;
    // vsi_nn_tensor_t            *tensor_output;
    // vsi_nn_graph_t             *graph;

    // uint8_t*                    input_data_ptr;

    void                        *heat_map;
    float                       heat_scale;
    uint32_t                    heat_map_size[4];
    int32_t                     heat_zero_point;
    void                        *scale;
    float                       scale_scale;
    uint32_t                    scale_size[4];
    int32_t                     scale_zero_point;
    void                        *offset;
    float                       offset_scale;
    uint32_t                    offset_size[4];
    int32_t                     offset_zero_point;
    void                        *land_marks;
    float                       land_marks_scale;
    uint32_t                    land_marks_size[4];
    int32_t                     land_marks_zero_point;

    CFaceInfo                  *cFace;
    CFaceInfo                  *cFace_orig;
    FaceInfo                   *tmpFaces;
    int                         faceNum;
    float                       face_score_threshold;
    int                         nn_created;
    network_performance_t       performance_info;
    net_info*                   netptr;


    void*                       qcontext;
    nn_input                    in_data;

    aml_memory_data_t           mem_data;
    aml_platform_info_t         *plat_info;
    aml_profile_config_t        profile_data;

    int                         heatmap_out_idx = 0;
    int                         scale_out_idx = 1;
    int                         offset_out_idx = 2;
    int                         landmarks_out_idx = 3;

    //class_perftest              c_perftest;

    FACE_DETECT_NN_METHOD       det_med;


} center_face_network_t;

center_face_network_t* center_face_network_init_aml(FACE_DETECT_NN_METHOD det_med, const char* model_path);
int center_face_network_process_aml(center_face_network_t* center_face_network, nn_rgb_frame_t& nn_image);
void center_face_network_deinit_aml(center_face_network_t* center_face_network);

#ifdef __cplusplus
};
#endif
#endif //NEUSDK_CENTER_FACE_NETWORK_H
