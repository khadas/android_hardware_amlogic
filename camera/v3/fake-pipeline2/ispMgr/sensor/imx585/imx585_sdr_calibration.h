/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "aml_isp_tuning.h"


//calibration_version
static int32_t _CALIBRATION_VERSION[1] = {20250822};

namespace Imx585SdrCalibration {
//aisp_top_ctl_t
static int32_t _CALIBRATION_TOP_CTL[50] = {
    0, // ISP input channels n+1
    0, // wdr enable 0:off 1:on
    0, // WDR input channels n+1
    0, // decmp enable 0:off 1:on
    0, // ifmt enable 0:off 1:on
    0, // bac enable 0:off 1:on
    1, // fpnr enable 0:off 1:on
    1, // ge enable 0:off 1:on
    1, // dpc enable 0:off 1:on
    0, // pat enable 0:off 1:on
    1, // og enable 0:off 1:on
    1, // sqrt_eotf enable 0:off 1:on
    0, // lcge enable 0:off 1:on
    1, // pdpc enable 0:off 1:on
    1, // cac enable 0:off 1:on
    1, // rawcnr enable 0:off 1:on
    1, // snr1 enable 0:off 1:on
    1, // mc_tnr enable 0:off 1:on
    1, // tnr0 enable 0:off 1:on
    1, // cubic_cs enable 0:off 1:on
    1, // ltm enable 0:off 1:on
    0, // gtm enable 0:off 1:on
    1, // lns_mesh enable 0:off 1:on
    1, // lns_rad enable 0:off 1:on
    1, // wb enable 0:off 1:on
    1, // blc enable 0:off 1:on
    1, // nr enable 0:off 1:on
    1, // pk enable 0:off 1:on
    1, // dnlp enable 0:off 1:on
    0, // dhz enable 0:off 1:on
    1, // lc enable 0:off 1:on
    1, // bsc enable 0:off 1:on
    1, // cnr2 enable 0:off 1:on
    1, // gamma enable 0:off 1:on
    1, // ccm enable 0:off 1:on
    1, // dmsc enable 0:off 1:on
    1, // csc enable 0:off 1:on
    1, // ptnr enable 0:off 1:on
    1, // amcm enable 0:off 1:on
    1, // flkr stat enable 0:off 1:on
    1, // flkr stat switch 0:from FEO 1:from NR 2:from Post
    1, // awb stat enable 0:off 1:on
    2, // awb stat switch 0:from FE 1:from GE 2:before WB 3:after WB 4:from DRC 5 or else:from peak
    1, // ae stat enable 0:off 1:on
    1, // ae stat switch 0:from GE 1:from LSC 2:before DRC 3:after DRC
    0, // af stat enable 0:off 1:on
    0, // af stat switch 0:from SNR 1:from DMS 2or3:from peak
    1, // WDR stat enable 0:off 1:on
    0, // debug path output 0:off 1:on
    0, // debug path data select
};

//aisp_res_t
static uint32_t _CALIBRATION_RES_CTL[7] = {
    0, //crop_en
    0, //crop_ofs_x
    0, //crop_ofs_y
    1920, //crop_width
    1080,  //crop_height
    0, //bin_en
    0, //bin_mode
};

//aisp_awb_t
static int32_t _CALIBRATION_AWB_CTL[25] = {
    1,       // u8, AWB auto enable
    1,       // u8, AWB manual mode, 0: manual gain mode, 1: manual temperature mode
    45,      //u8, AWB convergence speed.
    0,       //u8, mixed color temperature mode option. 0:mix mode 1:outdoor mode 2:indoor mode 3:auto mode
    32,      //u16, a cover range around planck curve
    1,       //u1, color temperature dynamic cover range enable
    0,       //u1, color temperature luma weighted calculation enable by luma value of local block
    1,       //u1, color temperature luma weighted calculation enable
    1,       //u1, color temperature adjust enable
    0,       //u1, local position weight
    1,       //u1, awb delay adjust enable
    10,      //u16, awb delay frame count
    200,     //u16, awb delay adjust tolerance by color temperature
    0,       //u1, awb Remove reference color
    0,       //u16, awb low luma ratio
    256,     //u1, awb color shading range
    256,     //u16, manual awb mode red gain
    256,     //u16, manual awb mode blue gain
    5000,     //u16, manual awb mode temperature
    0,       //bit[0] color temperature hist, [1] weight table, [2] ct table, [3] log
    0,       //u1, awb stable mode enable
    100,     //u16, awb stable mode ct delta det
    100,    //u16, awb stable mode color delta det
    0,      //u1, awb roi enable
    1,      //u12, awb roi weight init
};

static uint8_t _CALIBRATION_AWB_WEIGHT_H[17] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint8_t _CALIBRATION_AWB_WEIGHT_V[15] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

//_CALIBRATION_AWB_CT_POS
static uint32_t _CALIBRATION_AWB_CT_POS[20] = {10000,7500,6500,5000,4050,3850,2800,2400,2050};

//_CALIBRATION_AWB_CT_RG_COMPENSATE
static int32_t  _CALIBRATION_AWB_CT_RG_COMPENSATE[20] = {0,0,0,0,0,0,0,1,3};

//_CALIBRATION_AWB_CT_BG_COMPENSATE
static int32_t  _CALIBRATION_AWB_CT_BG_COMPENSATE[20] = {0,0,0,2,2,5,8,8,8};

//_CALIBRATION_AWB_CT_WGT
static int32_t _CALIBRATION_AWB_CT_WGT[20] = {1,1,2,3,2,1,1,1,1};

//_CALIBRATION_AWB_CT_DYN_CVRANGE
static int32_t _CALIBRATION_AWB_CT_DYN_CVRANGE[2][20] = {
    {-10,-8,-2,3,-4,-10,-12,-16,-16},
    {-10,-8,-2,3,-4,-10,-12,-15,-15},
};

//_CALIBRATION_AWB_REF_REMOVE_LUT
static int32_t _CALIBRATION_AWB_REF_REMOVE_LUT[20][3] =
{
    {2051, 1928, 64},
};

//aisp_ae_t
static int32_t _CALIBRATION_AE_CTL[32] = {
    1,  //ae auto enable
    0,  //ae exposure mode, 0: none, 1: spot mode 2:center mode 3: upper part mode 4: lower part mode
    0,  //ae exposure strategy, 0: none mode, 1: outdoor mode, 2:indoor mode
    0,  // ae route strategy, 0: exposure priority, 1: gain priority 2: external ae route
    3,  //ae route deflicker mode, 0: none, 1: anti-50hz, 2: anti-60hz, 3: auto detected
    64,   //exposure convergence speed [0, 128]
    128,  //ae global luma target compensation
    170,  //ae luma target srgb curve
    60,   //ae luma wdr target
    0,    //low light enhancement mode, 0: adjust exposure 1: adjust curve
    256,  //[0,256] low light enhancement strength
    4096, //[1024, 1024*(1<<8)]low light gain maximum limit
    16,    // [0,1024] high light reduce trigger threshold
    128,    // [0,1024] high light reduce strength
    10,   //ae tolerance
    0, //ae delay adjust enable
    10, //ae delay frame count
    100, //ae delay adjust tolerance
    400, //WDR mode only: ae WDR mode low light threshold, use ISO representation, ISO = times * 100
    77,   //WDR mode only: Max percentage of clipped pixels for long exposure: WDR mode only: 256 = 100% clipped pixels
    15,   //WDR mode only: Time filter for exposure ratio
    0,   //reduce fps feature enable.
    15,        //target fps of reduce frame rates.
    1600,   //trigger threshold of the reduce fps, use ISO representation, ISO = times * 100.
    50,   //lag threshold of the reduce fps, use ISO representation, (trigger threshold + lag threshold) or (trigger threshold - lag threshold).
    200,        // max isp gain limit, use ISO representation, ISO = times * 100
    1000000,     // max shutter time limit, this is absolute time, unit is us
    3200,       // max total gain limit, use ISO representation, ISO = times * 100
    16,       // max exposure ratio limit, this is times.
    (200*(1<<10)),  //  Light intensity at full exposure and zero gain , exp: 128lux = 128*(1<<10)
    2,       //feedback delay frame numbers of stats info in current system
    0,       //ae debug:bit[0] target, [1] ratio, [2] exposure calculate
};

//aisp_highlight_det_t
static int32_t _CALIBRATION_HIGHLIGHT_DETECT[27] = {
    //highlight all
    0,                  /**< u1, highlight enable, 0:disable; 1:enable */
    1,                  /**< u1, highlight mode, 0: manual, 1: auto */
    128,                /**< u8, manual highlight strength,range(0,255),recommend is (0,230), default is 128  */
    // highlight auto
    0,                  /**< u1, auto highlight car enable, if enable, when detected car light scene, will do highlight suppressive */
    128,                /**< u8, auto highlight car strength, range(0,255),recommend is (0,230), default is 128  */
    0,                  /**< u1, auto highlight window enable, if enable, when detected window scene, will do highlight suppressive */
    128,                /**< u8, auto highlight window strength, range(0,255),recommend is (0,128), default is 128 */
    //backlight compensation
    0,                  /**< u1,backligh compensation enable, 0:disable;1:enable */
    128,                /**< u8, backlight compensation strength,range(0,255),recommend is (0,230), default is 128  */
    //global
    102,                /**< u10, is used for highcontrast detect,the larger the value, the more difficult it is to detect as high-contrast scenes,default is 102 */
    1023,               /**< u12, is used for highcontrast detect,the smaller the value, the more difficult it is to detect as high-contrast scenes,default is 1023 */
    700,                /**< u10, is used for highcontrast detect,the larger the value, the more difficult it is to detect as high-contrast scenes,default is 700 */
    //local
    230,                /**< u8, the ratio0 to entry car light sense, the larger the vlaue, the more difficult to entry car light scene,default is 230*/
    20,                 /**< u8, the ratio1 to entry car light sense, the larger the vlaue, the more difficult to entry car light scene,default is 20*/
    0,                  /**< u8, the ratio2 to entry car light sense, the larger the vlaue, the more difficult to entry car light scene,default is 0*/
    2,                  /**< u4, local block threshold, if highlight area more than blk0xblk0, and less than (blk0+delta)x(blk0+delta),if no consider attenuation, it is car light scene,default is 2 */
    2,                  /**< u4, local block delta,if highlight area more than blk0xblk0, and less than (blk0+delta)x(blk0+delta),if no considerattenuation, it is car light scene,default is 2*/
    128,                /**< u8, the ratio0 to exit car light sense, the smaller the vlaue, the more difficult to exit,default is 128*/
    24,                 /**< u8, the ratio1 to exit car light sense, the smaller the vlaue, the more difficult to exit,default is 24*/
    5,                  /**< u8, auto highlight delay, default is 5*/
    10,                 /**< u5, auto highlight sense change threshold , default is 10  */
    //attenuation
    1,                  /**< u1, highlight attenuation enable, is used for car light detect, 0: disable, 1:enable */
    7,                  /**< u8, highlight attenuation threshold, is used for car light detect, default is 7*/
    6,                  /**< u4, highlight attenuation count, is used for car light detect, default is 6, max is 8*/
    //lowlight
    1,                  /**< u1, lowlight enable, 0: disable, 1:enable */
    20,                 /**< u5, 1: lowlight strength, default is 16 */
    //debug
    0,                  /**< u2, 1: print highlight/backlight parameters, 2, print car detect parameters,default is 0 */
};

static int32_t _CALIBRATION_AE_CORR_LUT[64] =  {128,128, 128, 128, 115, 100, 85, 75, 60, 50, 50};

static int32_t _CALIBRATION_AE_CORR_POS_LUT[64] = {42593+(0<<12),45590+(0<<12), 45590+(1<<12), 45590+(2<<12), 45590+(3<<12), 45590+(4<<12), 45590+(5<<12),45590+(6<<12),45590+(7<<12),45590+(8<<12),45590+(9<<12)};

static int32_t _CALIBRATION_AE_ROUTE[1+2*16] = {
/* shuttertime  | gain*/
/* x ms         | n gain */
    6,              /* total 8 joints */
    0, 10*(1<<12),  /*joint1: 0ms->10ms*/
    1, 2*(1<<12),   /*joint2: x1->x2 gain*/
    0, 20*(1<<12),  /*joint3: 10ms->20ms*/
    1, 4*(1<<12),   /*joint4: x2->x4 gain*/
    0, 40*(1<<12),  /*joint5: 20ms->40ms*/
    1, 64*(1<<12),  /*joint6: x4->x64 gain*/
};

static uint8_t _CALIBRATION_AE_WEIGHT_H[17] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint8_t _CALIBRATION_AE_WEIGHT_V[15] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint8_t _CALIBRATION_AE_WEIGHT_T[15][17] = {
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
};

//aisp_dn_det_t
static int32_t _CALIBRATION_DAYNIGHT_DETECT[7] = {
    0,    //light_control; 1:0n, 0: off
    0,    // status;
    70,   /**< u10, day brightness threshold, default is 70 */
    2500, /**< u10, night ir brightness threshold, default is 2500 */
    250,  /**< u10, night ir visible_light/IR threshold, default is 250*/
    2000, /**< u10, night light brightness threshold, default is 2000*/
    0,    /**< u1, daynight debug enable, default is 0*/
};

//aisp_af_t
static uint32_t _CALIBRATION_AF_CTL[23] = {
    1, //af_en;
    70 << 6,  //af_pos_min_down;
    70 << 6,  //af_pos_min;
    70 << 6,  //af_pos_min_up;
    112 << 6, //af_pos_inf_down;
    112 << 6, //af_pos_inf;
    112 << 6, //af_pos_inf_up;
    832 << 6, //af_pos_macro_down;
    832 << 6, //af_pos_macro;
    832 << 6, //af_pos_macro_up;
    915 << 6, //af_pos_max_down;
    915 << 6, //af_pos_max;
    915 << 6, //af_pos_max_up;
    11,  //af_fast_search_positions;
    6,   //af_skip_frames_init;
    2,   //af_skip_frames_move;
    30,  //af_dynamic_range_th;
    2 << ( 12 - 2 ),  //af_spot_tolerance;
    1 << ( 12 - 1 ),  //af_exit_th;
    16 << ( 12 - 4 ), //af_caf_trigger_th;
    4 << ( 12 - 4 ),  //af_caf_stable_th;
    0,//af_print_debug;
    1,//af_mode;0:AF, 1:CAF, 2:MANUAL, 3:CLBT;
};

static uint8_t _CALIBRATION_AF_WEIGHT_H[17] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint8_t _CALIBRATION_AF_WEIGHT_V[15] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

//aisp_flkr_t
static uint32_t _CALIBRATION_FLICKER_CTL[20] = {
    1,      //u32, whether delete invalid flicker
    0,      //u32, 0: half (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic, 1: the whole (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic.
    1,      //u32, 0:no lpf,1: [1 2 1]/4, 2: [1 2 2 2 1]/8, 3: [1 1 1 2 1 1 1]/8, 4 or else: [1 2 2 2 2 2 2 2 1]/16, lpf of row avg for flicker detection
    30,      //u32, output flicker result after flkr_det_cnt
    64000,  //u32, peaks/valleys interval thrd for valid wave
    3,      //u32, peaks/valleys value for valid wave
    5,     //u32, peaks/valleys value difference for valid wave
    1,      //u32, enable fft valid flicker detection
    512,   //u32, fft nlen, default value is recommended
    1,      //u32, fft mlen, default value is recommended
    100,      //u32, fft norm, default value is recommended
    1000,   //u32, threshold for valid flicker of fft, default value is recommended
    1,      //u32, sensor exposure information adjust gain, default is 1, 2x2bin is 2
    20,     //u32, normalize to u4
    500,    //u32, flkr_det_sum_pdif_th
    150,    //u32 flkr_det_scan_ofst
    20,     //u32 flkr_det_wave_ofst
    2000,   //u32 flkr_det_ae_diff_th
    10,     //u32 flkr_det_noflkr_cnt_th
    30,     //u32 sum_pdif ratio
};

static uint16_t _CALIBRATION_GTM[129]= {
    0,  32,  64,  96,  128, 160,  192,  224,  256,  288,  320,  352,  384,  416,  448,  480,  512,
    544,  576,  608,  640,  672,  704,  736,  768,  800,  832,  864,  896,  928,  960,  992,  1024,
    1056,  1088,  1120,  1152,  1184,  1216,  1248,  1280,  1312,  1344,  1376,  1408,  1440,  1472,
    1504,  1536,  1568,  1600,  1632,  1664,  1696,  1728,  1760,  1792,  1824,  1856,  1888,  1920,
    1952,  1984,  2016,  2048,  2080,  2112,  2144,  2176,  2208,  2240,  2272,  2304,  2336,  2368,
    2400,  2432,  2464,  2496,  2528,  2560,  2592,  2624,  2656,  2688,  2720,  2752,  2784,  2816,
    2848,  2880,  2912,  2944,  2976,  3008,  3040,  3072,  3104,  3136,  3168,  3200,  3232,  3264,
    3296,  3328,  3360,  3392,  3424,  3456,  3488,  3520,  3552,  3584,  3616,  3648,  3680,  3712,
    3744,  3776,  3808,  3840,  3872,  3904,  3936,  3968,  4000,  4032,  4064,  4095,
};

//aisp_ge_adj_t
static uint16_t _CALIBRATION_GE_ADJ[ISO_NUM_MAX][8] = {
/*stat_edge_thd|ge_hv_thrd|ge_hv_wtlut[4]|reserve*/
    { 72,     48,    10,10,10,10,    0,0,},
    { 72,     48,    12,12,12,12,    0,0,},
    { 72,     48,    14,14,14,14,    0,0,},
    { 72,     48,    14,14,14,14,    0,0,},
    { 96,     64,    16,16,16,16,    0,0,},
    { 80,     56,    16,16,16,16,    0,0,},
    { 96,     64,    18,18,18,18,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
};

//aisp_ge_adj_t
static uint16_t _CALIBRATION_GE_S_ADJ[ISO_NUM_MAX][8] = {
/*stat_edge_thd|ge_hv_thrd|ge_hv_wtlut[4]|reserve*/
    { 72,     48,    10,10,10,10,    0,0,},
    { 72,     48,    12,12,12,12,    0,0,},
    { 72,     48,    14,14,14,14,    0,0,},
    { 72,     48,    14,14,14,14,    0,0,},
    { 96,     64,    16,16,16,16,    0,0,},
    { 80,     56,    16,16,16,16,    0,0,},
    { 96,     64,    18,18,18,18,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
    {106,     72,    22,22,22,22,    0,0,},
};

//aisp_dpc_ctl_t
static uint8_t _CALIBRATION_DPC_CTL[8] = {
    1,         /**< cor_en */
    0,         /**< avg_dev_mode */
    3,         /**< avg_mode */
    0,         /**< avg_thd2_en */
    0,         /**< highlight_en */
    3,         /**< correct_mode */
    0,         /**< write_to_lut */
    0,         /**< reserve */
};

//aisp_dpc_ctl_t
static uint8_t _CALIBRATION_DPC_S_CTL[8] = {
    1,         /**< cor_en */
    0,         /**< avg_dev_mode */
    3,         /**< avg_mode */
    0,         /**< avg_thd2_en */
    0,         /**< highlight_en */
    3,         /**< correct_mode */
    0,         /**< write_to_lut */
    0,         /**< reserve */
};

//aisp_dpc_adj_t
static uint16_t _CALIBRATION_DPC_ADJ[ISO_NUM_MAX][12] = {
/* avg_gain_l0|avg_gain_h0|avg_gain_l1|avg_gain_h1|avg_gain_l2|avg_gain_h2|cond_en|max_min_bias_thd|std_diff_gain|std_gain|avg_dev_offset|reserve */
    {30,    750,    40,    650,    50,    550,    0,    3,     22,    12,    0,    0,},
    {30,    750,    40,    650,    50,    550,    0,    5,     22,    12,    0,    0,},
    {50,    750,    55,    600,    60,    500,    0,    10,    22,    12,    0,    0,},
    {50,    600,    55,    500,    60,    400,    0,    15,    22,    12,    0,    0,},
    {50,    500,    55,    400,    60,    350,    0,    20,    22,    12,    0,    0,},
    {50,    500,    55,    400,    60,    350,    0,    25,    22,    12,    0,    0,},
    {50,    450,    55,    400,    60,    350,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
};

//aisp_dpc_adj_t
static uint16_t _CALIBRATION_DPC_S_ADJ[ISO_NUM_MAX][12] = {
/* avg_gain_l0|avg_gain_h0|avg_gain_l1|avg_gain_h1|avg_gain_l2|avg_gain_h2|cond_en|max_min_bias_thd|std_diff_gain|std_gain|avg_dev_offset|reserve */
    {30,    750,    40,    650,    50,    550,    0,    3,     22,    12,    0,    0,},
    {30,    750,    40,    650,    50,    550,    0,    5,     22,    12,    0,    0,},
    {50,    750,    55,    600,    60,    500,    0,    10,    22,    12,    0,    0,},
    {50,    600,    55,    500,    60,    400,    0,    15,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    0,    20,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    0,    25,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
    {50,    400,    55,    350,    60,    300,    30,    30,    22,    12,    0,    0,},
};

//aisp_wdr_t
static int32_t _CALIBRATION_WDR_CTL[35] = {
    // 1) wdr hr regs
    0,                              // u1, WDR motion detection enable,0: disable, 1: enable,
    0,                              // u1, Pixel value wi/wo blc mode in MD,0: pixel value without blc for MD threshold calculation, 1: pixel value with blc for MD threshold calculation,
    0,                              // u2, Check saturation mode in MD,0:  check G & C with blc, 1: check G & C without blc, 2: check G & C with blc*, 3: check G & C without blc*,
    0,                              // u1, Motion map mode,0: final map determined by Gdiff, 1: final map determined by MAX3(Gmap, Rmap, Bmap),
    0,                              // u1, Check still in motion decision, 0: check G without blc, 1: check G & C without blc,
    0,                              // u8, Reduce motion map value in order to include more long exposure data,
    20,                             // u8, When motion map value is less than this threshold, this motion map is set as 0,
    2,                              // u2, WDR forcelong feature enable, 0: disable, 1:based on 32x32 long exposure data in previous frame, 2:based on theoretical model, increase short-exp data under specific condition to avoid discontinuity,
    0,                              // u1, WDR forcelong feature threshold calculation mode, 0: flong1 mode, 1:flong2 mode,
    3,                              // u2, WDR exposure fusing mode, 0:original long and short data, 1:check G with BLC, 2:check G without BLC, 3:check G & C with BLC, 4:check G & C without BLC,
    4,                              // u4, Final index calculated by ratio of max and avg 0->using max, (short exp) 15-> using avg(long exp),
    1,                              // u1,  WDR stat lpf enable,, 0: disable, 1:enable,
    // 2) fw regs for lo/hi_weight
    0,                              // s8, Low weight offset[0] for MD,
    0,                              // s8, Low weight offset[1] for MD,
    0,                              // s8, Low weight offset[2] for MD,
    0,                              // s8, Hi weight offset[0] for MD,
    0,                              // s8, Hi weight offset[0] for MD,
    0,                              // s8, Hi weight offset[0] for MD,
    // 3) fw regs for motion detection
    0,                              // u1, auto enable
    0,                              // u1, MD saturation thd calc mode, 0: user defined; 1: firmware calculation,
    0,                              // u1, MD weight calculation mode, 0: user defined; 1: fw calculation
    0,                              // s8, user defined gr saturation margin for motion detection
    0,                              // s8, user defined gb saturation margin for motion detection
    0,                              // s8, user defined rg saturation margin for motion detection
    0,                              // s8, user defined bg saturation margin for motion detection
    0,                              // s8, user defined ir saturation margin for motion detection
    // 4) fw regs for forcelong
    1,                              // u1,flong1 mode;0: used defined,1: firmware calculation
    800,                            // u14,threshold of day scene discrimination
    500,                            // u14,threshold of night scene discrimination
    1000,                           // u14,low threshold for day scene discrimination
    1600,                           // u14,high threshold for day scene discrimination
    2000,                           // u14,low threshold for night scene discrimination
    3800,                           // u14,high threshold for night scene discrimination
    // 5) fw regs for force exp
    0,                              // u1, force long exp function,0: disable; 1:enable
    0,                              // u3, when force long exp is enabled, using reg_wdr_force_exp_mode to select the out exp, 0: long exp; 1/2/3/7: short1 exp; 4: original long; 5: mapMask; 6:blendRatio;
};

static int32_t _CALIBRATION_WDR_ADJUST[ISO_NUM_MAX][6] = {
    /* mdetc ratio| noise gain | noise flor | bl cmpn | flong1_thd0 | flong1_thd1*/
    { 128,     1,     1,    32,     400,    600},
    { 256,     2,     2,    48,     400,    600},
    { 512,     4,     3,    56,     400,    600},
    {1024,     8,     4,    64,     400,    600},
    {1536,    16,     6,    78,     400,    600},
    {2048,    32,    11,    86,     400,    600},
    {2560,    64,    17,    92,     400,    600},
    {3072,    64,    21,    104,    400,    600},
    {3584,    64,    21,    110,    400,    600},
    {4096,    64,    21,    128,    400,    600},
};

static uint8_t _CALIBRATION_WDR_MDETC_LOWEIGHT[ISO_NUM_MAX][ISO_NUM_MAX] = {
/*ratio\iso: 100 | 200 | 400 | 800 | 1600 | 3200 | 6400 | 12800 | 25600 | 51200*/
/*128*/    { 64,   64,   64,   72,   80,    96,    128,   255,    255,    255,},
/*256*/    { 64,   64,   64,   80,   96,    96,    128,   255,    255,    255,},
/*512*/    { 72,   72,   80,   96,   112,   128,   192,   255,    255,    255,},
/*1024*/   { 80,   80,   88,   104,  128,   160,   192,   255,    255,    255,},
/*1536*/   { 88,   88,   88,   128,  160,   255,   255,   255,    255,    255,},
/*2048*/   { 96,   96,   128,  160,  192,   255,   255,   255,    255,    255,},
/*2560*/   {128,   128,  160,  192,  224,   255,   255,   255,    255,    255,},
/*3072*/   {160,   160,  176,  192,  224,   255,   255,   255,    255,    255,},
/*3584*/   {192,   192,  208,  224,  255,   255,   255,   255,    255,    255,},
/*4096*/   {255,   255,  255,  255,  255,   255,   255,   255,    255,    255,},
};

static uint8_t _CALIBRATION_WDR_MDETC_HIWEIGHT[ISO_NUM_MAX][ISO_NUM_MAX] = {
/*ratio\iso: 100 | 200 | 400 | 800 | 1600 | 3200 | 6400 | 12800 | 25600 | 51200*/
/*128*/    { 88,    90,    90,    96,    96,   128,   255,   255,   255,   255,},
/*256*/    { 90,    90,    96,   104,   112,   128,   255,   255,   255,   255,},
/*512*/    { 94,    94,   104,   112,   120,   128,   255,   255,   255,   255,},
/*1024*/   { 96,    96,   112,   120,   128,   128,   255,   255,   255,   255,},
/*1536*/   {104,   104,   112,   136,   168,   255,   255,   255,   255,   255,},
/*2048*/   {128,   128,   160,   160,   192,   255,   255,   255,   255,   255,},
/*2560*/   {160,   160,   176,   184,   192,   255,   255,   255,   255,   255,},
/*3072*/   {192,   192,   200,   224,   255,   255,   255,   255,   255,   255,},
/*3584*/   {255,   255,   255,   255,   255,   255,   255,   255,   255,   255,},
/*4096*/   {255,   255,   255,   255,   255,   255,   255,   255,   255,   255,},
};

static uint32_t _CALIBRATION_OE_EOTF[34]= {
    5,4,4,4,4,4,3,3,   //sqrt num
    11,12,13,13,13,13,14,15,     //sqrt step
    5,4,4,4,4,4,3,3,         //eotf num
    11,12,13,13,13,13,14,15,     //eotf step
    0,0         //sqrt_pre_ofst   eotf_pst_ofst
};

static uint32_t _CALIBRATION_SQRT1[] = {
         0,   8144,  16194,  24151,  32017,  39794,  47482,  55084,
     62601,  70034,  77385,  84655,  91846,  98958, 105993, 112952,
    119837, 126648, 133387, 140055, 146653, 153183, 159644, 166039,
    172368, 178633, 184833, 190971, 197047, 203063, 209018, 214914,
    220752, 232257, 243540, 254606, 265462, 276114, 286567, 296827,
    306900, 316790, 326502, 336042, 345413, 354620, 363667, 372559,
    381300, 398341, 414821, 430766, 446202, 461153, 475642, 489690,
    503316, 516539, 529378, 541847, 553964, 565743, 577197, 588341,
    599186, 609744, 620027, 630045, 639809, 649327, 658609, 667664,
    676500, 685125, 693546, 701770, 709805, 717656, 725330, 732833,
    740171, 747348, 754371, 761243, 767971, 774557, 781008, 787326,
    793516, 799583, 805528, 811357, 817072, 822676, 828174, 833568,
    838860, 844054, 849153, 854159, 859074, 863901, 868642, 873300,
    877877, 882375, 886795, 891141, 895413, 899613, 903745, 907808,
    911805, 919606, 927161, 934482, 941578, 948460, 955138, 961620,
    967916, 979977, 991380,1002178,1012418,1022141,1031386,1040187,1048575};

static uint32_t _CALIBRATION_EOTF1[] = {
         0,    512,   1027,   1542,   2060,   2578,   3099,   3621,
      4144,   4669,   5196,   5724,   6253,   6785,   7318,   7852,
      8388,   8926,   9465,  10006,  10549,  11093,  11639,  12186,
     12735,  13286,  13839,  14393,  14949,  15506,  16066,  16627,
     17189,  18320,  19458,  20602,  21754,  22913,  24080,  25253,
     26434,  27623,  28819,  30022,  31234,  32453,  33680,  34914,
     36157,  38667,  41210,  43786,  46397,  49042,  51723,  54440,
     57195,  59987,  62817,  65688,  68598,  71549,  74543,  77579,
     80659,  83784,  86955,  90172,  93437,  96751, 100115, 103530,
    106997, 110518, 114093, 117725, 121414, 125161, 128969, 132838,
    136770, 140767, 144830, 148962, 153162, 157434, 161780, 166200,
    170698, 175275, 179933, 184674, 189501, 194416, 199422, 204521,
    209715, 215007, 220401, 225899, 231503, 237218, 243047, 248992,
    255059, 261249, 267567, 274018, 280604, 287332, 294204, 301227,
    308404, 323245, 338770, 355029, 372075, 389966, 408766, 428548,
    449389, 494611, 545259, 602373, 667275, 741675, 827823, 928738,1048575};

static int32_t _CALIBRATION_RAWCNR_CTL[3] = {
    1,  //rawcnr_totblk_higfrq_en
    1,  //rawcnr_curblk_higfrq_en
    0,  //rawcnr_ishigfreq_mode
};

//aisp_rawcnr_adj_t
static uint16_t _CALIBRATION_RAWCNR_ADJ[ISO_NUM_MAX][10] = {
/*sad_cor_np_gain | sublk_sum_dif_thd|curblk_sum_difnxn_thd|ya_min|ya_max|ca_min|ca_max|reserve*/
    {8,      50,80,    1800,1600,    0,    30,    0,    10,    0,},
    {8,    100,150,    1800,1600,    0,    40,    0,    20,    0,},
    {8,    100,200,    1800,1600,    0,    42,    0,    22,    0,},
    {8,    200,300,    1800,1600,    0,    44,    0,    24,    0,},
    {8,    200,300,    1800,1600,    0,    46,    0,    26,    0,},
    {8,    380,460,    1800,1600,    0,    50,    0,    30,    0,},
    {8,    460,520,    1800,1600,    0,    56,    0,    34,    0,},
    {8,    460,520,    1800,1600,    0,    60,    0,    36,    0,},
    {8,    460,520,    1800,1600,    5,    65,    5,    36,    0,},
    {8,    460,520,    1800,1600,    5,    70,    5,    38,    0,},
};

//aisp_rawcnr_t->rawcnr_meta_gain_lut
static uint8_t _CALIBRATION_RAWCNR_META_GAIN_LUT[ISO_NUM_MAX][8] = {
    {20,20,18,16,16,16,16,8,},
    {20,20,18,16,16,16,16,16,},
    {20,20,18,16,16,16,16,16,},
    {20,20,18,16,16,16,16,16,},
    {20,20,18,16,16,16,16,16,},
    {22,22,20,18,18,18,18,18,},
    {24,24,20,20,20,20,20,20,},
    {24,24,20,20,20,20,20,20,},
    {24,24,20,20,20,20,20,20,},
    {24,24,20,20,20,20,20,20,},
};

//aisp_rawcnr_t->rawcnr_sps_csig_weight5x5
static int8_t _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5[ISO_NUM_MAX][25] = {
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
    {1,1,1,0,0,1,2,1,1,0,1,2,2,1,0,1,2,1,1,0,1,1,1,0,0,},
};

//aisp_snr_ctl_t
static int32_t _CALIBRATION_SNR_CTL[34] = {
    1,  //snr_luma_adj_en
    1,  //snr_sad_wt_adjust_en
    0,  //snr_mask_en
    1,  //snr_meta_en
    0,  //rad_snr1_en
    //snr_grad_gain[5]
    63, 48, 36, 32, 24,
    //snr_sad_th_mask_gain[4]
    32, 32, 32, 32,
    0, //snr_coring_mv_gain_x
    4, //snr_coring_mv_gain_xn
    //snr_coring_mv_gain_y[2]
    52, 52,
    1,  //snr_wt_var_adj_en
    4,  //snr_wt_var_th_x
    2,2,3,  //snr_wt_var_th_x
    255, 64, 64,  //snr_wt_var_th_y
    64,64,64,64,64,64,64,64,      //snr_mask_adj
};

//aisp_ snr_glb_adj_t
static uint16_t _CALIBRATION_SNR_GLB_ADJ[ISO_NUM_MAX][6] = {
/*snr_np_lut16_glb_adj|snr_meta2alp_glb_adj|snr_meta_gain_glb_adj|snr_wt_luma_gain_glb_adj|snr_grad_gain_glb_adj|snr_sad_th_mask_gain_glb_adj*/
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
    {256,    256,    256,    256,    256,    256,},
};

//aisp_snr_adj_t
static int16_t _CALIBRATION_SNR_ADJ[ISO_NUM_MAX][16] = {
/*weight|NP adj|cor_profile_adj|cor_profile_ofst|sad_wt_sum_th[2]|th_x0 x1 x2|var_flat_th_y[3]|sad_meta_ratio[4]*/
    { 72,    256,    24,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    { 80,    256,    24,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    { 96,    256,    24,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    {100,    256,    28,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    {100,    256,    28,    0,    1700,1024,    64,4,5,    32,44,48,    12,14,16,16,},
    {100,    256,    30,    0,    1800,1024,    64,7,8,    32,44,63,    8,10,12,16,},
    {100,    256,    30,    0,    2000,1536,    64,7,8,    32,48,63,    8,10,12,16,},
    {100,    256,    32,    4,    2100,2048,    64,7,8,    32,48,63,    8,10,12,16,},
    {100,    256,    32,    4,    2100,2048,    64,7,8,    32,48,63,    8,10,12,16,},
    {100,    256,    32,    6,    2100,2048,    64,7,8,    32,48,63,    8,10,12,16,},
};

//aisp_snr_t->snr_cur_wt
static uint16_t _CALIBRATION_SNR_CUR_WT[ISO_NUM_MAX][8] = {
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,460,500,},
    {255,280,320,360,400,400,400,400,},
    {255,280,300,300,300,300,300,300,},
    {255,280,300,300,300,300,300,300,},
    {255,280,300,300,300,300,300,300,},
};

//aisp_snr_t->snr_wt_luma_gain
static uint8_t _CALIBRATION_SNR_WT_LUMA_GAIN[ISO_NUM_MAX][8] = {
    {10,8,8,6,6,6,8,14,},
    {12,12,8,6,6,10,20,30,},
    {14,13,10,10,12,18,20,30,},
    {16,15,11,11,14,20,26,32,},
    {18,16,12,12,18,24,28,32,},
    {20,20,14,14,22,27,32,32,},
    {28,28,24,22,28,32,32,32,},
    {32,32,30,30,31,32,32,32,},
    {32,32,32,32,32,32,32,32,},
    {32,32,32,32,32,32,32,32,},
};

//aisp_snr_t->snr_sad_meta2alp
static uint8_t _CALIBRATION_SNR_SAD_META2ALP[ISO_NUM_MAX][8] = {
    {120,100,72,64,45,20,6,6,},
    {130,100,80,72,56,24,6,6,},
    {130,110,90,80,64,36,8,8,},
    {150,130,100,90,72,45,12,10,},
    {150,130,100,90,72,48,16,12,},
    {180,150,130,100,86,56,20,16,},
    {220,200,180,140,100,72,30,22,},
    {240,230,210,180,130,100,68,28,},
    {250,230,210,190,170,140,72,28,},
    {250,230,210,190,170,140,72,56,},
};

//aisp_snr_t->snr_meta_adj
static uint8_t _CALIBRATION_SNR_META_ADJ[ISO_NUM_MAX][8] = {
    {32,32,30,26,20,10,6,6,},
    {32,32,30,26,22,14,6,6,},
    {32,32,30,28,26,18,8,8,},
    {32,32,30,30,28,20,12,12,},
    {32,32,30,30,28,20,14,14,},
    {36,32,30,30,28,30,20,20,},
    {45,32,30,30,28,30,26,22,},
    {60,55,40,32,30,30,32,32,},
    {60,55,40,32,30,30,32,32,},
    {60,55,40,32,30,30,32,32,},
};

static uint8_t _CALIBRATION_SNR_PHS[4][24] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {1, 2, 1, 2, 2, 1, 2, 1, 1, 2, 2, 1, 2, 1, 1, 2, 1, 2, 1, 1, 2, 1, 2, 2,},
    {2, 1, 2, 1, 1, 2, 1, 2, 2, 1, 1, 2, 1, 2, 2, 1, 2, 1, 2, 2, 1, 2, 1, 1,},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,},
};

static uint8_t _CALIBRATION_NR_RAD_LUT65[3][65] = {
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 96,},
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 96, 128,},
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 72, 100, 128, 128,},
};

//aisp_psnr_adj_t
static uint16_t _CALIBRATION_PST_SNR_ADJ[ISO_NUM_MAX][2] = {
/* post NR Y | Post NR chroma*/
    {16,    16,},
    {20,    20,},
    {24,    24,},
    {26,    26,},
    {28,    28,},
    {36,    36,},
    {48,    48,},
    {56,    56,},
    {60,    60,},
    {63,    63,},
};

//aisp_tnr_ctl_t
static int32_t _CALIBRATION_TNR_CTL[30] = {
    60,5,6,    //reg_ma_mix_th_x[3]
    1,    //reg_rad_tnr0_en
    2,6,6,    //reg_ma_sad_pdtl4_x[3]
    2,8,12,    //reg_ma_sad_pdtl4_y[3]
    0,    //reg_ma_adp_dtl_mix_th_nfl
    16,16,16,16,    //reg_ma_sad_th_mask_gain[4]
    16,16,16,16,    //reg_ma_mix_th_mask_gain[4]
    64,    //reg_ma_mix_ratio
    70,300,700,1400,//reg_ma_sad_luma_adj_x[4];
    32,24,18,16,16,//reg_ma_sad_luma_adj_y[5];
    32,    //reg_ma_mix_th_iso_gain
};

//aisp_tnr_glb_adj_t
static uint16_t _CALIBRATION_TNR_GLB_ADJ[ISO_NUM_MAX][4] = {
/*tnr_ma_mix_h_th_glb_adj|tnr_ma_np_lut16_glb_adj|tnr_ma_sad2alp_glb_adj|tnr_mc_meta2alp_glb_adj*/
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
    {256,    256,    256,    256,},
};

//aisp_tnr_adj_t
static int16_t _CALIBRATION_TNR_ADJ[ISO_NUM_MAX][26] = {
/*tnr_np_gain|tnr_np_ofst|ma_mix_h_th_gain[4]|reg_ma_mix_h_th_y|reg_ma_mix_l_th_y|reg_ma_sad_var_th_x_xx|reg_ma_sad_var_th_y_xx|me_sad_cor_np_gain|me_sad_cor_np_ofst|me_meta_sad_th0|reg_me_meta_sad_th1*/
    { 4,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,     0,    0,    10,20,30,    10,20,30,},
    {6,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,     0,    0,    10,20,30,    10,20,30,},
    {8,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,    0,    0,    10,20,30,    10,20,30,},
    {10,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,    0,    0,    10,20,30,    10,20,30,},
    {12,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,    4,    0,    10,20,30,    10,20,30,},
    {18,     0,    16,20,22,24,    30,30,40,    30,30,40,    50,4,5,    32,32,32,    12,    0,    10,20,30,    10,20,30,},
    {18,     0,    16,20,22,24,    30,40,50,    25,35,45,    50,4,5,    32,32,32,    14,    0,    10,20,30,    10,20,30,},
    {20,     8,    25,35,40,50,    35,40,50,    25,35,45,    40,4,5,    32,32,32,     24,    0,    10,20,30,    10,20,30,},
    {30,    12,    30,40,45,55,    75,75,75,    50,50,50,    40,4,5,    32,32,32,     24,    0,    10,20,30,    10,20,30,},
    {30,    12,    30,40,45,55,    75,75,75,    50,50,50,    40,4,5,    32,32,32,     24,    0,    10,20,30,    10,20,30,},
};

//aisp_tnr_ratio_t
static int16_t _CALIBRATION_TNR_RATIO[RATIO_NUM_MAX][10] = {
/*tnr_sad_cor_np_gain_ratio|tnr_sad_cor_np_ofst_ratio|tnr_ma_sad_th_mask_gain_ratio|tnr_ma_mix_th_mask_gain_ratio*/
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
    {256,    0,    64,64,64,64,    64,64,64,64,},
};

//aisp_tnr_t->nr_ma_sad2alpha
static uint8_t _CALIBRATION_TNR_SAD2ALPHA[ISO_NUM_MAX][64] = {
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 36,36,36,35,35,34,34,34, 40,40,40,40,40,40,38,38,},
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 36,36,36,35,35,34,34,34, 45,45,45,45,45,43,41,40,},
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 38,38,38,37,36,36,36,36, 48,48,48,48,48,46,45,44,},
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 38,38,38,37,36,36,36,36, 54,54,54,54,52,50,50,48,},
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 40,40,40,40,40,38,38,38, 56,56,56,56,54,53,52,50,},
    {2,1,0,0,0,0,0,0, 16,16,16,15,15,15,14,14, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 28,28,28,27,27,26,26,25, 30,30,29,29,28,28,27,26, 40,40,40,40,40,38,38,38, 56,56,56,56,54,53,52,52,},
    {2,2,1,1,0,0,0,0, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 26,26,26,25,25,24,24,24, 30,30,30,29,29,29,28,28, 34,34,34,33,33,32,32,31, 42,42,42,42,42,40,40,40, 56,56,56,56,55,53,53,53,},
    {2,2,1,1,0,0,0,0, 20,20,20,19,19,18,18,18, 24,24,24,23,23,23,22,22, 26,26,26,25,25,24,24,24, 36,36,36,34,34,32,32,30, 40,40,40,39,39,38,38,38, 46,46,44,42,42,42,40,40, 58,58,58,58,58,56,54,54,},
    {4,3,2,1,1,0,0,0, 26,26,26,25,25,24,24,24, 30,30,30,29,29,29,28,28, 36,36,36,34,34,32,32,30, 40,40,40,39,39,38,38,37, 42,42,42,40,40,39,38,38, 48,48,46,45,45,44,43,42, 60,60,60,59,59,58,56,56,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,59,59,58,58,57,},
};

