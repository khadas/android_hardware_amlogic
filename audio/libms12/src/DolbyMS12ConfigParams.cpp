/*
 * Copyright (C) 2017 Amlogic Corporation.
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
 */

#define LOG_TAG "libms12"
// #define LOG_NDEBUG 0
// #define LOG_NALOGV 0

#include <utils/Log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <system/audio.h>
#include <utils/String8.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>

//#include <media/AudioSystem.h>

#include "DolbyMS12ConfigParams.h"


namespace android
{

#define MAX_ARGC 100
#define MAX_ARGV_STRING_LEN 1024

//here the file path is fake
//@@pcm [main pcm sounds]
#define DEFAULT_MAIN_PCM_FILE_NAME "/data/main48000Hz.wav"
//@@pcm [application sounds]
#define DEFAULT_APPLICATION_PCM_FILE_NAME "/data/app48khz.wav"
//@@pcm [system sounds]
#define DEFAULT_SYSTEM_PCM_FILE_NAME "/data/system48kHz.wav"
//@@pcm [ott sounds]
#define DEFAULT_OTT_PCM_FILE_NAME "/data/ott48kHz.wav"
//@@HE-AAC input file
#define DEFAULT_MAIN_HEAAC_V1_FILE_NAME "/data/main.loas"
#define DEFAULT_ASSOCIATE_HEAAC_V1_FILE_NAME "/data/associate.loas"
#define DEFAULT_MAIN_HEAAC_V2_FILE_NAME "/data/main.adts"
#define DEFAULT_ASSOCIATE_HEAAC_V2_FILE_NAME "/data/associate.adts"

//@@@DDPlus input file
#define DEFAULT_MAIN_DDP_FILE_NAME "/data/main.ac3"
#define DEFAULT_ASSOCIATE_DDP_FILE_NAME "/data/associate.ac3"

#define DEFAULT_OUTPUT_PCM_MULTI_FILE_NAME "/data/outputmulti.wav"
#define DEFAULT_OUTPUT_PCM_DOWNMIX_FILE_NAME "/data/outputdownmix.wav"
#define DEFAULT_OUTPUT_DAP_FILE_NAME "/data/outputdap.wav"
#define DEFAULT_OUTPUT_DD_FILE_NAME "/data/output.ac3"
#define DEFAULT_OUTPUT_DDP_FILE_NAME "/data/output.ec3"
#define DEFAULT_OUTPUT_MAT_FILE_NAME "/data/output.mat"
#define DEFAULT_SOUNDS_CHANNEL_CONFIGURATION 2//means 2/0 (L, R)

//DRC Mode
#define DDPI_UDC_COMP_LINE 2
#define DRC_MODE_BIT  0
#define DRC_HIGH_CUT_BIT 3
#define DRC_LOW_BST_BIT 16

#define MIN_USER_CONTROL_VALUES (-32)
#define MAX_USER_CONTROL_VALUES (32)

DolbyMS12ConfigParams::DolbyMS12ConfigParams():
    // mDolbyMS12GetOutProfile(NULL)
    // ,
    mParamNum(0)
    , mAudioOutFlags(AUDIO_OUTPUT_FLAG_NONE)
    , mAudioStreamOutFormat(AUDIO_FORMAT_PCM_16_BIT)
    , mAudioStreamOutChannelMask(AUDIO_CHANNEL_OUT_STEREO)
    , mAudioSteamOutSampleRate(48000)
    // , mAudioSteamOutDevices(AUDIO_DEVICE_OUT_SPEAKER)
    , mDolbyMS12OutChannelMask{AUDIO_CHANNEL_OUT_STEREO,
                               AUDIO_CHANNEL_OUT_STEREO,
                               AUDIO_CHANNEL_OUT_STEREO,
                               AUDIO_CHANNEL_OUT_STEREO,
                               AUDIO_CHANNEL_OUT_STEREO}
    , mDolbyMS12OutConfig(MS12_OUTPUT_MASK_DD)
    , mDolbyMS12OutFormat(AUDIO_FORMAT_AC3)
    , mDolbyMS12OutSampleRate(48000)
    , mConfigParams(NULL)
    , mStereoOutputFlag(true)
    // , mMultiOutputFlag(true)
    , mDRCBoost(100)
    , mDRCCut(100)
    , mDRCBoostSystem(100)
    , mDRCCutSystem(100)
    , mMainFlags(true)
    //, mMainFlags(false) // always have mMainFlags on? zz
    , mAppSoundFlags(false)
    , mSystemSoundFlags(false)
    , mDAPInitMode(4)
    , mDAPVirtualBassEnable(1)
    , mDBGOut(0)
    , mDRCModesOfDownmixedOutput(0)
    , mDAPDRCMode(0)
    , mDownmixMode(0)
    , mEvaluationMode(0)
    , mMaxChannels(6)//fixme, here choose 5.1ch
    , mDonwnmix71PCMto51(0)
    , mLockingChannelModeENC(0)//Encoder Channel Mode Locking Mode as 5.1
    , mRISCPrecisionFlag(1)
    , mDualMonoReproMode(0)
    , mVerbosity(2)
    , mOutputBitDepth(16)//use 16 bit per sample
    , mAssociatedAudioMixing(1)
    , mSystemAPPAudioMixing(1)
    , mUserControlVal(0)

    //DDPLUS SWITCHES
    , mDdplusAssocSubstream(1)//range 1~3, here choose 1
    , mCompressorProfile(0)

    //HE-AAC SWITCHES
    , mAssocInstanse(2)
    , mDefDialnormVal(108)
    , mTransportFormat(0)

    //DAP SWITCHES (device specific)
    , mDAPCalibrationBoost(0)
    , mDAPDMX(0)
    , mDAPGains(0)
    , mDAPSurDecEnable(false)
    , mHasAssociateInput(false)
    , mHasSystemInput(false)
    , mHasAppInput(false)
    , mDualOutputFlag(false)
    , mDualBitstreamOut(false)
    , mActivateOTTSignal(false)
    , mAtmosLock(false)//off(default) if mActivateOTTSignal is true
    , mPause(false)//Unpause(default) if mActivateOTTSignal is true
    , mMain1IsDummy(false)
    , mOTTSoundInputEnable(false)
{
    ALOGD("+%s() mAudioOutFlags %d mAudioStreamOutFormat %#x mHasAssociateInput %d mHasSystemInput %d AppInput %d\n",
          __FUNCTION__, mAudioOutFlags, mAudioStreamOutFormat, mHasAssociateInput, mHasSystemInput, mHasAppInput);
    mConfigParams = PrepareConfigParams(MAX_ARGC, MAX_ARGV_STRING_LEN);
    if (!mConfigParams) {
        ALOGD("%s() line %d prepare the array fail", __FUNCTION__, __LINE__);
    }
    memset(mDolbyMain1FileName, 0, sizeof(mDolbyMain1FileName));
    memcpy(mDolbyMain1FileName, DEFAULT_MAIN_DDP_FILE_NAME, sizeof(DEFAULT_MAIN_DDP_FILE_NAME));
    memset(mDolbyMain2FileName, 0, sizeof(mDolbyMain2FileName));
    memcpy(mDolbyMain2FileName, DEFAULT_DUMMY_DDP_FILE_NAME, sizeof(DEFAULT_DUMMY_DDP_FILE_NAME));
    char params_bin[] = "ms12_exec";
    sprintf(mConfigParams[mParamNum++], "%s", params_bin);
    ALOGD("-%s() main1 %s main2 %s", __FUNCTION__, mDolbyMain1FileName, mDolbyMain2FileName);
}

DolbyMS12ConfigParams::~DolbyMS12ConfigParams()
{
    ALOGD("+%s()", __FUNCTION__);
    CleanupConfigParams(mConfigParams, MAX_ARGC);
    ALOGD("-%s()", __FUNCTION__);
}


void DolbyMS12ConfigParams::SetAudioStreamOutParams(
    audio_output_flags_t flags
    , audio_format_t input_format
    , audio_channel_mask_t channel_mask
    , int sample_rate
    , int output_config)
{
    ALOGD("+%s()", __FUNCTION__);
    mAudioOutFlags = flags;
    mAudioStreamOutFormat = input_format;
    mDolbyMS12OutChannelMask[MS12_INPUT_MAIN] = channel_mask;

    mAudioSteamOutSampleRate = sample_rate;

    mDolbyMS12OutConfig = output_config & MS12_OUTPUT_MASK_PUBLIC;

    // speaker output w/o a DAP tuning file will use downmix output instead
    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_SPEAKER) {
        if (mDAPInitMode) {
            mDolbyMS12OutConfig |= MS12_OUTPUT_MASK_DAP;
        } else {
            mDolbyMS12OutConfig |= MS12_OUTPUT_MASK_STEREO;
        }
    }

