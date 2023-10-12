/**
 *  @file
 *
 *  @copyright Copyright (c) 2021 Amlogic, Inc.
 *
 *  This file and its contents ("Software") are protected by intellectual property rights including, without limitation,
 *  China. and/or foreign copyrights.  This Software is also the confidential and proprietary information of Amlogic, Inc.
 *  and its licensors.  You may not use, reproduce, disclose, distribute, modify, or otherwise prepare derivative works
 *  of this Software or any portion thereof except pursuant to a signed license agreement or nondisclosure agreement with
 *  Amlogic, Inc. or its authorized affiliates.  In the absence of such an agreement, you agree to promptly notify and
 *  return this Software to Amlogic, Inc.
 *
 **/
#include "aml_isp_tuning.h"

namespace Imx577SdrCalibration {

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
    0, // lns_rad enable 0:off 1:on
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
static int32_t _CALIBRATION_AWB_CTL[20] = {
    1,       // u8, AWB auto enable
    1,       // u8, AWB manual mode, 0: manual gain mode, 1: manual temperature mode
    32,      //u8, AWB convergence speed.
    0,       //u8, mixed color temperature mode option. 0:mix mode 1:outdoor mode 2:indoor mode 3:auto mode
    32,      //u16, a cover range around planck curve
    1,       //u1, color temperature dynamic cover range enable
    0,       //u1, color temperature luma weighted calculation enable by luma value of local block
    1,       //u1, color temperature luma weighted calculation enable
    0,       //u1, color temperature adjust enable
    1,       //u1, awb delay adjust enable
    10,      //u16, awb delay frame count
    200,     //u16, awb delay adjust tolerance by color temperature
    8,       //u16, awb low luma ratio
    256,     //u16, manual awb mode red gain
    256,     //u16, manual awb mode blue gain
    5000,    //u16, manual awb mode temperature
    0,       //u1, awb stable mode enable
    100,     //u16, awb stable mode ct delta det
    100,     //u16, awb stable mode color delta det
    0,       //bit[0] color temperature hist, [1] weight table, [2] ct table, [3] log
};

//_CALIBRATION_AWB_CT_POS
static uint32_t _CALIBRATION_AWB_CT_POS[20] = {10000,7500,6500,5000,4050,3850,2800,2400,2050};

//_CALIBRATION_AWB_CT_RG_COMPENSATION
static int32_t  _CALIBRATION_AWB_CT_RG_COMPENSATION[20] = {0,0,0,0,0,0,0,0,0,0};

//_CALIBRATION_AWB_CT_BG_COMPENSATION
static int32_t  _CALIBRATION_AWB_CT_BG_COMPENSATION[20] = {0,0,0,0,0,0,0,0,0,0};

//_CALIBRATION_AWB_CT_WGT
static int32_t _CALIBRATION_AWB_CT_WGT[20] = {1,1,2,3,2,1,1,1,1};

//_CALIBRATION_AWB_CT_DYN_CVRANGE
static int32_t _CALIBRATION_AWB_CT_DYN_CVRANGE[2][20] = {
    {-10,-8,-2,16,8,-12,-12,-16,-16},
    {-10,-8,-2,16,8,-12,-12,-16,-16},
};

//aisp_ae_t
static int32_t _CALIBRATION_AE_CTL[31] = {
    1,  //ae auto enable
    0,  //ae exposure mode, 0: none, 1: spot mode 2:center mode 3: upper part mode 4: lower part mode
    0,  //ae exposure strategy, 0: none mode, 1: outdoor mode, 2:indoor mode
    0,  // ae route strategy, 0: exposure priority, 1: gain priority 2: external ae route
    3,  //ae route deflicker mode, 0: none, 1: anti-50hz, 2: anti-60hz, 3: auto detected
    30,   //exposure convergence speed [0, 128]
    128,  //ae global luma target compensation
    186,  //ae luma target srgb curve
    60,   //ae luma wdr target
    0,    //low light enhancement mode, 0: adjust exposure 1: adjust curve
    256,  //[0,256] low light enhancement strength
    4096, //[1024, 1024*(1<<8)]low light gain maximum limit
    16,    // [0,1024] high light reduce trigger threshold
    128,    // [0,1024] high light reduce strength
    10,   //ae tolerance
    1, //ae delay adjust enable
    10, //ae delay frame count
    100, //ae delay adjust tolerance
    (2<<12), //WDR mode only: ae WDR mode low light threshold by log2 value of gain
    77,   //WDR mode only: Max percentage of clipped pixels for long exposure: WDR mode only: 256 = 100% clipped pixels
    15,   //WDR mode only: Time filter for exposure ratio
    0,   //reduce fps feature enable.
    15*256,        //target fps of reduce frame rates.
    (4<<12),   //trigger threshold of the reduce fps, write gain log2 value.
    (1<<10),   //lag threshold of the reduce fps, write gain log2 value.
    (0<<12),        // max isp gain limit, exp: x4 = log2(4)<<12 = 2<<12
    (1000<<12),     // max shutter time limit, exp:  1000ms = 1000<<12
    (28672),       // max total gain limit, exp:x1024 = log2(1024)<<12 = 10<<12, 54db = (54/6)<<12 = 9<<12
    (16<<6),       // max exposure ratio limit, exp: x128 = 128<<6
    2,       //feedback delay frame numbers of stats info in current system
    0,       //ae debug:bit[0] target, [1] ratio, [2] exposure calculate
};

static int32_t _CALIBRATION_AE_CORR_LUT[64] =  {128, 128, 90, 70, 50, 45, 36, 20, 20, 20};

static int32_t _CALIBRATION_AE_CORR_POS_LUT[64] = {41516+(0<<12), 41516+(1<<12), 41516+(2<<12), 41516+(3<<12), 41516+(4<<12), 41516+(5<<12),41516+(6<<12),41516+(7<<12),41516+(8<<12),41516+(9<<12)};

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

//aisp_dn_det_t
static int32_t _CALIBRATION_DAYNIGHT_DETECT[14] = {
    0,    //light_control; 1:0n, 0: off
    0,    // hist_stat_mode; 0: average based AE, 1: weight
    120,  // predict_day_thr;  default is 50
    60,   // predict_night_thr; default is 50
    8,    // dn_det_tran_ratio; default 16/128
    240,  // dn_det_day_thr; default 60
    240,  // dn_det_night_thr;  default 240
    2000, // dn_det_light_ct_low;
    5000, // dn_det_light_ct_high;
    1023, //dn_wdr_mean_ratio
    300, // dn_rg_blk_sum_thr
    400, //dn_rg_thr
    400, //dn_bg_thr
    0, //print_debug 0:not print 1:print
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
    {30,    750,    30,    750,    30,    750,    0,    3,     22,    14,    0,    0,},
    {30,    750,    30,    750,    30,    750,    0,    5,     20,    12,    0,    0,},
    {50,    600,    50,    600,    50,    600,    0,    10,    15,    10,    0,    0,},
    {80,    400,    80,    400,    80,    400,    0,    15,    10,    7,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    20,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    25,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
};

//aisp_dpc_adj_t
static uint16_t _CALIBRATION_DPC_S_ADJ[ISO_NUM_MAX][12] = {
/* avg_gain_l0|avg_gain_h0|avg_gain_l1|avg_gain_h1|avg_gain_l2|avg_gain_h2|cond_en|max_min_bias_thd|std_diff_gain|std_gain|avg_dev_offset|reserve */
    {30,    750,    30,    750,    30,    750,    0,    3,     22,    14,    0,    0,},
    {30,    750,    30,    750,    30,    750,    0,    5,     20,    12,    0,    0,},
    {50,    600,    50,    600,    50,    600,    0,    10,    15,    10,    0,    0,},
    {80,    400,    80,    400,    80,    400,    0,    15,    10,    7,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    20,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    25,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
    {100,    300,    100,    300,    100,    300,    0,    30,    5,    5,    0,    0,},
};

//aisp_wdr_t
static int32_t _CALIBRATION_WDR_CTL[35] = {
    // 1) wdr hr regs
    1,                              // u1, WDR motion detection enable,0: disable, 1: enable,
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
    1,                              // u1, auto enable
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
    { 72,    256,    28,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    { 80,    256,    28,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    { 90,    256,    30,    0,    1600,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    { 90,    256,    30,    0,    1700,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    {100,    256,    32,    0,    1800,1024,    64,4,5,    32,44,48,    12,14,16,20,},
    {100,    256,    32,    0,    1900,1024,    64,7,8,    32,44,48,    12,12,16,18,},
    {100,    256,    32,    0,    2000,1024,    64,7,8,    32,45,48,    10,12,14,16,},
    {100,    256,    32,    4,    2100,2048,    64,7,8,    32,48,60,    8,10,12,16,},
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
    {12,12,10,10,10,16,20,30,},
    {12,12,10,10,14,18,20,30,},
    {14,13,10,10,15,18,20,30,},
    {16,15,11,11,19,22,26,32,},
    {16,16,13,13,22,25,28,32,},
    {16,16,15,14,25,28,32,32,},
    {28,28,24,22,28,32,32,32,},
    {32,32,30,30,31,32,32,32,},
    {32,32,32,32,32,32,32,32,},
    {32,32,32,32,32,32,32,32,},
};

//aisp_snr_t->snr_sad_meta2alp
static uint8_t _CALIBRATION_SNR_SAD_META2ALP[ISO_NUM_MAX][8] = {
    {170,150,120,100,80,42,20,10,},
    {180,150,130,110,90,45,24,12,},
    {190,170,150,120,100,75,40,20,},
    {190,175,160,130,110,85,45,26,},
    {200,190,170,140,115,100,50,26,},
    {210,196,172,150,120,100,62,38,},
    {225,210,190,170,150,130,70,45,},
    {230,220,200,185,170,140,72,50,},
    {250,230,210,190,170,140,72,54,},
    {250,230,210,190,170,140,72,56,},
};

//aisp_snr_t->snr_meta_adj
static uint8_t _CALIBRATION_SNR_META_ADJ[ISO_NUM_MAX][8] = {
    {32,32,32,32,24,10,6,6,},
    {32,32,32,32,26,14,8,8,},
    {32,32,32,32,30,18,10,10,},
    {32,32,32,32,32,20,12,12,},
    {32,32,32,32,32,26,16,16,},
    {36,36,34,34,32,30,20,20,},
    {42,36,34,32,30,30,26,22,},
    {58,50,40,32,30,30,32,32,},
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
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128,},
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 128, 128,},
    {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 128, 128, 128, 128,},
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
    0,    //reg_rad_tnr0_en
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
    {2,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,     0,    0,    10,20,30,    10,20,30,},
    {4,     0,    16,20,24,24,    20,30,40,    20,30,40,    50,4,5,    32,32,32,     0,    0,    10,20,30,    10,20,30,},
    {6,     0,    16,20,24,28,    20,30,42,    20,30,42,    50,4,5,    32,32,32,    10,    0,    10,20,30,    10,20,30,},
    {8,     0,    16,20,30,36,    20,35,45,    20,35,45,    50,4,5,    32,32,32,    10,    0,    10,20,30,    10,20,30,},
    {12,     0,    16,20,30,36,    25,35,45,    25,35,45,    50,4,5,    32,32,32,    12,    0,    10,20,30,    10,20,30,},
    {14,     0,    16,20,33,38,    30,38,48,    30,38,48,    50,4,5,    32,32,32,    14,    0,    10,20,30,    10,20,30,},
    {16,     0,    16,20,33,38,    30,38,48,    30,38,48,    50,4,5,    30,30,32,    14,    0,    10,20,30,    10,20,30,},
    {18,     0,    16,20,33,38,    30,38,48,    30,38,48,    40,4,5,    26,27,32,     24,    0,    10,20,30,    10,20,30,},
    {20,     2,    30,40,45,55,    55,55,60,    50,50,50,    40,4,5,    24,24,32,     24,    0,    10,20,30,    10,20,30,},
    {20,    12,    30,40,45,55,    75,75,75,    50,50,50,    40,4,5,    24,24,32,     24,    0,    10,20,30,    10,20,30,},
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
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 52,52,52,52,52,52,52,52, 55,55,55,55,55,55,55,55,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 55,55,55,55,54,54,54,53, 58,58,58,58,58,58,55,55,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,58,58,58,58,58,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,58,58,58,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,58,58,58,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,60,60,60,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,60,60,60,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,60,60,60,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,60,60,60,},
    {12,10,8,6,6,0,0,0, 32,32,32,31,31,30,30,29, 38,38,38,37,37,36,36,35, 42,42,42,41,41,40,40,39, 46,45,44,43,40,40,40,40, 50,50,50,49,49,48,48,47, 56,56,56,55,55,54,54,53, 60,60,60,60,60,60,60,60,},
};

//aisp_tnr_t->tnr_mc_meta2alpha
static uint8_t _CALIBRATION_MC_META2ALPHA[ISO_NUM_MAX][64] = {
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
    {0,0,0,0,0,0,0,0, 8,7,6,5,5,5,5,5, 11,10,8,7,6,6,6,6, 13,11,11,10,10,8,7,7, 15,13,12,11,10,8,8,8,17, 15,14,13,11,10,10,10, 17,17,15,14,13,12,11,11, 17,17,17,16,15,14,13,12,},
};

//aisp_tnr_t->ptnr_alp_lut
static uint8_t _CALIBRATION_PST_TNR_ALP_LUT[ISO_NUM_MAX][8] = {
    {0,2,8,16,24,32,36,46,},
    {0,2,8,16,24,32,36,48,},
    {0,2,8,16,24,32,36,50,},
    {0,2,8,16,24,32,36,50,},
    {0,2,8,16,24,32,36,50,},
    {0,2,8,16,24,32,36,50,},
    {0,2,8,16,24,32,36,56,},
    {0,2,8,16,24,32,36,60,},
    {0,2,8,16,24,32,36,60,},
    {0,2,8,16,24,32,36,60,},
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
    {256, 256,}, //x32 gain
    {256, 256,}, //x64 gain
    {256, 256,}, //x128 gain
    {256, 256,}, //x256 gain
    {256, 256,}, //x512 gain
};

static int32_t _CALIBRATION_LENS_SHADING_CT_CORRECT[2] = {
/*    TL40 diff           |   CWF color diff */
    0, 0
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
    {128,}, //x1 gain
    {128,}, //x2 gain
    {128,}, //x4 gain
    {128,}, //x8 gain
    {128,}, //x16 gain
    {128,}, //x32 gain
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
    16, //cnr_map_str
};

//aisp_cnr_adj_t
static uint16_t _CALIBRATION_CNR_ADJ[ISO_NUM_MAX][6] = {
/*cnr_weight|umargin_up|umargin_dw|vmargin_up|vmargin_dw| alp_mode*/
    {32,    256,    256,    256,    256,    2,},
    {32,    256,    256,    256,    256,    2,},
    {40,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
    {48,    256,    256,    256,    256,    2,},
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
    {32,    840,    840,    840,    840,    40,    2,},
    {32,    840,    840,    840,    840,    40,    2,},
    {48,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
    {64,    840,    840,    840,    840,    40,    2,},
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
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
    {32, 64,},
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
    {32,    6,    80,      7,},
    {24,    6,    64,      7,},
    {18,    5,    48,      7,},
    {12,    5,    36,      7,},
    {10,    5,    24,      7,},
    {10,    5,    14,      7,},
    { 8,    5,     8,      7,},
    { 8,    5,     8,      7,},
    { 8,    5,     8,      7,},
    { 8,    5,     8,      7,},
};

static uint16_t _CALIBRATION_LTM_SATUR_LUT[63] = {
    11,   12,   13,   83,   90,   98,  106,  115,  125,  136,  148,  161,  175,  191,  208,  227,
    248,  271,  296,  322,  352,  383,  417,  454,  494,  536,  581,  630,  682,  737,  795,  858,
    923,  993, 1066, 1142, 1221, 1304, 1390, 1478, 1569, 1663, 1760, 1858, 1960, 2063, 2168, 2275,
    2384, 2495, 2607, 2721, 2836, 2952, 3069, 3187, 3306, 3426, 3546, 3667, 3788, 3910, 4018
};

//aisp_lc_t
static int32_t _CALIBRATION_LC_CTL[14] = {
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
    90, //u7, lc_single_bin_th, 0-100
    0,  //lc_single_bin_prot_en
    16, //u7, lc_single_bin_prot_strgth, 0 is strongest protection, 64 is without protection, max is 64
};

static int32_t _CALIBRATION_LC_STRENGTH[ISO_NUM_MAX][2] = {
/* lc_ypkbv_slope_lmt_0, lc_ypkbv_slope_lmt_1 */
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
    {48, 38,},
};

static uint16_t _CALIBRATION_LC_SATUR_LUT[63] = {
    12,   36,   55,   83,   90,   98,  106,  115,  125,  136,  148,  161,  175,  191,  208,  227,
    248,  271,  296,  322,  352,  383,  417,  454,  494,  536,  581,  630,  682,  737,  795,  858,
    923,  993, 1066, 1142, 1221, 1304, 1390, 1478, 1569, 1663, 1760, 1858, 1960, 2063, 2168, 2275,
    2384, 2495, 2607, 2721, 2836, 2952, 3069, 3187, 3306, 3426, 3546, 3667, 3788, 3910, 4018
};

//aisp_dnlp_t
static int32_t _CALIBRATION_DNLP_CTL[21] = {
    1,   // dnlp_auto_enable
    5,   // dnlp_cuvbld_min
    15,  // dnlp_cuvbld_max
    0,   // dnlp_clashBgn
    60,  // dnlp_clashEnd
    6,   // dnlp_blkext_ofst
    5,  // dnlp_whtext_ofst
    32,  // dnlp_blkext_rate
    60,  // dnlp_whtext_rate
    1,   // dnlp_dbg_map
    8,  // dnlp_final_gain
    40,  // dnlp_scurv_low_th
    80,  // dnlp_scurv_mid1_th
    130, // dnlp_scurv_mid2_th
    170, // dnlp_scurv_hgh1_th
    220, // dnlp_scurv_hgh2_th
    0,   // dnlp_mtdrate_adp_en
    1,   // dnlp_ble_en
    48,  // dnlp_scn_chg_th
    60, // dnlp_mtdbld_rate
    0,  //dnlp_str_fixed
};

//CALIBRATION_DNLP_STRENGTH, 8bit, normalization: 8 as 1, {ISO100,ISO200,ISO400,ISO800,ISO1600,ISO3200,ISO6400,ISO12800,ISO25600,ISO51200}
static int32_t _CALIBRATION_DNLP_STRENGTH[ISO_NUM_MAX] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

static uint16_t _CALIBRATION_DNLP_SCURV_LOW[65] =
    {0,3,7,11,17,23,30,39,50,63,77,94,112,131,150,170,191,213,235,257,280,303,327,350,374,398,421,444,466,487,508,527,546,565,583,600,617,634,650,666,682,697,712,727,742,756,771,785,799,813,827,841,855,869,883,897,911,925,939,953,967,981,995,1009,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_MID1[65] =
    {0,3,7,11,17,23,30,39,50,63,77,94,112,131,150,170,191,213,235,257,280,303,327,350,374,398,421,444,466,487,508,527,546,565,583,600,617,634,650,666,682,697,712,727,742,756,771,785,799,813,827,841,855,869,883,897,911,925,939,953,967,981,995,1009,1023};

static uint16_t _CALIBRATION_DNLP_SCURV_MID2[65] =
    {0,3,7,11,17,23,30,39,50,63,77,94,112,131,150,170,191,213,235,257,280,303,327,350,374,398,421,444,466,487,508,527,546,565,583,600,617,634,650,666,682,697,712,727,742,756,771,785,799,813,827,841,855,869,883,897,911,925,939,953,967,981,995,1009,1023};

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
    120, -60, 0,
    //pk_flt2_v1d[3]
    120, -60, 0,
    //pk_flt1_h1d[5]
    120, -60, 0, 0, 0,
    //pk_flt2_h1d[5]
    120, -60, 0, 0, 0,
    //pkosht_vsluma_lut[9]
    5, 5, 5, 4, 4, 3, 2, 1, 0,
    //pk_flt1_2d[3][4]
    124,    70,     -29,    0,
    70,     -37,    -16,    0,
    -29,    -16,    -3,     0,
    //pk_flt2_2d[3][4]
    124,    70,     -29,    0,
    70,     -37,    -16,    0,
    -29,    -16,    -3,     0,
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
    {22, 42, 43, 43, 45, 43, 42, 42,},
    {22, 42, 42, 43, 45, 43, 42, 42,},
    {18, 36, 42, 43, 44, 43, 42, 42,},
    {14, 28, 36, 42, 43, 43, 42, 42,},
    {12, 26, 32, 38, 40, 40, 40, 40,},
    {10, 24, 28, 32, 36, 38, 40, 40,},
    {8, 22, 24, 28, 28, 30, 30, 32,},
    {8, 20, 22, 24, 26, 28, 28, 30,},
    {8, 16, 20, 22, 22, 26, 26, 26,},
    {8, 10, 10, 10, 10, 10, 10, 10,},
};

//aisp_sharpen_t->peaking_flt2_gain_adp_motion
static uint8_t _CALIBRATION_PEAKING_FLT2_MOTION_ADP_GAIN[ISO_NUM_MAX][8] = {
    {22, 42, 43, 43, 45, 43, 42, 42,},
    {22, 42, 42, 43, 45, 43, 42, 42,},
    {18, 36, 42, 43, 44, 43, 42, 42,},
    {14, 28, 36, 42, 43, 43, 42, 42,},
    {12, 26, 32, 38, 40, 40, 40, 40,},
    {10, 24, 28, 32, 36, 38, 40, 40,},
    {8, 22, 24, 28, 28, 30, 30, 32,},
    {8, 20, 22, 24, 26, 28, 28, 30,},
    {8, 16, 20, 22, 22, 26, 26, 26,},
    {8, 10, 10, 10, 10, 10, 10, 10,},
};

//aisp_sharpen_t->peaking_gain_adp_luma
static uint8_t _CALIBRATION_PEAKING_GAIN_VS_LUMA_LUT[ISO_NUM_MAX][9] = {
    {4, 5, 6, 6, 7, 7, 6, 5, 3,},
    {4, 5, 5, 6, 7, 6, 5, 5, 3,},
    {3, 4, 5, 5, 5, 6, 5, 5, 2,},
    {3, 4, 4, 5, 5, 5, 6, 5, 2,},
    {2, 3, 3, 4, 4, 4, 4, 4, 2,},
    {2, 3, 3, 4, 4, 4, 4, 3, 2,},
    {0, 2, 3, 4, 4, 4, 4, 3, 2,},
    {0, 1, 2, 4, 4, 4, 4, 2, 2,},
    {0, 1, 2, 4, 4, 4, 4, 2, 2,},
    {0, 1, 2, 4, 4, 4, 4, 2, 2,},
};

//aisp_sharpen_t->peaking_gain_adp_grad1
static uint8_t _CALIBRATION_PEAKING_CIR_FLT1_GAIN[ISO_NUM_MAX][5] = {
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
};

//aisp_sharpen_t->peaking_gain_adp_grad2
static uint8_t _CALIBRATION_PEAKING_CIR_FLT2_GAIN[ISO_NUM_MAX][5] = {
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
    {25, 150, 64, 23, 150,},
};

//aisp_sharpen_t->peaking_gain_adp_grad3
static uint8_t _CALIBRATION_PEAKING_DRT_FLT1_GAIN[ISO_NUM_MAX][5] = {
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
};

//aisp_sharpen_t->peaking_gain_adp_grad4
static uint8_t _CALIBRATION_PEAKING_DRT_FLT2_GAIN[ISO_NUM_MAX][5] = {
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
    {20, 150, 40, 25, 140,},
};

//aisp_cm_ctl_t
static int32_t _CALIBRATION_CM_CTL[ISO_NUM_MAX][4] =
{
/* cm_sat | cm_hue | cm_contrast | cm_brightness */
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
    {512,    0,    1024,    0},
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

static int32_t _CALIBRATION_BLACK_LEVEL[9][5] =
{
{65600,65568,65568,65600,65600,},
{65568,65616,65616,65552,65568,},
{65648,65632,65632,65648,65648,},
{65488,65616,65616,65488,65488,},
{65776,65376,65376,65728,65776,},
{65920,64976,64976,65856,65920,},
{65920,64976,64976,65856,65920,},
{65920,64976,64976,65856,65920,},
{65920,64976,64976,65856,65920,},
};

static uint16_t _CALIBRATION_SHADING_RADIAL_R[129]=
{
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096
};

static uint16_t _CALIBRATION_SHADING_RADIAL_G[129]=
{
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096
};

static uint16_t _CALIBRATION_SHADING_RADIAL_B[129]=
{
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,4096,
4096,4096,4096,4096,4096,4096,4096,4096,4096
};

static uint8_t _CALIBRATION_SHADING_LS_D65_R[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_D65_G[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_D65_B[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_R[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_G[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_CWF_B[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_R[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_G[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_TL84_B[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_A_R[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_A_G[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};

static uint8_t _CALIBRATION_SHADING_LS_A_B[1024]=
{
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
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
2856,410,8130,8101,8068,408,8164,19,7875,553,
4000,389,8102,8149,8070,416,8155,0,8032,416,
4100,466,8025,8150,8070,399,8171,8188,8048,404,
5000,399,8093,8148,8107,397,8136,8190,8030,421,
6500,399,8075,8167,8113,416,8112,8189,8039,412,
7500,411,8083,8147,8119,440,8082,8184,8062,395,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0
};

static int8_t _CALIBRATION_CAC_RX[1024]=
{
-54,-47,-41,-36,-31,-26,-22,-18,-15,-12,-9,-7,-5,-3,-1,1,3,6,8,10,13,16,20,24,28,32,38,43,50,56,64,70,-52,-45,-39,-34,-29,-25,-21,-17,-14,-11,-9,-6,-4,-2,-1,1,3,5,7,10,12,15,18,22,26,31,36,42,48,54,62,68,-50,-43,-38,-32,-28,-23,-19,-16,-13,-10,-8,-6,-4,-2,-1,1,3,5,7,9,11,14,17,21,25,29,34,40,46,52,60,67,-48,-42,-36,-31,-26,-22,-18,-15,-12,-10,-7,-5,-4,-2,0,1,3,4,6,8,10,13,16,20,24,28,33,38,44,51,58,65,-47,-40,-35,-29,-25,-21,-17,-14,-11,-9,-7,-5,-3,-2,0,1,2,4,6,7,10,12,15,18,22,27,31,37,43,49,56,63,-45,-39,-33,-28,-24,-20,-16,-13,-10,-8,-6,-4,-3,-2,0,1,2,4,5,7,9,11,14,17,21,25,30,35,41,48,55,61,-44,-37,-32,-27,-22,-18,-15,-12,-10,-7,-6,-4,-3,-1,0,1,2,3,5,6,8,10,13,16,20,24,29,34,40,46,53,60,-42,-36,-31,-26,-21,-17,-14,-11,-9,-7,-5,-4,-3,-1,0,1,2,3,4,6,8,10,12,15,19,23,28,33,39,45,52,59,-41,-35,-30,-25,-20,-17,-13,-11,-8,-6,-5,-4,-2,-1,0,1,2,3,4,5,7,9,12,15,18,22,27,32,37,44,51,57,-40,-34,-29,-24,-20,-16,-13,-10,-8,-6,-5,-3,-2,-1,0,1,2,3,4,5,7,9,11,14,17,21,26,31,36,43,50,56,-39,-33,-28,-23,-19,-15,-12,-9,-7,-6,-4,-3,-2,-1,0,1,2,3,4,5,6,8,10,13,16,20,25,30,36,42,49,55,-38,-32,-27,-22,-18,-15,-11,-9,-7,-6,-4,-4,-3,-2,0,1,2,3,4,5,6,8,10,13,16,20,24,29,35,41,48,54,-38,-32,-27,-22,-18,-14,-11,-9,-7,-5,-4,-4,-3,-2,-1,1,3,3,4,5,6,7,10,12,15,19,24,29,34,40,47,54,-37,-31,-26,-21,-17,-14,-11,-8,-7,-5,-5,-4,-4,-3,-1,2,3,4,4,5,6,7,9,12,15,19,23,28,34,40,47,53,-37,-31,-26,-21,-17,-13,-11,-8,-7,-5,-5,-5,-5,-4,-2,3,5,5,5,5,6,7,9,12,15,18,23,28,33,39,46,53,-37,-31,-26,-21,-17,-13,-10,-8,-7,-5,-5,-5,-6,-6,-4,6,6,5,5,5,6,7,9,11,15,18,23,28,33,39,46,53,-37,-31,-25,-21,-17,-13,-10,-8,-6,-5,-5,-5,-6,-8,-9,9,7,6,5,5,6,7,9,11,15,18,23,27,33,39,46,53,-37,-31,-26,-21,-17,-13,-10,-8,-7,-5,-5,-5,-6,-6,-3,5,6,5,5,5,6,7,9,11,15,18,23,28,33,39,46,53,-37,-31,-26,-21,-17,-13,-11,-8,-7,-5,-5,-5,-5,-4,-1,3,4,5,5,5,6,7,9,12,15,18,23,28,33,40,46,53,-37,-31,-26,-21,-17,-14,-11,-8,-7,-5,-5,-4,-4,-3,-1,2,3,4,4,5,6,7,9,12,15,19,23,28,34,40,47,53,-38,-32,-27,-22,-18,-14,-11,-9,-7,-5,-4,-4,-3,-2,-1,1,2,3,4,5,6,7,10,12,15,19,24,29,34,40,47,54,-39,-33,-27,-22,-18,-15,-12,-9,-7,-6,-4,-3,-3,-2,0,1,2,3,4,5,6,8,10,13,16,20,24,29,35,41,48,55,-39,-33,-28,-23,-19,-15,-12,-10,-7,-6,-4,-3,-2,-1,0,1,2,3,4,5,6,8,10,13,17,21,25,30,36,42,49,55,-40,-34,-29,-24,-20,-16,-13,-10,-8,-6,-5,-3,-2,-1,0,1,2,3,4,5,7,9,11,14,17,21,26,31,37,43,50,56,-41,-35,-30,-25,-21,-17,-13,-11,-8,-6,-5,-4,-2,-1,0,1,2,3,4,5,7,9,12,15,18,22,27,32,38,44,51,58,-43,-37,-31,-26,-22,-18,-14,-11,-9,-7,-5,-4,-3,-1,0,1,2,3,4,6,8,10,13,16,19,23,28,33,39,45,52,59,-44,-38,-32,-27,-23,-19,-15,-12,-10,-8,-6,-4,-3,-2,0,1,2,3,5,6,8,11,13,17,20,24,29,34,40,46,53,60,-45,-39,-33,-28,-24,-20,-16,-13,-10,-8,-6,-5,-3,-2,0,1,2,4,5,7,9,11,14,18,21,26,30,36,41,48,55,62,-47,-41,-35,-30,-25,-21,-17,-14,-11,-9,-7,-5,-3,-2,0,1,2,4,6,8,10,12,15,19,23,27,32,37,43,49,57,63,-49,-42,-36,-31,-26,-22,-18,-15,-12,-10,-7,-5,-4,-2,0,1,3,4,6,8,11,13,16,20,24,28,33,39,45,51,58,65,-50,-44,-38,-33,-28,-24,-20,-16,-13,-10,-8,-6,-4,-2,-1,1,3,5,7,9,12,14,18,21,25,30,35,40,46,53,60,67,-52,-45,-39,-34,-29,-25,-21,-17,-14,-11,-9,-6,-4,-2,-1,1,3,5,7,10,12,15,18,22,26,31,36,41,48,54,61,68
};

static int8_t _CALIBRATION_CAC_RY[1024]=
{
-45,-42,-40,-38,-36,-34,-32,-30,-29,-27,-26,-25,-25,-24,-24,-24,-24,-25,-26,-27,-28,-29,-31,-32,-34,-36,-39,-41,-43,-46,-49,-51,-41,-38,-36,-34,-32,-30,-28,-26,-25,-24,-23,-22,-21,-21,-21,-21,-21,-21,-22,-23,-24,-25,-27,-29,-30,-32,-35,-37,-39,-42,-44,-46,-37,-34,-32,-30,-28,-26,-25,-23,-22,-20,-19,-19,-18,-18,-17,-17,-18,-18,-19,-20,-21,-22,-24,-25,-27,-29,-31,-33,-35,-37,-40,-42,-33,-31,-29,-27,-25,-23,-21,-20,-19,-17,-16,-16,-15,-15,-15,-15,-15,-15,-16,-17,-18,-19,-20,-22,-24,-25,-27,-29,-31,-34,-36,-38,-29,-27,-25,-23,-22,-20,-18,-17,-16,-15,-14,-13,-13,-12,-12,-12,-12,-13,-13,-14,-15,-16,-18,-19,-21,-22,-24,-26,-28,-30,-32,-34,-26,-24,-22,-20,-19,-17,-16,-15,-13,-12,-12,-11,-11,-10,-10,-10,-10,-11,-11,-12,-13,-14,-15,-16,-18,-19,-21,-23,-25,-27,-29,-30,-23,-21,-19,-18,-16,-15,-14,-12,-11,-10,-10,-9,-9,-9,-8,-8,-9,-9,-9,-10,-11,-12,-13,-14,-15,-17,-18,-20,-22,-23,-25,-27,-20,-18,-17,-15,-14,-13,-11,-10,-9,-9,-8,-8,-7,-7,-7,-7,-7,-7,-8,-8,-9,-10,-11,-12,-13,-14,-16,-17,-19,-21,-22,-24,-17,-16,-14,-13,-12,-11,-10,-9,-8,-7,-7,-6,-6,-6,-6,-6,-6,-6,-6,-7,-7,-8,-9,-10,-11,-12,-14,-15,-16,-18,-19,-21,-15,-13,-12,-11,-10,-9,-8,-7,-6,-6,-6,-5,-5,-5,-5,-5,-5,-5,-5,-6,-6,-7,-7,-8,-9,-10,-11,-13,-14,-15,-16,-18,-12,-11,-10,-9,-8,-7,-6,-6,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-5,-6,-7,-8,-8,-9,-10,-12,-13,-14,-15,-10,-9,-8,-7,-6,-6,-5,-5,-4,-4,-4,-4,-4,-5,-5,-5,-5,-4,-4,-4,-4,-4,-5,-5,-6,-7,-8,-8,-9,-10,-11,-12,-8,-7,-6,-6,-5,-4,-4,-3,-3,-3,-3,-3,-4,-5,-5,-5,-5,-4,-3,-3,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-9,-10,-6,-5,-5,-4,-4,-3,-3,-3,-2,-2,-2,-3,-4,-5,-6,-6,-4,-3,-3,-2,-2,-2,-3,-3,-3,-4,-4,-5,-5,-6,-6,-7,-4,-3,-3,-3,-2,-2,-2,-2,-1,-1,-2,-2,-3,-5,-7,-6,-4,-3,-2,-2,-1,-2,-2,-2,-2,-2,-3,-3,-3,-4,-4,-5,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-3,-8,-6,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,1,1,1,1,1,1,1,1,2,4,8,6,3,2,1,1,1,1,1,1,1,1,2,2,2,2,2,3,4,4,3,3,3,2,2,2,2,2,2,2,3,5,7,6,4,3,2,2,2,2,2,2,2,3,3,3,4,4,5,5,6,6,5,4,4,3,3,3,2,2,3,3,4,5,6,6,5,3,3,2,2,3,3,3,4,4,5,5,6,6,7,8,8,7,7,6,5,5,4,4,3,3,3,4,4,5,5,5,5,4,3,3,3,3,4,4,5,6,6,7,8,9,9,10,10,9,9,8,7,6,5,5,4,4,4,4,4,5,5,5,5,4,4,4,4,4,5,6,6,7,8,9,10,11,12,13,13,12,11,9,8,8,7,6,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,7,8,9,10,11,12,13,14,15,15,14,13,11,10,9,8,7,7,6,6,6,5,5,6,6,5,5,6,6,6,7,8,9,10,11,12,13,14,16,17,18,18,16,15,14,12,11,10,9,8,7,7,7,6,6,6,6,6,6,7,7,8,8,9,10,12,13,14,15,17,18,20,21,21,19,17,16,14,13,12,11,10,9,8,8,8,7,7,7,7,8,8,9,9,10,11,12,14,15,16,18,20,21,23,24,23,22,20,18,17,15,14,13,12,11,10,10,9,9,9,9,9,9,10,10,11,12,13,15,16,17,19,21,22,24,26,28,27,25,23,21,19,18,16,15,14,13,12,11,11,11,11,11,11,11,12,12,13,14,16,17,18,20,22,24,25,27,29,31,30,28,26,24,22,21,19,18,16,15,14,14,13,13,13,13,13,13,14,15,16,17,18,20,21,23,25,27,29,31,33,35,34,31,29,27,25,24,22,21,19,18,17,16,16,15,15,15,16,16,17,17,18,20,21,23,24,26,28,30,32,34,37,39,37,35,33,31,29,27,25,24,22,21,20,19,19,18,18,18,18,19,20,20,22,23,24,26,28,30,32,34,36,38,41,43,40,38,36,33,31,29,28,26,25,23,22,22,21,20,20,20,21,21,22,23,24,25,27,28,30,32,34,37,39,41,44,46
};

static int8_t _CALIBRATION_CAC_BX[1024]=
{
-24,-24,-24,-24,-23,-22,-21,-20,-18,-16,-15,-13,-10,-8,-6,-3,-1,2,4,6,9,11,13,15,17,19,20,21,22,23,24,24,-25,-25,-25,-24,-24,-23,-22,-20,-19,-17,-15,-13,-11,-9,-6,-4,-1,2,4,7,9,11,14,16,18,19,21,22,23,24,25,25,-26,-26,-26,-25,-24,-24,-22,-21,-19,-18,-16,-14,-11,-9,-6,-4,-1,2,4,7,9,12,14,16,18,20,21,23,24,25,25,26,-27,-27,-27,-26,-25,-24,-23,-22,-20,-18,-16,-14,-12,-9,-7,-4,-1,2,4,7,10,12,15,17,19,20,22,23,25,25,26,27,-28,-28,-27,-27,-26,-25,-24,-22,-21,-19,-17,-14,-12,-9,-7,-4,-1,2,5,7,10,13,15,17,19,21,23,24,25,26,27,27,-28,-28,-28,-27,-27,-26,-24,-23,-21,-19,-17,-15,-12,-10,-7,-4,-1,2,5,8,10,13,16,18,20,22,23,25,26,27,28,28,-29,-29,-29,-28,-27,-26,-25,-24,-22,-20,-18,-15,-13,-10,-7,-4,-1,2,5,8,11,13,16,18,20,22,24,25,27,28,28,29,-30,-30,-29,-29,-28,-27,-26,-24,-22,-20,-18,-16,-13,-10,-7,-4,-1,2,5,8,11,14,16,19,21,23,25,26,27,28,29,29,-30,-30,-30,-29,-28,-27,-26,-25,-23,-21,-19,-16,-14,-11,-8,-5,-1,2,5,8,11,14,17,19,21,23,25,27,28,29,29,30,-31,-31,-30,-30,-29,-28,-27,-25,-23,-21,-19,-17,-14,-11,-8,-5,-1,2,6,9,12,15,17,20,22,24,26,27,28,29,30,30,-31,-31,-31,-30,-29,-28,-27,-26,-24,-22,-20,-17,-14,-11,-8,-5,-1,2,6,9,12,15,18,20,22,24,26,27,29,30,30,31,-32,-32,-31,-31,-30,-29,-28,-26,-24,-22,-20,-17,-15,-12,-8,-5,-1,2,6,9,12,15,18,21,23,25,26,28,29,30,31,31,-32,-32,-32,-31,-30,-29,-28,-26,-25,-23,-20,-18,-15,-12,-9,-5,-1,2,6,10,13,16,18,21,23,25,27,28,29,30,31,32,-32,-32,-32,-31,-31,-29,-28,-27,-25,-23,-21,-18,-15,-12,-9,-5,-2,3,6,10,13,16,19,21,23,25,27,29,30,31,31,32,-33,-32,-32,-32,-31,-30,-28,-27,-25,-23,-21,-18,-16,-13,-9,-6,-2,3,7,10,13,16,19,21,24,26,27,29,30,31,32,32,-33,-33,-32,-32,-31,-30,-29,-27,-25,-23,-21,-19,-16,-13,-10,-6,-2,3,7,10,14,17,19,22,24,26,27,29,30,31,32,32,-33,-33,-32,-32,-31,-30,-29,-27,-25,-23,-21,-19,-16,-13,-10,-6,-2,3,7,11,14,17,19,22,24,26,28,29,30,31,32,32,-33,-33,-32,-32,-31,-30,-29,-27,-25,-23,-21,-19,-16,-13,-10,-6,-2,3,7,11,14,17,19,22,24,26,28,29,30,31,32,32,-33,-33,-32,-32,-31,-30,-29,-27,-25,-23,-21,-19,-16,-13,-10,-6,-2,3,7,10,14,17,19,22,24,26,27,29,30,31,32,32,-33,-32,-32,-32,-31,-30,-28,-27,-25,-23,-21,-18,-16,-13,-9,-6,-2,3,7,10,13,16,19,21,24,26,27,29,30,31,32,32,-32,-32,-32,-31,-31,-29,-28,-27,-25,-23,-21,-18,-15,-12,-9,-5,-2,3,6,10,13,16,19,21,23,25,27,29,30,31,31,32,-32,-32,-32,-31,-30,-29,-28,-26,-25,-23,-20,-18,-15,-12,-9,-5,-1,2,6,10,13,16,18,21,23,25,27,28,29,30,31,32,-32,-32,-31,-31,-30,-29,-28,-26,-24,-22,-20,-17,-15,-12,-8,-5,-1,2,6,9,12,15,18,21,23,25,26,28,29,30,31,31,-31,-31,-31,-30,-29,-28,-27,-26,-24,-22,-20,-17,-14,-11,-8,-5,-1,2,6,9,12,15,18,20,22,24,26,27,29,30,30,31,-31,-31,-30,-30,-29,-28,-27,-25,-23,-21,-19,-17,-14,-11,-8,-5,-1,2,6,9,12,15,17,20,22,24,26,27,28,29,30,30,-30,-30,-30,-29,-28,-27,-26,-25,-23,-21,-19,-16,-14,-11,-8,-5,-1,2,5,8,11,14,17,19,21,23,25,27,28,29,29,30,-30,-30,-29,-29,-28,-27,-26,-24,-22,-20,-18,-16,-13,-10,-7,-4,-1,2,5,8,11,14,16,19,21,23,25,26,27,28,29,29,-29,-29,-29,-28,-27,-26,-25,-24,-22,-20,-18,-15,-13,-10,-7,-4,-1,2,5,8,11,13,16,18,20,22,24,25,27,28,28,29,-28,-28,-28,-27,-27,-26,-24,-23,-21,-19,-17,-15,-12,-10,-7,-4,-1,2,5,8,10,13,16,18,20,22,23,25,26,27,28,28,-28,-28,-27,-27,-26,-25,-24,-22,-21,-19,-17,-14,-12,-9,-7,-4,-1,2,5,7,10,13,15,17,19,21,23,24,25,26,27,27,-27,-27,-27,-26,-25,-24,-23,-22,-20,-18,-16,-14,-12,-9,-7,-4,-1,2,4,7,10,12,15,17,19,20,22,23,25,25,26,27,-26,-26,-26,-25,-25,-24,-23,-21,-20,-18,-16,-14,-11,-9,-6,-4,-1,2,4,7,10,12,14,16,18,20,22,23,24,25,26,26
};

static int8_t _CALIBRATION_CAC_BY[1024]=
{
-18,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-29,-30,-30,-31,-31,-31,-31,-31,-31,-30,-30,-29,-28,-28,-27,-26,-25,-24,-23,-22,-21,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-29,-30,-30,-30,-30,-30,-30,-30,-29,-29,-28,-28,-27,-26,-25,-24,-23,-22,-21,-20,-17,-19,-20,-21,-22,-23,-24,-25,-25,-26,-27,-28,-28,-29,-29,-29,-29,-29,-29,-29,-29,-28,-27,-27,-26,-25,-24,-23,-22,-21,-20,-19,-17,-18,-19,-20,-21,-22,-23,-24,-24,-25,-26,-27,-27,-28,-28,-28,-28,-28,-28,-28,-28,-27,-26,-26,-25,-24,-23,-22,-22,-21,-20,-19,-16,-17,-18,-19,-20,-21,-22,-23,-23,-24,-25,-25,-26,-27,-27,-27,-27,-27,-27,-27,-26,-26,-25,-25,-24,-23,-22,-21,-21,-20,-19,-18,-15,-16,-17,-18,-19,-20,-20,-21,-22,-23,-24,-24,-25,-25,-26,-26,-26,-26,-26,-25,-25,-25,-24,-23,-23,-22,-21,-20,-19,-19,-18,-17,-14,-15,-16,-17,-18,-18,-19,-20,-21,-21,-22,-23,-23,-24,-24,-24,-25,-24,-24,-24,-24,-23,-23,-22,-21,-21,-20,-19,-18,-17,-16,-16,-13,-14,-15,-15,-16,-17,-18,-18,-19,-20,-21,-21,-22,-22,-23,-23,-23,-23,-23,-22,-22,-22,-21,-20,-20,-19,-18,-18,-17,-16,-15,-15,-12,-13,-13,-14,-15,-16,-16,-17,-18,-18,-19,-19,-20,-20,-21,-21,-21,-21,-21,-21,-20,-20,-19,-19,-18,-17,-17,-16,-15,-15,-14,-13,-11,-11,-12,-13,-13,-14,-15,-15,-16,-16,-17,-18,-18,-19,-19,-19,-19,-19,-19,-19,-18,-18,-17,-17,-16,-16,-15,-14,-14,-13,-12,-12,-9,-10,-11,-11,-12,-12,-13,-13,-14,-15,-15,-16,-16,-17,-17,-17,-17,-17,-17,-17,-16,-16,-16,-15,-14,-14,-13,-13,-12,-12,-11,-10,-8,-9,-9,-10,-10,-11,-11,-12,-12,-13,-13,-14,-14,-14,-15,-15,-15,-15,-15,-15,-14,-14,-13,-13,-12,-12,-11,-11,-10,-10,-9,-9,-7,-7,-7,-8,-8,-9,-9,-10,-10,-10,-11,-11,-12,-12,-13,-13,-13,-13,-13,-12,-12,-12,-11,-11,-10,-10,-9,-9,-9,-8,-8,-7,-5,-6,-6,-6,-7,-7,-7,-8,-8,-8,-9,-9,-9,-10,-10,-10,-11,-11,-10,-10,-10,-9,-9,-8,-8,-8,-7,-7,-7,-6,-6,-6,-4,-4,-4,-4,-5,-5,-5,-5,-6,-6,-6,-6,-7,-7,-7,-8,-8,-8,-8,-7,-7,-7,-6,-6,-6,-6,-5,-5,-5,-5,-4,-4,-2,-2,-3,-3,-3,-3,-3,-3,-3,-4,-4,-4,-4,-4,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-4,-3,-3,-3,-3,-3,-3,-3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,2,2,3,3,3,3,3,3,3,4,4,4,4,4,5,5,5,5,5,4,4,4,4,4,4,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5,6,6,6,6,7,7,7,8,8,8,8,7,7,7,6,6,6,6,5,5,5,5,4,4,5,6,6,6,7,7,7,8,8,8,9,9,9,10,10,10,11,11,10,10,10,9,9,8,8,8,7,7,7,6,6,6,7,7,7,8,8,9,9,10,10,10,11,11,12,12,13,13,13,13,13,12,12,12,11,11,10,10,9,9,9,8,8,7,8,9,9,10,10,11,11,12,12,13,13,14,14,14,15,15,15,15,15,15,14,14,13,13,12,12,11,11,10,10,9,9,9,10,11,11,12,12,13,13,14,15,15,16,16,17,17,17,17,17,17,17,16,16,15,15,14,14,13,13,12,12,11,10,11,11,12,13,13,14,15,15,16,16,17,18,18,19,19,19,19,19,19,19,18,18,17,17,16,16,15,14,14,13,12,12,12,13,13,14,15,15,16,17,18,18,19,19,20,20,21,21,21,21,21,21,20,20,19,19,18,17,17,16,15,15,14,13,13,14,15,15,16,17,18,18,19,20,21,21,22,22,23,23,23,23,23,22,22,22,21,20,20,19,18,18,17,16,15,15,14,15,16,17,18,18,19,20,21,21,22,23,23,24,24,24,24,24,24,24,24,23,23,22,21,21,20,19,18,17,16,16,15,16,17,18,19,20,20,21,22,23,24,24,25,25,26,26,26,26,26,25,25,25,24,23,23,22,21,20,19,18,18,17,16,17,18,19,20,21,22,23,23,24,25,25,26,26,27,27,27,27,27,27,26,26,25,25,24,23,22,21,21,20,19,18,17,18,19,20,21,22,23,24,24,25,26,27,27,28,28,28,28,28,28,28,28,27,26,26,25,24,23,22,22,21,20,19,17,18,19,20,21,22,23,24,25,26,27,27,28,28,29,29,29,29,29,29,28,28,27,27,26,25,24,23,22,21,20,19
};

static int16_t _CALIBRATION_AWB_RG_POS[15]=
{
1401,1607,1812,2018,2223,2429,2634,2840,3046,3251,3457,3662,3868,4074,4279
};

static int16_t _CALIBRATION_AWB_BG_POS[15]=
{
884,1056,1227,1398,1570,1741,1912,2084,2255,2426,2598,2769,2940,3112,3283
};

static int16_t _CALIBRATION_AWB_MESH_DIST_TAB[15][15] =
{
{-160,-144,-129,-115,-101,-87,-75,-63,-53,-43,-33,-25,-17,-10,-3,},
{-146,-130,-114,-99,-85,-71,-58,-46,-35,-25,-15,-7,1,8,15,},
{-132,-116,-99,-84,-69,-55,-42,-29,-18,-7,2,11,19,27,34,},
{-119,-102,-85,-69,-54,-39,-25,-12,-1,10,20,29,38,45,53,},
{-106,-88,-71,-55,-39,-24,-9,3,16,27,38,47,56,64,71,},
{-94,-75,-58,-41,-24,-8,6,19,32,44,55,65,74,83,90,},
{-82,-63,-45,-27,-10,5,21,35,49,61,73,83,93,101,109,},
{-70,-51,-32,-14,3,20,36,51,65,78,90,101,111,120,127,},
{-59,-40,-20,-1,16,33,50,66,81,95,107,119,129,138,146,},
{-49,-29,-9,9,28,46,64,81,96,111,124,136,147,156,165,},
{-39,-19,1,21,40,59,77,95,111,126,140,153,164,174,183,},
{-30,-9,11,31,51,71,90,108,125,141,156,170,182,192,201,},
{-21,-1,20,41,61,81,101,120,138,156,172,186,199,210,220,},
{-13,8,29,50,71,92,112,132,151,169,186,202,216,227,238,},
{-6,16,37,58,80,101,122,142,162,181,200,217,232,245,256,},
};

static int16_t _CALIBRATION_AWB_MESH_CT_TAB[15][15] =
{
{4068,3846,3676,3578,3219,3034,2660,2680,2576,2462,2346,2261,2177,2115,2107,},
{4507,4011,3830,3705,3645,3238,3062,2683,2712,2596,2469,2345,2244,2160,2123,},
{4774,4464,3963,3819,3745,3444,3261,3082,2715,2737,2604,2453,2324,2234,2178,},
{5211,4707,4066,3929,3821,3796,3461,3280,3103,2733,2693,2542,2409,2301,2230,},
{5398,4981,4659,4004,3910,3838,3860,3466,3245,3021,2708,2626,2486,2374,2283,},
{5748,5306,4908,4609,3956,3898,3854,3836,3379,3154,2934,2675,2562,2431,2320,},
{6090,5640,5220,4834,4569,3931,3886,3813,3755,3294,3061,2653,2631,2500,2384,},
{6488,5987,5541,5181,4779,4527,3947,3860,3761,3662,3201,2973,2625,2570,2442,},
{6936,6347,5877,5427,5199,4739,4485,3947,3820,3695,3354,3112,2882,2589,2505,},
{7415,6806,6235,5786,5391,5226,4701,4059,3937,3772,3617,3266,3018,2792,2547,},
{8128,7258,6679,6154,5754,5355,4977,4662,4078,3916,3716,3527,3177,2926,2708,},
{8942,7937,7156,6614,6126,5722,5323,4934,4610,4086,3878,3651,3425,3084,2839,},
{9556,8833,7768,7105,6559,6090,5673,5279,4890,4560,4081,3835,3571,3310,2989,},
{9990,9523,8638,7637,7064,6521,6058,5647,5257,4853,4500,4056,3775,3481,3001,},
{10007,9983,9386,8457,7543,6995,6472,6034,5590,5234,4796,4436,4026,3465,2974,},
};

//_CALIBRATION_AWB_CT_RG_CURVE
static int32_t _CALIBRATION_AWB_CT_RG_CURVE[4] = {5793,-1085,68,0};

//_CALIBRATION_AWB_CT_BG_CURVE
static int32_t _CALIBRATION_AWB_CT_BG_CURVE[4] = {-12,553,-23,0};

//CALIBRATION_AWB_WB_GOLDEN_D50
static int16_t _CALIBRATION_AWB_WB_GOLDEN_D50[2] = {2134,2150};

//CALIBRATION_AWB_WB_OTP_D50
static int16_t _CALIBRATION_AWB_WB_OTP_D50[2] = {2134,2150};

//Noise reduce calibration parameters
static uint16_t _CALIBRATION_NOISE_PROFILE[9][16] =
{
{1,12,21,27,28,31,32,33,33,33,32,30,27,24,20,15,},
{0,16,29,38,40,43,45,46,45,44,42,38,33,27,21,12,},
{0,23,42,54,56,60,62,63,62,60,56,50,43,35,25,14,},
{2,35,60,77,81,85,87,87,85,82,77,70,61,50,38,24,},
{9,55,89,107,115,120,122,122,120,115,108,98,86,71,54,35,},
{24,72,107,130,139,143,145,144,140,135,127,116,104,88,71,51,},
{24,72,107,130,139,143,145,144,140,135,127,116,104,88,71,51,},
{24,72,107,130,139,143,145,144,140,135,127,116,104,88,71,51,},
{24,72,107,130,139,143,145,144,140,135,127,116,104,88,71,51,},
};

static uint8_t _CALIBRATION_FPNR[2048*2*5] = {0};

//aisp_awb_info_t
static uint32_t _CALIBRATION_AWB_PRESET[6] =
{
    0,
    457,    //awb_sys_r_gain;
    256,    //awb_sys_g_gain;
    436,    //awb_sys_b_gain;
    5563,   //awb_sys_ct;
    20,     //awb_sys_cdiff;
};

static LookupTable calibration_top_ctl = {.ptr = _CALIBRATION_TOP_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_TOP_CTL ) / sizeof( _CALIBRATION_TOP_CTL[0] ), .width = sizeof( _CALIBRATION_TOP_CTL[0] )};
static LookupTable calibration_awb_ctl = {.ptr = _CALIBRATION_AWB_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CTL ) / sizeof( _CALIBRATION_AWB_CTL[0] ), .width = sizeof( _CALIBRATION_AWB_CTL[0] )};
static LookupTable calibration_res_ctl = {.ptr = _CALIBRATION_RES_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_RES_CTL ) / sizeof( _CALIBRATION_RES_CTL[0] ), .width = sizeof( _CALIBRATION_RES_CTL[0] )};
static LookupTable calibration_awb_ct_pos = { .ptr = _CALIBRATION_AWB_CT_POS, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_CT_POS) / sizeof(_CALIBRATION_AWB_CT_POS[0]), .width = sizeof(_CALIBRATION_AWB_CT_POS[0] ) };
static LookupTable calibration_awb_ct_rg_compensation = { .ptr = _CALIBRATION_AWB_CT_RG_COMPENSATION, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATION ) / sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATION[0] ), .width = sizeof( _CALIBRATION_AWB_CT_RG_COMPENSATION[0] )};
static LookupTable calibration_awb_ct_bg_compensation = { .ptr = _CALIBRATION_AWB_CT_BG_COMPENSATION, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATION) / sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATION[0]), .width = sizeof(_CALIBRATION_AWB_CT_BG_COMPENSATION[0] ) };
static LookupTable calibration_awb_ct_wgt = { .ptr = _CALIBRATION_AWB_CT_WGT, .rows = 1, .cols = sizeof( _CALIBRATION_AWB_CT_WGT ) / sizeof( _CALIBRATION_AWB_CT_WGT[0] ), .width = sizeof( _CALIBRATION_AWB_CT_WGT[0] )};
static LookupTable calibration_awb_ct_dyn_cvrange = { .ptr = _CALIBRATION_AWB_CT_DYN_CVRANGE, .rows = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE) / sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0]), .cols = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0]) / sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0][0]), .width = sizeof(_CALIBRATION_AWB_CT_DYN_CVRANGE[0][0] ) };
static LookupTable calibration_ae_ctl = {.ptr = _CALIBRATION_AE_CTL, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CTL ) / sizeof( _CALIBRATION_AE_CTL[0] ), .width = sizeof( _CALIBRATION_AE_CTL[0] )};
static LookupTable calibration_ae_corr_lut = {.ptr = _CALIBRATION_AE_CORR_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CORR_LUT ) / sizeof( _CALIBRATION_AE_CORR_LUT[0] ), .width = sizeof( _CALIBRATION_AE_CORR_LUT[0] )};
static LookupTable calibration_ae_corr_pos_lut = {.ptr = _CALIBRATION_AE_CORR_POS_LUT, .rows = 1, .cols = sizeof( _CALIBRATION_AE_CORR_POS_LUT ) / sizeof( _CALIBRATION_AE_CORR_POS_LUT[0] ), .width = sizeof( _CALIBRATION_AE_CORR_POS_LUT[0] )};
static LookupTable calibration_ae_route = {.ptr = _CALIBRATION_AE_ROUTE, .rows = 1, .cols = sizeof( _CALIBRATION_AE_ROUTE ) / sizeof( _CALIBRATION_AE_ROUTE[0] ), .width = sizeof( _CALIBRATION_AE_ROUTE[0] )};
static LookupTable calibration_ae_weight_h = {.ptr = _CALIBRATION_AE_WEIGHT_H, .rows = 1, .cols = sizeof( _CALIBRATION_AE_WEIGHT_H ) / sizeof( _CALIBRATION_AE_WEIGHT_H[0] ), .width = sizeof( _CALIBRATION_AE_WEIGHT_H[0] )};
static LookupTable calibration_ae_weight_v = {.ptr = _CALIBRATION_AE_WEIGHT_V, .rows = 1, .cols = sizeof( _CALIBRATION_AE_WEIGHT_V ) / sizeof( _CALIBRATION_AE_WEIGHT_V[0] ), .width = sizeof( _CALIBRATION_AE_WEIGHT_V[0] )};
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
static LookupTable calibration_fpnr = { .ptr = _CALIBRATION_FPNR, .rows = 1, .cols = sizeof(_CALIBRATION_FPNR) / sizeof(_CALIBRATION_FPNR[0]), .width = sizeof(_CALIBRATION_FPNR[0] ) };
static LookupTable calibration_awb_preset = { .ptr = _CALIBRATION_AWB_PRESET, .rows = 1, .cols = sizeof(_CALIBRATION_AWB_PRESET) / sizeof(_CALIBRATION_AWB_PRESET[0]), .width = sizeof(_CALIBRATION_AWB_PRESET[0] ) };

