/*
 * Copyright (C) 2020 Amlogic Corporation.
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



#ifndef  _AUDIO_AVSYNC_TABLE_H_
#define _AUDIO_AVSYNC_TABLE_H_

/* we use this table to tune the AV sync case for NTS,
 * if the value is big, it can delay video
 * if the value is small, it can advance the video
 */

/*below MS12 tunning is for roku tv*/
#define  AVSYNC_MS12_NONTUNNEL_PCM_LATENCY               (75)
#define  AVSYNC_MS12_NONTUNNEL_RAW_LATENCY               (15)
#define  AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY             (-15)
#define  AVSYNC_MS12_TUNNEL_PCM_LATENCY                  (85)
#define  AVSYNC_MS12_TUNNEL_RAW_LATENCY                  (80)
#define  AVSYNC_MS12_TUNNEL_ATMOS_LATENCY                (-20)

#define  AVSYNC_MS12_PCM_OUT_LATENCY                     (-40)
#define  AVSYNC_MS12_RAW_OUT_LATENCY                     (0)


#define  AVSYNC_MS12_NONTUNNEL_PCM_LATENCY_PROPERTY      "vendor.media.audio.hal.ms12.nontunnel.pcm"
#define  AVSYNC_MS12_NONTUNNEL_RAW_LATENCY_PROPERTY      "vendor.media.audio.hal.ms12.nontunnel.raw"
#define  AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY_PROPERTY    "vendor.media.audio.hal.ms12.nontunnel.atmos"
#define  AVSYNC_MS12_TUNNEL_PCM_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.tunnel.pcm"
#define  AVSYNC_MS12_TUNNEL_RAW_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.tunnel.raw"
#define  AVSYNC_MS12_TUNNEL_ATMOS_LATENCY_PROPERTY       "vendor.media.audio.hal.ms12.tunnel.atmos"

#define  AVSYNC_MS12_PCM_OUT_LATENCY_PROPERTY            "vendor.media.audio.hal.ms12.pcmout"
#define  AVSYNC_MS12_RAW_OUT_LATENCY_PROPERTY            "vendor.media.audio.hal.ms12.rawout"



/*below DDP tunning is for roku tv*/
#define  AVSYNC_DDP_NONTUNNEL_PCM_LATENCY               (0)
#define  AVSYNC_DDP_NONTUNNEL_RAW_LATENCY               (0)
#define  AVSYNC_DDP_TUNNEL_PCM_LATENCY                  (0)
#define  AVSYNC_DDP_TUNNEL_RAW_LATENCY                  (0)


#define  AVSYNC_DDP_NONTUNNEL_PCM_LATENCY_PROPERTY      "vendor.media.audio.hal.ddp.nontunnel.pcm"
#define  AVSYNC_DDP_NONTUNNEL_RAW_LATENCY_PROPERTY      "vendor.media.audio.hal.ddp.nontunnel.raw"
#define  AVSYNC_DDP_TUNNEL_PCM_LATENCY_PROPERTY         "vendor.media.audio.hal.ddp.tunnel.pcm"
#define  AVSYNC_DDP_TUNNEL_RAW_LATENCY_PROPERTY         "vendor.media.audio.hal.ddp.tunnel.raw"



#endif

