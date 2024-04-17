#include "GpuSysfsReader.h"

#include <log/log.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <android-base/stringprintf.h>
#include <dmabufinfo/dmabufinfo.h>
#include <dmabufinfo/dmabuf_sysfs_stats.h>

#include "filesystem.h"

#undef LOG_TAG
#define LOG_TAG "memtrack-gpusysfsreader"
#define CHAR_BUFFER_SIZE        1024

using namespace GpuSysfsReader;
using namespace android::dmabufinfo;
using namespace android::base;

namespace {
static int sf_pid = 0;
static int debug_level = 1;

static size_t read_pid_gl_used_memory(pid_t pid, char *tmp)
{
	size_t unaccounted_size = 0;
	FILE *ion_fp;
	char line[CHAR_BUFFER_SIZE];

	if ((ion_fp = fopen(tmp, "r")) == NULL) {
		ALOGD("open file %s error %s", tmp, strerror(errno));
		return -errno;
	}

	// Parse clients. Entries appear as follows:
	//kctx			   pid				used_pages
	//----------------------------------------------------
	//f0cd5000		 4370		1511
	while (fgets(line, sizeof(line), ion_fp) != NULL) {
		pid_t	 alloc_pid;
		int 	 num_matched;
		int 	 ctx_used_mem;

		if (unaccounted_size > 0)
			break;
		num_matched = sscanf(line, "%*x %d %d", &alloc_pid, &ctx_used_mem);
		if (num_matched == 2) {
			if (sf_pid == 0 || alloc_pid < sf_pid)
				sf_pid = alloc_pid;
			if (pid == alloc_pid) {
				unaccounted_size = ctx_used_mem;
				fclose(ion_fp);
				ion_fp = NULL;
				return unaccounted_size * PAGE_SIZE;
			}
		}
	}
	//close file fd
	fclose(ion_fp);
	ion_fp = NULL;
	return unaccounted_size * PAGE_SIZE;
}

static size_t sysfs_read_gl_app_cached_memory(pid_t pid, char *tmp)
{
	size_t unaccounted_size = 0;
	FILE *ion_fp;
	char line[CHAR_BUFFER_SIZE];

	if ((ion_fp = fopen(tmp, "r")) == NULL) {
		ALOGD("open file %s error %s", tmp, strerror(errno));
		return -errno;
	}

	// Parse clients. Entries appear as follows:
	//kctx			   pid				cached_pages
	//----------------------------------------------------
	//000000009e078926		 2649		 116
	while (fgets(line, sizeof(line), ion_fp) != NULL) {
		pid_t	 alloc_pid;
		int 	 num_matched;
		int 	 ctx_cached_mem;

		if (unaccounted_size > 0)
			break;
		num_matched = sscanf(line, "%*x %d %d", &alloc_pid, &ctx_cached_mem);
		if (num_matched == 2) {
			if (pid == alloc_pid) {
				unaccounted_size = ctx_cached_mem;
				fclose(ion_fp);
				ion_fp = NULL;
				return unaccounted_size * PAGE_SIZE;
			}
		}
	}
	//close file fd
	fclose(ion_fp);
	ion_fp = NULL;
	return unaccounted_size * PAGE_SIZE;
}

static size_t read_gl_device_cached_memory(pid_t pid)
{
	size_t unaccounted_size = 0;
	FILE *ion_fp = NULL;
	char tmp[CHAR_BUFFER_SIZE];
	char line[CHAR_BUFFER_SIZE];
	bool get_gl_dev_cached_mem = false;
	FILE *fl_fp = NULL;

	/* count GL device cached memory into pid 0,
	 * there's no permission yet to read
     * procfs for this HAL.
	 */
	if (pid == 0) {
		snprintf(tmp, CHAR_BUFFER_SIZE - 1, "%s/%s", kSysfsDevicePath, "mem_pool_size");
		if ((fl_fp = fopen(tmp, "r")) == NULL) {
			ALOGE("open %s failed!", tmp);
			return 0;
		}
		ALOGD("open %s success!", tmp);
		// Entries appear as follows:
		// 7610 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
		size_t buf_size;
		while (fgets(line, sizeof(line), fl_fp) != NULL) {
				ALOGD("line:%s", line);
				if (sscanf(line, "%zd %*d", &buf_size) != 1)
					continue;
				else {
					unaccounted_size = buf_size * PAGE_SIZE;
					fclose(fl_fp);
					ion_fp = NULL;
					get_gl_dev_cached_mem = true;
					break;
				}
			}
		if (!get_gl_dev_cached_mem) {
			fclose(fl_fp);
			fl_fp = NULL;
		}
		}
	return unaccounted_size;
}

static std::string GetProcessComm(const pid_t pid) {
    std::string pid_path = android::base::StringPrintf("/proc/%d/comm", pid);
    std::ifstream in{pid_path};
    if (!in) return std::string("N/A");
    std::string line;
    std::getline(in, line);
    if (!in) return std::string("N/A");
    return line;
}

static uint64_t getDmaBufPerProcess(const std::vector<DmaBuffer>& bufs) {
	// Create a reverse map from pid to dmabufs
    std::unordered_map<pid_t, std::set<ino_t>> pid_to_inodes = {};
    uint64_t total_size = 0;  // Total size of dmabufs in the system
    uint64_t kernel_rss = 0;  // Total size of dmabufs NOT mapped or opened by a process
    for (auto& buf : bufs) {
        for (auto pid : buf.pids()) {
            pid_to_inodes[pid].insert(buf.inode());
        }
        total_size += buf.size();
        if (buf.fdrefs().empty() && buf.maprefs().empty()) {
            kernel_rss += buf.size();
        }
    }
    // Create an inode to dmabuf map. We know inodes are unique..
    std::unordered_map<ino_t, DmaBuffer> inode_to_dmabuf;
    for (auto buf : bufs) {
        inode_to_dmabuf[buf.inode()] = buf;
    }

    uint64_t total_rss = 0, total_pss = 0;
    for (auto& [pid, inodes] : pid_to_inodes) {
        uint64_t pss = 0;
        uint64_t rss = 0;
		if (debug_level) {
			ALOGD("%16s:%-5d\n", GetProcessComm(pid).c_str(), pid);
			ALOGD("%22s %16s %16s %16s %16s\n", "Name", "Rss", "Pss", "nr_procs", "Inode");
		}
        for (auto& inode : inodes) {
            DmaBuffer& buf = inode_to_dmabuf[inode];
            uint64_t proc_pss = buf.Pss();
			if (debug_level)
	            ALOGD("%22s %13" PRIu64 " kB %13" PRIu64 " kB %16zu %16" PRIuMAX "\n",
	                   buf.name().empty() ? "<unknown>" : buf.name().c_str(), buf.size() / 1024,
	                   proc_pss / 1024, buf.pids().size(), static_cast<uintmax_t>(buf.inode()));
            rss += buf.size();
            pss += proc_pss;
        }
		if (debug_level) {
		    ALOGD("%22s %13" PRIu64 " kB %13" PRIu64 " kB %16s\n", "PROCESS TOTAL", rss / 1024,
		           pss / 1024, "");
		    ALOGD("----------------------\n");
		}
        total_rss += rss;
        total_pss += pss;
    }
	if (debug_level)
	    ALOGD("dmabuf total: %" PRIu64 " kB kernel_rss: %" PRIu64 " kB userspace_rss: %" PRIu64
	           " kB userspace_pss: %" PRIu64 " kB\n ",
	           total_size / 1024, kernel_rss / 1024, total_rss / 1024, total_pss / 1024);
	return total_pss;
}

} // namespace

