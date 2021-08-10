/*
 * Copyright (C) 2016 The Android Open Source Project
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

//#define VENDOR_DUMPSTATE_DEBUG

#include "DumpstateDevice.h"

#include <log/log.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "DumpstateUtil.h"

using android::os::dumpstate::CommandOptions;
using android::os::dumpstate::DumpFileToFd;
using android::os::dumpstate::RunCommandToFd;

namespace android {
namespace hardware {
namespace dumpstate {
namespace V1_1 {
namespace implementation {

#ifdef VENDOR_DUMPSTATE_DEBUG
/*
 * Converts seconds to milliseconds.
 */
#define SEC_TO_MSEC(second) (second * 1000)

/*
 * Converts milliseconds to seconds.
 */
#define MSEC_TO_SEC(millisecond) (millisecond / 1000)

const uint64_t NANOS_PER_SEC = 1000000000;

typedef enum{
    /* Explicitly change the `uid` and `gid` to be `shell`.*/
    DROP_ROOT,
    /* Don't change the `uid` and `gid`. */
    DONT_DROP_ROOT,
    /* Prefix the command with `/PATH/TO/su root`. Won't work non user builds. */
    SU_ROOT
}PrivilegeMode;

static const char* kSuPath = "/system/xbin/su";
static bool waitpid_with_timeout(pid_t pid, int timeout_ms, int* status) {
    sigset_t child_mask, old_mask;
    sigemptyset(&child_mask);
    sigaddset(&child_mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &child_mask, &old_mask) == -1) {
        printf("*** sigprocmask failed: %s\n", strerror(errno));
        return false;
    }

    timespec ts;
    ts.tv_sec = MSEC_TO_SEC(timeout_ms);
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    int ret = TEMP_FAILURE_RETRY(sigtimedwait(&child_mask, nullptr, &ts));
    int saved_errno = errno;

    // Set the signals back the way they were.
    if (sigprocmask(SIG_SETMASK, &old_mask, nullptr) == -1) {
        printf("*** sigprocmask failed: %s\n", strerror(errno));
        if (ret == 0) {
            return false;
        }
    }
    if (ret == -1) {
        errno = saved_errno;
        if (errno == EAGAIN) {
            errno = ETIMEDOUT;
        } else {
            printf("*** sigtimedwait failed: %s\n", strerror(errno));
        }
        return false;
    }

    pid_t child_pid = waitpid(pid, status, WNOHANG);
    if (child_pid != pid) {
        if (child_pid != -1) {
            printf("*** Waiting for pid %d, got pid %d instead\n", pid, child_pid);
        } else {
            printf("*** waitpid failed: %s\n", strerror(errno));
        }
        return false;
    }
    return true;
}

uint64_t Nanotime() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec * NANOS_PER_SEC + ts.tv_nsec);
}