int dynamic_sdr_calibrations_init_imx577(aisp_calib_info_t *calib)
{
	calib->calibrations[CALIBRATION_TOP_CTL] = &calibration_top_ctl;
	calib->calibrations[CALIBRATION_RES_CTL] = &calibration_res_ctl;
	calib->calibrations[CALIBRATION_AWB_CTL] = &calibration_awb_ctl;
	calib->calibrations[CALIBRATION_AWB_CT_POS] = &calibration_awb_ct_pos;
	calib->calibrations[CALIBRATION_AWB_CT_RG_COMPENSATE] = &calibration_awb_ct_rg_compensation;
	calib->calibrations[CALIBRATION_AWB_CT_BG_COMPENSATE] = &calibration_awb_ct_bg_compensation;
	calib->calibrations[CALIBRATION_AWB_CT_WGT] = &calibration_awb_ct_wgt;
	calib->calibrations[CALIBRATION_AWB_CT_DYN_CVRANGE] = &calibration_awb_ct_dyn_cvrange;
	calib->calibrations[CALIBRATION_AE_CTL] = &calibration_ae_ctl;
	calib->calibrations[CALIBRATION_AE_CORR_POS_LUT] = &calibration_ae_corr_pos_lut;
	calib->calibrations[CALIBRATION_AE_CORR_LUT] = &calibration_ae_corr_lut;
	calib->calibrations[CALIBRATION_AE_ROUTE] = &calibration_ae_route;
	calib->calibrations[CALIBRATION_AE_WEIGHT_H] = &calibration_ae_weight_h;
	calib->calibrations[CALIBRATION_AE_WEIGHT_V] = &calibration_ae_weight_v;
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

