/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <android-base/properties.h>
#include <log/log.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <inttypes.h>

#include "DumpstateUtil.h"

#include "Dumpstate.h"

using android::os::dumpstate::CommandOptions;
using android::os::dumpstate::DumpFileToFd;
using android::os::dumpstate::RunCommandToFd;

namespace aidl {
namespace android {
namespace hardware {
namespace dumpstate {

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

const char kVerboseLoggingProperty[] = "persist.dumpstate.verbose_logging.enabled";

ndk::ScopedAStatus Dumpstate::dumpstateBoard(const std::vector<::ndk::ScopedFileDescriptor>& in_fds,
                                             IDumpstateDevice::DumpstateMode in_mode,
                                             int64_t in_timeoutMillis) {
    (void)in_timeoutMillis;

    if (in_fds.size() < 1) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "No file descriptor");
    }

    int fd = in_fds[0].get();
    if (fd < 0) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid file descriptor");
    }

    switch (in_mode) {
        case IDumpstateDevice::DumpstateMode::FULL:
            return dumpstateBoardImpl(fd, true);

        case IDumpstateDevice::DumpstateMode::DEFAULT:
            return dumpstateBoardImpl(fd, false);

        case IDumpstateDevice::DumpstateMode::INTERACTIVE:
        case IDumpstateDevice::DumpstateMode::REMOTE:
        case IDumpstateDevice::DumpstateMode::WEAR:
        case IDumpstateDevice::DumpstateMode::CONNECTIVITY:
        case IDumpstateDevice::DumpstateMode::WIFI:
        case IDumpstateDevice::DumpstateMode::PROTO:
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(ERROR_UNSUPPORTED_MODE,
                                                                           "Unsupported mode");

        default:
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "Invalid mode");
    }

}