//aisp_tnr_t->tnr_mc_meta2alpha
static uint8_t _CALIBRATION_MC_META2ALPHA[ISO_NUM_MAX][64] = {
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
    {0,0,0,0,0,0,0,0, 5,5,5,5,5,3,3,3, 8,8,8,6,6,6,6,6, 10,10,8,8,8,8,6,6, 12,11,10,9,9,9,8,8, 14,12,10,9,9,9,9,9, 16,14,12,11,11,11,11,11, 16,16,14,13,13,12,12,12,},
};

//aisp_tnr_t->ptnr_alp_lut
static uint8_t _CALIBRATION_PST_TNR_ALP_LUT[ISO_NUM_MAX][8] = {
    {0,2,4,12,18,22,26,28,},
    {0,2,4,12,18,24,26,32,},
    {0,2,6,14,22,26,28,32,},
    {0,2,6,14,22,28,32,34,},
    {0,2,8,16,24,28,32,36,},
    {0,2,8,16,24,28,36,40,},
    {0,2,8,16,24,32,36,45,},
    {0,2,8,16,24,34,38,50,},
    {0,2,8,16,24,36,40,55,},
    {0,2,8,16,24,36,42,60,},
};

static uint32_t _CALIBRATION_COMPRESS_RATIO[2] =
{
    8, //decmpr tnr compress bits ratio = 16xbpp / 256.
    0, //reserved.
};

//aisp_lsc_adj_t
static uint16_t _CALIBRATION_LENS_SHADING_ADJ[ISO_NUM_MAX][2] = {
/* radial shding strength  | mesh shding strength*/
    {256, 256,}, //x1 gain
    {256, 256,}, //x2 gain
    {256, 256,}, //x4 gain
    {256, 256,}, //x8 gain
    {256, 256,}, //x16 gain
    {128,  128,}, //x32 gain
    {64,  64,}, //x64 gain
    {64,   64,}, //x128 gain
    {64,   64,}, //x256 gain
    {64,   64,}, //x512 gain
};

static int32_t _CALIBRATION_LENS_SHADING_CT_CORRECT[4] = {
/*    TL40 diff           |   CWF color diff */
    0, 0,
    4000, 4000
};

static uint32_t _CALIBRATION_ADP_LENS_SHADING_CTL[11] =
{
    0,  //adaptive lens shading en. 1: alsc by lut 2: alsc by stats
    128, // u8, adaptive adjust speed
    256,//adaptive speed max 256
    16, //adaptive stabilize threshold
    24, //adaptive stabilize maximum threshold >= th
    8, //delay frame numbers
    200,//red color shift minimum value
    800,//blue color shift minimum value
    700,//red color shift maximum value
    200,//blue color shift maximum value
    0, //debug
};

static int32_t _CALIBRATION_LENS_SHADING_ADP[129] =
{
    14,
    /*r_gain | b_gain | crlns_r | crlns b*/
    284, 588, 4506, 3696, //dnp 2800k
    360, 460, 4463, 3749, //dnp 3850k
    382, 438, 4437, 3770, //dnp 4150k
    504, 377, 4333, 3761, //dnp 6500k
    258, 656, 4650, 3640, //flu a
    394, 555, 4019, 3604, //flu cwf
    488, 421, 4369, 3752, //flu d65
    365, 586, 4057, 3645, //flu tl84
    482,  445, 4126, 3756, //ext 0
    498,  416, 4049, 3777,//ext led
    480,  399, 4383, 3655,
    449,  445, 3995, 4473,
    524,  339, 4200, 3722,
};

//aisp_dms_t
static uint16_t _CALIBRATION_DMS_ADJ[ISO_NUM_MAX][3] = {
/*plp_alp | detail_non_dir_th_min  | detail_non_dir_th_max*/
   {0,   5,  900},
   {0,   5,  800},
   {0,   5,  700},
   {0,   5,  700},
   {0,   5,  600},
   {0,   5,  550},
   {0,  10,  512},
   {0,  16,  512},
   {0,  16,  512},
   {0,  16,  512},
};

//aisp_ccm_t->ccm_str
static uint32_t _CALIBRATION_CCM_ADJ[ISO_NUM_MAX][1] = {
/* Color Correct strength */
    {110,}, //x1 gain
    {110,}, //x2 gain
    {110,}, //x4 gain
    {110,}, //x8 gain
    {100,}, //x16 gain
    {100,}, //x32 gain
    {100,}, //x64 gain
    {80,}, //x128 gain
    {80,}, //x256 gain
    {80,}, //x512 gain
};

//aisp_cnr_ctl_t
static int32_t _CALIBRATION_CNR_CTL[6] = {
    0,  //cnr_map_xthd
    16, //cnr_map_kappa
    15, //cnr_map_ythd0
    15, //cnr_map_ythd1
    12, //cnr_map_norm
    10, //cnr_map_str
};

