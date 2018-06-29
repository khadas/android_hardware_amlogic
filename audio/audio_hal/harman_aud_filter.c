/*
 harman_aud_filter.c
 */
#define LOG_TAG "harman_aud_filter"
//#define LOG_NDEBUG 0

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#define MAX_I32       ((int32_t)0x7fffffff)
#define MIN_I32       ((int32_t)0x80000000)
#define Fix_Point_Multiply_of_Filter(a, b)  (((int64_t)a*(int64_t)b))

#define GAIN_DB_DEFAULT	20

//#define DEBUG_MIC_GAIN 1

typedef  struct
{
    int32_t L_Channel_Filter_state;
    int32_t R_Channel_Filter_state;
}FILTER_PAIR_STATE_VAR_STRUCT;

FILTER_PAIR_STATE_VAR_STRUCT LPF_Pair_State[4] = {0};
FILTER_PAIR_STATE_VAR_STRUCT HPF_Pair_State[4] = {0};

FILTER_PAIR_STATE_VAR_STRUCT *state_pointer_LPF = LPF_Pair_State;
FILTER_PAIR_STATE_VAR_STRUCT *state_pointer_HPF = HPF_Pair_State;

static int32_t Gain_Value = 0x9FFF0; // 20 dB
static int32_t Gain_dB = GAIN_DB_DEFAULT;

static int32_t sat64_32(int64_t x)
{
    int32_t result;
    if(x > MAX_I32)
    {
        result = MAX_I32;
    }
    else if(x < MIN_I32)
    {
        result = MIN_I32;
    }
    else
    {
        result = (int32_t)x;
    }
    return result;
}

void Aud_Gain_Filter_Init(void)
{
    int val;

    val = property_get_int32("audio.mic.gain", GAIN_DB_DEFAULT);
    if (val != Gain_dB) {
        ALOGD("%s: Gain = %d", __func__, val);
        Gain_Value = (int32_t)round((0xFFF0 * pow(10, ((double)val / 20))));
        Gain_dB = val;
        ALOGD("%s: Gain_Value = 0x%x", __func__, Gain_Value);
    }
}

static void vProcess_Gain(void *inBuf, int numSamples)
{
    int i;
    int32_t *temp_input_pointer = (int32_t *)inBuf;
    int64_t  temp_I64;

#ifdef DEBUG_MIC_GAIN
    Aud_Gain_Filter_Init();
#endif

    for(i = 0; i < numSamples; i++)
    {
        temp_I64 = Fix_Point_Multiply_of_Filter(*temp_input_pointer, Gain_Value);
        temp_I64 = temp_I64 >> 16;
        *temp_input_pointer++ = (int32_t)temp_I64; //left

        temp_I64 = Fix_Point_Multiply_of_Filter(*temp_input_pointer, Gain_Value);
        temp_I64 = temp_I64 >> 16;
        *temp_input_pointer++ = (int32_t)temp_I64; //right
    }
}

static void vProcess_Filter(void* inBuf, void* outBuf, int32_t* filter_coefficient_pointer, FILTER_PAIR_STATE_VAR_STRUCT * pair_state_pointer, int numSamples)
{
    int i;
    int64_t temp;
    int32_t my_out;
    int32_t *LR_input_sample_data_pointer = (int32_t *)inBuf;
    int32_t *LR_output_sample_data_pointer = (int32_t *)outBuf;


    //process this filer for all samples
    for(i=0;i<numSamples;i++)
    {
        //Left  IIR
        temp =  Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[0],*LR_input_sample_data_pointer); //2.30 * 1.31 =3.61

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[1],pair_state_pointer[0].L_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[2],pair_state_pointer[1].L_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[3],pair_state_pointer[2].L_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[4],pair_state_pointer[3].L_Channel_Filter_state);


        temp = temp >> 29;
        my_out = sat64_32(temp);


        pair_state_pointer[1].L_Channel_Filter_state=pair_state_pointer[0].L_Channel_Filter_state;
        pair_state_pointer[0].L_Channel_Filter_state=*LR_input_sample_data_pointer++; // inrease the input sample pointer for the Right channel process
        pair_state_pointer[3].L_Channel_Filter_state=pair_state_pointer[2].L_Channel_Filter_state;
        pair_state_pointer[2].L_Channel_Filter_state=my_out;

        *LR_output_sample_data_pointer++=my_out;

        //right  IIR
        temp =  Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[0],*LR_input_sample_data_pointer); //2.30 * 1.31 =3.61

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[1],pair_state_pointer[0].R_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[2],pair_state_pointer[1].R_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[3],pair_state_pointer[2].R_Channel_Filter_state);

        temp += Fix_Point_Multiply_of_Filter(filter_coefficient_pointer[4],pair_state_pointer[3].R_Channel_Filter_state);


        temp = temp >> 29;
        my_out = sat64_32(temp);


        pair_state_pointer[1].R_Channel_Filter_state=pair_state_pointer[0].R_Channel_Filter_state;
        pair_state_pointer[0].R_Channel_Filter_state=*LR_input_sample_data_pointer++; // inrease the input sample pointer for the Right channel process
        pair_state_pointer[3].R_Channel_Filter_state=pair_state_pointer[2].R_Channel_Filter_state;
        pair_state_pointer[2].R_Channel_Filter_state=my_out;

        *LR_output_sample_data_pointer++=my_out;
    }
}

/*low pass filter: fc = 8K, sr = 48K*/
void Aud_Gain_LPFFilter_Process(void* inBuf, int numSamples)
{
    int i;
    int32_t pCoeff[5] = {0x4f61aef, 0x9ec35de, 0x4f61aef, 0x13d86b9b, 0xf84f28a7};

    //vProcess_Gain(inBuf, numSamples);

    vProcess_Filter(inBuf, inBuf, pCoeff, state_pointer_LPF, numSamples);
}

/*high pass filter: fc = 100Hz, sr = 16K*/
void Aud_Gain_HPFFilter_Process(void* inBuf, int numSamples)
{
    int32_t pCoeff[5] = {0x1f1f9edc, 0xc1c0c248, 0x1f1f9edc, 0x3e391892, 0xe1ba9d21};

    vProcess_Filter(inBuf, inBuf, pCoeff, state_pointer_HPF, numSamples);

    vProcess_Gain(inBuf, numSamples);
}

