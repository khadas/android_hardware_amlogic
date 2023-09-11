/* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: C++ file
 */

#define LOG_TAG "earc_utils"
//#define LOG_NDEBUG 0


#include <earc_utils.h>

#define CDS_VERSION 0x1
#define CDS_MAX  256

static void earc_cds_conf_to_str(char *earc_cds, char *cds_str, int hex, struct aml_arc_hdmi_desc *hdmi_descs)
{
    char *cds_blocks;
    char *audio_blocks;
    char cds_blockid;
    int blen = 0, dlen = 0, i, j, n = 0, m, index = 0;
    int tag_code = 0;

    if (!cds_str || !earc_cds)
        return;

    cds_str[0] = '\0';
    /* bypass version */
    cds_blocks = &earc_cds[1];
    for (i = 0; i < CDS_MAX - 1;) {
        /* block id */
        cds_blockid = cds_blocks[i];
        blen = cds_blocks[1 + i]; /* block length */
        if (cds_blockid == 1 || cds_blockid == 2) {
            audio_blocks = &cds_blocks[2 + i];
            for (j = 0; j < blen;) {
                /* CTA-861-G Audio Data Block */
                ALOGI("%s, tagl:%#x\n", __FUNCTION__, audio_blocks[j]);
                dlen = audio_blocks[j] & 0x1f; /* length of audio data block */
                tag_code = (audio_blocks[j] & 0xe0) >> 5;

                /* so far only get Audio Data Block Tag(tag_code = 1) */
                for (m = 0; m < dlen && tag_code == 1; m ++) {
                    /* skip 3 bytes which is for pcm format */
                    if (m % 3 == 0 && ((audio_blocks[1 + j + m] >> 3) & 0xf) == 0x1) {
                        m += 2;
                        continue;
                    }
                    if (hex) {
                        cds_str[index++] = audio_blocks[1 + j + m];
                    } else {
                        sprintf(cds_str + strlen(cds_str), "%d, ", audio_blocks[1 + j + m]);
                    }
                }
                /* Dolby Audio and Dolby Atmos
                 * over HDMI Specification.
                 * The audio_hw_profile.c fils also
                 * has the detail description.
                 */
                if (tag_code == 0x7) {
                    if (audio_blocks[j + 1] == 0x11 &&
                        audio_blocks[j + 2] == 0x46 &&
                        audio_blocks[j + 3] == 0xD0 &&
                        audio_blocks[j + 4] == 0x00 &&
                        audio_blocks[j + 6] == 0x01)
                        hdmi_descs->mat_fmt.MAT_PCM_48kHz_only = true;
                }
                if (tag_code == 1)
                    n += dlen;
                j += dlen + 1;
                ALOGV("%s, j:%d, cds_str:%s\n", __FUNCTION__, j, cds_str);
            }

            i += blen + 2;
        } else if (cds_blockid == 3) {
            /* ignore now */
            i += blen + 2;
        } else {
            break;
        }
    }

    if (!hex) {
        int length = strlen(cds_str);
        cds_str[length - 2] = '\0';
    }

    ALOGI("%s, bytes:%d, cds_str:%s:end\n", __FUNCTION__, n, cds_str);
}

/*
 * fetch CDS from eARC_RX, and will update CDS to EDID
 * cds_str: CTA short audio descriptor
 */
int earctx_fetch_cds(struct aml_mixer_handle *amixer, char *cds_str, int hex, struct aml_arc_hdmi_desc *hdmi_descs)
{
    char earc_cds[CDS_MAX] = {0};
    int retry = 0, retry_max = 5;

    while (retry++ < retry_max) {
        aml_mixer_ctrl_get_array(amixer, AML_MIXER_ID_EARCTX_CDS, earc_cds, CDS_MAX);

        earc_cds_conf_to_str(earc_cds, cds_str, hex, hdmi_descs);

        if (cds_str[0] == '\0')
            usleep(50 * 1000);
        else
            break;
    }

    return 0;
}
