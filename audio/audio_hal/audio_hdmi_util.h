/*
 * Copyright (C) 2021 Amlogic Corporation.
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



#ifndef  _AUDIO_HDMI_UTIL_H_
#define _AUDIO_HDMI_UTIL_H_


/*@ brief update edid
 * return void;
 */
void update_edid(struct aml_audio_device *adev, bool default_edid, void *edid_array, int edid_length);

/*@ brief "set_ARC_format" for HDMIRX
 * return zero if success;
 */
int set_arc_hdmi(struct audio_hw_device *dev, char *value, size_t len);

/*@ brief "set_ARC_format" for HDMIRX
 * return zero if success;
 */
int set_arc_format(struct audio_hw_device *dev, char *value, size_t len);

/*@ brief update dolby atmos decoding and rendering cap for ddp sad
 * return zero if success;
 */
int update_dolby_atmos_decoding_and_rendering_cap_for_ddp_sad(
    void *array
    , int count
    , bool is_acmod_28_supported
    , bool is_joc_supported);

/*@ brief update dolby MAT decoding cap for dolby MAT and dolby TRUEHD_sad
 * return zero if success;
 */
int update_dolby_MAT_decoding_cap_for_dolby_MAT_and_dolby_TRUEHD_sad(
    void *array
    , int count
    , bool is_mat_pcm_supported
    , bool is_truehd_supported);

/*@ brief get current edid
 * return zero if success;
 */
int get_current_edid(struct aml_audio_device *adev, char *edid_array, int edid_array_len);

/*@ brief after edited the audio sad, then update edid
 * return zero if success;
 */
int update_edid_after_edited_audio_sad(struct aml_audio_device *adev, struct format_desc *fmt_desc);


#endif //_AUDIO_HDMI_UTIL_H_