    if (mDolbyMS12OutChannelMask[MS12_INPUT_MAIN] == AUDIO_CHANNEL_OUT_7POINT1) {
        mMaxChannels = 8;
    } else {
        mMaxChannels = 6;
    }

    ALOGD("-%s() AudioStreamOut Flags %x Format %#x ChannelMask %x SampleRate %d OutputFormat %#x\n",
          __FUNCTION__, mAudioOutFlags, mAudioStreamOutFormat, mDolbyMS12OutChannelMask[MS12_INPUT_MAIN],
          mAudioSteamOutSampleRate, mDolbyMS12OutFormat);
}

bool DolbyMS12ConfigParams::SetDolbyMS12ParamsbyOutProfile(audio_policy_forced_cfg_t forceUse __unused)
{
    ALOGD("+%s()", __FUNCTION__);

    if (mDolbyMS12OutChannelMask[MS12_INPUT_MAIN] == AUDIO_CHANNEL_OUT_7POINT1) {
        mStereoOutputFlag = false;
    } else if (mDolbyMS12OutChannelMask[MS12_INPUT_MAIN] == AUDIO_CHANNEL_OUT_5POINT1) {
        mStereoOutputFlag = false;
    } else { //AUDIO_CHANNEL_OUT_STEREO
        mStereoOutputFlag = true;
    }
    if (mAudioStreamOutChannelMask == AUDIO_CHANNEL_OUT_7POINT1) {
        mMaxChannels = 8;
    } else {
        mMaxChannels = 6;
    }
    ALOGD("%s() mMaxChannels %x mStereoOutputFlag %#x\n", __FUNCTION__, mMaxChannels, mStereoOutputFlag);
    ALOGD("%s() mAudioStreamOutFormat %#x mDolbyMS12OutFormat %#x\n",
          __FUNCTION__, mAudioStreamOutFormat, mDolbyMS12OutFormat);

    //Todo, if DAP is enable, here need to modify!!!
    /*
        if ((mAudioStreamOutFormat == mDolbyMS12OutFormat) && \
                (mHasAssociateInput == false) && \
                (mHasSystemInput == false) && \
                (mAudioStreamOutChannelMask == mDolbyMS12OutChannelMask) && \
                (mAudioSteamOutSampleRate == mDolbyMS12OutSampleRate)) {
            ALOGD("-%s() dolbyms12 in/out Format/channelMask/SampleRate is same, bypass the audio!\n", __FUNCTION__);
            return false;//do not use dolbyms12
        }
        else {
            ALOGD("-%s() dolbyms12 in format is differ with out format!\n", __FUNCTION__);
            return true;
        }
    */
    ALOGD("-%s() Enable dolbyms12!\n", __FUNCTION__);
    return true;
}

//input and output
int DolbyMS12ConfigParams::SetInputOutputFileName(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);

    if (mActivateOTTSignal == false) {
        if (mHasAssociateInput == false) {
            sprintf(ConfigParams[*row_index], "%s", "-im");
            (*row_index)++;
            if ((mAudioStreamOutFormat == AUDIO_FORMAT_AC3) || (mAudioStreamOutFormat == AUDIO_FORMAT_E_AC3)) {
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_DDP_FILE_NAME);
                (*row_index)++;
                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            } else if ((mAudioStreamOutFormat == AUDIO_FORMAT_AAC) || (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V1)) {
                //fixme, which he-aac format is allowed to this flow.
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_HEAAC_V1_FILE_NAME);
                (*row_index)++;
                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            } else if (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V2) {
                //fixme, which he-aac format is allowed to this flow.
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_HEAAC_V2_FILE_NAME);
                (*row_index)++;
                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            } else { //others the format is pcm
                sprintf(ConfigParams[*row_index], "%s%d%s", DEFAULT_APPLICATION_PCM_FILE_NAME, mAudioSteamOutSampleRate, "kHz.wav");
                (*row_index)++;
                mMainFlags = true;
                mAppSoundFlags = true;
                mSystemSoundFlags = true;
            }
        } else {

            if ((mAudioStreamOutFormat == AUDIO_FORMAT_AC3) || (mAudioStreamOutFormat == AUDIO_FORMAT_E_AC3)) {
                sprintf(ConfigParams[*row_index], "%s", "-im");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_DDP_FILE_NAME);
                (*row_index)++;

                sprintf(ConfigParams[*row_index], "%s", "-ia");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_ASSOCIATE_DDP_FILE_NAME);
                (*row_index)++;

                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            } else if ((mAudioStreamOutFormat == AUDIO_FORMAT_AAC) || (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V1)) {
                sprintf(ConfigParams[*row_index], "%s", "-im");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_HEAAC_V1_FILE_NAME);
                (*row_index)++;

                sprintf(ConfigParams[*row_index], "%s", "-ia");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_ASSOCIATE_HEAAC_V1_FILE_NAME);
                (*row_index)++;

                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            } else if (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V2) {
                sprintf(ConfigParams[*row_index], "%s", "-im");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_MAIN_HEAAC_V2_FILE_NAME);
                (*row_index)++;

                sprintf(ConfigParams[*row_index], "%s", "-ia");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%s", DEFAULT_ASSOCIATE_HEAAC_V2_FILE_NAME);
                (*row_index)++;

                mMainFlags = true;
                mAppSoundFlags = false;
                mSystemSoundFlags = false;
            }
        }
    }

    // have active OTT signal,then configure input Main program input filename? zz
    if (mActivateOTTSignal == true) {
        if ((mAudioStreamOutFormat == AUDIO_FORMAT_AC3) || (mAudioStreamOutFormat == AUDIO_FORMAT_E_AC3)) {
            sprintf(ConfigParams[*row_index], "%s", "-im");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%s", mDolbyMain1FileName);
            (*row_index)++;

            sprintf(ConfigParams[*row_index], "%s", "-im2");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%s", mDolbyMain2FileName);
            (*row_index)++;
            ALOGD("%s() main1 %s main2 %s", __FUNCTION__, mDolbyMain1FileName, mDolbyMain2FileName);
        }
        if (mOTTSoundInputEnable == true) {
            sprintf(ConfigParams[*row_index], "%s", "-ios");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%s", DEFAULT_OTT_PCM_FILE_NAME);
            (*row_index)++;
        }
    }

    if (mHasSystemInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-is");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_SYSTEM_PCM_FILE_NAME);
        (*row_index)++;
    }

    if (mHasAppInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-ias");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_APPLICATION_PCM_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_DD) {
        sprintf(ConfigParams[*row_index], "%s", "-od");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_DD_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_DDP) {
        sprintf(ConfigParams[*row_index], "%s", "-odp");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_DDP_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_MAT) {
        sprintf(ConfigParams[*row_index], "%s", "-omat");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_MAT_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_DAP) {
        sprintf(ConfigParams[*row_index], "%s", "-o_dap_speaker");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_DAP_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_MC) {
        sprintf(ConfigParams[*row_index], "%s", "-om");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_PCM_MULTI_FILE_NAME);
        (*row_index)++;
    }

    if (mDolbyMS12OutConfig & MS12_OUTPUT_MASK_STEREO) {
        sprintf(ConfigParams[*row_index], "%s", "-oms");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%s", DEFAULT_OUTPUT_PCM_DOWNMIX_FILE_NAME);
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

int DolbyMS12ConfigParams::ChannelMask2ChannelConfig(audio_channel_mask_t channel_mask)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    int ChannelConfiguration;
    switch (channel_mask & ~AUDIO_CHANNEL_OUT_LOW_FREQUENCY) {
    case AUDIO_CHANNEL_OUT_MONO: // C
        ChannelConfiguration = 1;
        break;
    case AUDIO_CHANNEL_OUT_STEREO: // L, R
        ChannelConfiguration = 2;
        break;
    case (AUDIO_CHANNEL_OUT_FRONT_LEFT | AUDIO_CHANNEL_OUT_FRONT_RIGHT | AUDIO_CHANNEL_OUT_FRONT_CENTER): // L, R, C
        ChannelConfiguration = 3;
        break;
    case (AUDIO_CHANNEL_OUT_FRONT_LEFT | AUDIO_CHANNEL_OUT_FRONT_RIGHT | AUDIO_CHANNEL_OUT_BACK_CENTER): // L, R, S
        ChannelConfiguration = 4;
        break;
    case (AUDIO_CHANNEL_OUT_FRONT_LEFT | AUDIO_CHANNEL_OUT_FRONT_RIGHT | AUDIO_CHANNEL_OUT_FRONT_CENTER | AUDIO_CHANNEL_OUT_BACK_CENTER):// L, R, C, S
        ChannelConfiguration = 5;
        break;
    case (AUDIO_CHANNEL_OUT_FRONT_LEFT | AUDIO_CHANNEL_OUT_FRONT_RIGHT | AUDIO_CHANNEL_OUT_BACK_LEFT | AUDIO_CHANNEL_OUT_BACK_RIGHT):// L, R, LS, RS
        ChannelConfiguration = 6;
        break;
    case (AUDIO_CHANNEL_OUT_FRONT_LEFT | AUDIO_CHANNEL_OUT_FRONT_RIGHT | AUDIO_CHANNEL_OUT_FRONT_CENTER | AUDIO_CHANNEL_OUT_BACK_LEFT | AUDIO_CHANNEL_OUT_BACK_RIGHT):// L, R, C, LS, RS
        ChannelConfiguration = 7;
        break;
    case (AUDIO_CHANNEL_OUT_7POINT1 & ~AUDIO_CHANNEL_OUT_LOW_FREQUENCY):
        ChannelConfiguration = 21;
        break;
    default:
        ChannelConfiguration = DEFAULT_SOUNDS_CHANNEL_CONFIGURATION;
        break;
    }

    ALOGV("-%s() line %d ChannelConfiguration %d\n", __FUNCTION__, __LINE__, ChannelConfiguration);
    return ChannelConfiguration;
}

