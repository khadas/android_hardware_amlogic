/*
 * Copyright (C) 2010 Amlogic Corporation.
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

#ifndef _AUDIO_HW_PROFILE_H_
#define _AUDIO_HW_PROFILE_H_

int get_external_card(int type);
char*  get_hdmi_sink_cap(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs);
char*  get_hdmi_sink_cap_new(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs);
char*  get_hdmi_sink_cap_dolbylib(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs, int conv_support);
char*  get_hdmi_sink_cap_dolby_ms12(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs);
char *get_hdmi_arc_cap(struct audio_hw_device *dev, const char *keys, audio_format_t format);
char *strdup_hdmi_arc_cap_default(const char *keys, audio_format_t format);
char *strdup_a2dp_cap_default(const char *keys, audio_format_t format);

/*@ brief get the TV board inside capbility
 * return the strdup.
 */
char *strdup_tv_platform_cap_default(const char *keys, audio_format_t format);

/*@ brief out_get_parameters wrapper about the support sampling_rates/channels/formats
 * return the strdup.
 */
char *out_get_parameters_wrapper_about_sup_sampling_rates__channels__formats(const struct audio_stream *stream, const char *keys);

#endif