int RunCommand(const std::string& title, const std::vector<std::string>& full_command, PrivilegeMode mode, int timeout) {
    if (full_command.empty()) {
        ALOGE("No arguments on RunCommandToFd(%s)\n", title.c_str());
        return -1;
    }

    int size = full_command.size() + 1;  // null terminated
    int starting_index = 0;
    if (mode== SU_ROOT) {
        starting_index = 2;  // "su" "root"
        size += starting_index;
    }

    std::vector<const char*> args;
    args.resize(size);

    std::string command_string;
    if (mode == SU_ROOT) {
        args[0] = kSuPath;
        command_string += kSuPath;
        args[1] = "root";
        command_string += " root ";
    }
    for (size_t i = 0; i < full_command.size(); i++) {
        args[i + starting_index] = full_command[i].data();
        command_string += args[i + starting_index];
        if (i != full_command.size() - 1) {
            command_string += " ";
        }
    }
    args[size - 1] = nullptr;

    const char* command = command_string.c_str();
    const char* path = args[0];

    uint64_t start = Nanotime();
    pid_t pid = fork();

    /* handle error case */
    if (pid < 0) {
        //if (!silent) dprintf(fd, "*** fork: %s\n", strerror(errno));
        ALOGE("*** fork: %s\n", strerror(errno));
        return pid;
    }

    /* handle child case */
    if (pid == 0) {
        ALOGD("exit child process start\n");
        /* make sure the child dies when dumpstate dies */
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        /* just ignore SIGPIPE, will go down with parent's */
        struct sigaction sigact;
        memset(&sigact, 0, sizeof(sigact));
        sigact.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sigact, nullptr);

        execvp(path, (char**)args.data());
        // execvp's result will be handled after waitpid_with_timeout() below, but
        // if it failed, it's safer to exit dumpstate.
        ALOGD("execvp on command '%s' failed (error: %s)\n", command, strerror(errno));
        // Must call _exit (instead of exit), otherwise it will corrupt the zip
        // file.
        sleep(1000*1000);

        ALOGD("exit child process, execvp on command '%s' failed (error: %s)\n", command, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    /* handle parent case */
    int status;
    bool ret = waitpid_with_timeout(pid, timeout, &status);

    uint64_t elapsed = Nanotime() - start;
    if (!ret) {
        if (errno == ETIMEDOUT) {
            ALOGE("*** command '%s' timed out after %.3fs (killing pid %d)\n", command,
                   static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
        } else {
            ALOGE("command '%s': Error after %.4fs (killing pid %d)\n", command,
                   static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
        }
        kill(pid, SIGTERM);
        if (!waitpid_with_timeout(pid, 5000, nullptr)) {
            kill(pid, SIGKILL);
            if (!waitpid_with_timeout(pid, 5000, nullptr)) {
                ALOGE("could not kill command '%s' (pid %d) even with SIGKILL.\n", command, pid);
            }
        }
        return -1;
    }

    if (WIFSIGNALED(status)) {
        ALOGE("*** command '%s' failed: killed by signal %d\n", command, WTERMSIG(status));
    } else if (WIFEXITED(status) && WEXITSTATUS(status) > 0) {
        status = WEXITSTATUS(status);
        ALOGE("*** command '%s' failed: exit code %d\n", command, status);
    }

    return status;
}

int readSys(const char *path, char *buf, int count) {
    int fd, len = -1;

    if ( NULL == buf ) {
        ALOGE("buf is NULL");
        return len;
    }

    memset(buf, 0, count);

    if ((fd = open(path, O_RDONLY)) < 0) {
        ALOGE("readSys, open %s fail. Error info [%s]", path, strerror(errno));
        return len;
    }

    len = read(fd, buf, count);
    if (len < 0) {
        ALOGE("read error: %s, %s\n", path, strerror(errno));
    }

    close(fd);
    return len;
}
#endif


// Methods from ::android::hardware::dumpstate::V1_0::IDumpstateDevice follow.
Return<void> DumpstateDevice::dumpstateBoard(const hidl_handle& handle) {
#ifdef VENDOR_DUMPSTATE_DEBUG
    (void) handle;

    std::string path("/proc/pagetrace");
    char buf[2048] = {0};
    readSys(path.c_str(), buf, 2048);
    ALOGI("read:%s ,value: %s", path.c_str(), buf);

    //RunCommand("panel", {"vendor/bin/sh", "-c", "echo dump > /sys/class/lcd/debug"}, DONT_DROP_ROOT, 1000*1000);
    return Void();
#endif

    if (handle == nullptr || handle->numFds < 1) {
        ALOGE("no FDs\n");
        return Void();
    }

    int fd = handle->data[0];
    if (fd < 0) {
        ALOGE("invalid FD: %d\n", handle->data[0]);
        return Void();
    }

    DumpFileToFd(fd, "LITTLE cluster time-in-state", "/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state");
    //clock master
    DumpFileToFd(fd, "clkmsr", "/sys/kernel/debug/aml_clkmsr/clkmsr");

    //interrupts
    DumpFileToFd(fd, "INTERRUPTS", "/proc/interrupts");

    //page trace
    DumpFileToFd(fd, "pagetrace", "/proc/pagetrace");

    DumpFileToFd(fd, "rdma_mgr", "/sys/module/rdma_mgr/parameters/reset_count");

    //vframe
    DumpFileToFd(fd, "vframe", "/sys/class/video/vframe_states");
    DumpFileToFd(fd, "vframe", "/sys/class/ppmgr/ppmgr_vframe_states");
    DumpFileToFd(fd, "vframe", "/sys/class/ionvideo/vframe_states");
    DumpFileToFd(fd, "vframe", "/sys/class/vfm/map");

    //osd registers informations: /sys/class/graphics/fb0
    RunCommandToFd(fd, "OSD register", {"vendor/bin/sh", "-c", "echo dump > /sys/devices/platform/fb/graphics/fb0/debug"}, CommandOptions::WithTimeout(1).Build());
    DumpFileToFd(fd, "fb0", "/sys/devices/platform/fb/graphics/fb0/window_axis");
    DumpFileToFd(fd, "fb0", "/sys/devices/platform/fb/graphics/fb0/scale_width");
    DumpFileToFd(fd, "fb0", "/sys/devices/platform/fb/graphics/fb0/scale_height");
    RunCommandToFd(fd, "screencap", {"vendor/bin/sh", "-c", "screencap -p /sdcard/dump.png"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dd screen", {"vendor/bin/sh", "-c", "dd if=/dev/graphics/fb0 of=/sdcard/osd.dump.bin"}, CommandOptions::WithTimeout(1).Build());

    //video infor
    RunCommandToFd(fd, "amvideo", {"vendor/bin/sh", "-c", "echo 1 > /sys/module/amvideo/parameters/debug_flag"}, CommandOptions::WithTimeout(1).Build());
    DumpFileToFd(fd, "video", "/sys/class/video/video_state");
    DumpFileToFd(fd, "video", "/sys/class/video/frame_width");
    DumpFileToFd(fd, "video", "/sys/class/video/frame_height");

    DumpFileToFd(fd, "video", "/sys/class/video/axis");
    DumpFileToFd(fd, "video", "/sys/class/video/crop");
    DumpFileToFd(fd, "video", "/sys/class/video/screen_mode");

    DumpFileToFd(fd, "video", "/sys/module/amvideo/parameters/process_3d_type");
    DumpFileToFd(fd, "video", "/sys/class/video/frame_rate");

    DumpFileToFd(fd, "displaymode", "/sys/class/display/mode");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/disp_mode");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/attr");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/rawedid");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/disp_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/dc_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/aud_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/hdr_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/hdr_cap2");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/dv_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/dv_cap2");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/allm_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/contenttype_cap");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/hpd_state");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/hdcp_lstore");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/hdcp_mode");
    DumpFileToFd(fd, "displaymode", "/sys/class/amhdmitx/amhdmitx0/cedst_policy");

    //panel
    RunCommandToFd(fd, "panel", {"vendor/bin/sh", "-c", "echo dump > /sys/class/lcd/debug"}, CommandOptions::WithTimeout(1).Build());
    //backlight
    DumpFileToFd(fd, "backlight", "/sys/class/aml_bl/status");
    DumpFileToFd(fd, "backlight", "/sys/class/aml_bl/pwm");

    //vout
    DumpFileToFd(fd, "vout", "/sys/class/display/vinfo");

    // Dump CBUS/VCBUS registers
    /*
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo c8834400 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo c883c000 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo c883c200 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0106c00 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0106e00 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0107000 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0107200 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0109c00 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "CBUS/VCBUS", {"vendor/bin/sh", "-c", "echo d0109e00 128 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());
    */

    //Dump HDMI registers
    DumpFileToFd(fd, "aud_cts", "/sys/kernel/debug/amhdmitx/aud_cts");
    DumpFileToFd(fd, "bus_reg", "/sys/kernel/debug/amhdmitx/bus_reg");
    DumpFileToFd(fd, "hdmi_pkt", "/sys/kernel/debug/amhdmitx/hdmi_pkt");
    DumpFileToFd(fd, "hdmi_reg", "/sys/kernel/debug/amhdmitx/hdmi_reg");
    DumpFileToFd(fd, "hdmi_timing", "/sys/kernel/debug/amhdmitx/hdmi_timing");

    //DI
    RunCommandToFd(fd, "DI", {"vendor/bin/sh", "-c", "echo dumpreg > /sys/class/deinterlace/di0/debug"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "DI", {"vendor/bin/sh", "-c", "echo state > /sys/class/deinterlace/di0/debug"}, CommandOptions::WithTimeout(1).Build());
    DumpFileToFd(fd, "DI", "/sys/module/di/parameters/di_vscale_skip_count_real");
    DumpFileToFd(fd, "DI", "/sys/class/deinterlace/di0/provider_vframe_status");
    DumpFileToFd(fd, "DI", "/sys/class/video/video_state");
    DumpFileToFd(fd, "DI", "/sys/class/video/vframe_states");
    DumpFileToFd(fd, "DI", "/sys/class/amvecm/dump_reg");

    //super scaler state
    DumpFileToFd(fd, "super scaler", "/sys/module/amvideo/parameters/sharpness1_sr2_ctrl_32d7");
    DumpFileToFd(fd, "super scaler", "/sys/module/amvideo/parameters/sharpness1_sr2_ctrl_3280");

    //RunCommandToFd(fd, "CORE0", {"vendor/bin/sh", "-c", "echo 0xd010c800 0x65 > /sys/kernel/debug/aml_reg/dump;cat /sys/kernel/debug/aml_reg/dump"}, CommandOptions::WithTimeout(1).Build());

    //vdin
    RunCommandToFd(fd, "vdin", {"vendor/bin/sh", "-c", "echo state >/sys/devices/platform/vdin0/vdin/vdin0/attr"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "vdin", {"vendor/bin/sh", "-c", "echo state >/sys/devices/platform/vdin1/vdin/vdin1/attr"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "vdin", {"vendor/bin/sh", "-c", "echo start > /sys/devices/platform/vdin0/vdin/vdin0/vf_log;sleep 2;echo print > /sys/devices/platform/vdin0/vdin/vdin0/vf_log"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "vdin", {"vendor/bin/sh", "-c", "echo dump_reg > /sys/devices/platform/vdin0/vdin/vdin0/attr"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "vdin", {"vendor/bin/sh", "-c", "echo dump_reg > /sys/devices/platform/vdin1/vdin/vdin1/attr"}, CommandOptions::WithTimeout(1).Build());

    //hdcp
    RunCommandToFd(fd, "HDCP", {"vendor/bin/sh", "-c", "echo state > /sys/class/hdmirx/hdmirx0/debug"}, CommandOptions::WithTimeout(1).Build());

    //hotplug
    DumpFileToFd(fd, "info", "/sys/class/hdmirx/hdmirx0/info");
    //tvafe
    RunCommandToFd(fd, "tvafe", {"vendor/bin/sh", "-c", "echo D > /sys/class/tvafe/tvafe0/reg"}, CommandOptions::WithTimeout(1).Build());

    //amvecm
    DumpFileToFd(fd, "amvecm", "/sys/class/amvecm/hdr_dbg");
    DumpFileToFd(fd, "amvecm", "/sys/class/amvecm/hdr_reg");
    DumpFileToFd(fd, "amvecm", "/sys/module/am_vecm/parameters/cur_csc_type");

    //cm size
    //RunCommandToFd(fd, "cm size", {"vendor/bin/sh", "-c", "echo 0xd01075c0 0x205 > /sys/kernel/debug/aml_reg/paddr;echo 0xd01075c4 > /sys/kernel/debug/aml_reg/paddr;cat /sys/kernel/debug/aml_reg/paddr"}, CommandOptions::WithTimeout(1).Build());
    //RunCommandToFd(fd, "cm size", {"vendor/bin/sh", "-c", "echo 0xd01075c0 0x209 > /sys/kernel/debug/aml_reg/paddr;echo 0xd01075c4 > /sys/kernel/debug/aml_reg/paddr;cat /sys/kernel/debug/aml_reg/paddr"}, CommandOptions::WithTimeout(1).Build());

    //dmc monitor
    /* dmc dump is different for different chip and board
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo 0x000000000  0x80000000  > /sys/class/dmc_monitor/range"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"HDCP\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"HEVC FRONT\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"HEVC BACK\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"H265ENC\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VPU READ1\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VPU READ2\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VPU READ3\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VPU WRITE1\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VPU WRITE2\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"VDEC\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"HCODEC\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    RunCommandToFd(fd, "dmc_monitor", {"vendor/bin/sh", "-c", "echo \"GE2D\" > /sys/class/dmc_monitor/device"}, CommandOptions::WithTimeout(1).Build());
    DumpFileToFd(fd, "dmc_monitor", "/sys/class/dmc_monitor/dump");
    */

    return Void();
}

Return<DumpstateStatus> DumpstateDevice::dumpstateBoard_1_1(const hidl_handle& handle, const DumpstateMode mode,
                                                            uint64_t /*timeoutMillis*/) {
   if (handle == nullptr || handle->numFds < 1) {
            ALOGE("no FDs\n");
            return DumpstateStatus::ILLEGAL_ARGUMENT;
    }

    int fd = handle->data[0];
    if (fd < 0) {
        ALOGE("invalid FD: %d\n", fd);
        return DumpstateStatus::ILLEGAL_ARGUMENT;
    }
    switch (mode) {
        case DumpstateMode::FULL:
        case DumpstateMode::DEFAULT:
            DumpstateDevice::dumpstateBoard(handle);
            return DumpstateStatus::OK;

        case DumpstateMode::INTERACTIVE:
        case DumpstateMode::REMOTE:
        case DumpstateMode::WEAR:
        case DumpstateMode::CONNECTIVITY:
        case DumpstateMode::WIFI:
        case DumpstateMode::PROTO:
            ALOGE("The requested mode is not supported: %s\n", toString(mode).c_str());
            return DumpstateStatus::UNSUPPORTED_MODE;

        default:
            ALOGE("The requested mode is invalid: %s\n", toString(mode).c_str());
            return DumpstateStatus::ILLEGAL_ARGUMENT;
        }

}

// Methods from ::android::hardware::dumpstate::V1_1::IDumpstateDevice follow.
Return<void> DumpstateDevice::setVerboseLoggingEnabled(bool enable)
{
    ::android::base::SetProperty(kVerboseLoggingProperty, enable ? "true" : "false");
    return Void();
}

Return<bool> DumpstateDevice::getVerboseLoggingEnabled()
{
    return ::android::base::GetBoolProperty(kVerboseLoggingProperty, false);
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