int DolbyMS12ConfigParams::ChannelMask2LFEConfig(audio_channel_mask_t channel_mask)
{
    return (channel_mask & AUDIO_CHANNEL_OUT_LOW_FREQUENCY) ? 1 : 0;
}

//functional switches
int DolbyMS12ConfigParams::SetFunctionalSwitches(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if (mStereoOutputFlag == true) {
        if ((mDRCBoost >= 0) && (mDRCBoost <= 100)) {
            sprintf(ConfigParams[*row_index], "%s", "-bs");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mDRCBoostSystem);
            (*row_index)++;
        }

        if ((mDRCCut >= 0) && (mDRCCut <= 100)) {
            sprintf(ConfigParams[*row_index], "%s", "-cs");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mDRCCutSystem);
            (*row_index)++;
        }
    }

    // //fixme, not use this params
    // if (mMultiOutputFlag == true) {
    //     sprintf(ConfigParams[*row_index], "%s", "-mc");
    //     (*row_index)++;
    //     sprintf(ConfigParams[*row_index], "%d", 1);
    //     (*row_index)++;
    // }

    if ((mDRCBoost >= 0) && (mDRCBoost <= 100)) {
        sprintf(ConfigParams[*row_index], "%s", "-b");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCBoost);
        (*row_index)++;
    }

    if ((mDRCCut >= 0) && (mDRCCut <= 100)) {
        sprintf(ConfigParams[*row_index], "%s", "-c");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCCut);
        (*row_index)++;
    }


    if (mAppSoundFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-chas");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2ChannelConfig(mDolbyMS12OutChannelMask[MS12_INPUT_APP]));
        (*row_index)++;
    }

    // Channel configuration of OTT Sounds input
    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-chos");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2ChannelConfig(mDolbyMS12OutChannelMask[MS12_INPUT_UI]));
        (*row_index)++;
    }

    // use mHasSystemInput to replace mSystemSoundFlags instead //zz
    if (mHasSystemInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-chs");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2ChannelConfig(mDolbyMS12OutChannelMask[MS12_INPUT_SYSTEM]));
        (*row_index)++;
    }

    if ((mDAPInitMode >= 0) && (mDAPInitMode <=  4)) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_init_mode");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPInitMode);
        (*row_index)++;
    }

    if (mDAPVirtualBassEnable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-b_dap_vb_enable");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPVirtualBassEnable);
        (*row_index)++;
    }

    if (!mDBGOut) {
        sprintf(ConfigParams[*row_index], "%s", "-dbgout");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDBGOut);
        (*row_index)++;
    }

    {
        sprintf(ConfigParams[*row_index], "%s", "-drc");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCModesOfDownmixedOutput);
        (*row_index)++;
    }

    if (mDAPDRCMode == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_drc");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPDRCMode);
        (*row_index)++;
    }

    if (mDownmixMode == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dmx");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDownmixMode);
        (*row_index)++;
    }

    if (mEvaluationMode == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-eval");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mEvaluationMode);
        (*row_index)++;
    }

    {
        sprintf(ConfigParams[*row_index], "%s", "-las");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_APP]));
        (*row_index)++;
    }

    {
        sprintf(ConfigParams[*row_index], "%s", "-ls");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_SYSTEM]));
        (*row_index)++;
    }

    // LFE present in OTT Sounds input
    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-los");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_UI]));
        (*row_index)++;
    }

    if ((mMaxChannels == 6) || (mMaxChannels == 8)) {
        sprintf(ConfigParams[*row_index], "%s", "-max_channels");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mMaxChannels);
        (*row_index)++;
    } else { //here we choose 7.1 as the max channels
        sprintf(ConfigParams[*row_index], "%s", "-max_channels");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", 8);
        (*row_index)++;
    }

    if (mDonwnmix71PCMto51 == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-mc_5_1_dmx");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDonwnmix71PCMto51);
        (*row_index)++;
    }

    if (1/*mLockingChannelModeENC == 1*/) {
        sprintf(ConfigParams[*row_index], "%s", "-chmod_locking");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mLockingChannelModeENC);
        (*row_index)++;
    }

    if (mRISCPrecisionFlag == 0) {
        sprintf(ConfigParams[*row_index], "%s", "-p");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mRISCPrecisionFlag);
        (*row_index)++;
    }

    if (mDualMonoReproMode != 0) {
        sprintf(ConfigParams[*row_index], "%s", "-u");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDualMonoReproMode);
        (*row_index)++;
    }

    if ((mVerbosity >= 0) && (mVerbosity <= 3)) {
        sprintf(ConfigParams[*row_index], "%s", "-v");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mVerbosity);
        (*row_index)++;
    }

    if ((mOutputBitDepth == 16) || (mOutputBitDepth == 24) || (mOutputBitDepth == 32)) {
        sprintf(ConfigParams[*row_index], "%s", "-w");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mOutputBitDepth);
        (*row_index)++;
    }

    if ((mAssociatedAudioMixing == 0) && (mHasAssociateInput == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-xa");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mAssociatedAudioMixing);
        (*row_index)++;
    }

    //if (mSystemAPPAudioMixing == 0)
    {
        sprintf(ConfigParams[*row_index], "%s", "-xs");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mSystemAPPAudioMixing);
        (*row_index)++;
    }

    if (mHasAssociateInput == true) { //set this params when dual input.
        if (mUserControlVal > 32) {
            mUserControlVal = 32;
        } else if (mUserControlVal < -32) {
            mUserControlVal = -32;
        }
        sprintf(ConfigParams[*row_index], "%s", "-xu");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mUserControlVal);
        (*row_index)++;
    }


    //fixme, which params are suitable
    if (mMainFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-main1_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain1MixGain.target, mMain1MixGain.duration, mMain1MixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mHasAssociateInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-main2_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain2MixGain.target, mMain2MixGain.duration, mMain2MixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-ott_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mOTTMixGain.target, mOTTMixGain.duration, mOTTMixGain.shape);
        (*row_index)++;
    }

    if ((mMainFlags == true) && (mHasSystemInput == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_prim_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysPrimMixGain.target, mSysPrimMixGain.duration, mSysPrimMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mAppSoundFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_apps_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysApppsMixGain.target, mSysApppsMixGain.duration, mSysApppsMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mHasSystemInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_syss_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysSyssMixGain.target, mSysSyssMixGain.duration, mSysSyssMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//functional switches
int DolbyMS12ConfigParams::SetFunctionalSwitchesRuntime(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if (mStereoOutputFlag == true) {
        if ((mDRCBoost >= 0) && (mDRCBoost <= 100)) {
            sprintf(ConfigParams[*row_index], "%s", "-bs");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mDRCBoostSystem);
            (*row_index)++;
        }

        if ((mDRCCut >= 0) && (mDRCCut <= 100)) {
            sprintf(ConfigParams[*row_index], "%s", "-cs");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mDRCCutSystem);
            (*row_index)++;
        }
    }

    // //fixme, not use this params
    // if (mMultiOutputFlag == true) {
    //     sprintf(ConfigParams[*row_index], "%s", "-mc");
    //     (*row_index)++;
    //     sprintf(ConfigParams[*row_index], "%d", 1);
    //     (*row_index)++;
    // }

    if ((mDRCBoost >= 0) && (mDRCBoost <= 100)) {
        sprintf(ConfigParams[*row_index], "%s", "-b");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCBoost);
        (*row_index)++;
    }

    if ((mDRCCut >= 0) && (mDRCCut <= 100)) {
        sprintf(ConfigParams[*row_index], "%s", "-c");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCCut);
        (*row_index)++;
    }


    if (mAppSoundFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-chas");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDolbyMS12OutChannelMask[MS12_INPUT_APP]);
        (*row_index)++;
    }

    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-chos");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDolbyMS12OutChannelMask[MS12_INPUT_UI]);
        (*row_index)++;
    }

    if (mHasSystemInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-chs");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDolbyMS12OutChannelMask[MS12_INPUT_SYSTEM]);
        (*row_index)++;
    }

    {
        sprintf(ConfigParams[*row_index], "%s", "-drc");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDRCModesOfDownmixedOutput);
        (*row_index)++;
    }

    if (mDAPDRCMode == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_drc");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPDRCMode);
        (*row_index)++;
    }

    if (mDownmixMode == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dmx");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDownmixMode);
        (*row_index)++;
    }

    {
        sprintf(ConfigParams[*row_index], "%s", "-las");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_APP]));
        (*row_index)++;
    }
    {
        sprintf(ConfigParams[*row_index], "%s", "-ls");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_SYSTEM]));
        (*row_index)++;
    }

    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-los");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_UI]));
        (*row_index)++;
    }

    if (mDualMonoReproMode != 0) {
        sprintf(ConfigParams[*row_index], "%s", "-u");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDualMonoReproMode);
        (*row_index)++;
    }

    if ((mAssociatedAudioMixing == 0) && (mHasAssociateInput == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-xa");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mAssociatedAudioMixing);
        (*row_index)++;
    }

    //if (mSystemAPPAudioMixing == 0)
    {
        sprintf(ConfigParams[*row_index], "%s", "-xs");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mSystemAPPAudioMixing);
        (*row_index)++;
    }

    if (mHasAssociateInput == true) { //set this params when dual input.
        if (mUserControlVal > 32) {
            mUserControlVal = 32;
        } else if (mUserControlVal < -32) {
            mUserControlVal = -32;
        }
        sprintf(ConfigParams[*row_index], "%s", "-xu");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mUserControlVal);
        (*row_index)++;
    }


    //fixme, which params are suitable
    if (mMainFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-main1_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain1MixGain.target, mMain1MixGain.duration, mMain1MixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mHasAssociateInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-main2_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain2MixGain.target, mMain2MixGain.duration, mMain2MixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if ((mActivateOTTSignal == true) && (mMain1IsDummy == true) && (mOTTSoundInputEnable == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-ott_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mOTTMixGain.target, mOTTMixGain.duration, mOTTMixGain.shape);
        (*row_index)++;
    }

    if ((mMainFlags == true) && (mHasSystemInput == true)) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_prim_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysPrimMixGain.target, mSysPrimMixGain.duration, mSysPrimMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mAppSoundFlags == true) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_apps_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysApppsMixGain.target, mSysApppsMixGain.duration, mSysApppsMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    if (mHasSystemInput == true) {
        sprintf(ConfigParams[*row_index], "%s", "-sys_syss_mixgain");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysSyssMixGain.target, mSysSyssMixGain.duration, mSysSyssMixGain.shape);//choose mid-val
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