//aisp_cnr_adj_t
static uint16_t _CALIBRATION_CNR_ADJ[ISO_NUM_MAX][6] = {
/*cnr_weight|umargin_up|umargin_dw|vmargin_up|vmargin_dw| alp_mode*/
    {32,    32,    32,    32,    32,    0,},
    {32,    32,    32,    32,    32,    0,},
    {32,    32,    32,    32,    32,    0,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
    {48,    42,    32,    42,    32,    2,},
};

//aisp_purple_ctl_t
static int32_t _CALIBRATION_PURPLE_CTL[4] = {
    120,//purple_luma_osat_thd
    0,// pfr_mode
    3, //pfr_window_h
    2, //pfr_window_v
};

//aisp_purple_adj_t
static uint16_t _CALIBRATION_PURPLE_ADJ[ISO_NUM_MAX][7] = {
/*purple_weight|purple_umargin_up|purple_umargin_dw|purple_vmargin_up|purple_vmargin_dw| purple_cst_thd|purple_desat_en|*/
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
    {48,    48,    32,    48,    32,    5,    2,},
};

//aisp_ltm_t
static int32_t _CALIBRATION_LTM_CTL[14] = {
    1,       //ltm_auto_en
    8,       //ltm_damper64
    56,       //ltm_lmin_alpha
    32,       //ltm_lmax_alpha
    32,  //ltm_hi_gm_u7
    20,  //ltm_lo_gm_u6
    1,  //ltm_dtl_ehn_en
    63,  //ltm_vs_gtm_alpha
    0,  //ltm_cc_en
    1,  //ltm_lmin_med_en
    1,  //ltm_lmax_med_en
    1,  //ltm_bld_lvl_adp_en
    0,  //ltm_lo_hi_gm_auto
    63, //ltm_luma_alpha
};

static int32_t _CALIBRATION_LTM_LO_HI_GM[ISO_NUM_MAX][2] = {
/* ltm_lo_gm_u6 | ltm_hi_gm_u7 */
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
    {27, 64,},
};

static int32_t _CALIBRATION_LTM_CONTRAST[ISO_NUM_MAX] = {
/* u9, only for WDR mode*/
    400,
    400,
    400,
    400,
    400,
    400,
    400,
    400,
    400,
    400,
};

//aisp_sharpen_ltm_t
static int32_t _CALIBRATION_LTM_SHARP_ADJ[ISO_NUM_MAX][4] = {
/* alpha | shrp_r_u6 | shrp_s_u8 | shrp_smth_lvlsft */
    {20,    8,    20,      7,},
    {20,    8,    20,      7,},
    {18,    8,    20,      7,},
    {12,    8,    20,      7,},
    {12,    8,    20,      7,},
    {10,    8,    14,      7,},
    { 8,    8,     8,      7,},
    { 8,    8,     8,      7,},
    { 8,    8,     8,      7,},
    { 8,    8,     8,      7,},
};

static uint16_t _CALIBRATION_LTM_SATUR_LUT[63] = {
    11,   12,   13,   83,   90,   98,  106,  115,  125,  136,  148,  161,  175,  191,  208,  227,
    248,  271,  296,  322,  352,  383,  417,  454,  494,  536,  581,  630,  682,  737,  795,  858,
    923,  993, 1066, 1142, 1221, 1304, 1390, 1478, 1569, 1663, 1760, 1858, 1960, 2063, 2168, 2275,
    2384, 2495, 2607, 2721, 2836, 2952, 3069, 3187, 3306, 3426, 3546, 3667, 3788, 3910, 4018
};

//aisp_lc_t
static int32_t _CALIBRATION_LC_CTL[16] = {
    1,  //lc_auto_enable
    1, //lc_blkblend_mode
    6, //lc_lmtrat_minmax
    128, //lc_contrast_low u8
    256, //lc_contrast_hig u8
    0,  //lc_cc_en
    48,  //lc_ypkbv_slope_lmt[1]
    48,  //lc_ypkbv_slope_lmt[0]
    0,  //lc_str_fixed
    2,  //lc_damper64
    63, //lc_nodes_alpha;
	64, //lc_final_gain
    90, //u7, lc_single_bin_th, 0-100
    0,  //lc_single_bin_prot_en
    16, //u7, lc_single_bin_prot_strgth, 0 is strongest protection, 64 is without protection, max is 64
	0,  //u7, lc_ymaxv_lmt, limit the height of ymaxV from y=x, max is 64, 0: unrestricted; 64: ymaxV = maxV
};

static int32_t _CALIBRATION_LC_STRENGTH[ISO_NUM_MAX][2] = {
/* lc_ypkbv_slope_lmt_0, lc_ypkbv_slope_lmt_1 */
    {48, 56,},
    {48, 56,},
    {48, 56,},
    {48, 52,},
    {48, 48,},
    {48, 48,},
    {48, 48,},
    {48, 48,},
    {48, 48,},
    {48, 48,},
};

static uint16_t _CALIBRATION_LC_SATUR_LUT[63] = {
    12,   36,   55,   83,   90,   98,  106,  115,  125,  136,  148,  161,  175,  191,  208,  227,
    248,  271,  296,  322,  352,  383,  417,  454,  494,  536,  581,  630,  682,  737,  795,  858,
    923,  993, 1066, 1142, 1221, 1304, 1390, 1478, 1569, 1663, 1760, 1858, 1960, 2063, 2168, 2275,
    2384, 2495, 2607, 2721, 2836, 2952, 3069, 3187, 3306, 3426, 3546, 3667, 3788, 3910, 4018
};

//aisp_dnlp_t
static int32_t _CALIBRATION_DNLP_CTL[24] = {
    1,   // dnlp_auto_enable
    5,   // dnlp_cuvbld_min
    15,  // dnlp_cuvbld_max
    0,   // dnlp_clashBgn
    64,  // dnlp_clashEnd
    6,   // dnlp_blkext_ofst
    10,  // dnlp_whtext_ofst
    32,  // dnlp_blkext_rate
    63,  // dnlp_whtext_rate
    1,   // dnlp_dbg_map
    12,  // dnlp_final_gain
    8,  // dnlp_scurv_low_th
    16,  // dnlp_scurv_mid1_th
    64, // dnlp_scurv_mid2_th
    128, // dnlp_scurv_hgh1_th
    256, // dnlp_scurv_hgh2_th
    0,   // dnlp_mtdrate_adp_en
    1,   // dnlp_ble_en
    48,  // dnlp_scn_chg_th
    64, // dnlp_mtdbld_rate
    0,  //dnlp_str_fixed
    //if dnlp_by_iso_luma 0: luma_avg, so dnlp_scurv_low_th/dnlp_scurv_mid1_th/dnlp_scurv_mid2_th/dnlp_scurv_hgh1_th/dnlp_scurv_hgh2_th range is [0 - 255<<4].
    // If dnlp_by_iso_luma 1: iso, so dnlp_scurv_low_th/dnlp_scurv_mid1_th/dnlp_scurv_mid2_th/dnlp_scurv_hgh1_th/dnlp_scurv_hgh2_th range is [0 - 1024].
    // ISO100: 8; ISO200: 16; ISO400: 32; ISO800: 64; ISO1600: 128; ISO3200: 256; ISO6400: 512; ISO12800: 1024
    1,  //dnlp_by_iso_luma 1: iso 0: luma_avg
    0,  //dnlp_scurv_gain_mode
    0,  //dnlp_luma_dbg
};

//CALIBRATION_DNLP_STRENGTH, 8bit, normalization: 8 as 1, {ISO100,ISO200,ISO400,ISO800,ISO1600,ISO3200,ISO6400,ISO12800,ISO25600,ISO51200}
static int32_t _CALIBRATION_DNLP_STRENGTH[ISO_NUM_MAX] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

static uint16_t _CALIBRATION_DNLP_SCURV_LOW[65] =
{0,10,22,33,46,59,73,86,100,114,129,143,159,175,191,207,224,242,261,281,301,322,342,361,380,398,415,433,450,468,485,503,520,537,555,572,589,606,623,640,657,673,690,706,722,738,754,769,785,800,815,831,846,861,876,891,905,920,935,950,964,979,993,1008,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_MID1[65] =
{0,10,22,33,46,59,73,86,100,114,129,143,159,175,191,207,224,242,261,281,301,322,342,361,380,398,415,433,450,468,485,503,520,537,555,572,589,606,623,640,657,673,690,706,722,738,754,769,785,800,815,831,846,861,876,891,905,920,935,950,964,979,993,1008,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_MID2[65] =
{0,10,22,33,46,59,73,86,100,114,129,143,159,175,191,207,224,242,261,281,301,322,342,361,380,398,415,433,450,468,485,503,520,537,555,572,589,606,623,640,657,673,690,706,722,738,754,769,785,800,815,831,846,861,876,891,905,920,935,950,964,979,993,1008,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_HGH1[65] =
    {0,10,22,33,46,59,73,86,100,114,129,143,159,175,191,207,224,242,261,281,301,322,342,361,380,398,415,433,450,468,485,503,520,537,555,572,589,606,623,640,657,673,690,706,722,738,754,769,785,800,815,831,846,861,876,891,905,920,935,950,964,979,993,1008,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_HGH2[65] =
    {0,10,22,33,46,59,73,86,100,114,129,143,159,175,191,207,224,242,261,281,301,322,342,361,380,398,415,433,450,468,485,503,520,537,555,572,589,606,623,640,657,673,690,706,722,738,754,769,785,800,815,831,846,861,876,891,905,920,935,950,964,979,993,1008,1023};

//aisp_dhz_t
static int32_t _CALIBRATION_DHZ_CTL[10] = {
    1,   //dhz_auto_enable
    750,  //dhz_dlt_rat
    256,  //dhz_hig_dlt_rat
    256,  //dhz_low_dlt_rat
    8,  //dhz_lmtrat_lowc
    8,  //dhz_lmtrat_higc
    0,  //dhz_cc_en
    0,  //dhz_sky_prot_en
    614,  //dhz_sky_prot_stre
    0,   //dhz_str_fixed
};

//CALIBRATION_DHZ_STRENGTH, 16bit, normalization: 1024 as 1, {ISO100,ISO200,ISO400,ISO800,ISO1600,ISO3200,ISO6400,ISO12800,ISO25600,ISO51200}
static int32_t _CALIBRATION_DHZ_STRENGTH[ISO_NUM_MAX] = {1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};

//aisp_sharpen_ctl_t
static int32_t _CALIBRATION_PEAKING_CTL[54] = {
    //pk_flt1_v1d[3]
    110, 8, -63,
    //pk_flt2_v1d[3]
    126, -63, 0,
    //pk_flt1_h1d[5]
    94, 18, -44, -18, -3,
    //pk_flt2_h1d[5]
    122, -55, -7, 2, -1,
    //pkosht_vsluma_lut[9]
    2, 4, 5, 5, 5, 5, 5, 4, 2,
    //pk_flt1_2d[3][4]
    72,    13,     -9,    -2,
    13,     -5,    -7,    0,
    -9,    -7,    -2,     0,
    //pk_flt2_2d[3][4]
    72,    13,     -9,    -2,
    13,     -5,    -7,    0,
    -9,    -7,    -2,     0,
    /*124,    -11,     -5,    0,
    -11,     -11,    -2,    0,
    -5,    -2,    0,     0,*/
    1, //pk_motion_adp_gain_en   0:disable  1:enable
    1,  // pk_dejaggy_en         0:disable  1:enable
    0,  // pk_debug_mode  0:disable debug  18:dir flt1 gain 19:dir flt2 gain 20: cir flt1 gain 21: cir flt2 gain 22:gradinfo
    2,   //horizontal window size for peaking overshoot control, smaller value means stronger overshoot.
    2,   //horizontal window size for peaking overshoot control, smaller value means stronger overshoot.
};

//aisp_sharpen_adj_t
static uint16_t _CALIBRATION_PEAKING_ADJUST[ISO_NUM_MAX][6] = {
/*flt1_final_gain|flt2_final_gain|pre_flt_str|os_up|os_dw|pre_flt_range*/
    {40,    40,    12,    100,    120,    64,},
    {40,    40,    16,    100,    120,    32,},
    {40,    40,    20,    100,    120,    26,},
    {32,    32,    24,    100,    120,    22,},
    {32,    32,    32,    100,    120,    14,},
    {32,    32,    40,    100,    120,     8,},
    {16,    16,    48,    100,    120,     4,},
    {16,    16,    54,    100,    120,     0,},
    {16,    16,    60,    100,    120,     0,},
    {16,    16,    60,    100,    120,     0,},
};

//aisp_sharpen_t->peaking_flt1_gain_adp_motion
static uint8_t _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN[ISO_NUM_MAX][8] = {
    {16, 18, 18, 20, 22, 25, 28, 30,},
    {16, 18, 18, 20, 22, 25, 28, 30,},
    {11, 12, 14, 16, 18, 24, 27, 27,},
    {10, 12, 14, 16, 18, 22, 24, 24,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {8, 10, 10, 10, 10, 10, 10, 10,},
};

//aisp_sharpen_t->peaking_flt2_gain_adp_motion
static uint8_t _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[ISO_NUM_MAX][8] = {
    {16, 18, 18, 20, 22, 25, 28, 30,},
    {16, 18, 18, 20, 22, 25, 28, 30,},
    {11, 12, 14, 16, 18, 24, 27, 27,},
    {10, 12, 14, 16, 18, 22, 24, 24,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {10, 12, 14, 16, 18, 20, 22, 22,},
    {8, 10, 10, 10, 10, 10, 10, 10,},
};

//aisp_sharpen_t->peaking_gain_adp_luma
static uint8_t _CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[ISO_NUM_MAX][9] = {
    {4, 4, 5, 7, 9, 9, 7, 5, 4,},
    {3, 3, 4, 6, 9, 9, 7, 5, 3,},
    {3, 3, 4, 6, 8, 8, 6, 5, 3,},
    {2, 3, 3, 5, 8, 8, 6, 5, 3,},
    {2, 3, 3, 4, 7, 7, 6, 4, 3,},
    {1, 2, 2, 3, 6, 6, 5, 4, 2,},
    {0, 1, 2, 2, 5, 5, 5, 4, 2,},
    {0, 1, 2, 2, 3, 5, 5, 4, 2,},
    {0, 1, 2, 2, 3, 5, 5, 4, 2,},
    {0, 1, 2, 2, 3, 5, 5, 4, 2,},
};

//aisp_sharpen_t->peaking_gain_adp_grad1
static uint8_t _CALIBRATION_PEAKING_CIR_FLT1_GAIN[ISO_NUM_MAX][5] = {
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 110, 48, 15, 100,},
    {15, 100, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
};

//aisp_sharpen_t->peaking_gain_adp_grad2
static uint8_t _CALIBRATION_PEAKING_CIR_FLT2_GAIN[ISO_NUM_MAX][5] = {
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 120, 48, 15, 100,},
    {15, 110, 48, 15, 100,},
    {15, 100, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
    {15, 96, 48, 15, 100,},
};

//aisp_sharpen_t->peaking_gain_adp_grad3
static uint8_t _CALIBRATION_PEAKING_DRT_FLT1_GAIN[ISO_NUM_MAX][5] = {
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 100, 32, 15, 100,},
    {15, 100, 32, 15, 100,},
    {15, 96, 32, 15, 100,},
    {15, 96, 32, 15, 100,},
    {15, 84, 32, 15, 100,},
    {15, 84, 32, 15, 100,},

};

//aisp_sharpen_t->peaking_gain_adp_grad4
static uint8_t _CALIBRATION_PEAKING_DRT_FLT2_GAIN[ISO_NUM_MAX][5] = {
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 110, 32, 15, 100,},
    {15, 100, 32, 15, 100,},
    {15, 100, 32, 15, 100,},
    {15, 96, 32, 15, 100,},
    {15, 96, 32, 15, 100,},
    {15, 84, 32, 15, 100,},
    {15, 84, 32, 15, 100,},
};

//aisp_cm_ctl_t
static int32_t _CALIBRATION_CM_CTL[ISO_NUM_MAX][4] =
{
/* cm_sat | cm_hue | cm_contrast | cm_brightness */
    {560,    0,    1048,    0},
    {560,    0,    1048,    0},
    {560,    0,    1048,    0},
    {560,    0,    1048,    0},
    {550,    0,    1048,    0},
    {540,    0,    1048,    0},
    {540,    0,    1048,    0},
    {540,    0,    1048,    0},
    {540,    0,    1048,    0},
    {540,    0,    1048,    0},
};

//aisp_cm_t->cm_y_via_hue
static int8_t _CALIBRATION_CM_Y_VIA_HUE[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

//aisp_cm_t->cm_satglbgain_via_y
static int8_t _CALIBRATION_CM_SATGLBGAIN_VIA_Y[ISO_NUM_MAX][9] = {
    {0,0,0,0,0,0,0,0,0,},
    {0,0,0,0,0,0,0,0,0,},
    {0,0,0,0,0,0,0,0,0,},
    {0,0,0,0,0,0,0,0,0,},
    {0,0,0,0,0,0,0,0,0,},
    {-60,-40,-20,0,0,0,0,0,0,},
    {-60,-40,-20,0,0,0,0,0,0,},
    {-60,-40,-20,0,0,0,0,0,0,},
    {-60,-40,-20,0,0,0,0,0,0,},
    {-60,-40,-20,0,0,0,0,0,0,},
};

//aisp_cm_t->cm_sat_via_hs
static int8_t _CALIBRATION_CM_SAT_VIA_HS[3][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
};

//aisp_cm_t->cm_satgain_via_y
static int8_t _CALIBRATION_CM_SATGAIN_VIA_Y[5][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
};

//aisp_cm_t->cm_hue_via_h
static int8_t _CALIBRATION_CM_HUE_VIA_H[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

//aisp_cm_t->cm_hue_via_s
static int8_t _CALIBRATION_CM_HUE_VIA_S[5][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
};

//aisp_cm_t->cm_hue_via_y
static int8_t _CALIBRATION_CM_HUE_VIA_Y[5][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
};

//aisp_hlc_t
static int32_t _CALIBRATION_HLC_CTL[3] = {
    0,      //hlc_en
    240,    //hlc_luma_thd
    240,    //hlc_luma_trgt
};

//rgb2yuv_709f_u10[9] = {218, 732, 74, -118, -394,  512,  512, -465, -47};
//rgb2yuv_601f_u10[9] = {306, 601, 117, -173, -339, 512, 512, -429, -83};
static int32_t _CALIBRATION_CSC_COEF[9] = {306, 601, 117, -173, -339, 512, 512, -429, -83};

static int32_t _CALIBRATION_BLACK_LEVEL[9][5] =
{
{51152,51152,51152,51168,51152,},
{50672,50672,50688,50688,50672,},
{50240,50256,50272,50256,50240,},
{49120,49120,49152,49152,49120,},
{46848,46848,46928,46928,46848,},
{43648,43664,43648,43648,43648,},
{43648,43664,43648,43648,43648,},
{43648,43664,43648,43648,43648,},
{43648,43664,43648,43648,43648,},
};

static uint16_t _CALIBRATION_SHADING_RADIAL_R[129]=
{
4096,4096,4096,4096,4097,4097,4098,4099,4099,4100,4101,4102,4104,4106,4108,
4111,4113,4116,4119,4122,4126,4129,4133,4137,4141,4146,4151,4156,4161,4166,
4171,4177,4183,4189,4195,4201,4207,4214,4221,4227,4235,4242,4250,4257,4265,
4274,4282,4291,4300,4309,4319,4328,4338,4348,4359,4369,4380,4391,4402,4413,
4425,4437,4448,4459,4471,4483,4495,4508,4520,4534,4546,4560,4574,4587,4601,
4615,4630,4644,4659,4674,4690,4706,4722,4738,4756,4775,4794,4814,4835,4856,
4878,4901,4924,4949,4974,5000,5026,5054,5082,5112,5143,5175,5207,5242,5277,
5313,5351,5390,5431,5473,5517,5562,5610,5663,5718,5778,5843,5913,5986,6064,
6146,6233,6322,6431,6554,6685,6836,7007,7247
};

static uint16_t _CALIBRATION_SHADING_RADIAL_G[129]=
{
4096,4096,4096,4096,4097,4097,4098,4099,4099,4100,4101,4102,4104,4106,4108,
4111,4113,4116,4119,4122,4126,4129,4133,4137,4141,4146,4151,4156,4161,4166,
4171,4177,4183,4189,4195,4201,4207,4214,4221,4227,4235,4242,4250,4257,4265,
4274,4282,4291,4300,4309,4319,4328,4338,4348,4359,4369,4380,4391,4402,4413,
4425,4437,4448,4459,4471,4483,4495,4508,4520,4534,4546,4560,4574,4587,4601,
4615,4630,4644,4659,4674,4690,4706,4722,4738,4756,4775,4794,4814,4835,4856,
4878,4901,4924,4949,4974,5000,5026,5054,5082,5112,5143,5175,5207,5242,5277,
5313,5351,5390,5431,5473,5517,5562,5610,5663,5718,5778,5843,5913,5986,6064,
6146,6233,6322,6431,6554,6685,6836,7007,7247
};

static uint16_t _CALIBRATION_SHADING_RADIAL_B[129]=
{
4096,4096,4096,4096,4097,4097,4098,4099,4099,4100,4101,4102,4104,4106,4108,
4111,4113,4116,4119,4122,4126,4129,4133,4137,4141,4146,4151,4156,4161,4166,
4171,4177,4183,4189,4195,4201,4207,4214,4221,4227,4235,4242,4250,4257,4265,
4274,4282,4291,4300,4309,4319,4328,4338,4348,4359,4369,4380,4391,4402,4413,
4425,4437,4448,4459,4471,4483,4495,4508,4520,4534,4546,4560,4574,4587,4601,
4615,4630,4644,4659,4674,4690,4706,4722,4738,4756,4775,4794,4814,4835,4856,
4878,4901,4924,4949,4974,5000,5026,5054,5082,5112,5143,5175,5207,5242,5277,
5313,5351,5390,5431,5473,5517,5562,5610,5663,5718,5778,5843,5913,5986,6064,
6146,6233,6322,6431,6554,6685,6836,7007,7247
};

static int32_t _CALIBRATION_LENS_OTP_CENTER_OFFSET[2] = {60,-8};

static uint8_t _CALIBRATION_SHADING_LS_D65_R[1024]=
{
169,163,158,154,150,148,145,143,141,139,138,137,136,135,135,134,134,134,134,135,135,136,137,139,140,142,144,147,150,153,157,161,167,162,157,153,149,147,144,142,140,139,137,136,135,134,134,134,133,133,134,134,135,135,137,138,139,141,143,146,149,152,156,160,165,160,156,152,149,146,143,141,140,138,136,135,134,134,133,133,133,133,133,133,134,135,136,137,139,140,142,145,148,151,154,159,164,159,155,151,148,145,143,141,139,138,136,135,134,133,133,132,132,133,133,133,133,134,135,136,138,140,142,144,147,150,153,157,163,158,154,151,147,144,142,140,139,137,135,135,134,133,132,132,132,132,132,132,133,134,135,136,137,139,141,143,146,149,152,156,162,158,153,149,147,144,141,139,138,136,135,134,133,132,131,131,131,131,132,132,132,133,134,135,136,138,140,142,145,148,151,155,161,157,152,149,146,143,141,139,138,136,135,134,133,132,131,131,131,131,131,131,132,133,134,135,136,138,139,141,144,147,150,154,161,156,152,148,145,143,140,139,137,136,134,133,132,131,130,130,130,130,130,131,132,132,133,134,135,137,139,141,143,146,150,153,160,156,151,148,145,142,140,138,137,135,134,133,132,131,130,130,129,130,130,130,131,132,133,134,135,137,138,140,143,146,149,153,160,155,151,148,145,142,140,138,136,135,134,132,131,130,129,129,129,129,129,130,130,131,132,133,135,136,138,140,142,145,148,152,160,155,151,147,144,142,140,138,136,135,133,132,131,130,129,128,128,128,129,129,130,131,132,133,134,136,138,139,142,144,148,151,159,155,151,147,144,142,139,137,136,134,133,132,131,129,128,128,128,128,128,129,129,131,132,133,134,135,137,139,141,144,147,151,159,154,150,147,144,141,139,137,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,137,139,141,144,147,150,158,154,150,147,144,141,139,137,136,134,132,131,130,129,128,128,128,128,128,128,128,130,131,132,133,135,137,138,141,143,147,150,158,154,150,147,144,141,139,137,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,137,138,141,143,147,150,159,154,150,147,144,142,139,137,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,137,138,141,143,147,150,159,155,150,147,144,142,140,137,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,139,141,143,147,150,159,155,150,147,144,142,140,138,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,137,139,141,144,147,150,159,155,151,148,145,142,140,138,136,134,133,131,130,129,129,128,128,128,128,129,129,130,131,132,134,135,137,139,141,144,147,151,160,155,151,148,145,142,140,138,136,135,133,131,130,129,129,129,129,129,129,129,129,130,131,133,134,135,137,139,141,144,147,151,160,156,152,148,145,143,141,139,137,135,133,131,130,130,129,129,129,129,129,129,130,130,131,133,134,136,138,139,142,145,148,151,161,156,152,149,146,143,141,139,137,135,133,132,131,130,129,129,129,129,129,129,130,130,132,133,134,136,138,140,142,145,148,152,162,157,153,150,146,144,141,140,138,136,134,132,131,130,129,129,130,130,130,130,130,131,132,134,135,137,139,140,143,146,149,153,163,158,154,150,147,144,142,140,138,137,134,133,131,131,130,130,130,130,130,130,130,131,132,134,136,137,139,141,144,146,150,154,163,159,155,151,148,145,143,141,139,137,135,133,132,131,131,130,130,130,130,131,131,132,133,135,136,138,140,142,144,147,151,155,164,160,155,152,149,146,143,141,139,138,136,134,133,132,131,131,131,131,131,131,132,132,134,135,137,139,140,142,145,148,152,156,166,161,156,153,149,146,144,142,140,138,136,135,134,133,132,132,131,131,132,132,132,133,134,136,138,139,141,143,146,149,153,157,167,162,158,154,150,147,145,143,141,139,137,136,135,134,133,133,132,132,133,133,133,134,135,137,139,140,142,144,147,150,154,158,170,164,159,155,151,148,146,144,142,140,138,137,136,135,134,134,134,134,134,134,135,136,137,138,139,141,143,145,148,152,156,160,172,166,161,156,153,150,147,145,143,141,140,138,137,136,136,135,135,135,136,136,136,137,138,139,141,143,145,147,150,153,157,162,176,169,162,158,154,151,148,146,144,143,141,139,139,138,137,136,136,137,137,137,137,138,139,140,142,144,146,148,151,155,159,164,178,171,164,159,155,152,149,147,145,143,142,141,140,139,138,137,137,138,138,138,139,139,140,141,143,145,147,149,152,156,160,166
};

static uint8_t _CALIBRATION_SHADING_LS_D65_G[1024]=
{
165,160,155,152,149,146,144,142,140,139,137,136,136,135,134,134,134,134,134,134,135,136,137,138,140,141,143,146,148,152,155,159,164,159,154,151,148,146,143,141,140,138,137,136,135,134,134,133,133,133,133,134,134,135,136,138,139,140,142,145,148,150,154,158,163,158,154,150,147,145,143,141,139,138,136,135,134,134,133,133,133,133,133,133,134,134,135,137,138,140,142,144,147,150,153,157,162,157,153,150,147,144,142,140,139,137,136,135,134,133,133,132,132,132,132,133,133,134,135,136,138,139,141,143,146,149,152,156,161,156,152,149,146,144,141,140,138,137,135,134,133,133,132,132,132,132,132,132,133,133,134,135,137,138,140,142,145,148,151,155,160,155,151,148,145,143,141,139,138,136,135,134,133,132,132,131,131,131,131,132,132,133,134,135,136,138,139,141,144,147,150,154,159,155,151,148,145,142,140,139,137,136,135,134,133,132,131,131,131,131,131,131,132,132,133,134,136,137,139,141,143,146,149,153,158,154,150,147,144,142,140,138,137,136,134,133,132,131,131,130,130,130,130,131,131,132,133,134,135,137,138,140,142,145,148,152,158,154,150,147,144,142,140,138,136,135,134,133,132,131,130,130,129,129,130,130,131,132,132,133,135,136,138,140,142,145,148,151,158,153,150,147,144,142,140,138,136,135,134,133,131,130,130,129,129,129,129,130,130,131,132,133,134,136,138,139,141,144,147,151,157,153,149,146,144,141,139,138,136,135,133,132,131,130,129,129,128,128,129,129,130,131,131,133,134,136,137,139,141,144,147,150,157,153,149,146,144,141,139,137,136,134,133,132,131,130,129,128,128,128,128,129,129,130,131,132,134,135,137,139,141,143,146,150,157,153,149,146,143,141,139,137,136,134,133,132,130,129,129,128,128,128,128,128,129,130,131,132,133,135,137,138,140,143,146,149,157,153,149,146,143,141,139,137,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,136,138,140,143,146,149,157,153,149,146,144,141,139,137,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,136,138,140,143,146,149,157,153,149,146,144,141,139,137,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,136,138,140,143,146,149,157,153,149,146,144,142,140,138,136,134,133,131,130,129,129,128,128,128,128,128,129,130,131,132,133,135,136,138,140,143,146,149,157,153,150,147,144,142,140,138,136,134,133,132,130,129,129,128,128,128,128,128,129,130,131,132,133,135,136,138,140,143,146,149,158,154,150,147,144,142,140,138,136,135,133,132,131,130,129,129,129,128,128,129,129,130,131,132,134,135,137,138,141,143,146,150,158,154,150,147,145,142,140,138,137,135,133,132,131,130,129,129,129,129,129,129,129,130,131,132,134,135,137,139,141,143,147,150,159,155,151,148,145,143,141,139,137,135,133,132,131,130,130,129,129,129,129,129,130,130,131,133,134,136,137,139,141,144,147,150,159,155,151,148,146,143,141,139,138,136,134,132,131,130,130,129,129,129,129,129,130,130,131,133,134,136,138,139,141,144,147,151,160,156,152,149,146,144,142,140,138,136,134,133,132,131,130,130,130,130,130,130,130,131,132,133,135,136,138,140,142,145,148,151,161,157,153,150,147,144,142,140,139,137,135,133,132,131,131,130,130,130,130,130,131,131,132,134,136,137,139,140,143,145,149,152,162,157,154,150,148,145,143,141,139,138,136,134,133,132,131,131,131,131,131,131,131,132,133,135,136,138,139,141,143,146,150,153,163,158,155,151,148,146,144,142,140,138,136,135,134,133,132,131,131,131,131,131,132,133,134,135,137,138,140,142,144,147,150,154,164,160,156,152,149,147,144,142,141,139,137,136,134,134,133,132,132,132,132,132,133,133,134,136,138,139,141,143,145,148,151,155,166,161,157,153,150,147,145,143,142,140,138,137,136,135,134,133,133,133,133,133,134,135,135,137,139,140,141,143,146,149,153,156,168,163,158,154,151,148,146,144,142,141,139,138,137,136,135,134,134,134,134,135,135,136,137,138,139,141,142,144,147,150,154,158,170,165,159,155,152,149,147,145,143,142,140,139,138,137,136,136,136,136,136,136,136,137,138,139,140,142,143,146,148,151,155,159,174,167,161,157,153,151,148,146,144,143,141,140,139,138,137,137,137,137,137,137,138,138,139,140,141,143,145,147,150,153,157,161,176,169,163,158,155,152,149,147,145,144,143,141,140,139,139,138,138,138,138,138,139,139,140,141,142,144,146,148,151,154,159,163
};

static uint8_t _CALIBRATION_SHADING_LS_D65_B[1024]=
{
167,160,155,151,149,146,144,142,141,139,138,137,136,135,135,135,134,134,134,135,135,136,138,139,140,141,143,146,148,151,155,160,165,159,154,151,148,145,143,141,140,139,137,136,135,135,134,134,134,134,134,134,135,136,137,138,139,141,142,145,147,150,154,158,163,158,153,150,147,145,142,141,140,138,137,135,135,134,134,133,133,133,133,134,134,135,136,137,138,140,142,143,146,149,152,156,162,157,152,149,146,144,142,140,139,138,136,135,134,134,133,133,133,133,133,133,134,134,135,137,138,139,141,143,145,148,151,155,160,156,151,148,145,143,141,140,138,137,136,135,134,133,133,132,132,132,132,133,133,134,135,136,137,138,140,142,144,147,150,154,159,155,151,148,145,142,141,139,138,137,135,134,133,133,132,132,132,132,132,132,133,134,134,135,137,138,140,141,143,146,149,153,158,154,150,147,144,142,140,139,138,136,135,134,133,132,132,131,131,131,131,132,132,133,134,135,136,138,139,141,143,146,149,152,158,154,150,147,144,142,140,138,137,136,135,134,133,132,131,131,130,130,131,131,132,133,134,135,136,137,139,140,142,145,148,151,157,153,149,146,144,141,140,138,137,135,134,133,132,131,131,130,130,130,130,131,131,132,133,134,135,137,138,140,142,144,147,151,157,153,149,146,143,141,139,138,136,135,134,133,132,131,130,129,129,129,130,130,131,132,133,134,135,136,138,139,141,144,147,150,157,152,149,146,143,141,139,138,136,135,134,132,131,130,130,129,129,129,129,130,131,131,132,133,135,136,138,139,141,143,146,149,156,152,148,146,143,141,139,137,136,135,133,132,131,130,129,129,129,129,129,129,130,131,132,133,134,136,137,139,141,143,146,149,156,152,148,145,143,141,139,137,136,134,133,132,131,130,129,129,128,128,129,129,130,131,132,133,134,136,137,139,140,143,146,149,156,152,148,145,143,141,139,137,136,134,133,132,131,130,129,128,128,128,129,129,130,131,132,132,134,135,137,139,140,143,145,149,156,152,148,145,143,141,139,137,136,134,133,131,130,130,129,128,128,128,129,129,129,130,131,132,134,135,137,138,140,142,145,149,156,152,148,145,143,141,139,137,136,134,133,131,130,130,129,128,128,128,128,129,129,130,131,133,134,135,137,139,140,142,145,149,156,152,149,146,143,141,139,137,136,134,133,132,130,130,129,128,128,128,129,129,129,130,131,133,134,135,137,139,140,143,146,149,157,153,149,146,143,142,140,138,136,135,133,132,131,130,129,128,128,129,129,129,130,131,132,133,134,135,137,139,140,143,146,149,157,153,149,146,144,142,140,138,136,135,133,132,131,130,129,129,129,129,129,130,130,131,132,133,134,135,137,139,141,143,146,149,157,153,150,147,144,142,140,138,137,135,133,132,131,130,130,129,129,129,130,130,130,131,132,133,135,136,137,139,141,143,146,149,158,154,150,147,145,142,140,139,137,135,133,132,131,130,130,130,130,130,130,130,131,131,132,134,135,136,137,139,141,144,147,150,158,154,151,148,145,143,141,139,138,136,134,133,131,131,130,130,130,130,130,130,131,131,132,134,135,137,138,139,142,144,147,150,159,155,151,148,145,143,141,140,138,137,134,133,132,131,131,130,130,131,131,131,131,132,133,134,136,137,139,140,142,145,148,151,160,156,152,149,146,144,142,140,139,137,135,134,132,132,131,131,131,131,131,131,132,132,133,135,136,138,139,140,142,145,149,152,161,157,153,150,147,145,143,141,139,138,136,134,133,132,132,132,131,131,132,132,132,133,134,136,137,138,140,141,143,146,149,153,162,158,154,151,148,145,143,142,140,139,137,135,134,133,133,132,132,132,132,132,133,134,135,136,138,139,140,142,144,147,150,154,164,159,155,151,149,146,144,142,141,139,138,136,135,134,133,133,133,133,133,133,134,135,135,137,139,140,141,143,145,147,151,155,166,161,156,152,149,147,145,143,142,140,138,137,136,135,135,134,134,134,134,135,135,136,136,138,139,141,142,143,146,149,152,156,169,163,157,154,150,148,146,144,142,141,139,138,137,136,136,135,135,135,135,136,136,137,138,139,140,141,143,144,147,150,153,157,171,165,159,155,151,149,147,145,144,142,141,139,139,138,137,137,137,137,137,137,138,138,139,140,141,143,144,146,148,151,155,159,175,167,161,156,153,150,148,146,144,143,142,140,140,139,138,138,138,138,138,138,139,140,140,141,142,144,145,147,150,153,157,162,178,169,162,157,154,151,149,147,145,144,143,142,141,140,140,139,139,139,139,140,140,141,141,142,143,145,146,148,151,154,159,164
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_R[1024]=
{
168,162,157,153,150,147,144,142,141,139,138,136,136,135,135,134,135,135,135,135,136,137,138,139,141,142,144,147,150,153,157,161,166,161,156,152,149,146,144,141,140,138,137,136,135,134,134,134,134,134,134,135,135,136,137,139,140,142,144,146,149,152,156,160,164,159,155,151,148,145,143,141,139,138,136,135,134,134,133,133,133,133,134,134,135,136,137,138,139,141,143,145,148,151,155,159,163,159,154,150,147,144,142,140,139,137,136,135,134,133,133,132,133,133,133,133,134,135,136,137,138,140,142,144,147,150,154,158,162,158,153,149,146,144,141,140,138,136,135,134,133,133,132,132,132,132,133,133,134,134,135,137,138,139,141,143,146,149,153,157,162,157,152,149,146,143,140,139,137,136,135,134,133,132,131,131,132,132,132,132,133,134,135,136,137,139,141,143,145,148,152,156,161,156,152,148,145,142,140,138,137,135,134,133,132,132,131,131,131,131,132,132,132,133,134,135,136,138,140,142,144,147,151,155,160,155,151,148,144,142,140,138,136,135,134,133,132,131,130,130,130,130,131,131,132,133,134,135,136,138,139,141,144,147,150,154,159,155,151,147,144,141,139,137,136,135,133,132,131,130,130,130,130,130,130,131,132,133,133,135,136,137,139,141,143,146,149,153,159,154,150,147,144,141,139,137,136,134,133,132,131,130,129,129,129,129,130,130,131,132,133,134,135,137,139,141,143,146,149,152,158,154,150,146,144,141,139,137,135,134,133,132,131,130,129,128,128,129,129,130,131,132,133,134,135,137,138,140,142,145,149,152,158,154,150,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,129,129,130,131,132,133,135,136,138,140,142,145,148,152,158,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,129,130,131,132,133,134,136,138,140,142,145,148,151,158,153,149,146,143,141,138,136,135,133,132,131,130,129,128,128,128,128,128,129,129,131,132,133,134,136,138,140,142,144,147,151,157,153,149,146,143,140,138,137,135,133,132,131,130,129,128,128,128,128,128,128,129,130,131,133,134,136,138,139,142,144,148,151,157,153,149,146,143,141,138,137,135,133,132,131,130,129,128,128,128,128,128,128,129,130,132,133,134,136,138,139,142,144,148,151,157,153,149,146,143,141,139,137,135,133,132,131,130,129,128,128,128,128,128,128,129,130,132,133,134,136,138,140,142,145,148,151,158,153,150,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,129,129,130,132,133,135,136,138,140,142,145,148,152,158,154,150,147,144,141,139,137,135,134,132,131,130,129,128,128,129,129,129,129,130,131,132,133,135,136,138,140,142,145,148,152,159,154,150,147,144,142,140,137,136,134,132,131,130,129,129,128,129,129,129,129,130,131,132,134,135,136,138,140,143,146,149,152,159,155,151,147,144,142,140,138,136,135,133,131,130,129,129,129,129,129,129,130,130,131,132,134,135,137,139,140,143,146,149,153,160,155,151,148,145,142,140,138,137,135,133,131,130,130,129,129,129,129,129,130,130,131,132,134,135,137,139,141,144,147,150,153,160,156,152,148,146,143,141,139,137,135,133,132,131,130,129,129,130,130,130,130,130,131,133,134,136,138,140,142,144,147,150,154,161,157,153,149,146,143,141,139,138,136,134,132,131,130,130,130,130,130,130,130,131,132,133,135,136,138,140,142,145,148,151,155,162,158,153,150,147,144,142,140,138,137,134,133,132,131,130,130,130,130,130,131,131,132,134,136,137,139,141,143,145,149,152,156,163,158,154,151,147,145,142,140,139,137,135,134,133,132,131,131,131,131,131,131,132,133,134,136,138,139,142,144,146,150,153,158,164,159,155,151,148,146,143,141,140,138,136,134,133,132,132,131,131,131,132,132,133,134,135,137,139,140,142,145,147,150,154,159,166,161,156,152,149,146,144,142,140,139,137,135,134,133,133,132,132,132,133,133,134,135,136,138,139,141,143,145,148,152,156,160,168,163,158,154,150,147,145,143,141,140,138,136,136,134,134,133,133,134,134,134,135,136,137,139,140,142,144,147,150,153,157,162,170,165,160,155,152,149,146,144,142,141,139,138,137,136,135,135,135,135,135,136,136,137,138,140,141,143,146,148,151,154,159,163,174,167,161,157,153,150,147,145,143,142,140,139,138,137,136,136,136,136,136,137,138,139,140,141,142,144,147,149,152,156,160,165,176,169,162,158,154,151,148,146,144,143,141,140,139,138,138,137,137,137,137,138,139,140,140,141,143,145,148,150,154,157,162,167
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_G[1024]=
{
166,160,156,152,149,146,144,142,140,139,137,136,135,135,134,134,134,134,134,135,135,136,137,138,140,142,144,146,149,152,156,160,165,160,155,151,148,146,143,141,140,138,137,136,135,134,134,133,133,134,134,134,135,136,137,138,139,141,143,145,148,151,155,159,163,158,154,151,148,145,143,141,139,138,136,135,134,134,133,133,133,133,133,134,134,135,136,137,139,140,142,144,147,150,154,158,162,157,153,150,147,144,142,140,139,137,136,135,134,133,133,132,132,133,133,133,134,134,135,137,138,140,141,144,146,149,153,156,161,156,152,149,146,143,141,140,138,137,135,134,133,133,132,132,132,132,132,133,133,134,135,136,137,139,141,143,145,148,152,155,160,156,152,148,145,143,141,139,137,136,135,134,133,132,132,131,131,132,132,132,133,134,134,135,137,138,140,142,144,147,151,154,159,155,151,148,145,142,140,139,137,136,134,133,132,132,131,131,131,131,131,132,132,133,134,135,136,138,139,141,144,146,150,153,159,154,151,147,144,142,140,138,137,135,134,133,132,131,131,130,130,130,131,131,132,133,133,134,136,137,139,141,143,146,149,153,158,154,150,147,144,142,140,138,136,135,134,133,132,131,130,130,129,130,130,131,131,132,133,134,135,137,139,140,142,145,148,152,158,153,150,147,144,141,139,138,136,135,133,132,131,130,130,129,129,129,130,130,131,132,133,134,135,136,138,140,142,145,148,151,157,153,149,146,144,141,139,137,136,134,133,132,131,130,129,129,128,129,129,130,130,131,132,133,135,136,138,140,142,144,147,151,157,153,149,146,143,141,139,137,135,134,133,132,131,129,129,128,128,128,129,129,130,131,132,133,134,136,138,139,141,144,147,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,129,129,130,131,133,134,136,137,139,141,144,147,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,129,129,130,131,132,134,135,137,139,141,144,147,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,139,141,143,147,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,139,141,144,147,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,139,141,144,147,150,157,153,149,146,144,141,139,137,136,134,133,131,130,129,129,128,128,128,128,129,129,130,131,133,134,136,137,139,141,144,147,150,157,153,150,147,144,142,140,138,136,134,133,131,130,129,129,128,129,128,128,129,130,130,131,133,134,136,137,139,141,144,147,151,158,154,150,147,144,142,140,138,136,135,133,131,130,130,129,129,129,129,129,129,130,130,131,133,135,136,138,140,142,144,148,151,158,154,151,147,145,142,140,138,137,135,133,132,131,130,129,129,129,129,129,129,130,131,131,133,135,136,138,140,142,145,148,151,159,155,151,148,145,143,141,139,137,135,133,132,131,130,130,129,129,129,130,130,130,131,132,134,135,137,138,140,143,145,148,152,160,155,152,149,146,143,141,139,138,136,134,132,131,130,130,130,130,130,130,130,130,131,132,134,136,137,139,141,143,146,149,153,160,156,152,149,146,144,142,140,138,136,134,133,132,131,130,130,130,130,130,130,131,132,133,135,136,138,139,141,144,147,150,153,161,157,153,150,147,145,142,141,139,137,135,133,133,132,131,131,131,131,131,131,131,132,133,135,137,138,140,142,144,147,151,154,162,158,154,151,148,145,143,141,139,138,136,134,133,132,132,131,131,131,131,131,132,133,134,136,137,139,141,143,145,148,152,155,164,159,155,152,149,146,144,142,140,139,137,135,134,133,133,132,132,132,132,132,133,134,135,137,138,140,141,144,146,149,153,156,166,161,156,153,149,147,145,143,141,139,138,136,135,134,133,133,133,133,133,133,134,135,136,137,139,141,142,144,147,150,154,158,168,162,157,154,151,148,145,144,142,140,139,137,136,135,135,134,134,134,134,135,135,136,137,138,140,141,143,145,148,151,155,159,170,164,159,155,151,149,146,144,143,141,140,139,138,137,136,135,135,135,136,136,136,137,138,139,141,142,144,147,149,153,156,161,173,167,160,156,153,150,148,145,144,142,141,140,139,138,137,137,137,136,137,137,138,138,139,140,142,143,145,148,151,154,158,162,176,168,162,157,154,151,149,146,145,143,142,141,140,139,138,138,138,138,138,138,139,140,140,141,143,144,146,149,152,156,160,164
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_B[1024]=
{
168,161,156,152,149,147,144,142,141,139,138,137,136,135,135,134,134,134,135,135,136,137,138,139,140,142,144,146,149,152,156,161,166,161,155,151,148,146,144,142,140,139,137,136,135,135,134,134,134,134,134,134,135,136,137,138,139,141,143,145,148,151,155,159,164,159,154,151,148,145,143,141,140,138,137,135,135,134,134,133,133,133,133,134,134,135,136,137,139,140,142,144,147,150,153,158,163,158,153,150,147,144,142,140,139,138,136,135,134,134,133,133,133,133,133,133,134,135,136,137,138,139,141,143,146,149,153,157,162,157,152,149,146,144,141,140,138,137,136,135,134,133,133,132,132,132,132,133,133,134,135,136,137,139,140,142,145,148,151,155,161,156,151,148,145,143,141,139,138,137,135,134,133,133,132,132,132,132,132,132,133,134,135,135,137,138,140,142,144,147,151,154,159,155,151,147,145,142,141,139,138,136,135,134,133,132,131,131,131,131,131,132,132,133,134,135,136,138,140,141,143,146,150,153,159,154,150,147,144,142,140,138,137,136,134,134,132,131,131,131,130,130,131,131,132,133,134,135,136,137,139,141,143,145,149,153,158,154,150,147,144,142,140,138,136,135,134,133,132,131,130,130,130,130,130,131,131,132,133,134,136,137,138,140,142,145,148,152,158,153,150,147,144,141,140,138,136,135,134,133,132,131,130,129,129,129,130,130,131,132,133,134,135,136,138,140,142,145,148,151,157,153,149,146,143,141,139,137,136,134,133,132,131,130,129,129,129,129,129,130,131,131,132,133,135,136,138,140,141,144,147,151,157,153,149,146,143,141,139,137,136,134,133,132,131,130,129,129,129,129,129,129,130,131,132,133,135,136,138,139,141,144,147,150,157,153,149,146,143,141,139,137,136,134,133,132,131,130,129,128,128,128,129,129,130,131,132,133,134,136,137,139,141,143,147,150,157,153,149,146,143,141,139,137,136,134,133,131,130,129,129,128,128,128,129,129,130,131,132,133,134,136,137,139,141,143,146,150,157,153,149,146,143,141,139,137,135,134,132,131,130,129,129,128,128,128,128,129,129,131,131,132,134,136,137,139,141,143,146,150,157,153,149,146,143,141,139,137,136,134,132,131,130,129,129,128,128,128,129,129,129,130,131,133,134,136,137,139,141,143,146,150,157,153,149,146,143,141,139,137,136,134,133,131,130,130,129,128,128,128,129,129,130,131,131,133,134,136,137,139,141,143,147,150,157,153,149,146,144,142,140,138,136,134,133,132,130,130,129,128,128,129,129,129,130,131,132,133,134,136,137,139,141,144,147,150,158,154,150,147,144,142,140,138,136,135,133,132,131,130,129,129,129,129,129,129,130,131,132,133,135,136,138,139,141,144,147,150,158,154,150,147,145,142,140,138,137,135,133,132,131,130,130,129,129,129,130,130,130,131,132,133,135,136,138,139,142,144,147,151,159,155,151,148,145,143,141,139,137,135,133,132,131,130,130,130,130,130,130,130,130,131,132,134,135,137,138,140,142,145,148,151,160,155,151,148,145,143,141,139,137,136,134,132,131,131,130,130,130,130,130,131,131,131,132,134,136,137,139,140,142,145,148,152,160,156,152,149,146,144,142,140,138,137,134,133,132,131,131,130,130,131,131,131,131,132,133,135,136,137,139,140,143,146,149,152,161,157,153,149,147,144,142,140,138,137,135,134,132,131,131,131,131,131,131,131,132,132,133,135,137,138,140,141,143,146,150,153,162,158,154,150,147,145,143,141,139,138,136,134,133,132,132,132,131,131,132,132,132,133,134,136,137,138,140,142,144,147,150,154,163,158,154,151,148,146,144,142,140,138,137,135,134,133,133,132,132,132,132,132,133,134,135,136,138,139,141,142,145,147,151,155,165,160,155,152,149,146,144,143,141,139,137,136,135,134,133,133,133,133,133,133,134,134,136,137,139,140,142,143,146,148,153,157,167,161,156,153,150,147,145,143,142,140,138,137,136,135,134,134,134,134,134,135,135,136,137,138,140,141,143,144,147,150,153,157,169,163,158,154,151,148,146,144,143,141,139,138,137,136,136,135,135,135,135,136,136,137,138,139,141,142,143,145,148,151,154,159,172,166,159,155,152,149,147,145,143,142,141,139,139,138,137,137,136,136,137,137,138,138,139,140,142,143,145,147,149,152,156,160,176,168,161,157,153,150,148,146,145,143,142,141,140,139,138,138,138,138,138,138,139,140,140,141,143,144,146,148,150,154,158,163,179,170,163,158,154,152,149,147,146,144,143,142,141,140,140,139,139,139,139,140,140,141,141,142,143,145,147,149,152,155,160,166
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_R[1024]=
{
166,161,156,152,149,146,143,141,140,138,137,136,135,135,134,134,134,134,135,135,136,137,138,140,141,143,145,148,150,154,158,163,164,159,155,151,148,145,143,141,139,138,136,135,135,134,134,134,134,134,134,135,136,136,138,139,140,142,145,147,150,153,157,161,162,157,154,150,147,144,142,140,138,137,135,135,134,133,133,133,133,133,133,134,135,136,137,138,140,141,143,146,148,152,156,160,161,157,153,149,146,143,141,139,138,136,135,134,133,133,133,132,133,133,133,134,134,135,136,138,139,141,143,145,147,151,155,159,160,156,152,148,145,143,140,139,137,136,135,134,133,132,132,132,132,132,132,133,134,135,136,137,138,140,142,144,147,150,154,158,160,155,151,147,144,142,140,138,136,135,134,133,132,131,131,131,131,132,132,132,133,134,135,136,138,139,141,143,146,149,153,157,159,154,150,147,144,141,139,138,136,134,134,133,132,131,130,131,131,131,131,132,133,134,135,135,137,139,141,142,145,148,152,156,158,154,150,146,143,141,139,137,135,134,133,132,131,130,130,130,130,130,131,131,133,133,134,135,137,138,140,142,144,147,151,155,157,153,149,146,143,140,139,137,135,134,133,132,131,130,129,129,129,130,130,131,132,133,134,135,136,138,140,141,144,147,150,154,157,153,149,146,143,140,138,137,135,134,133,132,130,130,129,129,129,129,130,130,131,132,134,135,136,138,139,141,144,146,150,154,156,152,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,129,129,130,131,132,133,134,136,137,139,141,143,146,149,153,156,152,148,145,142,140,138,136,134,133,132,131,130,129,128,128,128,128,128,129,130,132,133,134,135,137,139,141,143,146,149,153,156,152,148,145,142,140,138,136,134,133,132,130,129,128,128,128,128,128,128,129,130,131,133,134,135,137,139,140,143,146,149,152,155,151,148,145,142,139,138,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,133,135,137,139,140,143,145,149,152,155,151,148,145,142,139,138,136,134,132,131,130,129,128,127,128,128,128,128,128,130,131,132,133,135,137,138,140,142,145,148,152,155,151,148,145,142,140,138,136,134,132,131,130,129,128,127,128,128,128,128,128,130,131,132,133,135,137,138,140,143,146,149,152,156,152,148,145,142,140,138,136,134,133,131,130,129,128,128,128,128,128,128,128,130,131,132,133,135,137,139,140,143,146,149,152,156,152,148,145,142,140,138,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,139,141,143,146,149,153,156,152,148,145,143,140,138,137,135,133,132,130,129,129,128,128,128,128,129,129,130,131,133,134,136,137,139,141,143,146,150,153,157,152,149,146,143,141,139,137,135,133,132,131,130,129,129,128,129,129,129,129,130,131,133,134,136,138,140,141,144,147,150,154,157,153,149,146,143,141,139,137,135,133,132,131,130,129,129,129,129,129,129,130,131,131,133,135,136,138,140,142,144,147,150,154,158,153,150,147,144,141,139,138,136,134,132,131,130,130,129,129,129,129,129,130,131,132,133,135,137,138,140,142,145,148,151,154,158,154,150,147,144,142,140,138,136,134,132,131,131,130,130,130,130,130,130,130,131,132,133,135,137,139,141,143,145,148,152,155,159,155,151,148,145,142,140,139,137,135,133,132,131,130,130,130,130,130,130,130,131,132,134,136,138,140,141,143,146,149,152,156,160,156,152,149,146,143,141,139,137,135,133,132,131,131,130,130,130,130,130,131,132,133,135,136,138,140,142,144,147,150,154,158,161,157,153,149,146,144,142,140,138,136,134,133,132,131,131,131,131,131,131,131,132,134,135,137,139,141,143,145,148,151,154,159,163,158,154,150,147,145,142,140,139,137,135,134,133,132,132,131,131,131,132,132,133,134,136,138,140,141,144,146,148,152,155,160,164,159,155,151,148,145,143,141,140,138,136,135,134,133,132,132,132,132,133,133,134,135,137,139,140,142,144,147,150,153,157,161,166,161,156,152,149,146,144,142,141,139,137,136,135,134,134,133,133,134,134,135,136,137,138,140,141,144,146,148,151,154,158,163,168,163,158,154,150,148,145,143,142,140,138,137,136,136,135,135,135,135,136,136,137,138,139,141,143,145,147,149,152,155,160,165,171,165,159,155,152,149,146,144,143,141,139,138,137,137,136,136,136,136,137,137,138,139,140,141,144,146,148,150,154,157,161,166,174,167,161,156,153,150,147,145,144,142,140,139,138,138,137,137,137,137,138,138,139,140,141,142,144,147,149,152,155,159,163,168
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_G[1024]=
{
164,159,154,151,148,145,143,141,139,138,137,136,135,134,134,134,134,134,134,135,135,136,138,139,140,142,144,147,149,153,157,161,162,158,153,150,147,144,142,140,139,137,136,135,134,134,133,133,133,133,134,134,135,136,137,138,140,141,143,146,148,152,156,160,161,157,153,149,146,144,141,140,138,137,136,135,134,133,133,133,133,133,133,134,134,135,136,138,139,141,143,145,148,151,154,159,160,156,152,148,145,143,141,139,138,136,135,134,133,133,133,132,132,132,133,133,134,135,136,137,138,140,142,144,147,150,153,157,159,155,151,148,145,142,140,139,137,136,135,134,133,132,132,132,132,132,132,133,133,134,135,136,138,139,141,143,146,149,152,156,158,154,150,147,144,142,140,138,136,135,134,133,132,132,131,131,131,131,132,132,133,134,135,136,137,139,140,142,145,148,151,155,157,153,150,146,144,141,139,138,136,135,134,133,132,131,131,131,131,131,131,132,133,133,134,135,137,138,140,142,144,147,151,154,157,153,149,146,143,141,139,137,136,134,133,133,132,131,130,130,130,130,131,131,132,133,134,135,136,138,139,141,144,147,150,154,156,152,149,146,143,140,139,137,135,134,133,132,131,130,130,129,129,130,130,131,132,132,133,134,136,137,139,141,143,146,149,153,156,152,148,145,142,140,138,137,135,134,133,132,131,130,129,129,129,129,129,130,131,132,133,134,135,137,139,141,143,146,149,152,155,151,148,145,142,140,138,136,135,134,133,131,130,129,129,128,128,129,129,130,131,132,133,134,135,137,138,140,142,145,148,152,155,151,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,128,129,129,130,131,132,133,135,136,138,140,142,145,148,152,155,151,147,145,142,140,138,136,134,133,132,131,130,129,128,128,128,128,128,129,130,131,132,133,135,136,138,140,142,145,148,151,155,151,147,145,142,140,138,136,134,133,132,131,130,129,128,128,128,128,128,129,130,131,132,133,134,136,138,140,142,144,148,151,155,151,147,145,142,140,138,136,134,133,132,131,129,129,128,128,128,128,128,129,129,131,132,133,134,136,138,140,142,144,148,151,155,151,148,145,142,140,138,136,134,133,132,131,130,129,128,128,128,128,128,129,129,131,132,133,134,136,138,140,142,145,148,151,155,151,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,128,128,129,130,131,132,133,135,136,138,140,142,145,148,151,155,151,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,128,128,129,130,131,132,133,135,136,138,140,142,145,148,152,156,152,148,145,143,140,139,137,135,133,132,131,130,129,129,129,129,128,129,129,130,131,132,134,135,137,138,140,142,145,148,152,156,152,149,146,143,141,139,137,135,134,132,131,130,130,129,129,129,129,129,129,130,131,132,134,135,137,139,140,143,146,149,152,157,153,149,146,143,141,139,137,136,134,132,131,130,130,129,129,129,129,129,130,130,131,132,134,136,137,139,141,143,146,149,153,157,153,150,147,144,142,140,138,136,134,132,131,131,130,130,130,130,129,130,130,131,131,133,135,136,138,139,141,144,146,150,153,158,154,150,147,144,142,140,138,137,135,133,132,131,131,130,130,130,130,130,130,131,132,133,135,136,138,140,142,144,147,150,154,159,155,151,148,145,143,141,139,137,135,133,132,132,131,131,130,130,130,130,131,131,132,134,135,137,139,140,142,145,148,151,155,160,155,152,149,146,143,141,140,138,136,134,133,132,132,131,131,131,131,131,131,132,133,134,136,138,139,141,143,145,148,152,156,161,156,153,149,146,144,142,140,139,137,135,134,133,132,132,131,131,131,131,132,132,133,135,137,138,140,142,144,146,149,153,157,162,158,153,150,147,145,143,141,139,138,136,135,134,133,132,132,132,132,132,133,133,134,136,138,139,141,142,145,147,150,154,158,164,159,155,151,148,146,143,142,140,138,137,135,135,134,133,133,133,133,133,134,134,135,137,138,140,142,143,145,148,151,155,159,166,160,156,152,149,146,144,143,141,139,138,137,136,135,134,134,134,134,134,135,136,136,138,139,141,142,144,146,149,152,156,161,168,162,157,153,150,148,145,143,142,140,139,138,137,136,136,135,135,135,136,136,137,138,139,140,142,143,145,148,150,154,158,162,171,164,159,155,151,149,146,145,143,141,140,139,138,137,137,137,137,137,137,137,138,139,140,141,143,144,146,149,152,155,159,164,173,166,160,156,153,150,148,146,144,143,141,140,139,139,138,138,138,138,138,138,139,140,141,142,143,145,147,150,153,157,161,166
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_B[1024]=
{
166,160,155,152,148,146,143,142,140,139,137,136,136,135,134,134,134,134,135,135,136,137,138,139,141,143,145,147,150,153,157,162,164,159,154,151,148,145,143,141,140,138,137,136,135,134,134,134,134,134,134,135,135,136,137,139,140,142,144,146,149,152,156,161,162,157,153,150,147,144,142,140,139,137,136,135,134,134,133,133,133,133,133,134,135,136,137,138,139,141,143,145,148,151,155,159,161,156,152,149,146,143,141,140,138,137,136,135,134,133,133,133,133,133,133,133,134,135,136,137,139,140,142,144,147,150,154,158,160,155,151,148,145,143,141,139,138,136,135,134,133,133,133,132,132,132,132,133,134,135,136,137,138,139,141,143,146,149,152,156,159,154,150,147,145,142,140,139,137,136,135,134,133,132,132,132,132,132,132,132,133,134,135,136,137,139,141,143,145,148,152,155,158,154,150,146,144,142,140,138,137,135,134,133,132,132,131,131,131,131,131,132,133,134,135,136,137,139,140,142,144,147,151,154,157,153,149,146,143,141,140,138,136,135,134,133,132,131,131,130,130,130,131,131,132,133,134,135,136,138,140,142,144,147,150,154,156,152,149,146,143,141,139,137,136,135,134,133,132,131,130,130,130,130,130,131,132,133,134,135,136,138,139,141,143,146,149,153,156,152,149,145,143,141,139,137,136,134,133,132,131,130,130,129,129,129,130,130,131,132,133,134,136,137,139,141,143,146,149,152,155,152,148,145,143,140,139,137,135,134,133,132,131,130,129,129,129,129,129,130,131,132,133,134,136,137,139,140,142,145,148,152,155,151,148,145,143,140,138,137,135,134,133,132,131,130,129,129,128,129,129,129,130,131,133,134,135,137,139,140,142,145,148,152,155,151,148,145,142,140,138,137,135,134,132,131,130,129,129,128,128,128,129,129,130,131,132,133,135,137,138,140,142,145,148,151,155,151,148,145,142,140,138,136,135,133,132,131,130,129,129,128,128,128,129,129,130,131,132,133,135,136,138,140,142,145,148,151,155,151,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,128,129,129,130,131,132,133,135,136,138,140,142,145,148,151,155,151,148,145,143,140,138,137,135,134,132,131,130,129,128,128,128,128,129,129,130,131,132,133,135,137,138,140,142,144,147,151,155,152,148,145,143,141,139,137,135,134,132,131,130,129,129,128,128,128,129,129,130,131,132,134,135,136,138,140,142,145,148,151,156,152,148,145,143,141,139,137,135,134,132,131,130,130,129,129,128,129,129,130,130,131,132,134,135,137,138,140,142,145,148,152,156,152,148,146,143,141,139,137,136,134,132,131,131,130,129,129,129,129,129,130,130,131,133,134,135,137,139,140,142,145,148,152,157,153,149,146,144,141,139,138,136,134,133,132,131,130,130,129,129,129,130,130,131,131,133,134,136,137,139,141,143,145,149,152,157,153,150,147,144,142,140,138,136,134,133,132,131,130,130,130,130,130,130,130,131,132,133,135,136,138,139,141,143,146,149,152,158,154,150,147,144,142,140,139,137,135,133,132,132,131,131,130,130,130,130,131,131,132,133,135,136,138,140,141,143,146,149,153,159,155,151,148,145,143,141,139,137,136,134,133,132,131,131,131,131,131,131,131,132,133,134,136,137,138,140,141,144,147,150,154,160,155,151,148,146,143,142,140,138,136,134,133,132,132,132,131,131,131,131,132,132,133,134,136,138,139,141,142,144,147,151,155,161,156,152,149,146,144,142,140,139,137,135,134,133,133,132,132,132,132,132,132,133,133,135,137,138,140,141,143,145,148,152,156,162,157,153,150,147,145,143,141,139,138,136,135,134,133,133,132,132,132,133,133,133,134,136,137,139,140,142,144,146,149,153,157,163,158,154,151,148,146,144,142,140,138,137,136,135,134,134,133,133,133,133,134,134,135,137,138,140,141,143,145,147,150,154,158,165,160,155,152,149,146,144,143,141,139,137,136,136,135,134,134,134,134,134,135,135,136,137,139,140,142,144,145,148,151,155,159,168,162,157,153,150,147,145,144,142,140,139,137,137,136,136,135,135,135,136,136,137,138,139,140,141,143,145,147,149,152,156,160,171,164,158,154,151,148,146,145,143,142,140,139,138,137,137,137,137,137,137,138,138,139,140,141,142,144,146,148,150,154,157,162,174,166,160,155,152,150,147,146,144,143,141,140,140,139,138,138,138,138,138,139,140,140,141,142,144,145,147,149,152,155,160,165,177,168,161,156,153,151,149,147,145,144,143,141,141,140,140,139,140,140,140,140,141,141,142,143,145,146,148,151,153,156,162,168
};

static uint8_t _CALIBRATION_SHADING_LS_A_R[1024]=
{
166,161,157,153,149,146,144,141,140,138,137,136,135,134,134,134,134,135,135,135,136,137,139,140,141,143,146,148,151,155,159,163,164,160,155,152,148,145,143,141,139,138,136,135,134,134,133,133,134,134,134,135,136,137,138,139,141,143,145,147,150,154,158,162,162,158,154,151,147,144,142,140,138,137,135,134,134,133,133,133,133,133,134,134,135,136,137,138,140,142,144,146,149,152,156,161,161,157,153,150,146,144,141,139,138,136,135,134,133,132,132,132,132,133,133,134,134,135,136,138,139,141,143,145,148,152,155,159,161,156,152,149,146,143,140,139,137,135,134,133,133,132,132,132,132,132,132,133,134,135,136,137,138,140,142,145,147,151,154,158,160,155,151,148,145,142,140,138,136,135,134,133,132,131,131,131,131,132,132,132,133,134,135,136,138,140,142,144,147,150,153,157,159,155,151,147,144,142,139,137,136,134,133,132,132,131,130,130,131,131,131,132,133,134,135,136,137,139,141,143,146,149,153,157,158,154,150,147,144,141,139,137,135,134,133,132,131,130,130,130,130,130,131,131,133,134,134,135,137,139,141,143,145,148,152,156,158,153,150,146,143,140,138,137,135,133,133,132,131,130,129,129,129,130,130,131,132,133,134,135,137,138,140,142,145,148,151,155,157,153,149,146,143,140,138,136,135,133,132,131,130,129,129,129,129,129,130,130,131,133,134,135,136,138,140,142,144,148,151,154,157,153,149,146,143,140,138,136,134,133,132,131,130,129,128,128,128,129,129,130,131,132,133,134,136,138,140,142,144,147,151,154,156,152,148,145,143,140,138,136,134,133,132,131,129,128,128,128,128,128,129,130,131,132,133,134,136,138,139,141,144,147,150,154,156,152,148,145,142,140,137,136,134,133,131,130,129,128,128,128,128,128,128,129,130,132,133,134,136,137,139,141,144,147,150,154,156,152,148,145,142,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,133,134,135,137,139,141,144,147,150,153,156,152,148,145,142,140,137,135,134,132,131,130,129,128,127,128,128,128,128,129,130,131,132,134,135,137,139,141,144,147,150,153,156,152,148,145,142,139,137,135,134,132,131,130,129,128,127,128,128,128,128,128,130,131,132,134,135,137,139,141,144,147,150,153,156,152,148,145,142,140,138,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,133,134,136,137,139,141,144,147,150,154,156,152,148,145,142,140,138,136,134,132,131,130,129,128,128,128,128,128,128,129,130,131,133,134,136,138,140,142,144,147,150,154,156,152,149,145,143,140,138,136,134,133,131,130,129,129,128,128,128,129,129,129,130,131,133,134,136,138,140,142,145,148,151,155,157,153,149,146,143,140,138,136,135,133,131,130,130,129,129,128,129,129,129,130,130,132,133,135,136,138,140,142,145,148,151,155,157,153,149,146,143,141,139,137,135,133,131,130,130,129,129,129,129,129,129,130,131,132,133,135,137,139,141,143,145,149,152,155,158,154,150,147,144,141,139,137,136,133,132,131,130,130,129,129,129,129,130,130,131,132,133,135,137,139,141,143,146,149,152,156,159,154,150,147,145,142,140,138,136,134,132,131,130,130,130,129,130,130,130,130,131,132,134,136,138,140,142,144,147,150,153,157,159,155,151,148,145,142,140,138,136,134,133,131,131,130,130,130,130,130,130,130,131,133,134,136,138,140,142,144,147,150,154,158,160,156,152,149,146,143,141,139,137,135,133,132,131,130,130,130,130,130,130,131,132,133,135,137,139,141,143,145,148,151,155,159,161,157,153,150,147,144,141,140,138,136,134,133,132,131,131,131,131,131,131,131,132,134,136,138,140,141,144,146,149,152,156,160,163,158,154,151,147,145,142,140,138,136,135,133,133,132,131,131,131,131,132,132,133,135,136,138,140,142,144,147,150,153,157,161,164,159,155,151,148,145,143,141,139,137,135,134,133,133,132,132,132,132,133,133,134,135,137,139,141,143,145,148,151,154,158,162,166,161,156,153,149,147,144,142,140,138,137,135,135,134,133,133,133,133,134,135,136,137,138,140,142,144,147,149,152,155,159,164,168,163,158,154,151,148,145,143,141,140,138,137,136,135,135,134,135,135,135,136,137,138,139,141,143,145,148,150,154,157,161,165,171,165,160,155,152,149,146,144,142,141,139,138,137,136,136,136,136,136,136,137,138,139,140,142,144,146,149,152,155,159,163,167,174,167,161,157,153,150,147,145,143,142,140,139,138,138,137,137,137,137,138,138,139,140,141,143,145,147,150,153,156,160,164,169
};

static uint8_t _CALIBRATION_SHADING_LS_A_G[1024]=
{
164,159,154,150,147,144,142,140,139,138,136,135,135,134,134,134,134,134,134,135,135,136,137,139,140,142,144,146,149,152,156,161,162,158,153,150,147,144,141,140,138,137,136,135,134,134,133,133,133,133,134,134,135,136,137,138,139,141,143,145,148,151,155,159,161,156,152,149,146,143,141,139,138,136,135,134,133,133,133,133,133,133,133,134,135,135,136,138,139,140,142,144,147,150,154,158,160,155,151,148,145,142,140,139,137,136,135,134,133,133,132,132,132,132,133,133,134,135,136,137,138,140,141,144,146,149,153,157,159,154,150,147,144,142,140,138,136,135,134,133,133,132,132,132,132,132,132,133,134,134,135,136,138,139,141,143,145,148,152,156,158,153,150,146,144,141,139,138,136,135,134,133,132,132,131,131,131,131,132,132,133,134,135,136,137,139,140,142,145,148,151,155,157,153,149,146,143,140,139,137,136,134,133,132,132,131,130,130,131,131,131,132,133,134,135,135,137,138,140,142,144,147,150,154,156,152,148,145,142,140,138,137,135,134,133,132,131,130,130,130,130,130,131,131,132,133,134,135,136,138,139,141,143,146,150,153,156,152,148,145,142,140,138,136,135,134,133,132,131,130,130,129,129,130,130,131,132,133,134,135,136,137,139,141,143,146,149,153,155,151,147,144,142,140,138,136,134,133,132,131,130,130,129,129,129,129,130,130,131,132,133,134,136,137,139,141,143,146,149,152,155,151,147,144,142,139,137,136,134,133,132,131,130,129,129,128,128,129,129,130,131,132,133,134,135,137,139,140,142,145,148,152,154,151,147,144,141,139,137,135,134,133,132,131,130,129,128,128,128,128,129,129,130,132,133,134,135,137,138,140,142,145,148,152,154,150,147,144,141,139,137,135,134,133,131,130,129,129,128,128,128,128,129,129,130,131,132,134,135,137,138,140,142,145,148,152,154,150,147,144,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,138,140,142,145,148,151,154,150,147,144,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,138,140,142,145,148,151,154,150,146,144,141,139,137,135,134,132,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,138,140,142,145,148,151,154,150,147,144,141,139,137,136,134,133,131,130,129,128,128,128,128,128,128,129,130,131,132,134,135,137,138,140,142,145,148,152,154,150,147,144,142,139,137,136,134,133,131,130,129,129,128,128,128,128,129,129,130,131,132,134,135,137,139,141,143,145,149,152,154,151,147,144,142,140,138,136,134,133,132,131,130,129,129,128,129,129,129,129,130,131,133,134,136,137,139,141,143,146,149,152,155,151,148,145,142,140,138,136,135,133,132,131,130,129,129,129,129,129,129,130,130,131,133,134,136,137,139,141,143,146,149,153,155,151,148,145,142,140,138,137,135,133,132,131,130,130,129,129,129,129,129,130,131,132,133,135,136,138,140,141,144,146,150,153,156,152,148,145,143,141,139,137,135,134,132,131,131,130,130,129,129,130,130,130,131,132,133,135,137,138,140,142,144,147,150,154,157,153,149,146,143,141,139,138,136,134,132,131,131,130,130,130,130,130,130,130,131,132,134,135,137,139,140,142,145,147,151,154,157,153,150,147,144,142,140,138,137,134,133,132,131,131,130,130,130,130,130,131,131,132,134,136,138,139,141,143,145,148,151,155,158,154,150,147,145,142,140,139,137,135,133,132,132,131,131,131,131,131,131,131,132,133,135,136,138,140,142,144,146,149,152,156,159,155,151,148,145,143,141,139,138,136,134,133,132,132,131,131,131,131,131,132,132,134,135,137,139,140,142,144,147,150,153,157,161,156,152,149,146,144,142,140,139,137,135,134,133,133,132,132,132,132,132,133,133,134,136,138,139,141,143,145,148,151,154,158,162,158,153,150,147,145,142,141,139,138,136,135,134,133,133,132,132,133,133,133,134,135,137,139,140,142,144,146,149,152,156,160,164,159,154,151,148,145,143,142,140,139,137,136,135,134,134,134,134,134,134,135,135,136,138,139,141,143,145,147,150,153,157,161,167,161,156,152,149,146,144,142,141,140,138,137,136,135,135,135,135,135,135,136,137,138,139,140,142,143,146,148,151,154,158,163,169,163,157,153,150,148,145,144,142,141,139,138,137,137,136,136,136,136,136,137,138,139,140,141,143,144,147,149,152,155,160,164,172,164,159,154,151,149,146,145,143,142,140,139,138,138,137,137,137,137,138,138,139,140,140,142,143,146,148,150,153,157,161,166
};

static uint8_t _CALIBRATION_SHADING_LS_A_B[1024]=
{
168,161,156,152,149,146,143,141,140,138,137,136,135,134,134,134,134,134,134,135,135,136,138,139,140,142,145,147,149,153,158,163,165,160,155,151,148,145,143,141,139,138,136,135,134,134,133,133,133,133,134,134,135,136,137,138,140,142,144,146,149,152,157,162,163,158,153,150,147,144,142,140,138,137,135,134,133,133,133,132,132,132,133,133,134,135,136,138,139,141,143,145,148,151,155,160,162,157,153,149,146,143,141,139,138,136,135,134,133,133,132,132,132,132,132,133,134,135,136,137,138,140,142,144,147,150,154,159,161,156,152,148,145,143,141,139,137,136,134,133,132,132,132,132,131,132,132,133,133,134,135,136,138,139,141,144,146,149,153,158,160,155,151,148,145,142,140,138,137,135,134,133,132,132,131,131,131,131,131,132,133,134,135,136,137,139,141,143,145,149,152,156,158,154,150,147,144,142,140,138,136,135,134,132,132,131,130,130,130,130,131,131,132,134,134,135,137,139,140,143,145,148,151,156,158,154,150,146,143,141,139,137,136,134,133,132,131,130,130,130,130,130,130,131,132,133,134,135,137,138,140,142,144,147,151,155,157,153,149,146,143,141,139,137,135,134,133,132,131,130,130,129,129,129,130,130,131,133,134,135,136,138,140,141,144,147,151,154,157,152,149,146,143,141,139,137,135,134,133,132,130,130,129,129,129,129,129,130,131,132,133,134,136,138,140,141,144,147,150,154,156,152,148,145,143,140,138,136,135,133,132,131,130,129,129,128,128,129,129,130,131,132,133,134,136,138,140,141,144,146,150,153,156,152,148,145,142,140,138,137,135,133,132,131,130,129,129,128,128,129,129,129,130,132,133,134,136,137,139,141,143,146,150,153,155,151,148,145,142,140,138,136,135,133,132,131,130,129,128,128,128,128,129,129,130,131,132,134,135,137,139,141,143,146,149,153,156,151,148,145,142,140,138,136,135,133,132,130,130,129,128,128,128,128,129,129,130,131,132,133,135,137,139,141,143,146,149,153,156,152,148,145,142,140,138,136,135,133,132,130,130,129,128,128,128,128,129,129,130,131,132,133,135,137,139,141,143,146,149,153,156,151,148,145,142,140,138,136,135,133,132,130,130,129,128,128,128,128,129,129,130,131,132,134,135,137,139,141,143,146,149,153,156,152,148,145,142,141,138,136,135,133,132,131,130,129,128,128,128,128,129,129,130,131,132,134,135,138,139,141,143,146,150,153,156,152,148,145,143,141,139,137,135,133,132,131,130,129,129,128,128,129,129,130,130,131,133,134,136,137,140,141,144,146,150,154,157,153,148,146,143,141,139,137,135,134,132,131,130,130,129,129,129,129,129,130,130,132,133,134,136,138,140,142,144,147,150,154,157,153,149,146,144,141,139,137,136,134,132,131,131,130,129,129,129,129,130,130,131,132,133,135,136,138,140,142,144,147,150,154,157,153,150,147,144,142,140,138,136,134,132,132,131,130,130,130,130,130,130,130,131,132,133,135,136,139,140,142,145,148,151,155,158,154,150,147,144,142,140,138,137,134,133,132,131,131,130,130,130,130,130,131,131,132,134,135,137,139,141,142,145,148,151,155,159,154,151,147,145,143,141,139,137,135,133,132,132,131,131,131,130,131,131,131,132,133,134,136,138,139,141,143,146,149,152,156,160,155,151,148,146,143,141,139,138,136,134,133,132,132,131,131,131,131,131,132,132,133,134,136,138,140,142,143,146,149,153,157,161,156,152,149,146,144,142,140,138,136,135,133,133,132,132,132,131,131,132,132,132,134,135,137,139,140,142,144,146,150,154,158,162,157,153,150,147,145,143,141,139,137,135,134,133,133,133,132,132,132,132,133,133,134,136,138,140,141,143,145,147,151,155,159,163,158,154,151,148,145,143,142,140,138,136,135,134,134,133,133,132,133,133,133,134,135,137,138,140,142,144,146,149,151,156,160,165,160,155,152,149,146,144,142,141,139,137,136,135,135,134,134,134,133,134,135,135,136,138,139,141,143,145,147,149,152,156,160,168,162,156,153,150,147,145,143,142,140,138,137,136,136,135,135,135,135,135,136,136,137,139,140,142,144,146,148,150,154,157,162,170,164,158,154,151,148,146,144,143,141,140,138,138,137,136,136,136,136,136,137,138,139,140,141,143,145,147,149,152,155,159,164,174,166,160,155,152,150,147,145,144,142,141,140,139,138,138,137,137,138,138,138,139,140,141,142,144,146,148,151,153,157,162,167,177,168,161,156,153,151,148,146,145,143,142,141,140,139,139,139,139,139,139,139,140,141,141,143,145,147,149,152,154,158,164,170
};

//aisp_lsc_ctl_t
static uint32_t _CALIBRATION_LENS_SHADING_CTL[4] =
{
    2, //mesh shading split mode 0:64x64 1: 32x64 2:32x32
    0, //mesh lut normalize select 0: 128 1:64 2:32 3:16
    32, //mesh hori-node numbers
    32, //mesh vert-node numbers
};

static uint16_t _CALIBRATION_GAMMA[129]=
{
0,86,134,169,198,223,245,265,283,300,316,331,346,359,372,385,397,409,420,431,441,451,461,471,481,490,499,508,516,525,533,541,549,557,565,572,580,587,594,601,608,615,622,628,635,641,648,654,660,667,673,679,685,691,697,702,708,714,719,725,730,736,741,747,752,757,762,767,773,778,783,788,793,797,802,807,812,817,821,826,831,835,840,844,849,853,858,862,867,871,875,880,884,888,892,897,901,905,909,913,917,921,925,929,933,937,941,945,949,953,957,960,964,968,972,976,979,983,987,990,994,998,1001,1005,1009,1012,1016,1019,1023
};

static int32_t _CALIBRATION_CCM[201]=
{
6,
2856,340,5,8102,8081,398,8162,23,7901,523,
4000,401,8102,8137,8090,404,8146,21,7994,433,
4100,401,8102,8137,8090,404,8146,21,7994,433,
5000,405,8108,8127,8106,417,8117,8,7996,443,
6500,406,8106,8128,8116,444,8081,3,8033,411,
7500,435,8098,8108,8125,461,8055,4,8035,409,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0
};

static int8_t _CALIBRATION_CAC_RX[1024]=
{
-3,-3,-4,-4,-4,-5,-5,-5,-5,-4,-4,-3,-3,-2,-1,0,0,1,2,3,3,4,4,5,5,5,5,4,4,4,3,3,-3,-3,-4,-4,-5,-5,-5,-5,-5,-5,-4,-4,-3,-2,-1,-1,0,1,2,3,4,4,5,5,5,5,5,5,4,4,4,3,-3,-4,-4,-5,-5,-5,-5,-5,-5,-5,-5,-4,-3,-2,-2,-1,0,1,2,3,4,4,5,5,5,5,5,5,5,4,4,3,-3,-4,-5,-5,-5,-6,-6,-6,-6,-5,-5,-4,-4,-3,-2,-1,0,2,3,3,4,5,5,5,6,6,6,5,5,5,4,3,-4,-4,-5,-5,-6,-6,-6,-6,-6,-6,-5,-5,-4,-3,-2,-1,1,2,3,4,4,5,6,6,6,6,6,6,5,5,4,4,-4,-5,-5,-6,-6,-6,-6,-6,-6,-6,-5,-5,-4,-3,-2,-1,1,2,3,4,5,5,6,6,6,6,6,6,6,5,5,4,-4,-5,-5,-6,-6,-6,-7,-7,-6,-6,-6,-5,-4,-3,-2,-1,1,2,3,4,5,6,6,6,7,7,6,6,6,5,5,4,-4,-5,-6,-6,-6,-7,-7,-7,-7,-7,-6,-6,-5,-4,-2,-1,1,2,3,5,5,6,7,7,7,7,7,6,6,6,5,4,-5,-5,-6,-6,-7,-7,-7,-7,-7,-7,-7,-6,-5,-4,-3,-1,1,2,4,5,6,6,7,7,7,7,7,7,6,6,5,5,-5,-5,-6,-7,-7,-7,-7,-7,-7,-7,-7,-6,-5,-4,-3,-1,1,3,4,5,6,7,7,7,8,7,7,7,7,6,5,5,-5,-6,-6,-7,-7,-7,-8,-8,-8,-8,-7,-7,-6,-5,-3,-1,1,3,5,6,7,7,8,8,8,8,7,7,7,6,6,5,-5,-6,-6,-7,-7,-8,-8,-8,-8,-8,-8,-7,-6,-5,-3,-1,1,3,5,6,7,8,8,8,8,8,8,7,7,6,6,5,-5,-6,-7,-7,-8,-8,-8,-8,-8,-8,-8,-8,-7,-6,-4,-2,1,4,6,7,7,8,8,8,8,8,8,8,7,7,6,5,-5,-6,-7,-7,-8,-8,-8,-8,-9,-9,-8,-8,-7,-6,-5,-2,1,4,6,7,8,8,9,9,8,8,8,8,7,7,6,5,-5,-6,-7,-7,-8,-8,-8,-9,-9,-9,-9,-8,-8,-7,-5,-2,2,5,7,8,8,9,9,9,9,8,8,8,7,7,6,6,-5,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-8,-8,-6,-3,2,6,8,8,9,9,9,9,9,9,8,8,7,7,6,6,-6,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-9,-8,-7,-4,4,7,8,9,9,9,9,9,9,9,8,8,8,7,6,6,-6,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-9,-9,-8,-7,6,8,9,9,9,9,9,9,9,9,8,8,8,7,6,6,-6,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-9,-9,-8,-7,7,8,9,9,9,9,9,9,9,9,8,8,8,7,6,6,-6,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-9,-8,-7,-5,4,7,8,9,9,9,9,9,9,9,8,8,8,7,6,6,-5,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-8,-8,-6,-3,3,6,8,8,9,9,9,9,9,9,8,8,7,7,6,6,-5,-6,-7,-7,-8,-8,-8,-9,-9,-9,-9,-8,-8,-7,-5,-2,2,5,7,8,8,9,9,9,9,8,8,8,7,7,6,6,-5,-6,-7,-7,-8,-8,-8,-8,-9,-9,-8,-8,-7,-6,-5,-2,2,4,6,7,8,8,9,9,9,8,8,8,7,7,6,5,-5,-6,-7,-7,-8,-8,-8,-8,-8,-8,-8,-8,-7,-6,-4,-2,1,4,6,7,8,8,8,8,8,8,8,8,7,7,6,5,-5,-6,-6,-7,-7,-8,-8,-8,-8,-8,-8,-7,-6,-5,-4,-1,1,3,5,6,7,8,8,8,8,8,8,7,7,6,6,5,-5,-6,-6,-7,-7,-7,-8,-8,-8,-8,-7,-7,-6,-5,-3,-1,1,3,5,6,7,7,8,8,8,8,8,7,7,6,6,5,-5,-5,-6,-7,-7,-7,-7,-8,-7,-7,-7,-6,-5,-4,-3,-1,1,3,4,5,6,7,7,7,8,7,7,7,7,6,6,5,-5,-5,-6,-6,-7,-7,-7,-7,-7,-7,-7,-6,-5,-4,-3,-1,1,2,4,5,6,6,7,7,7,7,7,7,6,6,5,5,-4,-5,-6,-6,-6,-7,-7,-7,-7,-7,-6,-6,-5,-4,-2,-1,1,2,4,5,5,6,7,7,7,7,7,6,6,6,5,4,-4,-5,-5,-6,-6,-6,-7,-7,-7,-6,-6,-5,-4,-3,-2,-1,1,2,3,4,5,6,6,6,7,7,6,6,6,5,5,4,-4,-5,-5,-6,-6,-6,-6,-6,-6,-6,-5,-5,-4,-3,-2,-1,1,2,3,4,5,5,6,6,6,6,6,6,6,5,5,4,-4,-4,-5,-5,-6,-6,-6,-6,-6,-6,-5,-5,-4,-3,-2,-1,1,2,3,4,5,5,6,6,6,6,6,6,5,5,4,4
};

static int8_t _CALIBRATION_CAC_RY[1024]=
{
-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-7,-8,-8,-8,-9,-9,-8,-8,-8,-8,-7,-7,-6,-6,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-8,-8,-9,-9,-9,-9,-8,-8,-8,-7,-7,-6,-6,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-8,-8,-9,-9,-9,-9,-8,-8,-8,-7,-7,-6,-6,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-8,-9,-9,-9,-9,-9,-9,-8,-8,-7,-7,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-6,-7,-8,-8,-9,-9,-9,-9,-9,-9,-8,-8,-7,-7,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-6,-7,-8,-8,-9,-9,-9,-9,-9,-9,-8,-8,-7,-6,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-2,-3,-3,-3,-4,-4,-5,-6,-6,-7,-7,-8,-9,-9,-9,-9,-9,-9,-8,-7,-7,-6,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-8,-9,-9,-9,-9,-8,-8,-7,-7,-6,-5,-5,-4,-4,-3,-3,-3,-2,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-6,-7,-8,-8,-9,-9,-9,-9,-8,-8,-7,-6,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-1,-2,-2,-3,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-9,-9,-9,-9,-8,-7,-7,-6,-5,-5,-4,-4,-3,-3,-3,-2,-2,-2,-1,-2,-2,-2,-3,-3,-3,-4,-4,-5,-6,-6,-7,-8,-9,-9,-9,-9,-8,-7,-6,-6,-5,-4,-4,-3,-3,-3,-2,-2,-2,-1,-1,-1,-2,-2,-2,-3,-3,-3,-4,-4,-5,-6,-7,-7,-8,-9,-9,-8,-8,-7,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-1,-1,-1,-1,-2,-2,-2,-2,-3,-3,-3,-4,-5,-5,-6,-7,-8,-9,-9,-8,-7,-6,-5,-5,-4,-4,-3,-3,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-2,-2,-2,-3,-3,-3,-4,-5,-5,-6,-8,-9,-9,-8,-6,-5,-5,-4,-3,-3,-3,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,-3,-3,-4,-4,-5,-7,-8,-8,-7,-6,-5,-4,-3,-3,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-3,-3,-4,-6,-8,-8,-6,-4,-3,-3,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-3,-4,-7,-7,-4,-3,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-2,-4,-5,-2,-1,-1,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,3,4,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,3,4,7,7,4,3,2,2,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,3,3,4,6,8,8,6,4,3,3,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,4,4,5,7,8,8,7,6,4,4,3,3,2,2,2,2,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,4,4,5,6,7,8,9,8,6,5,4,4,3,3,3,2,2,2,1,1,1,1,1,1,1,2,2,2,3,3,3,4,4,5,6,7,8,9,9,8,7,6,5,5,4,3,3,3,2,2,2,2,1,1,1,1,2,2,2,3,3,3,4,4,5,6,7,7,8,9,9,8,8,7,6,5,4,4,3,3,3,2,2,2,1,1,1,2,2,2,3,3,3,4,4,5,6,6,7,8,9,9,9,9,8,7,6,6,5,4,4,3,3,3,2,2,2,1,1,2,2,2,3,3,4,4,5,5,6,7,7,8,9,9,9,9,8,7,7,6,5,5,4,4,3,3,3,2,2,2,2,2,2,3,3,4,4,5,5,6,6,7,8,8,9,9,9,9,8,8,7,6,6,5,5,4,4,3,3,2,2,2,2,2,2,3,3,4,4,5,5,6,7,7,8,8,9,9,9,9,8,8,7,7,6,5,5,4,4,3,3,2,2,2,2,2,3,3,3,4,4,5,6,6,7,7,8,9,9,9,9,9,9,8,7,7,6,6,5,5,4,4,3,3,2,2,2,2,3,3,4,4,5,5,6,6,7,8,8,9,9,9,9,9,9,8,8,7,6,6,5,5,4,4,3,3,2,2,2,2,3,3,4,4,5,5,6,6,7,8,8,9,9,9,9,9,9,8,8,7,7,6,5,5,4,4,3,3,2,2
};

static int8_t _CALIBRATION_CAC_BX[1024]=
{
-19,-18,-16,-15,-14,-12,-11,-10,-8,-7,-5,-4,-3,-2,-1,-1,0,0,1,1,2,3,4,5,6,8,9,11,12,13,15,16,-19,-18,-16,-15,-14,-12,-11,-9,-8,-7,-5,-4,-3,-2,-1,-1,0,0,0,1,1,2,3,5,6,7,9,10,12,13,15,16,-19,-18,-16,-15,-14,-12,-11,-9,-8,-6,-5,-3,-2,-1,-1,0,0,0,0,1,1,2,3,4,6,7,9,10,12,13,15,16,-19,-18,-16,-15,-14,-12,-11,-9,-7,-6,-4,-3,-2,-1,0,0,0,0,0,0,1,1,2,4,5,7,8,10,12,13,14,16,-19,-17,-16,-15,-13,-12,-10,-9,-7,-5,-4,-2,-1,0,0,0,0,0,0,0,0,1,2,3,5,6,8,10,11,13,14,16,-19,-17,-16,-15,-13,-12,-10,-8,-7,-5,-3,-2,-1,0,1,1,0,0,-1,-1,-1,0,1,3,4,6,8,10,11,13,14,16,-19,-17,-16,-15,-13,-12,-10,-8,-6,-5,-3,-1,0,1,2,1,1,0,-1,-2,-1,-1,1,2,4,6,8,9,11,13,14,16,-19,-17,-16,-15,-13,-11,-10,-8,-6,-4,-2,-1,1,2,2,2,1,-1,-2,-2,-2,-1,0,2,4,5,7,9,11,13,14,16,-19,-17,-16,-15,-13,-11,-10,-8,-6,-4,-2,0,2,3,4,3,2,-1,-3,-4,-3,-2,-1,1,3,5,7,9,11,12,14,15,-19,-17,-16,-15,-13,-11,-9,-8,-6,-4,-1,1,2,4,5,4,2,-1,-4,-5,-4,-3,-1,1,3,5,7,9,11,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,-1,1,3,5,6,6,3,-1,-5,-6,-5,-4,-2,0,2,5,7,9,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,-1,2,4,6,8,8,5,-2,-7,-8,-7,-5,-3,0,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,0,2,5,7,9,10,7,-4,-10,-10,-8,-5,-3,-1,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,0,2,5,8,10,12,11,-7,-13,-11,-9,-6,-3,-1,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-2,0,3,5,8,11,14,17,-16,-15,-12,-9,-6,-3,-1,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-2,0,2,5,8,11,14,15,-11,-14,-12,-9,-6,-3,-1,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,0,2,5,7,10,11,9,-5,-11,-11,-8,-6,-3,-1,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,0,2,4,7,8,9,6,-3,-8,-9,-7,-5,-3,0,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,-1,1,4,6,7,7,4,-2,-6,-7,-6,-4,-2,0,2,4,6,8,10,12,14,15,-19,-17,-16,-14,-13,-11,-9,-7,-5,-3,-1,1,3,5,6,5,3,-1,-4,-6,-5,-4,-2,0,3,5,7,9,10,12,14,15,-19,-17,-16,-15,-13,-11,-9,-8,-6,-4,-2,0,2,3,4,4,2,-1,-3,-4,-4,-3,-1,1,3,5,7,9,11,12,14,15,-19,-17,-16,-15,-13,-11,-10,-8,-6,-4,-2,0,1,3,3,3,1,-1,-2,-3,-3,-2,0,1,3,5,7,9,11,12,14,15,-19,-17,-16,-15,-13,-12,-10,-8,-6,-4,-3,-1,1,2,2,2,1,0,-2,-2,-2,-1,0,2,4,6,7,9,11,13,14,16,-19,-17,-16,-15,-13,-12,-10,-8,-7,-5,-3,-2,0,1,1,1,1,0,-1,-1,-1,0,1,3,4,6,8,9,11,13,14,16,-19,-17,-16,-15,-13,-12,-10,-9,-7,-5,-4,-2,-1,0,1,1,0,0,-1,-1,0,0,2,3,5,6,8,10,11,13,14,16,-19,-17,-16,-15,-13,-12,-10,-9,-7,-6,-4,-3,-1,-1,0,0,0,0,0,0,0,1,2,4,5,7,8,10,11,13,14,16,-19,-18,-16,-15,-14,-12,-11,-9,-8,-6,-4,-3,-2,-1,0,0,0,0,0,0,1,2,3,4,5,7,8,10,12,13,14,16,-19,-18,-16,-15,-14,-12,-11,-9,-8,-6,-5,-4,-2,-2,-1,0,0,0,0,1,1,2,3,4,6,7,9,10,12,13,15,16,-19,-18,-16,-15,-14,-12,-11,-10,-8,-7,-5,-4,-3,-2,-1,-1,0,0,0,1,2,3,4,5,6,8,9,10,12,13,15,16,-19,-18,-16,-15,-14,-13,-11,-10,-8,-7,-6,-4,-3,-2,-1,-1,0,0,1,1,2,3,4,5,6,8,9,11,12,13,15,16,-19,-18,-16,-15,-14,-13,-11,-10,-9,-7,-6,-5,-4,-3,-2,-1,0,0,1,1,2,3,4,5,7,8,9,11,12,13,15,16,-18,-17,-16,-15,-14,-13,-11,-10,-9,-7,-6,-5,-4,-3,-2,-1,0,0,1,2,2,3,5,6,7,8,10,11,12,14,15,16
};

static int8_t _CALIBRATION_CAC_BY[1024]=
{
-9,-9,-9,-9,-9,-9,-8,-8,-8,-7,-7,-6,-5,-5,-4,-4,-3,-3,-4,-4,-4,-5,-6,-6,-7,-7,-8,-8,-9,-9,-9,-9,-8,-8,-8,-8,-8,-8,-8,-7,-7,-6,-6,-5,-4,-4,-3,-2,-2,-2,-2,-3,-3,-4,-5,-6,-6,-7,-7,-8,-8,-8,-8,-8,-8,-8,-8,-8,-7,-7,-7,-7,-6,-6,-5,-4,-3,-3,-2,-1,-1,-1,-1,-2,-2,-3,-4,-5,-5,-6,-6,-7,-7,-7,-8,-8,-7,-7,-7,-7,-7,-7,-6,-6,-5,-5,-4,-3,-2,-1,-1,0,1,1,0,0,-1,-2,-3,-4,-5,-5,-6,-6,-6,-7,-7,-7,-6,-6,-6,-6,-6,-6,-6,-5,-5,-4,-3,-3,-2,0,1,2,2,2,2,1,0,-1,-2,-3,-4,-5,-5,-5,-6,-6,-6,-6,-6,-6,-6,-6,-6,-5,-5,-5,-4,-3,-3,-2,-1,1,2,3,4,4,3,2,1,0,-1,-2,-3,-4,-4,-5,-5,-5,-6,-6,-5,-5,-5,-5,-5,-5,-4,-4,-3,-3,-2,-1,0,1,3,4,5,5,5,3,2,1,-1,-2,-3,-3,-4,-4,-5,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-3,-3,-2,-1,-1,1,2,4,5,7,7,6,4,3,1,0,-1,-2,-3,-3,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-3,-3,-3,-2,-2,-1,0,1,3,5,7,8,8,7,5,3,2,0,-1,-2,-2,-3,-3,-3,-4,-4,-4,-3,-3,-3,-3,-3,-3,-3,-2,-2,-1,-1,0,2,3,5,8,10,10,8,6,4,2,1,0,-1,-2,-2,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-1,-1,0,0,2,3,6,8,11,12,9,6,4,2,1,0,-1,-1,-2,-2,-2,-2,-3,-3,-2,-2,-2,-2,-2,-2,-2,-1,-1,-1,0,1,2,3,5,9,12,13,10,6,4,2,1,0,-1,-1,-1,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,0,0,1,2,4,8,13,15,9,5,3,2,1,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,0,0,0,1,2,3,5,12,15,7,4,2,1,0,0,0,0,0,-1,-1,-1,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,4,9,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-2,-3,-8,-14,-4,-2,-1,-1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,-1,-2,-4,-7,-13,-15,-8,-4,-2,-1,-1,0,0,0,1,1,1,1,1,1,2,2,2,2,2,1,1,1,1,1,0,-1,-1,-3,-5,-8,-13,-14,-10,-6,-3,-2,-1,0,0,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,0,-1,-2,-3,-5,-9,-12,-13,-10,-6,-4,-2,-1,0,1,1,1,2,2,2,2,2,3,3,3,3,3,3,2,2,2,1,0,0,-2,-3,-5,-8,-10,-11,-9,-6,-4,-2,-1,0,1,1,2,2,2,3,3,3,4,4,4,3,3,3,3,3,2,2,1,0,-1,-3,-5,-7,-9,-9,-8,-6,-4,-2,-1,0,1,2,2,3,3,3,3,3,4,4,4,4,4,4,3,3,3,2,1,0,-1,-3,-4,-6,-7,-8,-7,-5,-3,-2,0,1,2,2,3,3,4,4,4,4,5,5,5,5,5,4,4,4,3,3,2,1,0,-2,-3,-5,-6,-6,-5,-4,-2,-1,0,1,2,3,3,4,4,4,5,5,5,5,5,5,5,5,5,4,4,3,2,1,0,-1,-2,-4,-4,-5,-4,-3,-2,0,1,2,3,3,4,4,5,5,5,5,6,6,6,6,6,6,5,5,4,4,3,2,1,0,-1,-2,-3,-3,-3,-2,-1,1,2,3,3,4,5,5,5,6,6,6,7,7,7,7,6,6,6,6,5,4,4,3,2,1,0,-1,-1,-2,-1,0,0,1,2,3,4,5,5,6,6,6,7,7,7,7,7,7,7,7,7,6,6,5,4,4,3,2,1,0,0,0,0,1,2,2,3,4,5,6,6,6,7,7,7,7,8,8,8,8,8,8,7,7,6,6,5,5,4,3,2,2,1,1,1,2,3,3,4,5,6,6,7,7,7,8,8,8,9,9,9,9,8,8,8,8,7,7,6,5,5,4,3,3,3,3,3,3,4,4,5,6,6,7,7,8,8,8,8,9,9,9,9,9,9,9,9,8,8,7,7,6,6,5,5,4,4,4,4,4,5,6,6,7,7,8,8,9,9,9,9,9,10,10,10,10,10,10,9,9,9,8,8,7,7,6,6,5,5,5,5,6,6,7,7,8,8,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,9,9,9,8,8,7,7,6,6,6,6,7,7,7,8,8,9,9,10,10,10,10,10,10
};

//CALIBRATION_AWB_RG_POS
static int16_t _CALIBRATION_AWB_RG_POS[15]=
{
1249,1440,1630,1821,2011,2202,2392,2583,2773,2964,3154,3345,3535,3726,3916
};
//CALIBRATION_AWB_BG_POS
static int16_t _CALIBRATION_AWB_BG_POS[15]=
{
654,821,987,1153,1319,1485,1651,1817,1984,2150,2316,2482,2648,2814,2981
};

static int16_t _CALIBRATION_AWB_MESH_DIST_TAB[15][15] =
{
{-144,-129,-114,-101,-88,-75,-64,-53,-43,-33,-25,-17,-10,-3,2,},
{-130,-115,-100,-86,-72,-59,-47,-36,-26,-16,-7,1,8,14,21,},
{-117,-101,-86,-71,-57,-44,-31,-19,-9,0,10,18,26,33,39,},
{-104,-88,-72,-57,-42,-28,-15,-3,7,18,27,36,44,51,57,},
{-92,-75,-59,-43,-28,-13,0,12,24,35,45,54,62,69,76,},
{-80,-63,-46,-29,-14,1,14,28,40,51,62,71,80,87,94,},
{-69,-51,-33,-17,-1,14,29,43,56,68,79,89,97,105,112,},
{-58,-40,-22,-4,12,28,43,58,72,84,96,106,115,123,131,},
{-48,-29,-11,6,24,41,57,72,87,100,112,123,133,141,149,},
{-38,-19,-2,17,35,53,70,86,101,116,128,140,150,159,167,},
{-29,-10,9,28,46,64,82,99,115,130,144,156,167,177,185,},
{-21,-2,18,37,56,75,93,111,128,144,159,172,184,194,203,},
{-13,6,26,46,66,85,104,122,140,158,174,188,201,211,221,},
{-6,14,34,54,74,94,113,133,151,169,187,203,217,228,238,},
{1,21,41,62,82,102,122,142,161,180,199,216,232,245,256,},
};

//CALIBRATION_AWB_MESH_CT_TAB
static int16_t _CALIBRATION_AWB_MESH_CT_TAB[15][15] =
{
{3931,3721,3581,3497,3093,2940,2811,2731,2548,2494,2434,2369,2310,2219,2220,},
{4428,3892,3720,3624,3289,3127,2987,2864,2628,2599,2539,2464,2369,2303,2238,},
{4688,4341,3860,3728,3677,3317,3172,3035,2922,2679,2604,2504,2413,2338,2301,},
{5011,4650,4307,3835,3748,3739,3347,3211,3048,2883,2617,2534,2443,2366,2314,},
{5335,4881,4619,4284,3822,3778,3811,3332,3145,2957,2796,2553,2467,2384,2324,},
{5686,5255,4838,4590,4259,3817,3775,3737,3253,3057,2868,2708,2487,2400,2329,},
{6387,5610,5183,4786,4573,3867,3802,3727,3652,3174,2968,2775,2620,2418,2336,},
{6497,6405,5521,5106,4761,4539,3881,3776,3670,3558,3091,2880,2685,2533,2350,},
{7074,6348,5855,5459,5086,4742,4497,3886,3744,3604,3456,3007,2789,2596,2446,},
{7202,6953,6231,5812,5435,5055,4712,4378,3881,3701,3529,3160,2923,2695,2506,},
{8365,7167,6859,6254,5799,5410,5034,4683,4327,3864,3652,3444,3082,2830,2606,},
{8966,8265,7220,6800,6334,5778,5388,5012,4648,4278,3844,3599,3352,2994,2739,},
{9409,8897,8156,7301,6757,6374,5771,5359,4926,4598,4220,3808,3533,3250,2757,},
{10007,9365,8791,8085,7331,6691,6387,5746,5328,4907,4557,4163,3760,3221,2725,},
{10010,9994,9300,8675,7322,7297,6660,6376,5724,5296,4878,4506,3756,3183,2693,},
};

//CALIBRATION_AWB_CT_RG_CURVE
static int32_t _CALIBRATION_AWB_CT_RG_CURVE[4] = {5408,-1072,69,0};

//CALIBRATION_AWB_CT_BG_CURVE
static int32_t _CALIBRATION_AWB_CT_BG_CURVE[4] = {-118,474,-17,0};

//CALIBRATION_AWB_WB_GOLDEN_D50
static int16_t _CALIBRATION_AWB_WB_GOLDEN_D50[2] = {1894,1839};

//CALIBRATION_AWB_WB_OTP_D50
static int16_t _CALIBRATION_AWB_WB_OTP_D50[2] = {1894,1839};

//Noise reduce calibration parameters
static uint16_t _CALIBRATION_NOISE_PROFILE[9][16] =
{
{0,9,12,15,17,19,19,20,19,19,18,16,14,12,9,5,},
{0,8,21,22,23,23,23,23,23,22,22,20,19,17,15,13,},
{0,28,29,31,32,32,32,32,31,30,28,26,24,21,18,14,},
{0,40,40,43,44,45,45,45,43,42,39,36,33,28,24,18,},
{0,52,56,59,61,63,63,63,61,59,56,52,47,41,35,27,},
{78,79,84,88,91,92,92,91,89,85,79,73,65,56,45,34,},
{78,79,84,88,91,92,92,91,89,85,79,73,65,56,45,34,},
{78,79,84,88,91,92,92,91,89,85,79,73,65,56,45,34,},
{78,79,84,88,91,92,92,91,89,85,79,73,65,56,45,34,},
};

static uint8_t _CALIBRATION_FPNR[2048*2*5] = {0};

//aisp_awb_info_t
static uint32_t _CALIBRATION_AWB_PRESET[15] =
{
    0,
    457,    //awb_sys_r_gain;
    256,    //awb_sys_g_gain;
    436,    //awb_sys_b_gain;
    560,    //awb_sys_r_gain_comp;
    256,    //awb_sys_g_gain_comp;
    506,    //awb_sys_b_gain_comp;
    5563,   //awb_sys_ct;
    20,     //awb_sys_cdiff;
    5000,
    0 ,
    256,
    256,
    1,
    1,
};

static LookupTable calibration_top_ctl = {.ptr = _CALIBRATION_TOP_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_TOP_CTL ) / sizeof( _CALIBRATION_TOP_CTL[0] ), .width = sizeof( _CALIBRATION_TOP_CTL[0] )};
static LookupTable calibration_awb_ctl = {.ptr = _CALIBRATION_AWB_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CTL ) / sizeof( _CALIBRATION_AWB_CTL[0] ), .width = sizeof( _CALIBRATION_AWB_CTL[0] )};
static LookupTable calibration_res_ctl = {.ptr = _CALIBRATION_RES_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_RES_CTL ) / sizeof( _CALIBRATION_RES_CTL[0] ), .width = sizeof( _CALIBRATION_RES_CTL[0] )};
static LookupTable calibration_awb_ct_pos = { .ptr = _CALIBRATION_AWB_CT_POS, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_CT_POS) / sizeof(_CALIBRATION_AWB_CT_POS[0]), .width = sizeof(_CALIBRATION_AWB_CT_POS[0] ) };
static LookupTable calibration_awb_ct_rg_compensate = { .ptr = _CALIBRATION_AWB_CT_RG_COMPENSATE, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATE ) / sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATE[0] ), .width = sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATE[0] )};
static LookupTable calibration_awb_ct_bg_compensate = { .ptr = _CALIBRATION_AWB_CT_BG_COMPENSATE, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATE) / sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATE[0]), .width = sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATE[0] ) };
static LookupTable calibration_awb_ct_wgt = { .ptr = _CALIBRATION_AWB_CT_WGT, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_WGT ) / sizeof( _CALIBRATION_AWB_CT_WGT[0] ), .width = sizeof( _CALIBRATION_AWB_CT_WGT[0] )};
static LookupTable calibration_awb_ct_dyn_cvrange = { .ptr = _CALIBRATION_AWB_CT_DYN_CVRANGE, .rows = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE) / sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0]), .cols = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0]) / sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0][0]), .width = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0][0] ) };
static LookupTable calibration_awb_weight_h = { .ptr = _CALIBRATION_AWB_WEIGHT_H, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_WEIGHT_H ) / sizeof( _CALIBRATION_AWB_WEIGHT_H[0] ), .width = sizeof( _CALIBRATION_AWB_WEIGHT_H[0] )};
static LookupTable calibration_awb_weight_v = { .ptr = _CALIBRATION_AWB_WEIGHT_V, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_WEIGHT_V ) / sizeof( _CALIBRATION_AWB_WEIGHT_V[0] ), .width = sizeof( _CALIBRATION_AWB_WEIGHT_V[0] )};
static LookupTable calibration_awb_ref_remove_lut = { .ptr = _CALIBRATION_AWB_REF_REMOVE_LUT, .rows = sizeof( _CALIBRATION_AWB_REF_REMOVE_LUT ) / sizeof( _CALIBRATION_AWB_REF_REMOVE_LUT[0] ), .cols = sizeof( _CALIBRATION_AWB_REF_REMOVE_LUT[0] ) / sizeof( _CALIBRATION_AWB_REF_REMOVE_LUT[0][0] ), .width = sizeof( _CALIBRATION_AWB_REF_REMOVE_LUT[0][0] )};
static LookupTable calibration_ae_ctl = {.ptr = _CALIBRATION_AE_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CTL ) / sizeof( _CALIBRATION_AE_CTL[0] ), .width = sizeof( _CALIBRATION_AE_CTL[0] )};
static LookupTable calibration_highlight_detect = {.ptr = _CALIBRATION_HIGHLIGHT_DETECT, .rows = 1, .cols = sizeof( _CALIBRATION_HIGHLIGHT_DETECT ) / sizeof( _CALIBRATION_HIGHLIGHT_DETECT[0] ), .width = sizeof( _CALIBRATION_HIGHLIGHT_DETECT[0] )};
static LookupTable calibration_ae_corr_lut = {.ptr = _CALIBRATION_AE_CORR_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CORR_LUT ) / sizeof( _CALIBRATION_AE_CORR_LUT[0] ), .width = sizeof( _CALIBRATION_AE_CORR_LUT[0] )};
static LookupTable calibration_ae_corr_pos_lut = {.ptr = _CALIBRATION_AE_CORR_POS_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CORR_POS_LUT ) / sizeof( _CALIBRATION_AE_CORR_POS_LUT[0] ), .width = sizeof( _CALIBRATION_AE_CORR_POS_LUT[0] )};
static LookupTable calibration_ae_route = {.ptr = _CALIBRATION_AE_ROUTE, .rows = 1, .cols = sizeof( _CALIBRATION_AE_ROUTE ) / sizeof( _CALIBRATION_AE_ROUTE[0] ), .width = sizeof( _CALIBRATION_AE_ROUTE[0] )};
static LookupTable calibration_ae_weight_h = {.ptr = _CALIBRATION_AE_WEIGHT_H, .rows = 1, .cols = sizeof( _CALIBRATION_AE_WEIGHT_H ) / sizeof( _CALIBRATION_AE_WEIGHT_H[0] ), .width = sizeof( _CALIBRATION_AE_WEIGHT_H[0] )};
static LookupTable calibration_ae_weight_v = {.ptr = _CALIBRATION_AE_WEIGHT_V, .rows = 1, .cols = sizeof( _CALIBRATION_AE_WEIGHT_V ) / sizeof( _CALIBRATION_AE_WEIGHT_V[0] ), .width = sizeof( _CALIBRATION_AE_WEIGHT_V[0] )};
static LookupTable calibration_ae_weight_t = {.ptr = _CALIBRATION_AE_WEIGHT_T, .rows = sizeof( _CALIBRATION_AE_WEIGHT_T ) / sizeof( _CALIBRATION_AE_WEIGHT_T[0] ), .cols = sizeof( _CALIBRATION_AE_WEIGHT_T[0] ) / sizeof( _CALIBRATION_AE_WEIGHT_T[0][0] ), .width = sizeof( _CALIBRATION_AE_WEIGHT_T[0][0] )};
static LookupTable calibration_daynight_detect = {.ptr = _CALIBRATION_DAYNIGHT_DETECT, .rows = 1, .cols = sizeof( _CALIBRATION_DAYNIGHT_DETECT ) / sizeof( _CALIBRATION_DAYNIGHT_DETECT[0] ), .width = sizeof( _CALIBRATION_DAYNIGHT_DETECT[0] )};
static LookupTable calibration_af_ctl = {.ptr = _CALIBRATION_AF_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_AF_CTL ) / sizeof( _CALIBRATION_AF_CTL[0] ), .width = sizeof( _CALIBRATION_AF_CTL[0] )};
static LookupTable calibration_af_weight_h = {.ptr = _CALIBRATION_AF_WEIGHT_H, .rows = 1, .cols = sizeof( _CALIBRATION_AF_WEIGHT_H ) / sizeof( _CALIBRATION_AF_WEIGHT_H[0] ), .width = sizeof( _CALIBRATION_AF_WEIGHT_H[0] )};
static LookupTable calibration_af_weight_v = {.ptr = _CALIBRATION_AF_WEIGHT_V, .rows = 1, .cols = sizeof( _CALIBRATION_AF_WEIGHT_V ) / sizeof( _CALIBRATION_AF_WEIGHT_V[0] ), .width = sizeof( _CALIBRATION_AF_WEIGHT_V[0] )};
static LookupTable calibration_flicker_ctl = {.ptr = _CALIBRATION_FLICKER_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_FLICKER_CTL ) / sizeof( _CALIBRATION_FLICKER_CTL[0] ), .width = sizeof( _CALIBRATION_FLICKER_CTL[0] )};
static LookupTable calibration_gtm = { .ptr = _CALIBRATION_GTM, .rows = 1, .cols = sizeof( _CALIBRATION_GTM ) / sizeof( _CALIBRATION_GTM[0] ), .width = sizeof( _CALIBRATION_GTM[0] )};
static LookupTable calibration_ge_adj = { .ptr = _CALIBRATION_GE_ADJ, .rows = sizeof( _CALIBRATION_DPC_ADJ ) / sizeof( _CALIBRATION_DPC_ADJ[0] ), .cols = sizeof( _CALIBRATION_GE_ADJ[0] ) / sizeof( _CALIBRATION_GE_ADJ[0][0] ), .width = sizeof( _CALIBRATION_GE_ADJ[0][0] )};
static LookupTable calibration_ge_s_adj = { .ptr = _CALIBRATION_GE_S_ADJ, .rows = sizeof( _CALIBRATION_GE_S_ADJ ) / sizeof( _CALIBRATION_GE_S_ADJ[0] ), .cols = sizeof( _CALIBRATION_GE_S_ADJ[0] ) / sizeof( _CALIBRATION_GE_S_ADJ[0][0] ), .width = sizeof( _CALIBRATION_GE_S_ADJ[0][0] )};
static LookupTable calibration_dpc_ctl = { .ptr = _CALIBRATION_DPC_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_DPC_CTL ) / sizeof( _CALIBRATION_DPC_CTL[0] ), .width = sizeof( _CALIBRATION_DPC_CTL[0] )};
static LookupTable calibration_dpc_s_ctl = { .ptr = _CALIBRATION_DPC_S_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_DPC_S_CTL ) / sizeof( _CALIBRATION_DPC_S_CTL[0] ), .width = sizeof( _CALIBRATION_DPC_S_CTL[0] )};
static LookupTable calibration_dpc_adj = { .ptr = _CALIBRATION_DPC_ADJ, .rows = sizeof( _CALIBRATION_DPC_ADJ ) / sizeof( _CALIBRATION_DPC_ADJ[0] ), .cols = sizeof( _CALIBRATION_DPC_ADJ[0] ) / sizeof( _CALIBRATION_DPC_ADJ[0][0] ), .width = sizeof( _CALIBRATION_DPC_ADJ[0][0] )};
static LookupTable calibration_dpc_s_adj = { .ptr = _CALIBRATION_DPC_S_ADJ, .rows = sizeof( _CALIBRATION_DPC_S_ADJ ) / sizeof( _CALIBRATION_DPC_S_ADJ[0] ), .cols = sizeof( _CALIBRATION_DPC_S_ADJ[0] ) / sizeof( _CALIBRATION_DPC_S_ADJ[0][0] ), .width = sizeof( _CALIBRATION_DPC_S_ADJ[0][0] )};
static LookupTable calibration_wdr_ctl = {.ptr = _CALIBRATION_WDR_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_WDR_CTL ) / sizeof( _CALIBRATION_WDR_CTL[0] ), .width = sizeof( _CALIBRATION_WDR_CTL[0] )};
static LookupTable calibration_wdr_adjust = {.ptr = _CALIBRATION_WDR_ADJUST, .rows = sizeof( _CALIBRATION_WDR_ADJUST ) / sizeof( _CALIBRATION_WDR_ADJUST[0] ), .cols = sizeof( _CALIBRATION_WDR_ADJUST[0] ) / sizeof( _CALIBRATION_WDR_ADJUST[0][0] ), .width = sizeof( _CALIBRATION_WDR_ADJUST[0][0] )};
static LookupTable calibration_wdr_mdetc_loweight = { .ptr = _CALIBRATION_WDR_MDETC_LOWEIGHT, .rows = sizeof( _CALIBRATION_WDR_MDETC_LOWEIGHT ) / sizeof( _CALIBRATION_WDR_MDETC_LOWEIGHT[0]), .cols = sizeof( _CALIBRATION_WDR_MDETC_LOWEIGHT[0] ) / sizeof( _CALIBRATION_WDR_MDETC_LOWEIGHT[0][0] ), .width = sizeof( _CALIBRATION_WDR_MDETC_LOWEIGHT[0][0] )};
static LookupTable calibration_wdr_mdetc_hiweight = { .ptr = _CALIBRATION_WDR_MDETC_HIWEIGHT, .rows = sizeof( _CALIBRATION_WDR_MDETC_HIWEIGHT ) / sizeof( _CALIBRATION_WDR_MDETC_HIWEIGHT[0]), .cols = sizeof( _CALIBRATION_WDR_MDETC_HIWEIGHT[0] ) / sizeof( _CALIBRATION_WDR_MDETC_HIWEIGHT[0][0] ), .width = sizeof( _CALIBRATION_WDR_MDETC_HIWEIGHT[0][0] )};
static LookupTable calibration_oe_eotf = { .ptr = _CALIBRATION_OE_EOTF, .rows = 1, .cols = sizeof(_CALIBRATION_OE_EOTF) / sizeof(_CALIBRATION_OE_EOTF[0]), .width = sizeof(_CALIBRATION_OE_EOTF[0] ) };
static LookupTable calibration_sqrt1 = { .ptr = _CALIBRATION_SQRT1, .rows = 1, .cols = sizeof(_CALIBRATION_SQRT1) / sizeof(_CALIBRATION_SQRT1[0]), .width = sizeof(_CALIBRATION_SQRT1[0] ) };
static LookupTable calibration_eotf1 = { .ptr = _CALIBRATION_EOTF1, .rows = 1, .cols = sizeof( _CALIBRATION_EOTF1 ) / sizeof( _CALIBRATION_EOTF1[0] ), .width = sizeof( _CALIBRATION_EOTF1[0] )};
static LookupTable calibration_rawcnr_ctl = { .ptr = _CALIBRATION_RAWCNR_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_RAWCNR_CTL) / sizeof(_CALIBRATION_RAWCNR_CTL[0]), .width = sizeof(_CALIBRATION_RAWCNR_CTL[0] ) };
static LookupTable calibration_rawcnr_adj = { .ptr = _CALIBRATION_RAWCNR_ADJ, .rows = sizeof(_CALIBRATION_RAWCNR_ADJ) / sizeof(_CALIBRATION_RAWCNR_ADJ[0]), .cols = sizeof(_CALIBRATION_RAWCNR_ADJ[0]) / sizeof(_CALIBRATION_RAWCNR_ADJ[0][0]), .width = sizeof(_CALIBRATION_RAWCNR_ADJ[0][0] ) };
static LookupTable calibration_rawcnr_meta_gain_lut = { .ptr = _CALIBRATION_RAWCNR_META_GAIN_LUT, .rows = sizeof( _CALIBRATION_RAWCNR_META_GAIN_LUT ) / sizeof( _CALIBRATION_RAWCNR_META_GAIN_LUT[0] ), .cols = sizeof( _CALIBRATION_RAWCNR_META_GAIN_LUT[0] ) / sizeof( _CALIBRATION_RAWCNR_META_GAIN_LUT[0][0] ), .width = sizeof( _CALIBRATION_RAWCNR_META_GAIN_LUT[0][0] )};
static LookupTable calibration_rawcnr_sps_csig_weight5x5 = { .ptr = _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5, .rows = sizeof( _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5 ) / sizeof( _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5[0] ), .cols = sizeof( _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5[0] ) / sizeof( _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5[0][0] ), .width = sizeof( _CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5[0][0] )};
static LookupTable calibration_snr_ctl = { .ptr = _CALIBRATION_SNR_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_SNR_CTL ) / sizeof( _CALIBRATION_SNR_CTL[0] ), .width = sizeof( _CALIBRATION_SNR_CTL[0] )};
static LookupTable calibration_snr_glb_adj = { .ptr = _CALIBRATION_SNR_GLB_ADJ, .rows = sizeof( _CALIBRATION_SNR_GLB_ADJ ) / sizeof( _CALIBRATION_SNR_GLB_ADJ[0] ), .cols = sizeof( _CALIBRATION_SNR_GLB_ADJ[0] ) / sizeof( _CALIBRATION_SNR_GLB_ADJ[0][0] ), .width = sizeof( _CALIBRATION_SNR_GLB_ADJ[0][0] )};
static LookupTable calibration_snr_adj = { .ptr = _CALIBRATION_SNR_ADJ, .rows = sizeof( _CALIBRATION_SNR_ADJ ) / sizeof( _CALIBRATION_SNR_ADJ[0] ), .cols = sizeof( _CALIBRATION_SNR_ADJ[0] ) / sizeof( _CALIBRATION_SNR_ADJ[0][0] ), .width = sizeof( _CALIBRATION_SNR_ADJ[0][0] )};
static LookupTable calibration_snr_cur_wt = { .ptr = _CALIBRATION_SNR_CUR_WT, .rows = sizeof( _CALIBRATION_SNR_CUR_WT ) / sizeof( _CALIBRATION_SNR_CUR_WT[0] ), .cols = sizeof( _CALIBRATION_SNR_CUR_WT[0] ) / sizeof( _CALIBRATION_SNR_CUR_WT[0][0] ), .width = sizeof( _CALIBRATION_SNR_CUR_WT[0][0] )};
static LookupTable calibration_snr_wt_luma_gain = { .ptr = _CALIBRATION_SNR_WT_LUMA_GAIN, .rows = sizeof( _CALIBRATION_SNR_WT_LUMA_GAIN ) / sizeof( _CALIBRATION_SNR_WT_LUMA_GAIN[0] ), .cols = sizeof( _CALIBRATION_SNR_WT_LUMA_GAIN[0] ) / sizeof( _CALIBRATION_SNR_WT_LUMA_GAIN[0][0] ), .width = sizeof( _CALIBRATION_SNR_WT_LUMA_GAIN[0][0] )};
static LookupTable calibration_snr_sad_meta2alp = { .ptr = _CALIBRATION_SNR_SAD_META2ALP, .rows = sizeof( _CALIBRATION_SNR_SAD_META2ALP ) / sizeof( _CALIBRATION_SNR_SAD_META2ALP[0] ), .cols = sizeof( _CALIBRATION_SNR_SAD_META2ALP[0] ) / sizeof( _CALIBRATION_SNR_SAD_META2ALP[0][0] ), .width = sizeof( _CALIBRATION_SNR_SAD_META2ALP[0][0] )};
static LookupTable calibration_snr_meta_adj = { .ptr = _CALIBRATION_SNR_META_ADJ, .rows = sizeof( _CALIBRATION_SNR_META_ADJ ) / sizeof( _CALIBRATION_SNR_META_ADJ[0] ), .cols = sizeof( _CALIBRATION_SNR_META_ADJ[0] ) / sizeof( _CALIBRATION_SNR_META_ADJ[0][0] ), .width = sizeof( _CALIBRATION_SNR_META_ADJ[0][0] )};
static LookupTable calibration_snr_phs = { .ptr = _CALIBRATION_SNR_PHS, .rows = sizeof(_CALIBRATION_SNR_PHS) / sizeof(_CALIBRATION_SNR_PHS[0]), .cols = sizeof(_CALIBRATION_SNR_PHS[0]) / sizeof(_CALIBRATION_SNR_PHS[0][0]), .width = sizeof(_CALIBRATION_SNR_PHS[0][0] ) };
static LookupTable calibration_nr_rad_lut65 = { .ptr = _CALIBRATION_NR_RAD_LUT65, .rows = sizeof(_CALIBRATION_NR_RAD_LUT65) / sizeof(_CALIBRATION_NR_RAD_LUT65[0]), .cols = sizeof(_CALIBRATION_NR_RAD_LUT65[0]) / sizeof(_CALIBRATION_NR_RAD_LUT65[0][0]), .width = sizeof(_CALIBRATION_NR_RAD_LUT65[0][0] ) };
static LookupTable calibration_pst_snr_adj = { .ptr = _CALIBRATION_PST_SNR_ADJ, .rows = sizeof(_CALIBRATION_PST_SNR_ADJ) / sizeof(_CALIBRATION_PST_SNR_ADJ[0]), .cols = sizeof(_CALIBRATION_PST_SNR_ADJ[0]) / sizeof(_CALIBRATION_PST_SNR_ADJ[0][0]), .width = sizeof(_CALIBRATION_PST_SNR_ADJ[0][0] ) };
static LookupTable calibration_tnr_ctl = { .ptr = _CALIBRATION_TNR_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_TNR_CTL) / sizeof(_CALIBRATION_TNR_CTL[0]), .width = sizeof(_CALIBRATION_TNR_CTL[0] ) };
static LookupTable calibration_tnr_glb_adj = { .ptr = _CALIBRATION_TNR_GLB_ADJ, .rows = sizeof(_CALIBRATION_TNR_GLB_ADJ) / sizeof(_CALIBRATION_TNR_GLB_ADJ[0]), .cols = sizeof(_CALIBRATION_TNR_GLB_ADJ[0]) / sizeof(_CALIBRATION_TNR_GLB_ADJ[0][0]), .width = sizeof(_CALIBRATION_TNR_GLB_ADJ[0][0] ) };
static LookupTable calibration_tnr_adj = { .ptr = _CALIBRATION_TNR_ADJ, .rows = sizeof(_CALIBRATION_TNR_ADJ) / sizeof(_CALIBRATION_TNR_ADJ[0]), .cols = sizeof(_CALIBRATION_TNR_ADJ[0]) / sizeof(_CALIBRATION_TNR_ADJ[0][0]), .width = sizeof(_CALIBRATION_TNR_ADJ[0][0] ) };
static LookupTable calibration_tnr_ratio = {.ptr = _CALIBRATION_TNR_RATIO, .rows = sizeof( _CALIBRATION_TNR_RATIO ) / sizeof( _CALIBRATION_TNR_RATIO[0] ), .cols = sizeof( _CALIBRATION_TNR_RATIO[0] ) / sizeof( _CALIBRATION_TNR_RATIO[0][0] ), .width = sizeof( _CALIBRATION_TNR_RATIO[0][0] )};
static LookupTable calibration_tnr_sad2alpha = { .ptr = _CALIBRATION_TNR_SAD2ALPHA, .rows = sizeof(_CALIBRATION_TNR_SAD2ALPHA) / sizeof(_CALIBRATION_TNR_SAD2ALPHA[0]), .cols = sizeof(_CALIBRATION_TNR_SAD2ALPHA[0]) / sizeof(_CALIBRATION_TNR_SAD2ALPHA[0][0]), .width = sizeof(_CALIBRATION_TNR_SAD2ALPHA[0][0] ) };
static LookupTable calibration_mc_meta2alpha = { .ptr = _CALIBRATION_MC_META2ALPHA, .rows = sizeof(_CALIBRATION_MC_META2ALPHA) / sizeof(_CALIBRATION_MC_META2ALPHA[0]), .cols = sizeof(_CALIBRATION_MC_META2ALPHA[0]) / sizeof(_CALIBRATION_MC_META2ALPHA[0][0]), .width = sizeof(_CALIBRATION_MC_META2ALPHA[0][0] ) };
static LookupTable calibration_pst_tnr_alp_lut = { .ptr = _CALIBRATION_PST_TNR_ALP_LUT, .rows = sizeof(_CALIBRATION_PST_TNR_ALP_LUT) / sizeof(_CALIBRATION_PST_TNR_ALP_LUT[0]), .cols = sizeof(_CALIBRATION_PST_TNR_ALP_LUT[0]) / sizeof(_CALIBRATION_PST_TNR_ALP_LUT[0][0]), .width = sizeof(_CALIBRATION_PST_TNR_ALP_LUT[0][0] ) };
static LookupTable calibration_compress_ratio = { .ptr = _CALIBRATION_COMPRESS_RATIO, .rows = 1, .cols = sizeof(_CALIBRATION_COMPRESS_RATIO) / sizeof(_CALIBRATION_COMPRESS_RATIO[0]), .width = sizeof(_CALIBRATION_COMPRESS_RATIO[0] ) };
static LookupTable calibration_lens_shading_ct_correct = { .ptr = _CALIBRATION_LENS_SHADING_CT_CORRECT, .rows = 1, .cols = sizeof( _CALIBRATION_LENS_SHADING_CT_CORRECT ) / sizeof( _CALIBRATION_LENS_SHADING_CT_CORRECT[0] ), .width = sizeof( _CALIBRATION_LENS_SHADING_CT_CORRECT[0] )};
static LookupTable calibration_adp_lens_shading_ctl = { .ptr = _CALIBRATION_ADP_LENS_SHADING_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_ADP_LENS_SHADING_CTL) / sizeof(_CALIBRATION_ADP_LENS_SHADING_CTL[0]), .width = sizeof(_CALIBRATION_ADP_LENS_SHADING_CTL[0] ) };
static LookupTable calibration_lens_shading_adp = { .ptr = _CALIBRATION_LENS_SHADING_ADP, .rows = 1, .cols = sizeof(_CALIBRATION_LENS_SHADING_ADP) / sizeof(_CALIBRATION_LENS_SHADING_ADP[0]), .width = sizeof(_CALIBRATION_LENS_SHADING_ADP[0] ) };
static LookupTable calibration_lens_shading_adj = {.ptr = _CALIBRATION_LENS_SHADING_ADJ, .rows = sizeof( _CALIBRATION_LENS_SHADING_ADJ ) / sizeof( _CALIBRATION_LENS_SHADING_ADJ[0] ), .cols = sizeof( _CALIBRATION_LENS_SHADING_ADJ[0] ) / sizeof( _CALIBRATION_LENS_SHADING_ADJ[0][0] ), .width = sizeof( _CALIBRATION_LENS_SHADING_ADJ[0][0] )};
static LookupTable calibration_dms_adj = {.ptr = _CALIBRATION_DMS_ADJ, .rows = sizeof( _CALIBRATION_DMS_ADJ ) / sizeof( _CALIBRATION_DMS_ADJ[0] ), .cols = sizeof( _CALIBRATION_DMS_ADJ[0] ) / sizeof( _CALIBRATION_DMS_ADJ[0][0] ), .width = sizeof( _CALIBRATION_DMS_ADJ[0][0] )};
static LookupTable calibration_ccm_adj = {.ptr = _CALIBRATION_CCM_ADJ, .rows = sizeof( _CALIBRATION_CCM_ADJ ) / sizeof( _CALIBRATION_CCM_ADJ[0] ), .cols = sizeof( _CALIBRATION_CCM_ADJ[0] ) / sizeof( _CALIBRATION_CCM_ADJ[0][0] ), .width = sizeof( _CALIBRATION_CCM_ADJ[0][0] )};
static LookupTable calibration_cnr_ctl = {.ptr = _CALIBRATION_CNR_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_CNR_CTL ) / sizeof( _CALIBRATION_CNR_CTL[0] ), .width = sizeof( _CALIBRATION_CNR_CTL[0] )};
static LookupTable calibration_cnr_adj = {.ptr = _CALIBRATION_CNR_ADJ, .rows = sizeof( _CALIBRATION_CNR_ADJ ) / sizeof( _CALIBRATION_CNR_ADJ[0] ), .cols = sizeof( _CALIBRATION_CNR_ADJ[0] ) / sizeof( _CALIBRATION_CNR_ADJ[0][0] ), .width = sizeof( _CALIBRATION_CNR_ADJ[0][0] )};
static LookupTable calibration_purple_ctl = {.ptr = _CALIBRATION_PURPLE_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_PURPLE_CTL ) / sizeof( _CALIBRATION_PURPLE_CTL[0] ), .width = sizeof( _CALIBRATION_PURPLE_CTL[0] )};
static LookupTable calibration_purple_adj = {.ptr = _CALIBRATION_PURPLE_ADJ, .rows = sizeof( _CALIBRATION_PURPLE_ADJ ) / sizeof( _CALIBRATION_PURPLE_ADJ[0] ), .cols = sizeof( _CALIBRATION_PURPLE_ADJ[0] ) / sizeof( _CALIBRATION_PURPLE_ADJ[0][0] ), .width = sizeof( _CALIBRATION_PURPLE_ADJ[0][0] )};
static LookupTable calibration_ltm_ctl = {.ptr = _CALIBRATION_LTM_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_LTM_CTL ) / sizeof( _CALIBRATION_LTM_CTL[0] ), .width = sizeof( _CALIBRATION_LTM_CTL[0] )};
static LookupTable calibration_ltm_contrast = {.ptr = _CALIBRATION_LTM_CONTRAST, .rows = 1, .cols = sizeof( _CALIBRATION_LTM_CONTRAST ) / sizeof( _CALIBRATION_LTM_CONTRAST[0] ), .width = sizeof( _CALIBRATION_LTM_CONTRAST[0] )};
static LookupTable calibration_ltm_lo_hi_gm = {.ptr = _CALIBRATION_LTM_LO_HI_GM, .rows = sizeof( _CALIBRATION_LTM_LO_HI_GM ) / sizeof( _CALIBRATION_LTM_LO_HI_GM[0] ), .cols = sizeof( _CALIBRATION_LTM_LO_HI_GM[0] ) / sizeof( _CALIBRATION_LTM_LO_HI_GM[0][0] ), .width = sizeof( _CALIBRATION_LTM_LO_HI_GM[0][0] )};
static LookupTable calibration_lc_strength = {.ptr = _CALIBRATION_LC_STRENGTH, .rows = sizeof( _CALIBRATION_LC_STRENGTH ) / sizeof( _CALIBRATION_LC_STRENGTH[0] ), .cols = sizeof( _CALIBRATION_LC_STRENGTH[0] ) / sizeof( _CALIBRATION_LC_STRENGTH[0][0] ), .width = sizeof( _CALIBRATION_LC_STRENGTH[0][0] )};
static LookupTable calibration_dnlp_strength = {.ptr = _CALIBRATION_DNLP_STRENGTH, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_STRENGTH ) / sizeof( _CALIBRATION_DNLP_STRENGTH[0] ), .width = sizeof( _CALIBRATION_DNLP_STRENGTH[0] )};
static LookupTable calibration_dhz_strength = {.ptr = _CALIBRATION_DHZ_STRENGTH, .rows = 1, .cols = sizeof( _CALIBRATION_DHZ_STRENGTH ) / sizeof( _CALIBRATION_DHZ_STRENGTH[0] ), .width = sizeof( _CALIBRATION_DHZ_STRENGTH[0] )};
static LookupTable calibration_dnlp_scurv_low = {.ptr = _CALIBRATION_DNLP_SCURV_LOW, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_SCURV_LOW ) / sizeof( _CALIBRATION_DNLP_SCURV_LOW[0] ), .width = sizeof( _CALIBRATION_DNLP_SCURV_LOW[0] )};
static LookupTable calibration_dnlp_scurv_mid1 = {.ptr = _CALIBRATION_DNLP_SCURV_MID1, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_SCURV_MID1 ) / sizeof( _CALIBRATION_DNLP_SCURV_MID1[0] ), .width = sizeof( _CALIBRATION_DNLP_SCURV_MID1[0] )};
static LookupTable calibration_dnlp_scurv_mid2 = {.ptr = _CALIBRATION_DNLP_SCURV_MID2, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_SCURV_MID2 ) / sizeof( _CALIBRATION_DNLP_SCURV_MID2[0] ), .width = sizeof( _CALIBRATION_DNLP_SCURV_MID2[0] )};
static LookupTable calibration_dnlp_scurv_hgh1 = {.ptr = _CALIBRATION_DNLP_SCURV_HGH1, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_SCURV_HGH1 ) / sizeof( _CALIBRATION_DNLP_SCURV_HGH1[0] ), .width = sizeof( _CALIBRATION_DNLP_SCURV_HGH1[0] )};
static LookupTable calibration_dnlp_scurv_hgh2 = {.ptr = _CALIBRATION_DNLP_SCURV_HGH2, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_SCURV_HGH2 ) / sizeof( _CALIBRATION_DNLP_SCURV_HGH2[0] ), .width = sizeof( _CALIBRATION_DNLP_SCURV_HGH2[0] )};
static LookupTable calibration_ltm_sharp_adj = {.ptr = _CALIBRATION_LTM_SHARP_ADJ, .rows = sizeof( _CALIBRATION_LTM_SHARP_ADJ ) / sizeof( _CALIBRATION_LTM_SHARP_ADJ[0] ), .cols = sizeof( _CALIBRATION_LTM_SHARP_ADJ[0] ) / sizeof( _CALIBRATION_LTM_SHARP_ADJ[0][0] ), .width = sizeof( _CALIBRATION_LTM_SHARP_ADJ[0][0] )};
static LookupTable calibration_ltm_satur_lut = { .ptr = _CALIBRATION_LTM_SATUR_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_LTM_SATUR_LUT ) / sizeof( _CALIBRATION_LTM_SATUR_LUT[0] ), .width = sizeof( _CALIBRATION_LTM_SATUR_LUT[0] )};
static LookupTable calibration_lc_ctl = {.ptr = _CALIBRATION_LC_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_LC_CTL ) / sizeof( _CALIBRATION_LC_CTL[0] ), .width = sizeof( _CALIBRATION_LC_CTL[0] )};
static LookupTable calibration_lc_satur_lut = { .ptr = _CALIBRATION_LC_SATUR_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_LC_SATUR_LUT ) / sizeof( _CALIBRATION_LC_SATUR_LUT[0] ), .width = sizeof( _CALIBRATION_LC_SATUR_LUT[0] )};
static LookupTable calibration_dnlp_ctl = {.ptr = _CALIBRATION_DNLP_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_DNLP_CTL ) / sizeof( _CALIBRATION_DNLP_CTL[0] ), .width = sizeof( _CALIBRATION_DNLP_CTL[0] )};
static LookupTable calibration_dhz_ctl = {.ptr = _CALIBRATION_DHZ_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_DHZ_CTL ) / sizeof( _CALIBRATION_DHZ_CTL[0] ), .width = sizeof( _CALIBRATION_DHZ_CTL[0] )};
static LookupTable calibration_peaking_ctl = { .ptr = _CALIBRATION_PEAKING_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_PEAKING_CTL) / sizeof(_CALIBRATION_PEAKING_CTL[0]), .width = sizeof(_CALIBRATION_PEAKING_CTL[0] ) };
static LookupTable calibration_peaking_adjust = { .ptr = _CALIBRATION_PEAKING_ADJUST, .rows = sizeof(_CALIBRATION_PEAKING_ADJUST) / sizeof(_CALIBRATION_PEAKING_ADJUST[0]), .cols = sizeof(_CALIBRATION_PEAKING_ADJUST[0]) / sizeof(_CALIBRATION_PEAKING_ADJUST[0][0]), .width = sizeof(_CALIBRATION_PEAKING_ADJUST[0][0] ) };
static LookupTable calibration_peaking_flt1_motion_adp_gain = { .ptr = _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN ) / sizeof( _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN[0][0] )};
static LookupTable calibration_peaking_flt2_motion_adp_gain = { .ptr = _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN ) / sizeof( _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[0][0] )};
static LookupTable calibration_peaking_gain_vs_luma_lut = { .ptr = _CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT, .rows = sizeof(_CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT) / sizeof(_CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[0]), .cols = sizeof(_CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[0]) / sizeof(_CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[0][0]), .width = sizeof(_CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[0][0] ) };
static LookupTable calibration_peaking_cir_flt1_gain = { .ptr = _CALIBRATION_PEAKING_CIR_FLT1_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_CIR_FLT1_GAIN ) / sizeof( _CALIBRATION_PEAKING_CIR_FLT1_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_CIR_FLT1_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_CIR_FLT1_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_CIR_FLT1_GAIN[0][0] )};
static LookupTable calibration_peaking_cir_flt2_gain = { .ptr = _CALIBRATION_PEAKING_CIR_FLT2_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_CIR_FLT2_GAIN ) / sizeof( _CALIBRATION_PEAKING_CIR_FLT2_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_CIR_FLT2_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_CIR_FLT2_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_CIR_FLT2_GAIN[0][0] )};
static LookupTable calibration_peaking_drt_flt2_gain = { .ptr = _CALIBRATION_PEAKING_DRT_FLT2_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_DRT_FLT2_GAIN ) / sizeof( _CALIBRATION_PEAKING_DRT_FLT2_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_DRT_FLT2_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_DRT_FLT2_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_DRT_FLT2_GAIN[0][0] )};
static LookupTable calibration_peaking_drt_flt1_gain = { .ptr = _CALIBRATION_PEAKING_DRT_FLT1_GAIN, .rows = sizeof( _CALIBRATION_PEAKING_DRT_FLT1_GAIN ) / sizeof( _CALIBRATION_PEAKING_DRT_FLT1_GAIN[0] ), .cols = sizeof( _CALIBRATION_PEAKING_DRT_FLT1_GAIN[0] ) / sizeof( _CALIBRATION_PEAKING_DRT_FLT1_GAIN[0][0] ), .width = sizeof( _CALIBRATION_PEAKING_DRT_FLT1_GAIN[0][0] )};
static LookupTable calibration_cm_ctl = { .ptr = _CALIBRATION_CM_CTL, .rows = sizeof(_CALIBRATION_CM_CTL) / sizeof(_CALIBRATION_CM_CTL[0]), .cols = sizeof(_CALIBRATION_CM_CTL[0]) / sizeof(_CALIBRATION_CM_CTL[0][0]), .width = sizeof(_CALIBRATION_CM_CTL[0][0] ) };
static LookupTable calibration_cm_y_via_hue = { .ptr = _CALIBRATION_CM_Y_VIA_HUE, .rows = 1, .cols = sizeof(_CALIBRATION_CM_Y_VIA_HUE) / sizeof(_CALIBRATION_CM_Y_VIA_HUE[0]), .width = sizeof(_CALIBRATION_CM_Y_VIA_HUE[0] ) };
static LookupTable calibration_cm_satglbgain_via_y = { .ptr = _CALIBRATION_CM_SATGLBGAIN_VIA_Y, .rows = sizeof(_CALIBRATION_CM_SATGLBGAIN_VIA_Y) / sizeof(_CALIBRATION_CM_SATGLBGAIN_VIA_Y[0]), .cols = sizeof(_CALIBRATION_CM_SATGLBGAIN_VIA_Y[0]) / sizeof(_CALIBRATION_CM_SATGLBGAIN_VIA_Y[0][0]), .width = sizeof(_CALIBRATION_CM_SATGLBGAIN_VIA_Y[0][0] ) };
static LookupTable calibration_cm_sat_via_hs = { .ptr = _CALIBRATION_CM_SAT_VIA_HS, .rows = sizeof(_CALIBRATION_CM_SAT_VIA_HS) / sizeof(_CALIBRATION_CM_SAT_VIA_HS[0]), .cols = sizeof(_CALIBRATION_CM_SAT_VIA_HS[0]) / sizeof(_CALIBRATION_CM_SAT_VIA_HS[0][0]), .width = sizeof(_CALIBRATION_CM_SAT_VIA_HS[0][0] ) };
static LookupTable calibration_cm_satgain_via_y = { .ptr = _CALIBRATION_CM_SATGAIN_VIA_Y, .rows = sizeof(_CALIBRATION_CM_SATGAIN_VIA_Y) / sizeof(_CALIBRATION_CM_SATGAIN_VIA_Y[0]), .cols = sizeof(_CALIBRATION_CM_SATGAIN_VIA_Y[0]) / sizeof(_CALIBRATION_CM_SATGAIN_VIA_Y[0][0]), .width = sizeof(_CALIBRATION_CM_SATGAIN_VIA_Y[0][0] ) };
static LookupTable calibration_cm_hue_via_h = { .ptr = _CALIBRATION_CM_HUE_VIA_H, .rows = 1, .cols = sizeof(_CALIBRATION_CM_HUE_VIA_H) / sizeof(_CALIBRATION_CM_HUE_VIA_H[0]), .width = sizeof(_CALIBRATION_CM_HUE_VIA_H[0] ) };
static LookupTable calibration_cm_hue_via_s = { .ptr = _CALIBRATION_CM_HUE_VIA_S, .rows = sizeof(_CALIBRATION_CM_HUE_VIA_S) / sizeof(_CALIBRATION_CM_HUE_VIA_S[0]), .cols = sizeof(_CALIBRATION_CM_HUE_VIA_S[0]) / sizeof(_CALIBRATION_CM_HUE_VIA_S[0][0]), .width = sizeof(_CALIBRATION_CM_HUE_VIA_S[0][0] ) };
static LookupTable calibration_cm_hue_via_y = { .ptr = _CALIBRATION_CM_HUE_VIA_Y, .rows = sizeof(_CALIBRATION_CM_HUE_VIA_Y) / sizeof(_CALIBRATION_CM_HUE_VIA_Y[0]), .cols = sizeof(_CALIBRATION_CM_HUE_VIA_Y[0]) / sizeof(_CALIBRATION_CM_HUE_VIA_Y[0][0]), .width = sizeof(_CALIBRATION_CM_HUE_VIA_Y[0][0] ) };
static LookupTable calibration_hlc_ctl = { .ptr = _CALIBRATION_HLC_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_HLC_CTL) / sizeof(_CALIBRATION_HLC_CTL[0]), .width = sizeof(_CALIBRATION_HLC_CTL[0] ) };
static LookupTable calibration_csc_coef = { .ptr = _CALIBRATION_CSC_COEF, .rows = 1, .cols = sizeof(_CALIBRATION_CSC_COEF) / sizeof(_CALIBRATION_CSC_COEF[0]), .width = sizeof(_CALIBRATION_CSC_COEF[0] ) };
static LookupTable calibration_version = { .ptr = _CALIBRATION_VERSION, .rows = 1, .cols = sizeof(_CALIBRATION_VERSION) / sizeof(_CALIBRATION_VERSION[0]), .width = sizeof(_CALIBRATION_VERSION[0] ) };

