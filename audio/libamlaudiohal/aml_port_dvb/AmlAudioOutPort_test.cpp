/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "AmlAudioOutPort_Test"
#include <AmlAudioOutPort.h>
#include <utils/Log.h>
#include <unistd.h>
#include <Aml_DVB_Audio.h>

//using namespace android;
/*#define DEFAULT_READ_SIZE 1024
int fp_cur_pos = 0;
long file_length = 0;
static int get_file_lenght(char *file_path) {
    //char system_file_path[PROPERTY_VALUE_MAX];
    //memcpy(file_path,path,strlen(path));
    //int ret =property_get(FilePath, (char *)path, NULL);
    if (1) {
        ALOGI("<%s::%d>--[path:%s]", __FUNCTION__, __LINE__, file_path);
        FILE *fp = fopen((const char *)file_path, "r");
        if (fp) {
            fseek(fp,0,SEEK_SET);
            fseek(fp,0,SEEK_END);
            file_length = ftell(fp);
            fseek(fp,0,SEEK_SET);
            ALOGI("<%s::%d>--[file_length:%ld]", __FUNCTION__, __LINE__, file_length);
            fclose(fp);
        }
        fp = NULL;

        return 0;
    }
    else {
        ALOGI("<%s::%d>--[property_get return 0]", __FUNCTION__, __LINE__);
        return -1;
    }
}

static int read_data_from_test_file(char *file_path,void *destiny_buffer, int length) {
    //ALOGI("<%s::%d>", __FUNCTION__, __LINE__);
    int get_data_length = 0;
    int file_eof = 0;

    FILE *fp = fopen((const char *)file_path, "r");
    int read_size = (length > 0) ? length : DEFAULT_READ_SIZE;

    if (fp) {
        fseek(fp, fp_cur_pos, SEEK_SET);
        file_eof = feof(fp);
        if (file_eof == 0) {
            get_data_length = fread(destiny_buffer, 1, read_size, fp);
            ALOGV("<%s::%d>--[get_data_length:%d]--[length:%d]", __FUNCTION__, __LINE__, get_data_length, length);
            fp_cur_pos = fp_cur_pos + get_data_length;
            ALOGI("fp_cur_pos:%0x",fp_cur_pos);
            if (get_data_length < read_size) {
                ALOGI("get_data_length < read_size");
                memset(destiny_buffer, 0, read_size);
            }
            ALOGV("file_length:%0lx ",file_length);
            if (fp_cur_pos >= file_length) //read the test data again
                //fp_cur_pos = 0;
                read_size = 0;
        }
        else {
            ALOGI("<%s::%d>--[file to end]", __FUNCTION__, __LINE__);
            read_size = 0;
        }
        fclose(fp);
    }
    else {
        ALOGI("<%s::%d>--[open %s fail, set 0 data]", __FUNCTION__, __LINE__, file_path);
        memset(destiny_buffer, 0, read_size);
    }
    fp = NULL;

    return read_size;
}

int aml_audio_dump_audio_bitstreams(const char *path, const void *buf, size_t bytes) {
    if (!path) {
        return 0;
    }

    FILE *fp = fopen(path, "a+");
    if (fp) {
        int flen = fwrite((char *)buf, 1, bytes, fp);
        ALOGI("flen:%d",flen);
        fclose(fp);
    }

    return 0;
}
*/

int main(int argc, char** argv) {

     int status = 0;
     char *filepath = nullptr;
     for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'h':
                // hwsync pcm
                //flags = audio_output_flags_t(AUDIO_OUTPUT_FLAG_HW_AV_SYNC|AUDIO_OUTPUT_FLAG_DIRECT);
                break;
            }
        } else {
            filepath = argv[i];
        }
    }
    audio_hal_start_decoder(0,1);
    usleep(60000000);
    audio_hal_stop_decoder();
    /*sp<AmlAudioOutPort> aml_audioport = new AmlAudioOutPort();
    aml_audioport->createAudioPatch();
    usleep(60000000);
    aml_audioport->releaseAudioPatch();
    audio_output_flags_t flags = audio_output_flags_t(AUDIO_OUTPUT_FLAG_PRIMARY);

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'h':
                // hwsync pcm
                flags = audio_output_flags_t(AUDIO_OUTPUT_FLAG_HW_AV_SYNC|AUDIO_OUTPUT_FLAG_DIRECT);
                break;
            }
        } else {
            filepath = argv[i];
        }
    }


    status = aml_audioport->start();
    ALOGI("AmlAudioOutPort start return %d", status);
    unsigned char buffer[1024 * 20];
    if (filepath == nullptr) {
        printf("filepath null\n");
        return -1;
    }
    int size = 8212;
    int read_size = 0;
    int actual_write_size = 0;
    ALOGI("argc:%d argv:%s",argc,argv[1]);
    get_file_lenght(filepath);
    do {
        read_size = read_data_from_test_file(filepath,(void *)buffer, size);
        //aml_audio_dump_audio_bitstreams("/data/dump.es", buffer, read_size);
        actual_write_size = aml_audioport->write((const void *)buffer, read_size,false);
        ALOGI("actual_write_size:%d, request size:%d", actual_write_size, read_size);
    } while (read_size >0);
    aml_audioport->stop();*/
    return status;
}
