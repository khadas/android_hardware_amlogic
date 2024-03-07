/*
 * Copyright (C) 2013 The Android Open Source Project
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
//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_HotplugThread"
#include <android/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <string.h>
#include "EmulatedCameraHotplugThread.h"
#include "EmulatedCameraFactory.h"
#include <poll.h>
#include <cutils/properties.h>

#define FAKE_HOTPLUG_FILE "/data/misc/media/emulator.camera.hotplug"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE+16))

#define SubscriberInfo EmulatedCameraHotplugThread::SubscriberInfo

namespace android {

EmulatedCameraHotplugThread::EmulatedCameraHotplugThread(
    const int* cameraIdArray,
    size_t size) :
        Thread(/*canCallJava*/false),
            mInotifyFd(-1),
            mSocketFd(-1)
{

    mRunning = true;
    //mInotifyFd = 0;
    memset(&sa, 0, sizeof(struct sockaddr_nl));
    for (size_t i = 0; i < size; ++i) {
        //int id = cameraIdArray[i];
#if 0
        if (createFileIfNotExists(id)) {
            mSubscribedCameraIds.push_back(id);
        }
#endif
    }
}

EmulatedCameraHotplugThread::~EmulatedCameraHotplugThread() {
}

status_t EmulatedCameraHotplugThread::requestExitAndWait() {
    CAMHAL_LOGE("%s: Not implemented. Use requestExit + join instead",
          __FUNCTION__);
    return INVALID_OPERATION;
}

void EmulatedCameraHotplugThread::requestExit() {
    Mutex::Autolock al(mMutex);

    CAMHAL_LOGV("%s: Requesting thread exit", __FUNCTION__);
    mRunning = false;

#if 0
    const bool rmWatchFailed = false;

    Vector<SubscriberInfo>::iterator it;
    for (it = mSubscribers.begin(); it != mSubscribers.end(); ++it) {


        if (inotify_rm_watch(mInotifyFd, it->WatchID) == -1) {

            CAMHAL_LOGE("%s: Could not remove watch for camID '%d',"
                  " error: '%s' (%d)",
                 __FUNCTION__, it->CameraID, strerror(errno),
                 errno);

            rmWatchFailed = true ;
        } else {
            CAMHAL_LOGV("%s: Removed watch for camID '%d'",
                __FUNCTION__, it->CameraID);
        }

    }

    if (rmWatchFailed) { // unlikely
        // Give the thread a fighting chance to error out on the next
        // read
        if (TEMP_FAILURE_RETRY(close(mInotifyFd)) == -1) {
            CAMHAL_LOGE("%s: close failure error: '%s' (%d)",
                 __FUNCTION__, strerror(errno), errno);
        }
    }
#endif

    if (shutdown(mSocketFd, SHUT_RD) < 0) {
        CAMHAL_LOGD("shutdown socket failed errno=%s", strerror(errno));
    }
    if (close(mSocketFd) < 0) {
        CAMHAL_LOGD("close socket failed errno=%s", strerror(errno));
    }

    CAMHAL_LOGV("%s: Request exit complete.", __FUNCTION__);
}

status_t EmulatedCameraHotplugThread::readyToRun() {
    Mutex::Autolock al(mMutex);

    mInotifyFd = -1;

    do {
        CAMHAL_LOGV("%s: Initializing inotify", __FUNCTION__);

#if 0
        mInotifyFd = inotify_init();
        if (mInotifyFd == -1) {
            CAMHAL_LOGE("%s: inotify_init failure error: '%s' (%d)",
                 __FUNCTION__, strerror(errno), errno);
            mRunning = false;
            break;
        }
#endif
        memset(&sa,0,sizeof(sa));
        sa.nl_family = AF_NETLINK;
        sa.nl_groups = NETLINK_KOBJECT_UEVENT;
        sa.nl_pid = 0;//getpid(); both is ok

        mSocketFd = socket(AF_NETLINK,SOCK_RAW,NETLINK_KOBJECT_UEVENT);
        if (mSocketFd >= 0) {
            if (bind(mSocketFd,(struct sockaddr *)&sa,sizeof(sa)) == -1) {
                mRunning = false;
                CAMHAL_LOGE("bind error:%s, disable the hotplug thread\n",strerror(errno));
            }
        } else {
            mRunning = false;
            CAMHAL_LOGE("socket creating failed:%s, disable the hotplug thread\n",strerror(errno));
        }

        /**
         * For each fake camera file, add a watch for when
         * the file is closed (if it was written to)
         */
        Vector<int>::const_iterator it, end;
        it = mSubscribedCameraIds.begin();
        end = mSubscribedCameraIds.end();
        for (; it != end; ++it) {
            int cameraId = *it;
            if (!addWatch(cameraId)) {
                mRunning = false;
                break;
            }
        }
    } while(false);

    if (!mRunning) {
        status_t err = -errno;

#if 0
        if (mInotifyFd != -1) {
            TEMP_FAILURE_RETRY(close(mInotifyFd));
        }
#endif

        return err;
    }
    return OK;
}

