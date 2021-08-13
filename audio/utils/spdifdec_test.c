#define LOG_TAG "audio_spdif_test"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <cutils/log.h>
#include <aml_audio_spdifdec.h>

#define BI_SIZE 6144
#define BO_SIZE 0x2000


int main(int argc, char * argv[])
{
    FILE *fi = NULL, *fo = NULL;
    char *bitstream_in = NULL;
    int ret;
    int offset = 0;
    int bi_size = BI_SIZE;
    void *p_spdifdec = NULL;
    void *payload_addr = NULL;
    int32_t n_bytes_payload = 0;
    int n_bytes_spdifdec_consumed = 0;
    char buffer[8] = {0};
    long blk_num;

    if (argc < 5) {
        fprintf(stderr,"Usage: %s -i IEC61937.bin -o audio.raw", argv[0]);
        return -1;
    }

    fi = fopen(argv[2], "rb");
    if (fi == NULL) {
        fprintf(stderr,"Failed to open and read: %s", argv[2]);
        ALOGE("Failed to open and read file: %s!",argv[2]);
        goto ERROR;
    }

    fo = fopen(argv[4], "wb");
    if (fo == NULL) {
        fprintf(stderr,"Failed to open and write : %s", argv[4]);
        ALOGE("Failed to open and write file: %s!",argv[4]);
        goto ERROR;
    }

    long file_length = 0;
    fseek(fi, 0L, SEEK_END);
    file_length = ftell(fi);
    if (bi_size) {
        blk_num = file_length / bi_size;
    } else {
        ALOGE("divisor bi_size = %d", bi_size);
        goto ERROR;
    }

    bitstream_in = (char *)malloc(bi_size);
    if (bitstream_in == NULL) {
        fprintf(stderr, "%s line %d mem!!\n", __FUNCTION__, __LINE__);
        ALOGE("Failed to malloc bitstream_in!");
        goto ERROR;
    }

    ret = aml_spdif_decoder_open((void **)&p_spdifdec);
    if (ret) {
        fprintf(stderr, "SPDIF decoder initialization failed!");
        ALOGE("SPDIF decoder initialization failed!");
        goto ERROR;
    }

    fseek(fi,0,SEEK_SET);

    for (int i = 0; i < blk_num; i++) {
        offset = i * bi_size;
        fseek(fi, offset, SEEK_SET);
        fread(bitstream_in, bi_size, 1, fi);
        memcpy(buffer, bitstream_in, sizeof(buffer));
        ALOGV("0x%2x 0x%2x 0x%2x 0x%2x\n", buffer[0], buffer[1], buffer[2], buffer[3]);

        {
            aml_spdif_decoder_process(p_spdifdec
                , (const void *)(bitstream_in)
                , (int32_t)(bi_size)
                , (int32_t *)&n_bytes_spdifdec_consumed
                , (void **)&payload_addr
                , (int32_t *)&n_bytes_payload);
            offset += n_bytes_spdifdec_consumed;
            ALOGV("n_bytes_spdifdec_consumed %d payload_addr %p n_bytes_payload %d\n", n_bytes_spdifdec_consumed, payload_addr,  n_bytes_payload);

            if (fo && payload_addr && (n_bytes_payload > 0)) {
                fwrite(payload_addr, 1, n_bytes_payload, fo);
            }
        }
    }

ERROR:
    if (bitstream_in) {
        free(bitstream_in);
        bitstream_in = NULL;
    }

    if (fo) {
        fclose(fo);
        fo = NULL;
    }

    if (fi) {
        fclose(fi);
        fi = NULL;
    }

    if (p_spdifdec) {
        aml_spdif_decoder_close(p_spdifdec);
        p_spdifdec = NULL;
    }

  return 0;
}