int DolbyMS12ConfigParams::SetFunctionalSwitchesRuntime_lite(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);

    sprintf(ConfigParams[*row_index], "%s", "-main1_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain1MixGain.target, mMain1MixGain.duration, mMain1MixGain.shape);//choose mid-val
    (*row_index)++;

    sprintf(ConfigParams[*row_index], "%s", "-main2_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mMain2MixGain.target, mMain2MixGain.duration, mMain2MixGain.shape);//choose mid-val
    (*row_index)++;

    sprintf(ConfigParams[*row_index], "%s", "-ott_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mOTTMixGain.target, mOTTMixGain.duration, mOTTMixGain.shape);
    (*row_index)++;

    sprintf(ConfigParams[*row_index], "%s", "-sys_prim_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysPrimMixGain.target, mSysPrimMixGain.duration, mSysPrimMixGain.shape);//choose mid-val
    (*row_index)++;

    sprintf(ConfigParams[*row_index], "%s", "-sys_apps_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysApppsMixGain.target, mSysApppsMixGain.duration, mSysApppsMixGain.shape);//choose mid-val
    (*row_index)++;

    sprintf(ConfigParams[*row_index], "%s", "-sys_syss_mixgain");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d", mSysSyssMixGain.target, mSysSyssMixGain.duration, mSysSyssMixGain.shape);//choose mid-val
    (*row_index)++;

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}


//ddplus switches
int DolbyMS12ConfigParams::SetDdplusSwitches(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if ((mHasAssociateInput == true) && ((mAudioStreamOutFormat == AUDIO_FORMAT_AC3) || (mAudioStreamOutFormat == AUDIO_FORMAT_E_AC3))) {
        sprintf(ConfigParams[*row_index], "%s", "-at");
        (*row_index)++;
        ALOGD("+%s() mDdplusAssocSubstream %d\n", __FUNCTION__, mDdplusAssocSubstream);
        sprintf(ConfigParams[*row_index], "%d", mDdplusAssocSubstream);//choose mid-val
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//PCM switches
int DolbyMS12ConfigParams::SetPCMSwitches(char **ConfigParams, int *row_index)
{
    if ((mAudioStreamOutFormat & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_PCM) {
        {
            sprintf(ConfigParams[*row_index], "%s", "-chp");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", ChannelMask2ChannelConfig(mDolbyMS12OutChannelMask[MS12_INPUT_MAIN]));
            (*row_index)++;
        }

        {
            sprintf(ConfigParams[*row_index], "%s", "-lp");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", ChannelMask2LFEConfig(mDolbyMS12OutChannelMask[MS12_INPUT_MAIN]));
            (*row_index)++;
        }

        if (mActivateOTTSignal == false) {
            if ((mCompressorProfile >= 0) && (mCompressorProfile <= 5)) {
                sprintf(ConfigParams[*row_index], "%s", "-rp");
                (*row_index)++;
                sprintf(ConfigParams[*row_index], "%d", mCompressorProfile);
                (*row_index)++;
            }
        }
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

#if 0
//PCM switches
int DolbyMS12ConfigParams::SetPCMSwitchesRuntime(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if ((mSystemSoundFlags == true) || (mAppSoundFlags == true)) {
        mChannelConfigInExtPCMInput = APPSoundChannelMaskConvertToChannelConfiguration(mAudioStreamOutChannelMask);
        {
            sprintf(ConfigParams[*row_index], "%s", "-chp");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mChannelConfigInExtPCMInput);
            (*row_index)++;
        }

        if ((mChannelConfigInExtPCMInput == 1) || (mChannelConfigInExtPCMInput == 2)) {
            mLFEPresentInExtPCMInput = false;
            sprintf(ConfigParams[*row_index], "%s", "-lp");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", 0);
            (*row_index)++;
        }
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}
#endif
//HE-AAC switches, all none-run-time
int DolbyMS12ConfigParams::SetHEAACSwitches(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if ((mHasAssociateInput == true) && ((mAudioStreamOutFormat == AUDIO_FORMAT_AAC) || \
                                         (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V1) || (mAudioStreamOutFormat == AUDIO_FORMAT_HE_AAC_V2))) {
        {
            sprintf(ConfigParams[*row_index], "%s", "-as");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mAssocInstanse);
            (*row_index)++;
        }

        if ((mDefDialnormVal >= 0) && (mDefDialnormVal <= 127)) {
            sprintf(ConfigParams[*row_index], "%s", "-dn");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mDefDialnormVal);
            (*row_index)++;
        }

        if ((mTransportFormat >= 0) && (mTransportFormat <= 3)) {
            sprintf(ConfigParams[*row_index], "%s", "-t");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mTransportFormat);
            (*row_index)++;
        }
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}


//OTT PROCESSING GRAPH SWITCHES
int DolbyMS12ConfigParams::SetOTTProcessingGraphSwitches(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    {
        //TODO: already exist in other setting, remove here , otherwise will cause error when initializing MS12 zz
        //sprintf(ConfigParams[*row_index], "%s", "-chmod_locking");
        //(*row_index)++;
        //sprintf(ConfigParams[*row_index], "%d", mLockingChannelModeENC);
        //(*row_index)++;
    }

    if (mActivateOTTSignal == true) {
        sprintf(ConfigParams[*row_index], "%s", "-ott");
        (*row_index)++;

        // no matter mAtmosLock == true or mAtmosLock == false
        // we all need to set -atmos_locking flag
        // otherwise atmos_locking function can not perform correctly.
        //if (mAtmosLock == true)
        {
            // MS1.3.2 use "atmos_locking" instead of "atmos_lock"
            // if we use "atmos_lock" we will get following error log from ms12:
            // "ERROR: Encoder Atmos locking parameter invalid (-atmos_locking: 0 <auto> or 1 <5.1.2 Atmos>)"
            //sprintf(ConfigParams[*row_index], "%s", "-atmos_lock");
            sprintf(ConfigParams[*row_index], "%s", "-atmos_locking");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mAtmosLock);
            (*row_index)++;
        }

        if ((mPause == true) || (mPause == false)) {
            sprintf(ConfigParams[*row_index], "%s", "-pause");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mPause);
            (*row_index)++;
        }
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//OTT PROCESSING GRAPH SWITCHES(runtime)
int DolbyMS12ConfigParams::SetOTTProcessingGraphSwitchesRuntime(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if (mActivateOTTSignal == true) {

        // no matter mAtmosLock == true or mAtmosLock == false
        // we all need to set -atmos_locking flag
        // otherwise atmos_locking function can not perform correctly.
        //if (mAtmosLock == true)
        {
            // MS1.3.2 use "atmos_locking" instead of "atmos_lock"
            // if we use "atmos_lock" we will get following error log from ms12:
            // "ERROR: Encoder Atmos locking parameter invalid (-atmos_locking: 0 <auto> or 1 <5.1.2 Atmos>)"
            //sprintf(ConfigParams[*row_index], "%s", "-atmos_lock");
            sprintf(ConfigParams[*row_index], "%s", "-atmos_locking");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mAtmosLock);
            (*row_index)++;
        }

        if ((mPause == true) || (mPause == false)) {
            sprintf(ConfigParams[*row_index], "%s", "-pause");
            (*row_index)++;
            sprintf(ConfigParams[*row_index], "%d", mPause);
            (*row_index)++;
        }
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//DAP SWITCHES (device specific)
//all run-time
int DolbyMS12ConfigParams::SetDAPDeviceSwitches(char **ConfigParams, int *row_index)
{
    String8 tmpParam("");
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    if ((mDAPCalibrationBoost >= 0) && (mDAPCalibrationBoost <= 192)) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_calibration_boost");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPCalibrationBoost);
        (*row_index)++;
    }

    if (mDAPDMX == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_dmx");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPDMX);
        (*row_index)++;
    }

    if ((mDAPGains >= -2080) && (mDAPGains <= 480)) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_gains");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPGains);
        (*row_index)++;
    }

    if (mDAPSurDecEnable == false) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_surround_decoder_enable");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d", mDAPSurDecEnable);
        (*row_index)++;
    }

    sprintf(ConfigParams[*row_index], "%s", "-dap_surround_virtualizer");
    (*row_index)++;
    sprintf(ConfigParams[*row_index], "%d,%d,%d,%d,%d", DeviceDAPSurroundVirtualizer.virtualizer_enable,
            DeviceDAPSurroundVirtualizer.headphone_reverb, DeviceDAPSurroundVirtualizer.speaker_angle,
            DeviceDAPSurroundVirtualizer.speaker_start, DeviceDAPSurroundVirtualizer.surround_boost);
    (*row_index)++;

    if (DeviceDAPGraphicEQ.eq_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_graphic_eq");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d", DeviceDAPGraphicEQ.eq_enable, DeviceDAPGraphicEQ.eq_nb_bands);
        tmpParam += String8::format("%s", ConfigParams[*row_index]);
        int i = 0;
        for (i = 0; i < DeviceDAPGraphicEQ.eq_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPGraphicEQ.eq_band_center[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }

        for (i = 0; i < DeviceDAPGraphicEQ.eq_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPGraphicEQ.eq_band_target[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        memcpy(ConfigParams[*row_index], tmpParam.string(), strlen(tmpParam.string()));
        (*row_index)++;
    }

    tmpParam.clear();
    if (DeviceDAPOptimizer.optimizer_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_optimizer");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d", DeviceDAPOptimizer.optimizer_enable, DeviceDAPOptimizer.opt_nb_bands);
        tmpParam += String8::format("%s", ConfigParams[*row_index]);
        int i = 0;
        for (i = 0; i < DeviceDAPOptimizer.opt_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPOptimizer.opt_band_center_freq[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        for (i = 0; i < mMaxChannels * DeviceDAPOptimizer.opt_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPOptimizer.opt_band_gains[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        memcpy(ConfigParams[*row_index], tmpParam.string(), strlen(tmpParam.string()));
        (*row_index)++;
    }


    if (DeviceDAPBassEnhancer.bass_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_bass_enhancer");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d,%d", DeviceDAPBassEnhancer.bass_enable,
                DeviceDAPBassEnhancer.bass_boost, DeviceDAPBassEnhancer.bass_cutoff, DeviceDAPBassEnhancer.bass_width);
        (*row_index)++;
    }


    tmpParam.clear();
    if (DeviceDAPRegulator.regulator_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_regulator");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d,%d,%d,%d",
                DeviceDAPRegulator.regulator_enable, DeviceDAPRegulator.regulator_mode,
                DeviceDAPRegulator.regulator_overdrive, DeviceDAPRegulator.regulator_timbre,
                DeviceDAPRegulator.regulator_distortion, DeviceDAPRegulator.reg_nb_bands);
        tmpParam += String8::format("%s", ConfigParams[*row_index]);
        int i = 0;
        for (i = 0; i < DeviceDAPRegulator.reg_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPRegulator.reg_band_center[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        for (i = 0; i < DeviceDAPRegulator.reg_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPRegulator.reg_low_thresholds[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        for (i = 0; i < DeviceDAPRegulator.reg_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPRegulator.reg_high_thresholds[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        for (i = 0; i < DeviceDAPRegulator.reg_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", DeviceDAPRegulator.reg_isolated_bands[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        memcpy(ConfigParams[*row_index], tmpParam.string(), strlen(tmpParam.string()));
        (*row_index)++;
    }

    if (mDAPVirtualBassEnable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_virtual_bass");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", DeviceDAPVirtualBass.virtual_bass_mode,
                DeviceDAPVirtualBass.virtual_bass_low_src_freq, DeviceDAPVirtualBass.virtual_bass_high_src_freq,
                DeviceDAPVirtualBass.virtual_bass_overall_gain, DeviceDAPVirtualBass.virtual_bass_slope_gain,
                DeviceDAPVirtualBass.virtual_bass_subgains[0], DeviceDAPVirtualBass.virtual_bass_subgains[1],
                DeviceDAPVirtualBass.virtual_bass_subgains[2], DeviceDAPVirtualBass.virtual_bass_low_mix_freq,
                DeviceDAPVirtualBass.virtual_bass_high_mix_freq);
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//DAP SWITCHES (content specific)
//all run-time
int DolbyMS12ConfigParams::SetDAPContentSwitches(char **ConfigParams, int *row_index)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    {
        sprintf(ConfigParams[*row_index], "%s", "-dap_mi_steering");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d,%d", ContentDAPMISteering.mi_ieq_enable, ContentDAPMISteering.mi_dv_enable, ContentDAPMISteering.mi_de_enable, ContentDAPMISteering.mi_surround_enable);
        (*row_index)++;
    }

    if (ContentDAPLeveler.leveler_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_leveler");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", ContentDAPLeveler.leveler_enable, ContentDAPLeveler.leveler_amount, ContentDAPLeveler.leveler_ignore_il);
        (*row_index)++;
    }


    if (ContentDAPIEQ.ieq_enable == 1) {
        String8 tmpParam("");
        sprintf(ConfigParams[*row_index], "%s", "-dap_ieq");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", ContentDAPIEQ.ieq_enable, ContentDAPIEQ.ieq_amount, ContentDAPIEQ.ieq_nb_bands);
        tmpParam += String8::format("%s", ConfigParams[*row_index]);
        int i = 0;
        for (i = 0; i < ContentDAPIEQ.ieq_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", ContentDAPIEQ.ieq_band_center[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        for (i = 0; i < ContentDAPIEQ.ieq_nb_bands; i++) {
            sprintf(ConfigParams[*row_index], ",%d", ContentDAPIEQ.ieq_band_target[i]);
            tmpParam += String8::format("%s", ConfigParams[*row_index]);
        }
        memcpy(ConfigParams[*row_index], tmpParam.string(), strlen(tmpParam.string()));
        (*row_index)++;
    }


    if (ContenDAPDialogueEnhancer.de_enable == 1) {
        sprintf(ConfigParams[*row_index], "%s", "-dap_dialogue_enhancer");
        (*row_index)++;
        sprintf(ConfigParams[*row_index], "%d,%d,%d", ContenDAPDialogueEnhancer.de_enable, ContenDAPDialogueEnhancer.de_amount, ContenDAPDialogueEnhancer.de_ducking);
        (*row_index)++;
    }

    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

//get dolby ms12 config params
char **DolbyMS12ConfigParams::GetDolbyMS12ConfigParams(int *argc)
{
    ALOGD("+%s()\n", __FUNCTION__);

    if (argc && mConfigParams) {
        char params_bin[] = "ms12_exec";
        sprintf(mConfigParams[mParamNum++], "%s", params_bin);
        SetInputOutputFileName(mConfigParams, &mParamNum);
        SetFunctionalSwitches(mConfigParams, &mParamNum);
        SetDdplusSwitches(mConfigParams, &mParamNum);
        SetPCMSwitches(mConfigParams, &mParamNum);
        SetHEAACSwitches(mConfigParams, &mParamNum);
        SetOTTProcessingGraphSwitches(mConfigParams, &mParamNum);
        if (mDAPInitMode) {
            SetDAPDeviceSwitches(mConfigParams, &mParamNum);
            SetDAPContentSwitches(mConfigParams, &mParamNum);
        }
        *argc = mParamNum;
        ALOGV("%s() line %d argc %d\n", __FUNCTION__, __LINE__, *argc);
        //here is to check the config params

        int config_params_check = 1;
        if (config_params_check) {
            int i = 0;
            for (i = 0; i < mParamNum; i++) {
                ALOGD("param #%d: %s\n", i, mConfigParams[i]);
            }
        }
    }

    ALOGD("-%s()", __FUNCTION__);
    return mConfigParams;
}

int DolbyMS12ConfigParams::ms_get_int_array_from_str(char **p_csv_string, int num_el, int *p_vals)
{
    char *endstr;
    int i;

    for (i = 0; i < num_el; i++) {
        int val = strtol(*p_csv_string, &endstr, 0);
        if (*p_csv_string == endstr) {
            return -1;
        }
        p_vals[i] = val;
        *p_csv_string = endstr;
        if (**p_csv_string == ',') {
            (*p_csv_string)++;
        }
    }

    return 0;
}

int DolbyMS12ConfigParams::ms_get_int_from_str(char **p_csv_string, int *p_vals)
{
    char *endstr;
    int val = strtol(*p_csv_string, &endstr, 0);

    if (*p_csv_string == endstr) {
        return -1;
    } else {
        *p_vals = val;
        *p_csv_string = endstr;
        if (**p_csv_string == ',') {
            (*p_csv_string)++;
        }
    }

    return 0;
}

char **DolbyMS12ConfigParams::UpdateDolbyMS12RuntimeConfigParams(int *argc, char *cmd)
{
    ALOGV("+%s()", __FUNCTION__);
    ALOGV("ms12 runtime cmd: %s", cmd);

    strcpy(mConfigParams[0], "ms12_runtime");

    *argc = 1;
    mParamNum = 1;

    std::string token;
    std::istringstream cmd_string(cmd);
    int index = 1, val;
    char *opt = NULL;

    while (cmd_string >> token) {
        strncpy(mConfigParams[mParamNum], token.c_str(), MAX_ARGV_STRING_LEN);
        mConfigParams[mParamNum][MAX_ARGV_STRING_LEN - 1] = '\0';
        ALOGV("argv[%d] = %s", mParamNum, mConfigParams[mParamNum]);
        mParamNum++;
        (*argc)++;
    }

    while (index < *argc) {
        if (!opt) {
            if ((mConfigParams[index][0] == '-') && (mConfigParams[index][1] < '0' || mConfigParams[index][1] > '9')) {
                opt = mConfigParams[index] + 1;
            } else {
                ALOGE("Invalid option sequence, skipped %s", mConfigParams[index]);
            }
            index++;
            continue;
        }

        if (strcmp(opt, "u") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 2)) {
                ALOGI("-u DualMonoReproMode: %d", val);
                mDualMonoReproMode = val;
            }
        } else if (strcmp(opt, "b") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 100)) {
                ALOGI("-b DRCBoost: %d", val);
                mDRCBoost = val;
            }
        } else if (strcmp(opt, "bs") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 100)) {
                ALOGI("-bs DRCBoostStereo: %d", val);
                mDRCBoostSystem = val;
            }
        } else if (strcmp(opt, "c") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 100)) {
                ALOGI("-c DRCCut: %d", val);
                mDRCCut = val;
            }
        } else if (strcmp(opt, "cs") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 100)) {
                ALOGI("-c DRCCutStereo: %d", val);
                mDRCCutSystem = val;
            }
        } else if (strcmp(opt, "dmx") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 2)) {
                ALOGI("-c Downmix Mode: %d", val);
                /* Fixme: [he-aac] 2 = ARIB is not used on AOSP */
                mDownmixMode = val;
            }
        } else if (strcmp(opt, "drc") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 1)) {
                ALOGI("-drc DRCModesOfDownmixedOutput: %d", val);
                mDRCModesOfDownmixedOutput = val;
            }
        } else if (strcmp(opt, "at") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 3)) {
                ALOGI("-at AC4Ac: %d", val);
                mDdplusAssocSubstream = val;
            }
        } else if (strcmp(opt, "xa") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 1)) {
                ALOGI("-xa Associated audio mixing: %d", val);
                mAssociatedAudioMixing = val;
            }
        } else if (strcmp(opt, "xu") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= MIN_USER_CONTROL_VALUES) && (val <= MAX_USER_CONTROL_VALUES)) {
                ALOGI("-xu User control values:[-32 (mute assoc) to 32 (mute main)] %d", val);
                mUserControlVal = val;
            }
        } else if (strcmp(opt, "dap_surround_decoder_enable") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 1)) {
                ALOGI("-dap_surround_decoder_enable DAPSurDecEnable: %d", val);
                mDAPSurDecEnable = val;
            }
        } else if (strcmp(opt, "dap_drc") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 1)) {
                ALOGI("-dap_drc DAPDRCMode: %d", val);
                mDAPDRCMode = val;
            }
        } else if (strcmp(opt, "dap_bass_enhancer") == 0) {
            int param[4];
            if (sscanf(mConfigParams[index], "%d,%d,%d,%d",
                &param[0], &param[1], &param[2], &param[3]) == 4) {
                if ((param[0] >= 0) && (param[0] <= 1))
                    DeviceDAPBassEnhancer.bass_enable = param[0];
                if ((param[1] >= 0) && (param[1] <= 384))
                    DeviceDAPBassEnhancer.bass_boost = param[1];
                if ((param[2] >= 20) && (param[2] <= 20000))
                    DeviceDAPBassEnhancer.bass_cutoff = param[2];
                if ((param[3] >= 2) && (param[3] <= 64))
                    DeviceDAPBassEnhancer.bass_width = param[3];
                ALOGI("-dap_bass_enhancer DeviceDAPBassEnhancer: %d %d %d %d", param[0], param[1], param[2], param[3]);
            }
        } else if (strcmp(opt, "dap_dialogue_enhancer") == 0) {
            int param[3];
            if (sscanf(mConfigParams[index], "%d,%d,%d",
                &param[0], &param[1], &param[2]) == 3) {
                if ((param[0] >= 0) && (param[0] <= 1))
                    ContenDAPDialogueEnhancer.de_enable = param[0];
                if ((param[1] >= 0) && (param[1] <= 16))
                    ContenDAPDialogueEnhancer.de_amount = param[1];
                if ((param[2] >= 0) && (param[2] <= 16))
                    ContenDAPDialogueEnhancer.de_ducking = param[2];
                ALOGI("-dap_dialogue_enhancer ContenDAPDialogueEnhancer: %d %d %d", param[0], param[1], param[2]);
            }
        } else if (strcmp(opt, "dap_graphic_eq") == 0) {
            DAPGraphicEQ eq;
            char *ptr = mConfigParams[index];
            if (ms_get_int_from_str(&ptr, &eq.eq_enable) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &eq.eq_nb_bands) < 0)
                goto eq_error;
            if (eq.eq_nb_bands > 20)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, eq.eq_nb_bands,
                &eq.eq_band_center[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, eq.eq_nb_bands,
                &eq.eq_band_target[0]) < 0)
                goto eq_error;
            DeviceDAPGraphicEQ = eq;
            ALOGI("-dap_graphic_eq DeviceDAPGraphicEQ: %d %d", eq.eq_enable, eq.eq_nb_bands);
        } else if (strcmp(opt, "dap_ieq") == 0) {
            DAPIEQ ieq;
            char *ptr = mConfigParams[index];
            if (ms_get_int_from_str(&ptr, &ieq.ieq_enable) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &ieq.ieq_amount) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &ieq.ieq_nb_bands) < 0)
                goto eq_error;
            if (ieq.ieq_nb_bands > 20)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, ieq.ieq_nb_bands,
                &ieq.ieq_band_center[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, ieq.ieq_nb_bands,
                &ieq.ieq_band_target[0]) < 0)
                goto eq_error;
            ContentDAPIEQ = ieq;
            ALOGI("-dap_ieq: %d %d %d", ieq.ieq_enable, ieq.ieq_amount, ieq.ieq_nb_bands);
        } else if (strcmp(opt, "dap_gains") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= -2080) && (val <= 480)) {
                ALOGI("-dap_gains: %d", val);
                mDAPGains = val;
            }
        } else if (strcmp(opt, "dap_leveler") == 0) {
            int param[3];
            if (sscanf(mConfigParams[index], "%d,%d,%d",
                &param[0], &param[1], &param[2]) == 3) {
                if ((param[0] >= 0) && (param[0] <= 1))
                    ContentDAPLeveler.leveler_enable = param[0];
                if ((param[1] >= 0) && (param[1] <= 10))
                    ContentDAPLeveler.leveler_amount = param[1];
                if ((param[2] >= 0) && (param[2] <= 1))
                    ContentDAPLeveler.leveler_ignore_il = param[2];
                ALOGI("-dap_leveler: %d %d %d", param[0], param[1], param[2]);
            }
        } else if (strcmp(opt, "dap_mi_steering") == 0) {
            int param[4];
            if (sscanf(mConfigParams[index], "%d,%d,%d,%d",
                &param[0], &param[1], &param[2], &param[3]) == 4) {
                if ((param[0] >= 0) && (param[0] <= 1))
                    ContentDAPMISteering.mi_ieq_enable = param[0];
                if ((param[1] >= 0) && (param[1] <= 1))
                    ContentDAPMISteering.mi_dv_enable = param[1];
                if ((param[2] >= 0) && (param[2] <= 1))
                    ContentDAPMISteering.mi_de_enable = param[2];
                if ((param[3] >= 0) && (param[3] <= 1))
                     ContentDAPMISteering.mi_surround_enable = param[3];
                ALOGI("-dap_mi_steering %d %d %d %d",param[0], param[1], param[2], param[3]);
            }
        } else if (strcmp(opt, "dap_surround_virtualizer") == 0) {
            int param[5];
            if (sscanf(mConfigParams[index], "%d,%d,%d,%d,%d",
                &param[0], &param[1], &param[2], &param[3], &param[4]) == 5) {
                if ((param[0] >= 0) && (param[0] <= 1))
                    DeviceDAPSurroundVirtualizer.virtualizer_enable = param[0];
                if ((param[1] >= -2080) && (param[1] <= 192))
                    DeviceDAPSurroundVirtualizer.headphone_reverb = param[1];
                if ((param[2] >= 5) && (param[2] <= 30))
                    DeviceDAPSurroundVirtualizer.speaker_angle = param[2];
                if ((param[3] >= 20) && (param[3] <= 2000))
                    DeviceDAPSurroundVirtualizer.speaker_start = param[3];
                if ((param[4] >= 0) && (param[4] <= 96))
                    DeviceDAPSurroundVirtualizer.surround_boost = param[4];
                ALOGI("-dap_surround_virtualizer: %d %d %d %d %d", param[0], param[1], param[2], param[3], param[4]);
            }
        } else if (strcmp(opt, "atmos_locking") == 0) {
            val = atoi(mConfigParams[index]);
            mAtmosLock = val ? true : false;
            ALOGI("-atmos_lock: %d", mAtmosLock);
        } else if (strcmp(opt, "dap_calibration_boost") == 0) {
            val = atoi(mConfigParams[index]);
            if ((val >= 0) && (val <= 192)) {
                mDAPCalibrationBoost = val;
                ALOGI("-dap_calibration_boost: %d", mDAPCalibrationBoost);
            }
        } else if (strcmp(opt, "dap_virtual_bass") == 0) {
            int param[10];
            if (sscanf(mConfigParams[index], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                &param[0], &param[1], &param[2], &param[3], &param[4], &param[5],
                &param[6], &param[7], &param[8], &param[9]) == 10) {
                if ((param[0] >= 0) && (param[0] <= 3))
                    DeviceDAPVirtualBass.virtual_bass_mode = param[0];
                if ((param[1] >= 30) && (param[1] <= 90))
                    DeviceDAPVirtualBass.virtual_bass_low_src_freq = param[1];
                if ((param[2] >= 90) && (param[2] <= 270))
                    DeviceDAPVirtualBass.virtual_bass_high_src_freq = param[2];
                if ((param[3] >= -480) && (param[3] <= 0))
                    DeviceDAPVirtualBass.virtual_bass_overall_gain = param[3];
                if ((param[4] >= -3) && (param[4] <= 0))
                    DeviceDAPVirtualBass.virtual_bass_slope_gain = param[4];
                if ((param[5] >= -480) && (param[5] <= 0))
                    DeviceDAPVirtualBass.virtual_bass_subgains[0] = param[5];
                if ((param[6] >= -480) && (param[6] <= 0))
                    DeviceDAPVirtualBass.virtual_bass_subgains[1] = param[6];
                if ((param[7] >= -480) && (param[7] <= 0))
                    DeviceDAPVirtualBass.virtual_bass_subgains[2] = param[7];
                if ((param[8] >= 0) && (param[8] <= 375))
                    DeviceDAPVirtualBass.virtual_bass_low_mix_freq = param[8];
                if ((param[9] >= 281) && (param[9] <= 938))
                    DeviceDAPVirtualBass.virtual_bass_high_mix_freq = param[9];
                ALOGI("-dap_virtual_bass %d %d %d %d %d %d %d %d %d %d",param[0], param[1], param[2], param[3], param[4],
                    param[5], param[6], param[7], param[8], param[9]);
            }
        } else if (strcmp(opt, "dap_regulator") == 0) {
            DAPRegulator regulator;
            char *ptr = mConfigParams[index];
            if (ms_get_int_from_str(&ptr, &regulator.regulator_enable) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &regulator.regulator_mode) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &regulator.regulator_overdrive) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &regulator.regulator_timbre) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &regulator.regulator_distortion) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &regulator.reg_nb_bands) < 0)
                goto eq_error;
            if (regulator.reg_nb_bands > 20)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, regulator.reg_nb_bands,
                &regulator.reg_band_center[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, regulator.reg_nb_bands,
                &regulator.reg_low_thresholds[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, regulator.reg_nb_bands,
                &regulator.reg_high_thresholds[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, regulator.reg_nb_bands,
                &regulator.reg_isolated_bands[0]) < 0)
                goto eq_error;
            DeviceDAPRegulator = regulator;
            ALOGI("-dap_regulator: %d",regulator.regulator_enable);
        } else if (strcmp(opt, "dap_optimizer") == 0) {
            DAPOptimizer optimizer;
            char *ptr = mConfigParams[index];
            if (ms_get_int_from_str(&ptr, &optimizer.optimizer_enable) < 0)
                goto eq_error;
            if (ms_get_int_from_str(&ptr, &optimizer.opt_nb_bands) < 0)
                goto eq_error;
            if (optimizer.opt_nb_bands > 20)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, optimizer.opt_nb_bands,
                &optimizer.opt_band_center_freq[0]) < 0)
                goto eq_error;
            if (ms_get_int_array_from_str(&ptr, optimizer.opt_nb_bands * mMaxChannels,
                &optimizer.opt_band_gains[0]) < 0)
                goto eq_error;
            DeviceDAPOptimizer = optimizer;
            ALOGI("-dap_optimizer %d",optimizer.optimizer_enable);
        }