bool EmulatedCameraHotplugThread::threadLoop() {

    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.camhal.check.dev.path", property, "true");
    if (strstr(property,"true")) {
        int cameraId;
        const char* kDevicePath = "/dev/";
        int videoINotifyFD = inotify_init();
        if (videoINotifyFD < 0) {
            CAMHAL_LOGE("%s: inotify init failed! Exiting threadloop", __FUNCTION__);
            usleep(5000);
            return true;
        }

        int videoWd = inotify_add_watch(videoINotifyFD, kDevicePath, IN_CREATE | IN_DELETE);
        if (videoWd < 0) {
            CAMHAL_LOGE("%s: inotify add watch failed! Exiting threadloop", __FUNCTION__);
            if (videoINotifyFD >= 0)
                close(videoINotifyFD);
            usleep(5000);
            return true;
        }

        struct pollfd videoPollFd = {.fd = videoINotifyFD, .events = POLLIN};

        char eventBuf[512];
        const int kPrefixLen = 5;
        constexpr char kPrefix[] = "video";

        while (mRunning) {
            int pollRet = poll(&videoPollFd, 1, 250);
            if (pollRet == 0) {
                // no read event in 100ms
                videoPollFd.revents = 0;
                continue;
            } else if (pollRet < 0) {
                CAMHAL_LOGV("%s: error while polling for /dev/*: %d", __FUNCTION__, errno);
                videoPollFd.revents = 0;
                continue;
            } else if (videoPollFd.revents & POLLERR) {
                CAMHAL_LOGV("%s: polling /dev/ returned POLLERR", __FUNCTION__);
                videoPollFd.revents = 0;
                continue;
            } else if (videoPollFd.revents & POLLHUP) {
                CAMHAL_LOGV("%s: polling /dev/ returned POLLHUP", __FUNCTION__);
                videoPollFd.revents = 0;
                continue;
            } else if (videoPollFd.revents & POLLNVAL) {
                CAMHAL_LOGV("%s: polling /dev/ returned POLLNVAL", __FUNCTION__);
                videoPollFd.revents = 0;
                continue;
            }
            // videoPollFd.revents must contain POLLIN, so safe to reset it before reading
            videoPollFd.revents = 0;

            int offset = 0;
            int ret = read(videoINotifyFD, eventBuf, sizeof(eventBuf));
            if (ret >= (int)sizeof(struct inotify_event)) {
                while (offset < ret) {
                    struct inotify_event* event = (struct inotify_event*)&eventBuf[offset];
                    if (event->wd == videoWd) {
                        if (!strncmp(kPrefix, event->name, kPrefixLen)) {
                            std::string deviceId(event->name + kPrefixLen);
                            cameraId = stoi(deviceId);
                            if (event->mask & IN_CREATE) {
                                gEmulatedCameraFactory.onStatusChanged(cameraId, CAMERA_DEVICE_STATUS_PRESENT);
                            }
                            if (event->mask & IN_DELETE) {
                                gEmulatedCameraFactory.onStatusChanged(cameraId, CAMERA_DEVICE_STATUS_NOT_PRESENT);
                            }
                        }
                    }
                    offset += sizeof(struct inotify_event) + event->len;
                }
            }
        }

        if (!mRunning) {
            //TEMP_FAILURE_RETRY(close(mInotifyFd));
            if (videoINotifyFD >= 0)
                close(videoINotifyFD);
            return false;
        }
    } else {
        // If requestExit was already called, mRunning will be false
        int len;
        char buf[4096];
        struct iovec iov;
        struct msghdr msg;
        char *v4l2_dev_name_string;
        char *video4linux_string;
        char *camera0_string;
        char *camera1_string;

        char *action_string;
        //int i;
        int cameraId;
        int halStatus;

        while (mRunning) {
            memset(&msg,0,sizeof(msg));
            iov.iov_base=(void *)buf;
            iov.iov_len=sizeof(buf);
            msg.msg_name=(void *)&sa;
            msg.msg_namelen=sizeof(sa);
            msg.msg_iov=&iov;
            msg.msg_iovlen=1;

            len = recvmsg(mSocketFd, &msg, 0);
            if (len < 0) {
                break;
            } else if ((len<32) || (len > (int)sizeof(buf))) {
                CAMHAL_LOGD("invalid message");
                break;
            }
            if (len < 4096)
                buf[len] = '\0';

            //buf like that:    add@/devices/lm1/usb1/1-1/1-1.3/1-1.3:1.0/video4linux/video0 ACTION=add DEVPATH=/devices/lm1/usb1/1-1/1-1.3/1-1.3:1.0/video4linux/video0 ...
            //                  add@/devices/platform/camera0/video4linux/v4l-subdev0 ACTION=add DEVPATH=/devices/platform/camera0/video4linux/v4l-subdev0 ...
            //                  add@/devices/platform/camera0/video4linux/video50 ACTION=add DEVPATH=/devices/platform/camera0/video4linux/video50...
            //                  add@/devices/platform/camera0/video4linux/video60 ACTION=add DEVPATH=/devices/platform/camera0/video4linux/video60 ...
            //                  add@/devices/platform/camera0/media0  ACTION=add
            CAMHAL_LOGD("buf=%s\n", buf);
            video4linux_string = strstr(buf, "video4linux");
            camera0_string = strstr(buf, "camera0");
            camera1_string = strstr(buf, "camera1");

            if (video4linux_string == NULL && camera0_string == NULL && camera1_string == NULL) {
                CAMHAL_LOGD("not video or camera0 or camera1 event\n");
                break;
            }
            if (NULL != video4linux_string) {
                CAMHAL_LOGV("video=%s\n", video4linux_string);
                action_string = strchr(video4linux_string, '\0');
                action_string ++;
                CAMHAL_LOGD("action string=%s\n", action_string);

                if (strstr(action_string, "ACTION=add") != NULL) {
                    halStatus = CAMERA_DEVICE_STATUS_PRESENT;
                } else if (strstr(action_string, "ACTION=remove") != NULL) {
                    halStatus = CAMERA_DEVICE_STATUS_NOT_PRESENT;
                } else {
                    CAMHAL_LOGD("no find add or remove\n");
                    break;
                }

                v4l2_dev_name_string = video4linux_string + 12; // skip video4linux/ - get video60 or v4l-subdev0
                if (0 == strncmp(v4l2_dev_name_string, "video", 5) ) {
                    video4linux_string += 17;
                    cameraId = strtol(video4linux_string, NULL, 10);
                    if (ISP_CAM_VIDEO_DEV_BEGIN_NUM <= cameraId &&
                        cameraId < MIPI_ONLY_CAM_VIDEO_DEV_BEGIN_NUM &&
                        halStatus == CAMERA_DEVICE_STATUS_PRESENT) {
                        // isp video node
                        char dev_name[64];
                        sprintf(dev_name, "%s%d", "/dev/video", cameraId);
                        gEmulatedCameraFactory.onStatusReady(dev_name);
                    } else {
                        gEmulatedCameraFactory.onStatusChanged(cameraId,
                        halStatus);
                    }
                } else {
                    CAMHAL_LOGD(" %s is not v4l2 video device.\n",v4l2_dev_name_string );
                    break;
                }
            } else {
                char * camerax_string = camera0_string;
                if (NULL == camerax_string) {
                    camerax_string = camera1_string;
                }
                if (camerax_string) {
                    action_string = strchr(camerax_string, '\0');
                    action_string ++;

                    if (strstr(action_string, "ACTION=add") != NULL) {
                        // aml media node
                        camerax_string += 13; // skip camera0/media
                        cameraId = strtol(camerax_string, NULL, 10);

                        char dev_name[64];
                        sprintf(dev_name, "%s%d", "/dev/media", cameraId);
                        CAMHAL_LOGD("camera: %s ready. notify\n", dev_name);
                        gEmulatedCameraFactory.onStatusReady(dev_name);
                    }
                }
            }
        }

        if (!mRunning) {
            //TEMP_FAILURE_RETRY(close(mInotifyFd));
            return false;
        }
    }
    return true;
}

