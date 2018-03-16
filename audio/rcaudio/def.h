#ifndef HUITONG_DEF_H
#define HUITONG_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#define RAS_CMD_MASK                    0x07
#define RAS_START_CMD                   0x04
#define RAS_DATA_TIC1_CMD               0x01
#define RAS_STOP_CMD                    0x02
#define RAS_DATA_RAW_CMD                0x03

//#ifdef LOG_TAG
//#undef LOG_TAG
//#endif
//#define LOG_TAG "huitong_audio_hw"

//#define LOG_NDEBUG 0
//#define LOG_NDEBUG_FUNCTION
#ifdef LOG_NDEBUG_FUNCTION
#define LOGFUNC(...) ((void)0)
#else
#define LOGFUNC(...) (ALOGD(__VA_ARGS__))
#endif

#ifdef __cplusplus
}
#endif
#endif