eq_error:
        index++;
        opt = NULL;
    }

    ALOGV("-%s()", __FUNCTION__);
    return mConfigParams;
}


char **DolbyMS12ConfigParams::GetDolbyMS12RuntimeConfigParams(int *argc)
{
    ALOGD("+%s()", __FUNCTION__);

    if (argc && mConfigParams) {
        char params_bin[] = "ms12_exec";
        sprintf(mConfigParams[mParamNum++], "%s", params_bin);
        SetFunctionalSwitchesRuntime(mConfigParams, &mParamNum);
        SetDdplusSwitches(mConfigParams, &mParamNum);
        //SetPCMSwitchesRuntime(mConfigParams, &mParamNum);
        SetOTTProcessingGraphSwitchesRuntime(mConfigParams, &mParamNum);
        if (mDAPInitMode) {
            SetDAPDeviceSwitches(mConfigParams, &mParamNum);
            SetDAPContentSwitches(mConfigParams, &mParamNum);
        }
        *argc = mParamNum;
        ALOGV("%s() line %d argc %d\n", __FUNCTION__, __LINE__, *argc);
        //here is to check the config params

        int config_params_check = 1;
        if (config_params_check) {
            int i = 0;
            for (i = 0; i < mParamNum; i++) {
                ALOGD("param #%d: %s\n", i, mConfigParams[i]);
            }
        }
    }

    ALOGD("-%s()", __FUNCTION__);
    return mConfigParams;
}