String8 EmulatedCameraHotplugThread::getFilePath(int cameraId) const {
    return String8::format(FAKE_HOTPLUG_FILE ".%d", cameraId);
}

bool EmulatedCameraHotplugThread::createFileIfNotExists(int cameraId) const
{
    String8 filePath = getFilePath(cameraId);
    // make sure this file exists and we have access to it
    int fd = TEMP_FAILURE_RETRY(
                open(filePath.string(), O_WRONLY | O_CREAT | O_TRUNC,
                     /* mode = ug+rwx */ S_IRWXU | S_IRWXG ));
    if (fd == -1) {
        CAMHAL_LOGE("%s: Could not create file '%s', error: '%s' (%d)",
             __FUNCTION__, filePath.string(), strerror(errno), errno);
        return false;
    }

    // File has '1' by default since we are plugged in by default
    if (TEMP_FAILURE_RETRY(write(fd, "1\n", /*count*/2)) == -1) {
        CAMHAL_LOGE("%s: Could not write '1' to file '%s', error: '%s' (%d)",
             __FUNCTION__, filePath.string(), strerror(errno), errno);
        close(fd);
        return false;
    }

    TEMP_FAILURE_RETRY(close(fd));
    return true;
}

int EmulatedCameraHotplugThread::getCameraId(String8 filePath) const {
    Vector<int>::const_iterator it, end;
    it = mSubscribedCameraIds.begin();
    end = mSubscribedCameraIds.end();
    for (; it != end; ++it) {
        String8 camPath = getFilePath(*it);

        if (camPath == filePath) {
            return *it;
        }
    }

    return NAME_NOT_FOUND;
}