static LookupTable calibration_black_level = { .ptr = _CALIBRATION_BLACK_LEVEL, .rows = sizeof( _CALIBRATION_BLACK_LEVEL ) / sizeof( _CALIBRATION_BLACK_LEVEL[0] ), .cols = sizeof( _CALIBRATION_BLACK_LEVEL[0] ) / sizeof( _CALIBRATION_BLACK_LEVEL[0][0] ), .width = sizeof( _CALIBRATION_BLACK_LEVEL[0][0] )};
static LookupTable calibration_noise_profile = { .ptr = _CALIBRATION_NOISE_PROFILE, .rows = sizeof(_CALIBRATION_NOISE_PROFILE) / sizeof(_CALIBRATION_NOISE_PROFILE[0]), .cols = sizeof(_CALIBRATION_NOISE_PROFILE[0]) / sizeof(_CALIBRATION_NOISE_PROFILE[0][0]), .width = sizeof(_CALIBRATION_NOISE_PROFILE[0][0] ) };
static LookupTable calibration_awb_mesh_dist_tab = { .ptr = _CALIBRATION_AWB_MESH_DIST_TAB, .rows = sizeof(_CALIBRATION_AWB_MESH_DIST_TAB) / sizeof(_CALIBRATION_AWB_MESH_DIST_TAB[0]), .cols = sizeof(_CALIBRATION_AWB_MESH_DIST_TAB[0]) / sizeof(_CALIBRATION_AWB_MESH_DIST_TAB[0][0]), .width = sizeof(_CALIBRATION_AWB_MESH_DIST_TAB[0][0] ) };
static LookupTable calibration_awb_mesh_ct_tab = { .ptr = _CALIBRATION_AWB_MESH_CT_TAB, .rows = sizeof( _CALIBRATION_AWB_MESH_CT_TAB ) / sizeof( _CALIBRATION_AWB_MESH_CT_TAB[0] ), .cols = sizeof( _CALIBRATION_AWB_MESH_CT_TAB[0] ) / sizeof( _CALIBRATION_AWB_MESH_CT_TAB[0][0] ), .width = sizeof( _CALIBRATION_AWB_MESH_CT_TAB[0][0] )};
static LookupTable calibration_awb_rg_pos = { .ptr = _CALIBRATION_AWB_RG_POS, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_RG_POS ) / sizeof( _CALIBRATION_AWB_RG_POS[0] ), .width = sizeof( _CALIBRATION_AWB_RG_POS[0] )};
static LookupTable calibration_awb_bg_pos = { .ptr = _CALIBRATION_AWB_BG_POS, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_BG_POS) / sizeof(_CALIBRATION_AWB_BG_POS[0]), .width = sizeof(_CALIBRATION_AWB_BG_POS[0] ) };
static LookupTable calibration_awb_ct_rg_curve = { .ptr = _CALIBRATION_AWB_CT_RG_CURVE, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_RG_CURVE ) / sizeof( _CALIBRATION_AWB_CT_RG_CURVE[0] ), .width = sizeof( _CALIBRATION_AWB_CT_RG_CURVE[0] )};
static LookupTable calibration_awb_ct_bg_curve = { .ptr = _CALIBRATION_AWB_CT_BG_CURVE, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_BG_CURVE ) / sizeof( _CALIBRATION_AWB_CT_BG_CURVE[0] ), .width = sizeof( _CALIBRATION_AWB_CT_BG_CURVE[0] )};
static LookupTable calibration_awb_wb_golden_d50 = { .ptr = _CALIBRATION_AWB_WB_GOLDEN_D50, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50 ) / sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50[0] ), .width = sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50[0] )};
static LookupTable calibration_awb_wb_otp_d50 = { .ptr = _CALIBRATION_AWB_WB_OTP_D50, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_WB_OTP_D50 ) / sizeof( _CALIBRATION_AWB_WB_OTP_D50[0] ), .width = sizeof( _CALIBRATION_AWB_WB_OTP_D50[0] )};
static LookupTable calibration_ccm = { .ptr = _CALIBRATION_CCM, .rows = 1, .cols = sizeof( _CALIBRATION_CCM ) / sizeof( _CALIBRATION_CCM[0] ), .width = sizeof( _CALIBRATION_CCM[0] )};
static LookupTable calibration_gamma = { .ptr = _CALIBRATION_GAMMA, .rows = 1, .cols = sizeof( _CALIBRATION_GAMMA ) / sizeof( _CALIBRATION_GAMMA[0] ), .width = sizeof( _CALIBRATION_GAMMA[0] )};
static LookupTable calibration_cac_rx = { .ptr = _CALIBRATION_CAC_RX, .rows = 1, .cols = sizeof( _CALIBRATION_CAC_RX ) / sizeof( _CALIBRATION_CAC_RX[0] ), .width = sizeof( _CALIBRATION_CAC_RX[0] )};
static LookupTable calibration_cac_ry = { .ptr = _CALIBRATION_CAC_RY, .rows = 1, .cols = sizeof( _CALIBRATION_CAC_RY ) / sizeof( _CALIBRATION_CAC_RY[0] ), .width = sizeof( _CALIBRATION_CAC_RY[0] )};
static LookupTable calibration_cac_bx = { .ptr = _CALIBRATION_CAC_BX, .rows = 1, .cols = sizeof( _CALIBRATION_CAC_BX ) / sizeof( _CALIBRATION_CAC_BX[0] ), .width = sizeof( _CALIBRATION_CAC_BX[0] )};
static LookupTable calibration_cac_by = { .ptr = _CALIBRATION_CAC_BY, .rows = 1, .cols = sizeof( _CALIBRATION_CAC_BY ) / sizeof( _CALIBRATION_CAC_BY[0] ), .width = sizeof( _CALIBRATION_CAC_BY[0] )};
static LookupTable calibration_shading_radial_r = { .ptr = _CALIBRATION_SHADING_RADIAL_R, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_RADIAL_R ) / sizeof( _CALIBRATION_SHADING_RADIAL_R[0] ), .width = sizeof( _CALIBRATION_SHADING_RADIAL_R[0] )};
static LookupTable calibration_shading_radial_g = { .ptr = _CALIBRATION_SHADING_RADIAL_G, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_RADIAL_G ) / sizeof( _CALIBRATION_SHADING_RADIAL_G[0] ), .width = sizeof( _CALIBRATION_SHADING_RADIAL_G[0] )};
static LookupTable calibration_shading_radial_b = { .ptr = _CALIBRATION_SHADING_RADIAL_B, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_RADIAL_B ) / sizeof( _CALIBRATION_SHADING_RADIAL_B[0] ), .width = sizeof( _CALIBRATION_SHADING_RADIAL_B[0] )};
static LookupTable calibration_shading_ls_d65_r = { .ptr = _CALIBRATION_SHADING_LS_D65_R, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_D65_R ) / sizeof( _CALIBRATION_SHADING_LS_D65_R[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_D65_R[0] )};
static LookupTable calibration_shading_ls_d65_g = { .ptr = _CALIBRATION_SHADING_LS_D65_G, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_D65_G ) / sizeof( _CALIBRATION_SHADING_LS_D65_G[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_D65_G[0] )};
static LookupTable calibration_shading_ls_d65_b = { .ptr = _CALIBRATION_SHADING_LS_D65_B, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_D65_B ) / sizeof( _CALIBRATION_SHADING_LS_D65_B[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_D65_B[0] )};
static LookupTable calibration_shading_ls_cwf_r = { .ptr = _CALIBRATION_SHADING_LS_CWF_R, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_R ) / sizeof( _CALIBRATION_SHADING_LS_CWF_R[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_CWF_R[0] )};
static LookupTable calibration_shading_ls_cwf_g = { .ptr = _CALIBRATION_SHADING_LS_CWF_G, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_G ) / sizeof( _CALIBRATION_SHADING_LS_CWF_G[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_CWF_G[0] )};
static LookupTable calibration_shading_ls_cwf_b = { .ptr = _CALIBRATION_SHADING_LS_CWF_B, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_B ) / sizeof( _CALIBRATION_SHADING_LS_CWF_B[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_CWF_B[0] )};
static LookupTable calibration_shading_ls_tl84_r = { .ptr = _CALIBRATION_SHADING_LS_TL84_R, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_R ) / sizeof( _CALIBRATION_SHADING_LS_TL84_R[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_TL84_R[0] )};
static LookupTable calibration_shading_ls_tl84_g = { .ptr = _CALIBRATION_SHADING_LS_TL84_G, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_G ) / sizeof( _CALIBRATION_SHADING_LS_TL84_G[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_TL84_G[0] )};
static LookupTable calibration_shading_ls_tl84_b = { .ptr = _CALIBRATION_SHADING_LS_TL84_B, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_B ) / sizeof( _CALIBRATION_SHADING_LS_TL84_B[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_TL84_B[0] )};
static LookupTable calibration_shading_ls_a_r = { .ptr = _CALIBRATION_SHADING_LS_A_R, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_A_R ) / sizeof( _CALIBRATION_SHADING_LS_A_R[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_A_R[0] )};
static LookupTable calibration_shading_ls_a_g = { .ptr = _CALIBRATION_SHADING_LS_A_G, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_A_G ) / sizeof( _CALIBRATION_SHADING_LS_A_G[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_A_G[0] )};
static LookupTable calibration_shading_ls_a_b = { .ptr = _CALIBRATION_SHADING_LS_A_B, .rows = 1, .cols = sizeof( _CALIBRATION_SHADING_LS_A_B ) / sizeof( _CALIBRATION_SHADING_LS_A_B[0] ), .width = sizeof( _CALIBRATION_SHADING_LS_A_B[0] )};
static LookupTable calibration_lens_shading_ctl = { .ptr = _CALIBRATION_LENS_SHADING_CTL, .rows = 1, .cols = sizeof(_CALIBRATION_LENS_SHADING_CTL) / sizeof(_CALIBRATION_LENS_SHADING_CTL[0]), .width = sizeof(_CALIBRATION_LENS_SHADING_CTL[0] ) };
static LookupTable calibration_lens_otp_center_offset = {.ptr = _CALIBRATION_LENS_OTP_CENTER_OFFSET, .rows = 1, .cols = sizeof( _CALIBRATION_LENS_OTP_CENTER_OFFSET ) / sizeof( _CALIBRATION_LENS_OTP_CENTER_OFFSET[0] ), .width = sizeof( _CALIBRATION_LENS_OTP_CENTER_OFFSET[0] )};
static LookupTable calibration_fpnr = { .ptr = _CALIBRATION_FPNR, .rows = 1, .cols = sizeof(_CALIBRATION_FPNR) / sizeof(_CALIBRATION_FPNR[0]), .width = sizeof(_CALIBRATION_FPNR[0] ) };
static LookupTable calibration_awb_preset = { .ptr = _CALIBRATION_AWB_PRESET, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_PRESET) / sizeof(_CALIBRATION_AWB_PRESET[0]), .width = sizeof(_CALIBRATION_AWB_PRESET[0] ) };