ndk::ScopedAStatus Dumpstate::getVerboseLoggingEnabled(bool* _aidl_return) {
    *_aidl_return = getVerboseLoggingEnabledImpl();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Dumpstate::setVerboseLoggingEnabled(bool in_enable) {
    ::android::base::SetProperty(kVerboseLoggingProperty, in_enable ? "true" : "false");
    return ndk::ScopedAStatus::ok();
}

bool Dumpstate::getVerboseLoggingEnabledImpl() {
    return ::android::base::GetBoolProperty(kVerboseLoggingProperty, false);
}

void Dumpstate::dumpstateBoardOfSystem(int fd, int64_t maxtime) {
    (void)maxtime;

    DumpFileToFd(fd, "LITTLE cluster time-in-state", "/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state");
    //clock master
    DumpFileToFd(fd, "clkmsr", "/sys/kernel/debug/aml_clkmsr/clkmsr");

    //interrupts
    DumpFileToFd(fd, "INTERRUPTS", "/proc/interrupts");

    //page trace
    DumpFileToFd(fd, "pagetrace", "/proc/pagetrace");

    DumpFileToFd(fd, "rdma_mgr", "/sys/module/rdma_mgr/parameters/reset_count");

    //codec_mm
    DumpFileToFd(fd, "codec_mm config", "/sys/class/codec_mm/config");
    DumpFileToFd(fd, "codec_mm dump", "/sys/class/codec_mm/codec_mm_dump");
    DumpFileToFd(fd, "codec_mm keeper", "/sys/class/codec_mm/codec_mm_keeper_dump");
    DumpFileToFd(fd, "codec_mm scatter", "/sys/class/codec_mm/codec_mm_scatter_dump");
    DumpFileToFd(fd, "codec_mm fastplay enable", "/sys/class/codec_mm/fastplay_enable");
    DumpFileToFd(fd, "codec_mm tvp enable", "/sys/class/codec_mm/tvp_enable");
    DumpFileToFd(fd, "codec_mm tvp_region", "/sys/class/codec_mm/tvp_region");

    //dmabuf_manage
    DumpFileToFd(fd, "dmabuf_manage_config", "/sys/class/dmabuf_manage/dmabuf_manage_config");
    DumpFileToFd(fd, "dmabuf_manage_dump", "/sys/class/dmabuf_manage/dmabuf_manage_dump");

    //resource_mgr
    DumpFileToFd(fd, "resource_mgr ver", "/sys/class/resource_mgr/ver");
    DumpFileToFd(fd, "resource_mgr config", "/sys/class/resource_mgr/config");
    DumpFileToFd(fd, "resource_mgr res", "/sys/class/resource_mgr/res");
    DumpFileToFd(fd, "resource_mgr usage", "/sys/class/resource_mgr/usage");
    return;
}

void Dumpstate::dumpstateBoardOfAudio(int fd, int64_t maxtime) {
    (void)maxtime;

    ALOGI("dumpstateBoardOfAudio() Fd: %d", fd);

    //license decoder efuse check
    DumpFileToFd(fd, "Efuse dolby_enable", "/sys/class/amaudio/dolby_enable");
    DumpFileToFd(fd, "Efuse dts_enable", "/sys/class/amaudio/dts_enable");

    return;
}

void Dumpstate::dumpstateBoardOfDisplay(int fd, int64_t maxtime) {
    char buf[PATH_MAX] = { 0 };
    char path[PATH_MAX] = { 0 };
    int len = 0;

    if (snprintf(buf, PATH_MAX, "/proc/self/fd/%d", fd) > 0) {
        len = readlink(buf, path, PATH_MAX - 1);
        if (len > 0) {
            path[len] = 0;
            ALOGI("dumpstateBoardOfDisplay file %s", path);
            RunCommandToFd(fd, "Display", {"/vendor/bin/dumpstate_display", path},
                CommandOptions::WithTimeout(maxtime).Build());
        }
    }

    //hdmitx
    DumpFileToFd(fd, "hdmitx_reg", "/sys/class/amhdmitx/amhdmitx0/dump_debug_reg");
    DumpFileToFd(fd, "hdmitx_config", "/sys/class/amhdmitx/amhdmitx0/hdmitx_basic_config");
    DumpFileToFd(fd, "hdmitx_pkt", "/sys/class/amhdmitx/amhdmitx0/hdmitx_pkt_dump");
    DumpFileToFd(fd, "hdmitx_cur_status", "/sys/class/amhdmitx/amhdmitx0/hdmitx_cur_status");
    DumpFileToFd(fd, "hdmirx_info", "/sys/class/amhdmitx/amhdmitx0/hdmirx_info");
    DumpFileToFd(fd, "clkmsr", "/sys/class/amhdmitx/amhdmitx0/clkmsr");

    //videotunnel
    DumpFileToFd(fd, "vt_instance", "/sys/class/videotunnel/instance");
    DumpFileToFd(fd, "vt_state", "/sys/class/videotunnel/state");

    //drm
    DumpFileToFd(fd, "state", "/sys/class/drm/card0/state");
    DumpFileToFd(fd, "reg_dump", "/sys/class/drm/card0/reg_dump");

    //GPU
    DumpFileToFd(fd, "gpu_kmd_version", "/sys/module/mali_kbase/version");
    DumpFileToFd(fd, "gpu_used_pages", "/sys/class/misc/mali0/device/gpu_memory");
    DumpFileToFd(fd, "gpu_device_cached_pages", "/sys/class/misc/mali0/device/mem_pool_size");
    DumpFileToFd(fd, "gpu_ctx_cached_pages", "/sys/class/misc/mali0/device/ctx_mem_pool_size");

    return;
}

void Dumpstate::dumpstateBoardOfMedia(int fd, int64_t maxtime) {
    uint64_t start = Nanotime() / NANOS_PER_SEC;
    uint64_t elapsed = 0;
    uint64_t rest = 0;

    if (fd > 0) {
        RunCommandToFd(fd, "Dump Drm info", { "drminfo" });
        DumpFileToFd(fd, "Dump Decoder Status", "/sys/class/dec_report/status");
        DumpFileToFd(fd, "Notify Media Service Event", "/sys/class/resource_mgr/res_report");
        elapsed = Nanotime() / NANOS_PER_SEC - start;
        rest = maxtime - elapsed;
        if (rest > 0)
            sleep(rest);
    }
}

ndk::ScopedAStatus Dumpstate::dumpstateBoardImpl(const int fd, const bool full) {
    uint64_t start = Nanotime() / NANOS_PER_SEC;
    uint64_t elapsed = 0;

    (void)full;
    ALOGI("DumpstateDevice::dumpstateBoard() FD: %d start at %" PRId64, fd, start);
#ifdef VENDOR_DUMPSTATE_DEBUG
    (void) handle;

    std::string path("/proc/pagetrace");
    char buf[2048] = {0};
    readSys(path.c_str(), buf, 2048);
    ALOGI("read:%s ,value: %s", path.c_str(), buf);

    //RunCommand("panel", {"vendor/bin/sh", "-c", "echo dump > /sys/class/lcd/debug"}, DONT_DROP_ROOT, 1000*1000);
    return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "No file descriptor");
#endif
    ALOGD("DumpstateDevice::dumpstateBoard() FD: %d\n", fd);
    dprintf(fd, "verbose logging: %s\n", getVerboseLoggingEnabledImpl() ? "enabled" : "disabled");
    elapsed = Nanotime() / NANOS_PER_SEC - start;
    dumpstateBoardOfSystem(fd, 28 - elapsed);
    elapsed = Nanotime() / NANOS_PER_SEC - start;
    dumpstateBoardOfAudio(fd, 28 - elapsed);
    elapsed = Nanotime() / NANOS_PER_SEC - start;
    dumpstateBoardOfDisplay(fd, 28 - elapsed);
    elapsed = Nanotime() / NANOS_PER_SEC - start;
    dumpstateBoardOfMedia(fd, 28 - elapsed);
    elapsed = Nanotime() / NANOS_PER_SEC;
    ALOGI("DumpstateDevice::dumpstateBoard() elapsed total %" PRId64, elapsed - start);
    return ndk::ScopedAStatus::ok();
}

}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
}  // namespace aidl