int EmulatedCameraHotplugThread::getCameraId(int wd) const {
    for (size_t i = 0; i < mSubscribers.size(); ++i) {
        if (mSubscribers[i].WatchID == wd) {
            return mSubscribers[i].CameraID;
        }
    }

    return NAME_NOT_FOUND;
}

SubscriberInfo* EmulatedCameraHotplugThread::getSubscriberInfo(int cameraId)
{
    for (size_t i = 0; i < mSubscribers.size(); ++i) {
        if (mSubscribers[i].CameraID == cameraId) {
            return (SubscriberInfo*)&mSubscribers[i];
        }
    }

    return NULL;
}

bool EmulatedCameraHotplugThread::addWatch(int cameraId) {
    String8 camPath = getFilePath(cameraId);
    int wd = 0;
#if 0
    int wd = inotify_add_watch(mInotifyFd,
                               camPath.string(),
                               IN_CLOSE_WRITE);

    if (wd == -1) {
        CAMHAL_LOGE("%s: Could not add watch for '%s', error: '%s' (%d)",
             __FUNCTION__, camPath.string(), strerror(errno),
             errno);

        mRunning = false;
        return false;
    }
#endif

    CAMHAL_LOGV("%s: Watch added for camID='%d', wd='%d'",
          __FUNCTION__, cameraId, wd);

    SubscriberInfo si = { cameraId, wd };
    mSubscribers.push_back(si);

    return true;
}

bool EmulatedCameraHotplugThread::removeWatch(int cameraId) {
    SubscriberInfo* si = getSubscriberInfo(cameraId);

    if (!si) return false;

#if 0
    if (inotify_rm_watch(mInotifyFd, si->WatchID) == -1) {

        CAMHAL_LOGE("%s: Could not remove watch for camID '%d', error: '%s' (%d)",
             __FUNCTION__, cameraId, strerror(errno),
             errno);

        return false;
    }
#endif

    Vector<SubscriberInfo>::iterator it;
    for (it = mSubscribers.begin(); it != mSubscribers.end(); ++it) {
        if (it->CameraID == cameraId) {
            break;
        }
    }

    if (it != mSubscribers.end()) {
        mSubscribers.erase(it);
    }

    return true;
}

int EmulatedCameraHotplugThread::readFile(String8 filePath) const {

    int fd = TEMP_FAILURE_RETRY(
                open(filePath.string(), O_RDONLY, /*mode*/0));
    if (fd == -1) {
        CAMHAL_LOGE("%s: Could not open file '%s', error: '%s' (%d)",
             __FUNCTION__, filePath.string(), strerror(errno), errno);
        return -1;
    }

    char buffer[1];
    int length;

    length = TEMP_FAILURE_RETRY(
                    read(fd, buffer, sizeof(buffer)));

    int retval;

    CAMHAL_LOGV("%s: Read file '%s', length='%d', buffer='%c'",
         __FUNCTION__, filePath.string(), length, buffer[0]);

    if (length == 0) { // EOF
        retval = 0; // empty file is the same thing as 0
    } else if (buffer[0] == '0') {
        retval = 0;
    } else { // anything non-empty that's not beginning with '0'
        retval = 1;
    }

    TEMP_FAILURE_RETRY(close(fd));

    return retval;
}

} //namespace android
