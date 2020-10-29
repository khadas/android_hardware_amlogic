/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2019/09/04
 *  @par function description:
 *  - 1 amlogic health@2.0 hal
 */

#ifndef ANDROID_HEALTHSERVICE_MODE_H
#define ANDROID_HEALTHSERVICE_MODE_H


typedef enum {
    STORAGE_PRE_EOL_INFO_NONE                           = 0,
    STORAGE_PRE_EOL_INFO_NORMAL                         = 1,
    STORAGE_PRE_EOL_INFO_WARNING                        = 2,//consumed 80% of reserved block
    STORAGE_PRE_EOL_INFO_URGENT                         = 3
    //0x04-0xff Reserved
} pre_eof_info_type;

typedef enum {
    LIFE_TIME_EST_NONE                                  = 0,// not defined
    LIFE_TIME_EST_10_PERCENT                            = 1,//0%-10% device life time used
    LIFE_TIME_EST_20_PERCENT                            = 2,//10%-20% device life time used
    LIFE_TIME_EST_30_PERCENT                            = 3,//20%-30% device life time used
    LIFE_TIME_EST_40_PERCENT                            = 4,//30%-40% device life time used
    LIFE_TIME_EST_50_PERCENT                            = 5,//40%-50% device life time used
    LIFE_TIME_EST_60_PERCENT                            = 6,//50%-60% device life time used
    LIFE_TIME_EST_70_PERCENT                            = 7,//60%-70% device life time used
    LIFE_TIME_EST_80_PERCENT                            = 8,//70%-80% device life time used
    LIFE_TIME_EST_90_PERCENT                            = 9,//80%-90% device life time used
    LIFE_TIME_EST_A_PERCENT                             = 10,//90%-100% device life time used
    LIFE_TIME_EST_B_PERCENT                             = 11//exceeded its maximum estimated device life time
    //Others Reserved
} life_time_est_type;

typedef enum {
    STORAGE_CSD_VERSION_0                               = 0,// Revision 1.0(for MMC v4.0)
    STORAGE_CSD_VERSION_1                               = 1,// Revision 1.1(for MMC v4.1)
    STORAGE_CSD_VERSION_2                               = 2,// Revision 1.2(for MMC v4.2)
    STORAGE_CSD_VERSION_3                               = 3,// Revision 1.3(for MMC v4.3)
    STORAGE_CSD_VERSION_4                               = 4,// Revision 1.4(Obsolete)
    STORAGE_CSD_VERSION_5                               = 5,// Revision 1.5(for MMC v4.41)
    STORAGE_CSD_VERSION_6                               = 6,// Revision 1.6(for MMC v4.5, v4.51)
    STORAGE_CSD_VERSION_7                               = 7,// Revision 1.7(for MMC v5.0, v5.01)
    STORAGE_CSD_VERSION_8                               = 8,// Revision 1.8(for MMC v5.1)
    //9-255 Reserved
} ext_csd_rev;


#endif // ANDROID_HEALTHSERVICE_MODE_H