uint64_t GpuSysfsReader::memtrack_get_gpuMem(int pid)
{
    char path[CHAR_BUFFER_SIZE];
    unsigned int result = 0;
    //if the gpu is mali450,return
    FILE *mali_fp;
    if ((mali_fp = fopen(MALI, "r")) != NULL) {
        fclose(mali_fp);
        mali_fp = NULL;
        return result;
    }
    snprintf(path, CHAR_BUFFER_SIZE, "%s/%s", kSysfsDevicePath, "gpu_memory");
    result = read_pid_gl_used_memory(pid, path);
	if (debug_level > 1)
		ALOGD("%s: gl_used_memory:%u", __FUNCTION__, result);
    int result_cache = read_gl_device_cached_memory(pid);
	if (debug_level > 1)
		ALOGD("%s: gl_device_cached_memory:%d", __FUNCTION__, result_cache);
    result += result_cache;
    memset(path,0,sizeof(char) * CHAR_BUFFER_SIZE);
    snprintf(path, CHAR_BUFFER_SIZE, "%s/%s", kSysfsDevicePath, "ctx_mem_pool_size");
    int result_ctx_cache = sysfs_read_gl_app_cached_memory(pid, path);
	if (debug_level > 1)
		ALOGD("%s: gl_ctx_cached_memory:%d", __FUNCTION__, result_ctx_cache);
    result += result_ctx_cache;

    return result;
}

uint64_t GpuSysfsReader::memtrack_get_EGL_Mem(int pid)
{
    std::vector<DmaBuffer> bufs;
    if (pid != -1) {
        if (!ReadDmaBufInfo(pid, &bufs)) {
            ALOGE("Unable to read dmabuf info for %d", pid);
            return 0;
        }
    } else {
        return 0;
    }
	if (bufs.empty()) {
		ALOGE("dmabuf info not found");
		return 0;
	}
	getDmaBufPerProcess(bufs);
	return 0;
}