char **DolbyMS12ConfigParams::GetDolbyMS12RuntimeConfigParams_lite(int *argc)
{
    ALOGD("+%s()", __FUNCTION__);

    if (argc && mConfigParams) {
        char params_bin[] = "ms12_exec";
        sprintf(mConfigParams[mParamNum++], "%s", params_bin);
        SetFunctionalSwitchesRuntime_lite(mConfigParams, &mParamNum);
        *argc = mParamNum;
        ALOGV("%s() line %d argc %d\n", __FUNCTION__, __LINE__, *argc);
        //here is to check the config params

        int config_params_check = 1;
        if (config_params_check) {
            int i = 0;
            for (i = 0; i < mParamNum; i++) {
                ALOGD("param #%d: %s\n", i, mConfigParams[i]);
            }
        }
    }

    ALOGD("-%s()", __FUNCTION__);
    return mConfigParams;
}

char **DolbyMS12ConfigParams::PrepareConfigParams(int max_raw_size, int max_column_size)
{
    ALOGD("+%s() line %d\n", __FUNCTION__, __LINE__);
    int i = 0;
    int cnt = 0;
    char **ConfigParams = (char **)malloc(sizeof(char *)*max_raw_size);
    if (ConfigParams == NULL) {
        ALOGE("%s::%d, malloc error\n", __FUNCTION__, __LINE__);
        goto Error_Prepare;
    }

    for (i = 0; i < MAX_ARGC; i++) {
        ConfigParams[i] = (char *)malloc(max_column_size);
        if (ConfigParams[i] == NULL) {
            ALOGE("%s() line %d, malloc error\n", __FUNCTION__, __LINE__);
            for (cnt = 0; cnt < i; cnt++) {
                free(ConfigParams[cnt]);
                ConfigParams[cnt] = NULL;
            }
            free(ConfigParams);
            ConfigParams = NULL;
            goto Error_Prepare;
        }
    }
    ALOGD("+%s() line %d\n", __FUNCTION__, __LINE__);
    return ConfigParams;
Error_Prepare:
    ALOGD("-%s() line %d error prepare\n", __FUNCTION__, __LINE__);
    return NULL;
}

