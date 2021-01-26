#define LOG_TAG "Aml_DVB_Audio"
#include <AmlAudioOutPort.h>
#include <utils/Log.h>
#include <unistd.h>
#include <Aml_DVB_Audio.h>

using namespace android;

sp<AmlAudioOutPort> aml_audioport = new AmlAudioOutPort();
int audio_hal_start_decoder(int fmt, int has_video) {
   char temp_buf[64] = {0};
   int ret = aml_audioport->createAudioPatch();
   if (ret == NO_ERROR) {
      aml_audioport->setParameters(String8("hal_param_tuner_in=dtv"));
      sprintf (temp_buf, "hal_param_dtv_audio_fmt=%d", fmt);
      aml_audioport->setParameters(String8(temp_buf));
      memset(temp_buf,0,sizeof(temp_buf));
      sprintf (temp_buf, "hal_param_has_dtv_video=%d", has_video);
      aml_audioport->setParameters(String8(temp_buf));
      aml_audioport->setParameters(String8("hal_param_dtv_patch_cmd=1"));
   } else {
      ALOGI("createAudioPatch fail");
      return -1;
   }
   return NO_ERROR;
}

int audio_hal_stop_decoder(void) {
   aml_audioport->setParameters(String8("hal_param_dtv_patch_cmd=2"));
   return aml_audioport->releaseAudioPatch();
}

int audio_hal_pause_decoder(void) {
   return aml_audioport->setParameters(String8("hal_param_dtv_patch_cmd=3"));
}

int audio_hal_resume_decoder(void) {

  return aml_audioport->setParameters(String8("hal_param_dtv_patch_cmd=4"));
}

int audio_hal_set_ad(int fmt, int pid) {
  ALOGI("fmt %d pid %d",fmt,pid);
  return NO_ERROR;
}

int audio_hal_set_volume(float volume) {
    char temp_buf[64] = {0};
    sprintf (temp_buf, "SOURCE_GAIN=%f %f %f %f ", 1.0,volume,1.0,1.0);
    return aml_audioport->setParameters(String8("temp_buf"));

}

int audio_hal_set_mute(int mute) {
   if (mute) {
       return aml_audioport->setParameters(String8("parental_control_av_mute=true"));
   } else {
       return aml_audioport->setParameters(String8("parental_control_av_mute=false"));
   }

}

int audio_hal_set_output_mode(int mode) {
    char temp_buf[64] = {0};
    sprintf (temp_buf, "hal_param_audio_output_mode=%d", mode);
    return aml_audioport->setParameters(String8(temp_buf));
}

int audio_hal_set_pre_gain(int gain) {
    ALOGI("gain %d",gain);
    return NO_ERROR;
}

int audio_hal_set_pre_mute(int mute) {
    if (mute ) {
       return aml_audioport->setParameters(String8("parental_control_av_mute=1"));
    } else {
       return aml_audioport->setParameters(String8("parental_control_av_mute=0"));
    }
}

int audio_hal_get_status(void *status) {
    ALOGI("status %p",status);
    return NO_ERROR;
}//TBD
