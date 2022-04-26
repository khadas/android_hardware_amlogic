#ifndef __GLOBAL_RESOURCE_H__
#define __GLOBAL_RESOURCE_H__
#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>
#include <cutils/properties.h>
#include <utils/threads.h>
#include <vector>
#include "Isp3a.h"

#define HAL_MAX_CAM_NUM (3)
#define HAL_MAX_FD_NUM (3)

namespace android {
class GlobalResource {
        public:
            static GlobalResource* getInstance();
            static void DeleteInstance();
            int getFd(char* device_name,int id, std::vector<int>&fds);
        private:
            static GlobalResource* mInstance;
            static int mDeviceFd[HAL_MAX_CAM_NUM][HAL_MAX_FD_NUM];
            static Mutex mLock;
            isp3a* mISP;
        private:
            GlobalResource() {};
            ~GlobalResource(){};
};
}
#endif