void DolbyMS12ConfigParams::CleanupConfigParams(char **ConfigParams, int max_raw_size)
{
    ALOGD("+%s() line %d\n", __FUNCTION__, __LINE__);
    int i = 0;
    for (i = 0; i < max_raw_size; i++) {
        if (ConfigParams[i]) {
            free(ConfigParams[i]);
            ConfigParams[i] = NULL;
        }
    }

    if (ConfigParams) {
        free(ConfigParams);
        ConfigParams = NULL;
    }

    ALOGD("-%s() line %d\n", __FUNCTION__, __LINE__);
    return ;
}

void DolbyMS12ConfigParams::ResetConfigParams(void)
{
    ALOGV("+%s() line %d\n", __FUNCTION__, __LINE__);
    int i = 0;
    if (mConfigParams) {
        for (i = 0; i < MAX_ARGC; i++) {
            if (mConfigParams[i]) {
                memset(mConfigParams[i], 0, MAX_ARGV_STRING_LEN);
            }
        }
    }
    mParamNum = 0;//reset the input params
    mHasAssociateInput = false;
    mHasSystemInput = false;
    mMainFlags = 1;
    ALOGV("%s() mHasAssociateInput %d mHasSystemInput %d\n", __FUNCTION__, mHasAssociateInput, mHasSystemInput);
    ALOGV("-%s() line %d\n", __FUNCTION__, __LINE__);
    return ;
}

}//end android



