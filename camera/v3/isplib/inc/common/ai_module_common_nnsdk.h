#ifndef NEUSDK_AI_MODULE_COMMON_NNSDK_H
#define NEUSDK_AI_MODULE_COMMON_NNSDK_H

//#include <vsi_nn_pub.h>
//#include <vnn_global.h>
#include "ai_module_common_vsi_define.h"


#include "nn_sdk.h"
#include <string.h>
//#include <opencv2/opencv.hpp>
//#include <vector>
#include <stdint.h>
#include <float.h>
#include <math.h>


#define NC_MESSAGE(format, args...) printf(format "\n", ##args)
#define NC_WARNING(format, args...) printf(format "\n", ##args)
#define NC_ERROR(format, args...) printf(format "\n", ##args)
#define NC_DEBUG(format, args...) printf(format "\n", ##args)

#ifdef __cplusplus
extern "C"{
#endif

const int MAX_BUF_SIZE = 16;

typedef struct {
    int run_count;
    int enable;
    int step;
    int64_t acc_run_graph_tick;
    int64_t acc_preprocess_tick;
    int64_t acc_postprocess_tick;
    int64_t acc_total_tick;
} network_performance_t;

typedef struct {
    unsigned int num;                          //num of tensor
    aml_memory_data_t* buf[MAX_BUF_SIZE];      //array of tensor
} tensor_data_t;

typedef struct {
    tensor_info* inptr;
    tensor_info* outptr;
    void* context;
    tensor_data_t* inputs;
    tensor_data_t* outputs;
} net_info;

int malloc_tensor_data(void* context, tensor_info* pInfo, tensor_data_t* pData);
void free_tensor_data(void* context, tensor_data_t* pData);

net_info* init_nnsdk_network(const char* nb_fn,  aml_platform_info_t* aml_platform_info, int mode);

typedef void* (*module_createFunc)(aml_config* config);

typedef int (*destroy_networkFunc)(void* context);

typedef int (*module_input_setFunc)(void* context, nn_input* pInput);

typedef void* (*module_output_getFunc)(void* context, aml_output_config_t outconfig);

typedef void* (*module_output_get_simpleFunc)(void* context);

typedef int (*getTensorInfoFunc)(void *context, const char* model_data, tensor_info** in_tInfo, tensor_info** out_tInfo);

typedef int (*freeTensorInfoFunc)(tensor_info* tinfo);

typedef int (*mallocBufferFunc)(void* context, aml_memory_config_t* mem_config, aml_memory_data_t* memory_data);

typedef int (*freeBufferFunc)(void* context, aml_memory_config_t* mem_config, aml_memory_data_t* mem_data);

typedef int (*readChipInfoFunc)(aml_platform_info_t* platform_info);

typedef int  (*enableProfileFunc)(void *context, aml_profile_config_t* profile_data);
typedef int  (*getProfileInfoFunc)(void *context, aml_profile_config_t* profile_data);
typedef int  (*disableProfileFunc)(void *context, aml_profile_config_t* profile_data);

typedef struct _nnsdk_func_api{
    module_createFunc module_create;
    destroy_networkFunc destroy_network;
    module_input_setFunc module_input_set;
    module_output_getFunc module_output_get;
    module_output_get_simpleFunc module_output_get_simple;
    getTensorInfoFunc getTensorInfo;
    freeTensorInfoFunc freeTensorInfo;
    mallocBufferFunc mallocBuffer;
    freeBufferFunc freeBuffer;
    readChipInfoFunc readChipInfo;
    enableProfileFunc enableProfile;
    getProfileInfoFunc getProfileInfo;
    disableProfileFunc disableProfile;

}NNSDK_FUNC_PTR;


void show_profile(void *context, int profile_en);

inline float Float16ToFloat32(const signed short *src, float *dst, int length)
    {
        signed int t1;
        signed int t2;
        signed int t3;
        float out;
        int i;
        for (i = 0; i < length; i++)
        {
            t1 = src[i] & 0x7fff; // Non-sign bits
            t2 = src[i] & 0x8000; // Sign bit
            t3 = src[i] & 0x7c00; // Exponent

            t1 <<= 13; // Align mantissa on MSB
            t2 <<= 16; // Shift sign bit into position

            t1 += 0x38000000; // Adjust bias

            t1 = (t3 == 0 ? 0 : t1); // Denormals-as-zero

            t1 |= t2;
            *((unsigned int *)&out) = t1; // Re-insert sign bit
            dst[i] = out;
        }
        return out;
    }

inline float quant_to_float(uint8_t *x, info_t info)
{
	//this function can be used to convert the nnsdk output to float
	//now only UINT8 and INT8 are supported
	//support others format later
	float output = 0;
//     typedef enum _nn_buffer_format_e
//     {
//         /*! \brief A float type of buffer data */
//         NN_BUFFER_FORMAT_FP32       = 0,
//         /*! \brief A half float type of buffer data */
//         NN_BUFFER_FORMAT_FP16       = 1,
//         /*! \brief A 8 bit unsigned integer type of buffer data */
//         NN_BUFFER_FORMAT_UINT8      = 2,
//         /*! \brief A 8 bit signed integer type of buffer data */
//         NN_BUFFER_FORMAT_INT8       = 3,
//         /*! \brief A 16 bit unsigned integer type of buffer data */
//         NN_BUFFER_FORMAT_UINT16     = 4,
//         /*! \brief A 16 signed integer type of buffer data */
//         NN_BUFFER_FORMAT_INT16      = 5
//     } nn_buffer_format_e;     .data_format
//
//     typedef enum _nn_buffer_quantize_format_e
//     {
//         /*! \brief Not quantized format */
//         NN_BUFFER_QUANTIZE_NONE                    = 0,
//         /*! \brief The data is quantized with dynamic fixed point */
//         NN_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT     = 1,
//         /*! \brief The data is quantized with TF asymmetric format */
//         NN_BUFFER_QUANTIZE_TF_ASYMM                = 2
//     } nn_buffer_quantize_format_e;    .quantization_format
	if (info.data_format == NN_BUFFER_FORMAT_UINT8)
	{
		output = (x[0] - info.TF_zeropoint) * info.TF_scale;
	}
	else if (info.data_format == NN_BUFFER_FORMAT_INT8)
	{

		if (1)
		{
			int8_t *i8_tmp;
			i8_tmp = (int8_t *)(x);
			output = (i8_tmp[0] - info.TF_zeropoint) * info.TF_scale; //maybe fast
		}
		else
		{
			int8_t i8_tmp;
			memcpy(&i8_tmp, x, sizeof(int8_t));
			output = (i8_tmp - info.TF_zeropoint) * info.TF_scale; //maybe slow
		}
	}
	else if (info.data_format == NN_BUFFER_FORMAT_FP16)
	{

		Float16ToFloat32((signed short *)x, &output, 1);
		//printf("use fp16 %f\n", output);
		//return output;
	}
	else if (info.data_format == NN_BUFFER_FORMAT_FP32)
	{
		//float output = 0;
		memcpy(&output, x, sizeof(float));
		//return output;
	}

	return output;
}

#ifdef __cplusplus
};
#endif


#endif //NEUSDK_AI_MODULE_COMMON_NNSDK_H
