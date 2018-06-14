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

//#define DEBUG_MIC_GAIN 1

typedef  struct
{
	int32_t L_Channel_Filter_state;
	int32_t R_Channel_Filter_state;
}FILTER_PAIR_STATE_VAR_STRUCT;

FILTER_PAIR_STATE_VAR_STRUCT Filter0_Pair_State[4] = {0};

FILTER_PAIR_STATE_VAR_STRUCT *state_pointer = Filter0_Pair_State;

const int32_t Gain_Table[]=
{
	 0x27CD0E   ,   //  32 dB
	 0x259317   ,   //  31.5 dB
	 0x237901   ,   //  31 dB
	 0x217D06   ,   //  30.5 dB
	 0x1F9D74   ,   //  30 dB
	 0x1DD8B7   ,   //  29.5 dB
	 0x1C2D4C   ,   //  29 dB
	 0x1A99CB   ,   //  28.5 dB
	 0x191CDC   ,   //  28 dB
	 0x17B53C   ,   //  27.5 dB
	 0x1661BA	,	//	27 dB
	 0x152136	,	//	26.5 dB
	 0x13F2A0	 ,	 //	 26	dB
     0x12D4F7	 ,	 //	 25.5 dB
	 0x11C749	 ,	 //	 25	 dB
	 0x10C8B0	 ,	 //	 24.5 dB
	 0xFD950	 ,	 //  24  dB
	 0xEF650	 ,	 //  23.5	 dB
	 0xE2010	 ,	 //  23  dB
	 0xD55D0	 ,	 //  22.5	 dB
	 0xC96D0	 ,	 //  22  dB
	 0xBE290	 ,	 //  21.5	 dB
	 0xB3850	 ,	 //  21  dB
	 0xA97B0	 ,	 //  20.5	 dB
	 0x9FFF0	 ,	 //  20  dB
	 0x970C0	 ,	 //  19.5	 dB
	 0x8E990	 ,	 //  19  dB
	 0x869F0	 ,	 //  18.5	 dB
	 0x7F170	 ,	 //  18  dB
	 0x77FB0	 ,	 //  17.5	 dB
	 0x71450	 ,	 //  17  dB
	 0x6AEF0	 ,	 //  16.5	 dB
	 0x64F40	 ,	 //  16  dB
	 0x5F4E0	 ,	 //  15.5	 dB
	 0x59F90	 ,	 //  15  dB
	 0x54F10	 ,	 //  14.5	 dB
	 0x50300	 ,	 //  14  dB
	 0x4BB40	 ,	 //  13.5	 dB
	 0x47780	 ,	 //  13  dB
	 0x43780	 ,	 //  12.5	 dB
	 0x3FB20	 ,	 //  12  dB
	 0x3C220	 ,	 //  11.5	 dB
	 0x38C50	 ,	 //  11  dB
	 0x35980	 ,	 //  10.5	 dB
	 0x32980	 ,	 //  10  dB
	 0x2FC40	 ,	 //  9.5 dB
	 0x2D180	 ,	 //  9	 dB
	 0x2A920	 ,	 //  8.5 dB
	 0x28300	 ,	 //  8	 dB
	 0x25F10	 ,	 //  7.5 dB
	 0x23D10	 ,	 //  7	 dB
	 0x21D00	 ,	 //  6.5 dB
	 0x1FEC0	 ,	 //  6	 dB
	 0x1E230	 ,	 //  5.5 dB
	 0x1C730	 ,	 //  5	 dB
	 0x1ADC0	 ,	 //  4.5 dB
	 0x195B0	 ,	 //  4	 dB
	 0x17F00	 ,	 //  3.5 dB
	 0x16990	 ,	 //  3	 dB
	 0x15560	 ,	 //  2.5 dB
	 0x14240	 ,	 //  2	 dB
	 0x13040	 ,	 //  1.5 dB
	 0x11F30	 ,	 //  1	 dB
	 0x10F20	 ,	 //  0.5 dB
	 0x0FFF0	 ,	 // 0 dB
	 0x0F1A0	 ,	 //  -0.5	 dB
	 0x0E420	 ,	 //  -1  dB
	 0x0D760	 ,	 //  -1.5	 dB
	 0x0CB50	 ,	 //  -2  dB
	 0x0BFF0	 ,	 //  -2.5	 dB
	 0x0B530	 ,	 //  -3  dB
	 0x0AB10	 ,	 //  -3.5	 dB
	 0x0A180	 ,	 //  -4  dB
	 0x09870	 ,	 //  -4.5	 dB
	 0x08FF0	 ,	 //  -5  dB
	 0x087E0	 ,	 //  -5.5	 dB
	 0x08040	 ,	 //  -6  dB
	 0x07920	 ,	 //  -6.5	 dB
	 0x07250	 ,	 //  -7  dB
	 0x06BF0	 ,	 //  -7.5	 dB
	 0x065E0	 ,	 //  -8  dB
	 0x06030	 ,	 //  -8.5	 dB
	 0x05AD0	 ,	 //  -9  dB
	 0x055C0	 ,	 //  -9.5	 dB
	 0x050F0	 ,	 //  -10 dB
};

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

static void vProcess_Gain(void *inBuf, int numSamples)
{
    int i;
	int Gain_Index = 24;//0-->32dB, 64-->0dB, 0.5dB/step

    int32_t *temp_input_pointer = (int32_t *)inBuf;
    int64_t  temp_I64;

#ifdef DEBUG_MIC_GAIN
	char buf[PROPERTY_VALUE_MAX];
	if (property_get("audio.mic.gain", buf, NULL) > 0)
	{
		Gain_Index = atoi(buf);
		ALOGD("%s: Gain_Index = %d", __func__, Gain_Index);
	}
#endif

    for(i = 0; i < numSamples; i++)
    {
        temp_I64 = Fix_Point_Multiply_of_Filter(*temp_input_pointer, Gain_Table[Gain_Index]);
        temp_I64 = temp_I64 >> 16;
        *temp_input_pointer++ = (int32_t)temp_I64; //left

        temp_I64 = Fix_Point_Multiply_of_Filter(*temp_input_pointer, Gain_Table[Gain_Index]);
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

void Aud_Gain_Filter_Process(void* inBuf, int numSamples)
{
	int i;
	int32_t pCoeff[5] = {0x1fd29fca, 0xc05ac06c, 0x1fd29fca, 0x3fa4ff41, 0xe05a8019};

    vProcess_Gain(inBuf, numSamples);

	vProcess_Filter(inBuf, inBuf, pCoeff, state_pointer, numSamples);
}