int dynamic_sdr_calibrations_init_imx585(aisp_calib_info_t *calib)
{
    calib->calibrations[CALIBRATION_TOP_CTL] = &calibration_top_ctl;
    calib->calibrations[CALIBRATION_RES_CTL] = &calibration_res_ctl;
    calib->calibrations[CALIBRATION_AWB_CTL] = &calibration_awb_ctl;
    calib->calibrations[CALIBRATION_AWB_CT_POS] = &calibration_awb_ct_pos;
    calib->calibrations[CALIBRATION_AWB_CT_RG_COMPENSATE] = &calibration_awb_ct_rg_compensate;
    calib->calibrations[CALIBRATION_AWB_CT_BG_COMPENSATE] = &calibration_awb_ct_bg_compensate;
    calib->calibrations[CALIBRATION_AWB_CT_WGT] = &calibration_awb_ct_wgt;
    calib->calibrations[CALIBRATION_AWB_CT_DYN_CVRANGE] = &calibration_awb_ct_dyn_cvrange;
    calib->calibrations[CALIBRATION_AWB_WEIGHT_H]= &calibration_awb_weight_h;
    calib->calibrations[CALIBRATION_AWB_WEIGHT_V] = &calibration_awb_weight_v;
    calib->calibrations[CALIBRATION_AE_CTL] = &calibration_ae_ctl;
    calib->calibrations[CALIBRATION_HIGHLIGHT_DETECT] = &calibration_highlight_detect;
    calib->calibrations[CALIBRATION_AWB_REF_REMOVE_LUT] = &calibration_awb_ref_remove_lut;
    calib->calibrations[CALIBRATION_AE_CORR_POS_LUT] = &calibration_ae_corr_pos_lut;
    calib->calibrations[CALIBRATION_AE_CORR_LUT] = &calibration_ae_corr_lut;
    calib->calibrations[CALIBRATION_AE_ROUTE] = &calibration_ae_route;
    calib->calibrations[CALIBRATION_AE_WEIGHT_H] = &calibration_ae_weight_h;
    calib->calibrations[CALIBRATION_AE_WEIGHT_V] = &calibration_ae_weight_v;
    calib->calibrations[CALIBRATION_AE_WEIGHT_T] = &calibration_ae_weight_t;
    calib->calibrations[CALIBRATION_DAYNIGHT_DETECT] = &calibration_daynight_detect;
    calib->calibrations[CALIBRATION_AF_CTL] = &calibration_af_ctl;
    calib->calibrations[CALIBRATION_AF_WEIGHT_H] = &calibration_af_weight_h;
    calib->calibrations[CALIBRATION_AF_WEIGHT_V] = &calibration_af_weight_v;
    calib->calibrations[CALIBRATION_FLICKER_CTL] = &calibration_flicker_ctl;
    calib->calibrations[CALIBRATION_GTM] = &calibration_gtm;
    calib->calibrations[CALIBRATION_GE_ADJ] = &calibration_ge_adj;
    calib->calibrations[CALIBRATION_GE_S_ADJ] = &calibration_ge_s_adj;
    calib->calibrations[CALIBRATION_DPC_CTL] = &calibration_dpc_ctl;
    calib->calibrations[CALIBRATION_DPC_S_CTL] = &calibration_dpc_s_ctl;
    calib->calibrations[CALIBRATION_DPC_ADJ] = &calibration_dpc_adj;
    calib->calibrations[CALIBRATION_DPC_S_ADJ] = &calibration_dpc_s_adj;
    calib->calibrations[CALIBRATION_WDR_CTL] = &calibration_wdr_ctl;
    calib->calibrations[CALIBRATION_WDR_ADJUST] = &calibration_wdr_adjust;
    calib->calibrations[CALIBRATION_WDR_MDETC_LOWEIGHT] = &calibration_wdr_mdetc_loweight;
    calib->calibrations[CALIBRATION_WDR_MDETC_HIWEIGHT] = &calibration_wdr_mdetc_hiweight;
    calib->calibrations[CALIBRATION_OE_EOTF] = &calibration_oe_eotf;
    calib->calibrations[CALIBRATION_SQRT1] = &calibration_sqrt1;
    calib->calibrations[CALIBRATION_EOTF1] = &calibration_eotf1;
    calib->calibrations[CALIBRATION_RAWCNR_CTL] = &calibration_rawcnr_ctl;
    calib->calibrations[CALIBRATION_RAWCNR_ADJ] = &calibration_rawcnr_adj;
    calib->calibrations[CALIBRATION_RAWCNR_META_GAIN_LUT] = &calibration_rawcnr_meta_gain_lut;
    calib->calibrations[CALIBRATION_RAWCNR_SPS_CSIG_WEIGHT5X5] = &calibration_rawcnr_sps_csig_weight5x5;
    calib->calibrations[CALIBRATION_SNR_CTL] = &calibration_snr_ctl;
    calib->calibrations[CALIBRATION_SNR_GLB_ADJ] = &calibration_snr_glb_adj;
    calib->calibrations[CALIBRATION_SNR_ADJ] = &calibration_snr_adj;
    calib->calibrations[CALIBRATION_SNR_CUR_WT] = &calibration_snr_cur_wt;
    calib->calibrations[CALIBRATION_SNR_WT_LUMA_GAIN] = &calibration_snr_wt_luma_gain;
    calib->calibrations[CALIBRATION_SNR_SAD_META2ALP] = &calibration_snr_sad_meta2alp;
    calib->calibrations[CALIBRATION_SNR_META_ADJ] = &calibration_snr_meta_adj;
    calib->calibrations[CALIBRATION_SNR_PHS] = &calibration_snr_phs;
    calib->calibrations[CALIBRATION_NR_RAD_LUT65] = &calibration_nr_rad_lut65;
    calib->calibrations[CALIBRATION_PST_SNR_ADJ] = &calibration_pst_snr_adj;
    calib->calibrations[CALIBRATION_TNR_CTL] = &calibration_tnr_ctl;
    calib->calibrations[CALIBRATION_TNR_GLB_ADJ] = &calibration_tnr_glb_adj;
    calib->calibrations[CALIBRATION_TNR_ADJ] = &calibration_tnr_adj;
    calib->calibrations[CALIBRATION_TNR_RATIO] = &calibration_tnr_ratio;
    calib->calibrations[CALIBRATION_TNR_SAD2ALPHA] = &calibration_tnr_sad2alpha;
    calib->calibrations[CALIBRATION_MC_META2ALPHA] = &calibration_mc_meta2alpha;
    calib->calibrations[CALIBRATION_PST_TNR_ALP_LUT] = &calibration_pst_tnr_alp_lut;
    calib->calibrations[CALIBRATION_COMPRESS_RATIO] = &calibration_compress_ratio;
    calib->calibrations[CALIBRATION_LENS_SHADING_CT_CORRECT] = &calibration_lens_shading_ct_correct;
    calib->calibrations[CALIBRATION_ADP_LENS_SHADING_CTL] = &calibration_adp_lens_shading_ctl;
    calib->calibrations[CALIBRATION_LENS_SHADING_ADP] = &calibration_lens_shading_adp;
    calib->calibrations[CALIBRATION_LENS_SHADING_ADJ] = &calibration_lens_shading_adj;
    calib->calibrations[CALIBRATION_DMS_ADJ] = &calibration_dms_adj;
    calib->calibrations[CALIBRATION_CCM_ADJ] = &calibration_ccm_adj;
    calib->calibrations[CALIBRATION_CNR_CTL] = &calibration_cnr_ctl;
    calib->calibrations[CALIBRATION_CNR_ADJ] = &calibration_cnr_adj;
    calib->calibrations[CALIBRATION_PURPLE_CTL] = &calibration_purple_ctl;
    calib->calibrations[CALIBRATION_PURPLE_ADJ] = &calibration_purple_adj;
    calib->calibrations[CALIBRATION_LTM_CTL] = &calibration_ltm_ctl;
    calib->calibrations[CALIBRATION_LTM_LO_HI_GM] = &calibration_ltm_lo_hi_gm;
    calib->calibrations[CALIBRATION_LTM_CONTRAST] = &calibration_ltm_contrast;
    calib->calibrations[CALIBRATION_LTM_SHARP_ADJ] = &calibration_ltm_sharp_adj;
    calib->calibrations[CALIBRATION_LTM_SATUR_LUT] = &calibration_ltm_satur_lut;
    calib->calibrations[CALIBRATION_LC_CTL] = &calibration_lc_ctl;
    calib->calibrations[CALIBRATION_LC_SATUR_LUT] = &calibration_lc_satur_lut;
    calib->calibrations[CALIBRATION_LC_STRENGTH] = &calibration_lc_strength;
    calib->calibrations[CALIBRATION_DNLP_CTL] = &calibration_dnlp_ctl;
    calib->calibrations[CALIBRATION_DNLP_STRENGTH] = &calibration_dnlp_strength;
    calib->calibrations[CALIBRATION_DNLP_SCURV_LOW] = &calibration_dnlp_scurv_low;
    calib->calibrations[CALIBRATION_DNLP_SCURV_MID1] = &calibration_dnlp_scurv_mid1;
    calib->calibrations[CALIBRATION_DNLP_SCURV_MID2] = &calibration_dnlp_scurv_mid2;
    calib->calibrations[CALIBRATION_DNLP_SCURV_HGH1] = &calibration_dnlp_scurv_hgh1;
    calib->calibrations[CALIBRATION_DNLP_SCURV_HGH2] = &calibration_dnlp_scurv_hgh2;
    calib->calibrations[CALIBRATION_DHZ_CTL] = &calibration_dhz_ctl;
    calib->calibrations[CALIBRATION_DHZ_STRENGTH] = &calibration_dhz_strength;
    calib->calibrations[CALIBRATION_PEAKING_CTL] = &calibration_peaking_ctl;
    calib->calibrations[CALIBRATION_PEAKING_ADJUST] = &calibration_peaking_adjust;
    calib->calibrations[CALIBRATION_PEAKING_FLT1_MOTION_ADP_GAIN] = &calibration_peaking_flt1_motion_adp_gain;
    calib->calibrations[CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN] = &calibration_peaking_flt2_motion_adp_gain;
    calib->calibrations[CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT] = &calibration_peaking_gain_vs_luma_lut;
    calib->calibrations[CALIBRATION_PEAKING_CIR_FLT1_GAIN] = &calibration_peaking_cir_flt1_gain;
    calib->calibrations[CALIBRATION_PEAKING_CIR_FLT2_GAIN] = &calibration_peaking_cir_flt2_gain;
    calib->calibrations[CALIBRATION_PEAKING_DRT_FLT2_GAIN] = &calibration_peaking_drt_flt2_gain;
    calib->calibrations[CALIBRATION_PEAKING_DRT_FLT1_GAIN] = &calibration_peaking_drt_flt1_gain;
    calib->calibrations[CALIBRATION_CM_CTL] = &calibration_cm_ctl;
    calib->calibrations[CALIBRATION_CM_Y_VIA_HUE] = &calibration_cm_y_via_hue;
    calib->calibrations[CALIBRATION_CM_SATGLBGAIN_VIA_Y] = &calibration_cm_satglbgain_via_y;
    calib->calibrations[CALIBRATION_CM_SAT_VIA_HS] = &calibration_cm_sat_via_hs;
    calib->calibrations[CALIBRATION_CM_SATGAIN_VIA_Y] = &calibration_cm_satgain_via_y;
    calib->calibrations[CALIBRATION_CM_HUE_VIA_H] = &calibration_cm_hue_via_h;
    calib->calibrations[CALIBRATION_CM_HUE_VIA_S] = &calibration_cm_hue_via_s;
    calib->calibrations[CALIBRATION_CM_HUE_VIA_Y] = &calibration_cm_hue_via_y;
    calib->calibrations[CALIBRATION_HLC_CTL] = &calibration_hlc_ctl;
    calib->calibrations[CALIBRATION_CSC_COEF] = &calibration_csc_coef;
    calib->calibrations[CALIBRATION_VERSION] = &calibration_version;

    calib->calibrations[CALIBRATION_BLACK_LEVEL] = &calibration_black_level;
    calib->calibrations[CALIBRATION_CAC_RX] = &calibration_cac_rx;
    calib->calibrations[CALIBRATION_CAC_RY] = &calibration_cac_ry;
    calib->calibrations[CALIBRATION_CAC_BX] = &calibration_cac_bx;
    calib->calibrations[CALIBRATION_CAC_BY] = &calibration_cac_by;
    calib->calibrations[CALIBRATION_SHADING_RADIAL_R] = &calibration_shading_radial_r;
    calib->calibrations[CALIBRATION_SHADING_RADIAL_G] = &calibration_shading_radial_g;
    calib->calibrations[CALIBRATION_SHADING_RADIAL_B] = &calibration_shading_radial_b;
    calib->calibrations[CALIBRATION_SHADING_LS_D65_R] = &calibration_shading_ls_d65_r;
    calib->calibrations[CALIBRATION_SHADING_LS_D65_G] = &calibration_shading_ls_d65_g;
    calib->calibrations[CALIBRATION_SHADING_LS_D65_B] = &calibration_shading_ls_d65_b;
    calib->calibrations[CALIBRATION_SHADING_LS_CWF_R] = &calibration_shading_ls_cwf_r;
    calib->calibrations[CALIBRATION_SHADING_LS_CWF_G] = &calibration_shading_ls_cwf_g;
    calib->calibrations[CALIBRATION_SHADING_LS_CWF_B] = &calibration_shading_ls_cwf_b;
    calib->calibrations[CALIBRATION_SHADING_LS_TL84_R] = &calibration_shading_ls_tl84_r;
    calib->calibrations[CALIBRATION_SHADING_LS_TL84_G] = &calibration_shading_ls_tl84_g;
    calib->calibrations[CALIBRATION_SHADING_LS_TL84_B] = &calibration_shading_ls_tl84_b;
    calib->calibrations[CALIBRATION_SHADING_LS_A_R] = &calibration_shading_ls_a_r;
    calib->calibrations[CALIBRATION_SHADING_LS_A_G] = &calibration_shading_ls_a_g;
    calib->calibrations[CALIBRATION_SHADING_LS_A_B] = &calibration_shading_ls_a_b;
    calib->calibrations[CALIBRATION_LENS_SHADING_CTL] = &calibration_lens_shading_ctl;
    calib->calibrations[CALIBRATION_LENS_OTP_CENTER_OFFSET] = &calibration_lens_otp_center_offset;
    calib->calibrations[CALIBRATION_GAMMA] = &calibration_gamma;
    calib->calibrations[CALIBRATION_CCM] = &calibration_ccm;
    calib->calibrations[CALIBRATION_AWB_RG_POS] = &calibration_awb_rg_pos;
    calib->calibrations[CALIBRATION_AWB_BG_POS] = &calibration_awb_bg_pos;
    calib->calibrations[CALIBRATION_AWB_MESH_DIST_TAB] = &calibration_awb_mesh_dist_tab;
    calib->calibrations[CALIBRATION_AWB_MESH_CT_TAB] = &calibration_awb_mesh_ct_tab;
    calib->calibrations[CALIBRATION_AWB_CT_RG_CURVE] = &calibration_awb_ct_rg_curve;
    calib->calibrations[CALIBRATION_AWB_CT_BG_CURVE] = &calibration_awb_ct_bg_curve;
    calib->calibrations[CALIBRATION_AWB_WB_GOLDEN_D50] = &calibration_awb_wb_golden_d50;
    calib->calibrations[CALIBRATION_AWB_WB_OTP_D50] = &calibration_awb_wb_otp_d50;
    calib->calibrations[CALIBRATION_NOISE_PROFILE] = &calibration_noise_profile;
    calib->calibrations[CALIBRATION_FPNR] = &calibration_fpnr;
    calib->calibrations[CALIBRATION_AWB_PRESET] = &calibration_awb_preset;

    return 0;
}
}

