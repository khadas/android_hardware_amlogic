/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *      used for memory track.
 */

#define LOG_TAG "memtrack_aml"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <inttypes.h>
#include <dirent.h>
#include <stdint.h>

#include <hardware/memtrack.h>
#include <log/log.h>
#include <dirent.h>
#include <cutils/properties.h>

#define IONHEAP "/d/ion/heaps"
#define IONPATH "/d/ion"
#define GPUCTX "/d/mali0/ctx"
#define MALI0 "/d/mali0"
#define PLATFORM_DEVICE "/sys/class/misc/mali0/device"
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define CHAR_BUFFER_SIZE        1024
#define INIT_BUFS_ARRAY_SIZE    1024
#define PAGE_SIZE               4096

#define UNUSED(x) (void)x
/**
 * debug_level:
 * 0 -> close memtrack
 * 1 -> close memtrack debug log (default)
 * 2 -> open memtrack debug log
 * 3 -> choose GPU sysfs path
 */
static int debug_level = 1;

static int sf_pid;

#if BUILD_KERNEL_4_9 == true
/**
 * Auxilary struct to keep information about single buffer.
 */
typedef struct __buf_info_t {
    uint64_t id;
    size_t size;
} buf_info_t;

/**
 * Auxilary struct to keep information about all buffers.
 */
typedef struct __bufs_array {
    size_t length;
    size_t capacity;
    buf_info_t* data;
} bufs_array_t;

/**
 * Preallocate memory for the array of buffer
 *
 * @param[in]     arr       Pointer to bufs_array_t struct defining the new array.
 * @param[in]     init_cap  Number of buffers for which memory should be preallocated.
 */
static void bufs_array_init(bufs_array_t* arr, size_t init_cap) {
    if (arr == NULL) return;
    arr->data = (buf_info_t *)malloc(init_cap * sizeof(buf_info_t));
    arr->capacity = init_cap;
    arr->length = 0;
}

/**
 * Add new buffer to existing array of buffers.
 *
 * @param[in]     arr       Pointer to bufs_array_t struct to which new buffer should be added.
 * @param[in]     buf_info  Pointer to the buf_info_t struct with info about the buffer to add.
 */
static void bufs_array_add(bufs_array_t* arr, buf_info_t* buf_info) {
    if (arr == NULL || buf_info == NULL) return;
    if (arr->capacity == arr->length) {
        arr->capacity *= 2;
        arr->data = (buf_info_t *)realloc(arr->data, arr->capacity * sizeof(buf_info_t));
    }
    arr->data[arr->length++] = *buf_info;
}

/**
 * Free memory used by array of buffers.
 *
 * @param[in]     arr       Poiner to bufs_array_t structure for which memory should be freed.
 */
static void bufs_array_free(bufs_array_t* arr) {
    if (arr == NULL) return;
    free(arr->data);
    arr->data = NULL;
    arr->capacity = arr->length = 0;
}

/**
 * Get total size of the buffers provided in the bufs param
 * using info from the bufs_info param. Assuming that all buffers are sorted.
 *
 * @param[in]     bufs_info     Buffers info array
 * @param[in]     bufs          Array of buffers to calculate total size of
 * @param[in]     bufs_size     The number of elements of bufs array
 *
 * @return total size of the buffers provided in the bufs param
 */
static int get_bufs_size(bufs_array_t* bufs_info, uint64_t* bufs, size_t bufs_size) {
    if (bufs_size == 0 || bufs_info == NULL || bufs == NULL || bufs_info->length == 0) return 0;
    size_t result = 0;
    size_t bufs_i = 0, bi_i = 0;
    while (bufs_i < bufs_size && bi_i < bufs_info->length) {
        if (bufs[bufs_i] < bufs_info->data[bi_i].id) {
            bufs_i++;
        } else if (bufs[bufs_i] > bufs_info->data[bi_i].id) {
            bi_i++;
        } else {
            result += bufs_info->data[bi_i].size;
            bufs_i++;
            bi_i++;
        }
    }
    return result;
}

static int is_allocate_client(const char *client_name, int size) {
    static const char * comms[] = {
        "allocator@2.0-s",
        "allocator@3.0-s",
        "allocator@4.0-s",
    };
    unsigned int i, result = 0;

    // the comm is 16 chars
    if (client_name == NULL || size <= 0) {
        return 0;
    }

    for (i=0; i < sizeof(comms) / sizeof(const char *); i++) {
        result |= (strncmp(client_name, comms[i], size) == 0);
    }

    return result;
}
#endif

static struct hw_module_methods_t memtrack_module_methods = {
    .open = NULL,
};

struct memtrack_record record_templates[] = {
    {
        .flags = MEMTRACK_FLAG_SMAPS_UNACCOUNTED |
                 MEMTRACK_FLAG_PRIVATE |
                 MEMTRACK_FLAG_NONSECURE,
    },

/*
    {
        .flags = MEMTRACK_FLAG_SMAPS_ACCOUNTED |
                 MEMTRACK_FLAG_PRIVATE |
                 MEMTRACK_FLAG_NONSECURE,
    },
*/
};

#if BUILD_KERNEL_4_9 == false
static void memtrack_get_taskname_of_pid(pid_t pid, char ** task_name)
{
    // get pid-name by pid
    char proc_pid_path[CHAR_BUFFER_SIZE];
    char buf[CHAR_BUFFER_SIZE];

    memset(proc_pid_path, 0, sizeof(proc_pid_path));
    sprintf(proc_pid_path, "/proc/%d/comm", pid);
    FILE* fp = fopen(proc_pid_path, "r");
    if (NULL == fp) {
        return;
    } else {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, CHAR_BUFFER_SIZE-1, fp) == NULL) {
            ALOGD("fgets error %s", strerror(errno));
            fclose(fp);
            fp = NULL;
            return;
        }
        fclose(fp);
        fp = NULL;
        sscanf(buf, "%s", *task_name);
        ALOGD("get task_name %s", *task_name);
    }
}
#endif

// just return 0
int aml_memtrack_init(const struct memtrack_module *module __unused)
{
    ALOGD("memtrack init");
    return 0;
}

static size_t read_pid_egl_memory(pid_t pid)
{
#if SKIP_COUNT_ION == true
    return 0;
#endif
    size_t unaccounted_size = 0;
    FILE *ion_fp;
    FILE *egl_fp;
    char tmp[CHAR_BUFFER_SIZE];
    struct dirent  *de;
    DIR *p_dir;

    memset(tmp, 0, sizeof(tmp));
#if BUILD_KERNEL_4_9 == true
    char egl_ion_dir[] = IONHEAP;
    p_dir = opendir( egl_ion_dir );
    if (!p_dir) {
        ALOGD("fail to open %s\n", egl_ion_dir);
        return 0;
    }


    while ((de = readdir(p_dir))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", egl_ion_dir, de->d_name);
        if ((ion_fp = fopen(tmp, "r")) == NULL) {
            ALOGD("open file %s error %s", tmp, strerror(errno));
            closedir(p_dir);
            return 0;
        }
        if ((egl_fp = fopen(tmp, "r")) == NULL) {
            ALOGD("open file %s error %s", tmp, strerror(errno));
            fclose(ion_fp);
            closedir(p_dir);
            ion_fp = NULL;
            p_dir = NULL;
            return 0;
        }
        //Parse bufs. Entries appear as follows:
        //buf= ece0a300 heap_id= 4    size= 8486912    kmap= 0    dmap= 0
        int num_entries_found = 0;
        bufs_array_t bufs_array;
        bufs_array_init(&bufs_array, INIT_BUFS_ARRAY_SIZE);
        while (true) {
            char line[CHAR_BUFFER_SIZE];
            buf_info_t buf_info;
            int num_matched;
            bool isExit = false;
            memset(line, 0, sizeof(line));
            if (fgets(line, sizeof(line), ion_fp) == NULL) {
                break;
            }
            num_matched = sscanf(line, "%*s %" PRIx64 " %*s %*u %*s %zu %*s %*d %*s %*d",
                &buf_info.id, &buf_info.size);
            if (num_matched == 2) {
                // We've found an entry ...
                num_entries_found++;
                bufs_array_add(&bufs_array, &buf_info);
            } else {
                // Early termination: if we fail to parse a line after
                // hitting the correct section then we know we've finished.
                if (strstr(line, "orphaned")) {
                    isExit = true;
                }
                if (num_entries_found && isExit) {
                    break;
                }
            }
        }

        // Parse clients. Entries appear as follows:
        //client(ecfebd00)  composer@2.2-se      pid(3226)

        uint64_t* surface_flinger_bufs = NULL;
        size_t surface_flinger_bufs_size = 0;
        uint64_t* current_pids_bufs = NULL;
        size_t current_pids_bufs_size = 0;
        uint64_t* bufs;
        size_t* bufs_size;
        int need_to_rescan = 0;
        pid_t sf_pid = 0;
        while (true) {
            char     line[CHAR_BUFFER_SIZE];
            char     alloc_client[CHAR_BUFFER_SIZE];
            pid_t    alloc_pid;
            int      num_matched;
            memset(line, 0, sizeof(line));
            if (need_to_rescan == 1) {
                need_to_rescan = 0;
            } else {
                if (fgets(line, sizeof(line), egl_fp) == NULL) {
                    break;
                }
            }
            num_matched = sscanf(line, "client(%*x) %1023s pid(%d)",
                                        alloc_client, &alloc_pid);

            if (num_matched == 2) {
                // We've found an entry ...

                if (alloc_pid == pid) {
                    if (current_pids_bufs == NULL) {
                        current_pids_bufs = (uint64_t*)malloc(bufs_array.length * sizeof(uint64_t));
                    }
                    bufs = current_pids_bufs;
                    bufs_size = &current_pids_bufs_size;
                } else if (strncmp(alloc_client, "surfaceflinger", sizeof(alloc_client)) == 0) {
                    sf_pid = alloc_pid;
                    if (surface_flinger_bufs == NULL) {
                        surface_flinger_bufs = (uint64_t*)malloc(bufs_array.length * sizeof(uint64_t));
                    }
                    bufs = surface_flinger_bufs;
                    bufs_size = &surface_flinger_bufs_size;
                } else {
                    bufs = NULL;
                }
                while (bufs && (*bufs_size < bufs_array.length)) {
                    //Parse client's buffers lines, which look like this:
                    // handle= ecf9da00     buf= ecf1d600 heap_id= 4    size= 8486912

                    uint64_t buf_id;
                    if (fgets(line, sizeof(line), egl_fp) == NULL) {
                        break;
                    }
                    num_matched = sscanf(line, " handle= %*x buf= %" PRIx64 " %*s %*d %*s %*u", &buf_id);
                    if (num_matched == 1) {
                        // We've found an entry ...
                        bufs[(*bufs_size)++] = buf_id;
                    } else {
                        //We are out of entries, but most likely moved to the next client.
                        //Don't need to read another line from file.
                        if (strstr(line, "---")) {
                            need_to_rescan = 1;
                            break;
                        }

                    }
                }
            }
        }

        if (pid == sf_pid) {
            // TODO:
            // If the buffer alloc requested by app, we need remove from surfaceflinger
            // the buffer was allocated by SF, but should account to app side!!!
            // count to app, then app itself can apply policy for memory saving!
            //
            // IMPL later...
            unaccounted_size += get_bufs_size(&bufs_array, surface_flinger_bufs,
                                              surface_flinger_bufs_size);
        } else if (current_pids_bufs_size > 0) {
            //Remove buffers from current_pids_bufs that are present in surface_flinger_bufs.
            //Assuming that both buf arrays are sorted.
            size_t pid_i_r = 0, pid_i_w = 0, sf_i = 0;
            while (pid_i_r < current_pids_bufs_size) {
                if (sf_i < surface_flinger_bufs_size) {
                    if (current_pids_bufs[pid_i_r] < surface_flinger_bufs[sf_i]) {
                        current_pids_bufs[pid_i_w] = current_pids_bufs[pid_i_r];
                        pid_i_r++;
                        pid_i_w++;
                    } else if (current_pids_bufs[pid_i_r] > surface_flinger_bufs[sf_i]) {
                        sf_i++;
                    } else {
                        pid_i_r++;
                        sf_i++;
                    }
                } else {
                    current_pids_bufs[pid_i_w] = current_pids_bufs[pid_i_r];
                    pid_i_r++;
                    pid_i_w++;
                }
            }
            current_pids_bufs_size = pid_i_w;
            unaccounted_size += get_bufs_size(&bufs_array, current_pids_bufs, current_pids_bufs_size);
        }


        //Free any memory used
        bufs_array_free(&bufs_array);
        if (current_pids_bufs != NULL) {
            free(current_pids_bufs);
            current_pids_bufs = NULL;
            current_pids_bufs_size = 0;
        }
        if (surface_flinger_bufs != NULL) {
            free(surface_flinger_bufs);
            surface_flinger_bufs = NULL;
            surface_flinger_bufs_size = 0;
        }
        fclose(ion_fp);
        fclose(egl_fp);
        ion_fp = NULL;
        egl_fp = NULL;
    }

    closedir(p_dir);
#else
    ALOGD("sf_pid:%d", sf_pid);
    if (sf_pid != pid)
        return 0;
    char egl_ion_dir[] = IONPATH;
    char line[CHAR_BUFFER_SIZE];

    memset(line, 0, sizeof(line));
    snprintf(tmp, CHAR_BUFFER_SIZE-1, "%s/%s", egl_ion_dir, "ion-dev/num_of_alloc_bytes");
    ALOGD("tmp1:%s", tmp);
    if ((ion_fp = fopen(tmp, "r")) == NULL) {
        ALOGD("open file %s error %s", tmp, strerror(errno));
        return 0;
    }
    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        ALOGD("line1:%s", line);
        int num_matched = 0;
        size_t alloc_bytes = 0;
        num_matched = sscanf(line, "%zd", &alloc_bytes);
        ALOGD("alloc_bytes1:%zd", alloc_bytes);
        if (num_matched == 1) {
            unaccounted_size = alloc_bytes;
        }
    }
    fclose(ion_fp);
    ion_fp = NULL;

    memset(tmp, 0, sizeof(tmp));
    memset(line, 0, sizeof(line));
    snprintf(tmp, CHAR_BUFFER_SIZE-1, "%s/%s", egl_ion_dir, "ion_system_heap/num_of_alloc_bytes");
    if ((ion_fp = fopen(tmp, "r")) == NULL) {
        ALOGD("open file %s error %s", tmp, strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        int num_matched = 0;
        size_t alloc_bytes = 0;
        num_matched = sscanf(line, "%zd", &alloc_bytes);
        if (num_matched == 1) {
            unaccounted_size += alloc_bytes;
        }
    }
#endif
    fclose(ion_fp);
    ion_fp = NULL;
    return unaccounted_size;
}

static size_t read_egl_cached_memory(pid_t pid)
{

#if BUILD_KERNEL_4_9 == true
    size_t unaccounted_size = 0;
    FILE *ion_fp;
    char tmp[CHAR_BUFFER_SIZE];
    char line[CHAR_BUFFER_SIZE];
    char egl_ion_dir[] = IONHEAP;

    snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", egl_ion_dir, "vmalloc_ion");
    if ((ion_fp = fopen(tmp, "r")) == NULL) {
        ALOGD("open file %s error %s", tmp, strerror(errno));
        return 0;
    }

    // Parse clients. Entries appear as follows:
    //client(edf39c80)  allocator@2.0-s      pid(3226)
    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        char     alloc_client[CHAR_BUFFER_SIZE];
        pid_t    alloc_pid;
        int      num_matched;

        if (unaccounted_size > 0)
            break;

        num_matched = sscanf(line, "client(%*x) %1023s pid(%d)",
                                    alloc_client, &alloc_pid);
        if (num_matched == 2) {
            if (pid == alloc_pid && is_allocate_client(alloc_client, sizeof(alloc_client))) {
                while (fgets(line, sizeof(line), ion_fp) != NULL) {
                    //Parse client's buffers lines, which look like this:
                    //     total cached         68481024
                    size_t buf_size;
                    num_matched = sscanf(line, " total cached  %zu", &buf_size);
                    if (num_matched == 1) {
                        // We've found an entry ...
                        unaccounted_size = buf_size;
                        break;
                    }
                }
            }
        }
    }

    //close file fd
    fclose(ion_fp);
    ion_fp = NULL;
    if (debug_level == 2 && unaccounted_size > 0)
        ALOGD("EGL cached mtrack: pid %d unaccounted_size:%u\n", pid, unaccounted_size);
    return unaccounted_size;
#else
    UNUSED(pid);
    return 0;
#endif
}

// mali midgard/bifrost
static int debugfs_read_gl_app_cached_memory(char *path)
{
    FILE *file;
    char line[1024];

    int gpu_size = 0;

    if ((file = fopen(path, "r")) == NULL) {
        ALOGD("open file %s error %s", path, strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
            if (sscanf(line, "%d %*d", &gpu_size) != 1)
                continue;
            else
                break;
    }
    fclose(file);
    file = NULL;
    return gpu_size * PAGE_SIZE;
}

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
    //kctx             pid              used_pages
    //----------------------------------------------------
    //f0cd5000       4370       1511
    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        pid_t    alloc_pid;
        int      num_matched;
        int      ctx_used_mem;

        if (unaccounted_size > 0)
            break;
        num_matched = sscanf(line, "%*x %d %d", &alloc_pid, &ctx_used_mem);
        if (debug_level == 2)
            ALOGD("alloc_pid:%d sf_pid:%d", alloc_pid, sf_pid);
        if (num_matched == 2) {
            if (sf_pid == 0 || alloc_pid < sf_pid)
                sf_pid = alloc_pid;
            if (pid == alloc_pid) {
                unaccounted_size = ctx_used_mem;
                if (debug_level == 2)
                    ALOGI("read_pid_gl_used_memory: unaccounted_size=%d", unaccounted_size);
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
    //kctx             pid              cached_pages
    //----------------------------------------------------
    //000000009e078926       2649        116
    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        pid_t    alloc_pid;
        int      num_matched;
        int      ctx_cached_mem;

        if (unaccounted_size > 0)
            break;
        num_matched = sscanf(line, "%*x %d %d", &alloc_pid, &ctx_cached_mem);
        if (debug_level == 2)
                ALOGD("sysfs_read_gl_app_cached_memory num_matched:%d alloc_pid:%d sf_pid:%d", num_matched, alloc_pid, sf_pid);
        if (num_matched == 2) {
            if (pid == alloc_pid) {
                unaccounted_size = ctx_cached_mem;
                if (debug_level == 2)
                    ALOGI("read_pid_gl_used_memory: unaccounted_size=%d", unaccounted_size);
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
    char gl_dev_dir[CHAR_BUFFER_SIZE];
    DIR *platform_dir;
    struct dirent *dir;
    bool get_gl_dev_cached_mem = false;

#if BUILD_KERNEL_4_9 == true
    char gl_ion_dir[] = IONHEAP;

    snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", gl_ion_dir, "vmalloc_ion");
    if ((ion_fp = fopen(tmp, "r")) == NULL) {
        ALOGD("open file %s error %s", tmp, strerror(errno));
        return 0;
    }

    // Parse clients. Entries appear as follows:
    //client(edf39c80)  allocator@2.0-s      pid(3226)
    while (fgets(line, sizeof(line), ion_fp) != NULL) {
        char     alloc_client[CHAR_BUFFER_SIZE];
        pid_t    alloc_pid;
        int      num_matched;
        FILE *fl_fp = NULL;

        if (unaccounted_size > 0)
            break;

        num_matched = sscanf(line, "client(%*x) %1023s pid(%d)",
                                    alloc_client, &alloc_pid);
        if (num_matched == 2) {
            if (pid == alloc_pid &&
                is_allocate_client(alloc_client, sizeof(alloc_client))) {
                platform_dir = opendir(PLATFORM_DEVICE);
                if (platform_dir == NULL) {
                    ALOGE("open %s failed!", PLATFORM_DEVICE);
                    fclose(ion_fp);
                    ion_fp = NULL;
                    return 0;
                }
                while ((dir = readdir(platform_dir))) {
                    if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
                        continue;
                    if (strstr(dir->d_name, "bifrost") ||
                        strstr(dir->d_name, "mali")) {
                        snprintf(gl_dev_dir, CHAR_BUFFER_SIZE, "%s/%s", PLATFORM_DEVICE, dir->d_name);
                        break;
                    }
                }
                closedir(platform_dir);
                snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", gl_dev_dir, "mem_pool_size");
                if ((fl_fp = fopen(tmp, "r")) == NULL) {
                    ALOGE("open %s failed!", tmp);
                    fclose(ion_fp);
                    ion_fp = NULL;
                    return 0;
                }
                ALOGI("open %s success!", tmp);
                // Entries appear as follows:
                // 7610 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
                size_t buf_size;
                while (fgets(line, sizeof(line), fl_fp) != NULL) {
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
                if (!get_gl_dev_cached_mem)
                    fclose(fl_fp);
                    ion_fp = NULL;
                }
            }
        }

    //close file fd
    fclose(ion_fp);
    ion_fp = NULL;
#else
    FILE *fl_fp = NULL;
    char task_name[CHAR_BUFFER_SIZE];
    int num_matched;

    char * sname = task_name;
    memset(task_name, 0, sizeof(task_name));
    memtrack_get_taskname_of_pid(pid, &sname);

    // count GL device cached memory into allocator service
    if (debug_level == 2)
        ALOGI("read_gl_device_cached_memory task_name:%s", task_name);
    if (strstr(task_name, "allocator")) {
        snprintf(tmp, CHAR_BUFFER_SIZE-1, "%s/%s", PLATFORM_DEVICE, "mem_pool_size");
        if ((fl_fp = fopen(tmp, "r")) == NULL) {
            ALOGE("open %s failed!", tmp);
            return 0;
        }
        ALOGI("open %s success!", tmp);
        // Entries appear as follows:
        // 7610 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
        size_t buf_size;
        while (fgets(line, sizeof(line), fl_fp) != NULL) {
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
#endif
    return unaccounted_size;
}

static unsigned int memtrack_get_gpuMem(int pid)
{
    FILE *fp;
    char *cp, tmp[CHAR_BUFFER_SIZE];
    unsigned int result = 0;
    DIR *gpu_debugfs_ctx_dir;
    DIR *gpu_sysfs_ctx_dir;
    struct dirent *dir;
    int gpid = -1;
    char debugfs_gl_mem_dir[] = MALI0;
    char sysfs_gl_mem_dir[] = PLATFORM_DEVICE;

    gpu_debugfs_ctx_dir = opendir(GPUCTX);

    if (!gpu_debugfs_ctx_dir || debug_level == 3) {
        ALOGD("sysfs path");
        snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", sysfs_gl_mem_dir, "gpu_memory");
        result = read_pid_gl_used_memory(pid, tmp);
        if (debug_level == 2 && result > 0)
            ALOGD("pid=%d gl_used_memory=%d", pid, result);
        int result_cache = read_gl_device_cached_memory(pid);
        result += result_cache;
        if (debug_level == 2 && result_cache > 0)
            ALOGD("pid=%d gl_device_cached_memory=%d",
                pid, result_cache);
        memset(tmp,0,sizeof(char) * CHAR_BUFFER_SIZE);
        snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", sysfs_gl_mem_dir, "ctx_mem_pool_size");
        int result_ctx_cache = sysfs_read_gl_app_cached_memory(pid, tmp);
        if (debug_level == 2 && result_cache > 0)
            ALOGD("pid=%d sysfs_read_gl_app_cached_memory=%d",
                pid, result_ctx_cache);
        result += result_ctx_cache;
        closedir(gpu_debugfs_ctx_dir);
        return result;
    } else {
        ALOGD("debugfs path");
        snprintf(tmp, CHAR_BUFFER_SIZE, "%s/%s", debugfs_gl_mem_dir, "gpu_memory");
        result = read_pid_gl_used_memory(pid, tmp);
        if (debug_level == 2 && result > 0)
            ALOGD("pid=%d gl_used_memory=%d", pid, result);
        int result_cache = read_gl_device_cached_memory(pid);
        result += result_cache;
        if (debug_level == 2 && result_cache > 0)
            ALOGD("pid=%d gl_device_cached_memory=%d",
                    pid, result_cache);
        while ((dir = readdir(gpu_debugfs_ctx_dir))) {
            memset(tmp,0,sizeof(char) * CHAR_BUFFER_SIZE);
            strcpy(tmp, dir->d_name);
            if (debug_level > 2) ALOGD("gpudir name=%s\n", dir->d_name);
            if ((cp=strchr(tmp, '_'))) {
                *cp = '\0';
                gpid = atoi(tmp);
                if (debug_level > 2) ALOGD("gpid=%d, pid=%d\n", gpid, pid);
                if (gpid == pid) {
                    sprintf(tmp, GPUCTX"/%s/%s", dir->d_name, "mem_pool_size");
                    result += debugfs_read_gl_app_cached_memory(tmp);
                    closedir(gpu_debugfs_ctx_dir);
                    return result;
                }
            }
        }
        closedir(gpu_debugfs_ctx_dir);
    }
    return result;
}

static int memtrack_get_memory(pid_t pid, enum memtrack_type type,
                             struct memtrack_record *records,
                             size_t *num_records)
{
    size_t unaccounted_size = 0;

    unsigned int gpu_size = 0;
    unsigned int egl_mem_size = 0;
    unsigned int egl_cache_size = 0;

    size_t allocated_records =  ARRAY_SIZE(record_templates);
    *num_records = ARRAY_SIZE(record_templates);

    if (records == NULL) {
        return 0;
    }

    memcpy(records, record_templates, sizeof(struct memtrack_record) * allocated_records);

    if (type == MEMTRACK_TYPE_GL) {
        gpu_size = memtrack_get_gpuMem(pid);
        if (debug_level == 2 && gpu_size > 0)
            ALOGD("GL mtrack: pid %d gpu_size:%u\n", pid, gpu_size);
        unaccounted_size += gpu_size;
    } else if (type == MEMTRACK_TYPE_GRAPHICS) {
        egl_mem_size = read_pid_egl_memory(pid);
        unaccounted_size += egl_mem_size;
        if (debug_level == 2 && egl_mem_size > 0)
            ALOGD("EGL mtrack: pid %d unaccounted_size:%u\n", pid, egl_mem_size);
    } else if (type == MEMTRACK_TYPE_OTHER) {
        egl_cache_size = read_egl_cached_memory(pid);
        unaccounted_size += egl_cache_size;
        if (debug_level == 2 && egl_cache_size > 0)
            ALOGD("EGL mtrack: pid %d egl_cache_size:%u unaccounted_size:%u\n", pid, egl_cache_size, unaccounted_size);
    }

    if (allocated_records > 0) {
        records[0].size_in_bytes = unaccounted_size;
        if (debug_level == 2 && unaccounted_size > 0)
            ALOGD("pid:%d Graphics type:%d unaccounted_size:%u\n", pid, type, unaccounted_size);
    }

    return 0;
}

int aml_memtrack_get_memory(const struct memtrack_module *module __unused,
                                pid_t pid,
                                int type,
                                struct memtrack_record *records,
                                size_t *num_records)
{
    char dbg_level[PROPERTY_VALUE_MAX];

    if (pid <= 0)
        return -EINVAL;
    if (property_get("vendor.memtrack.dbg_level", dbg_level, "1") > 0)
        debug_level = atoi(dbg_level);

    switch (debug_level) {
        case 0:
            return 0;
        default:
            if (type == MEMTRACK_TYPE_GL || type == MEMTRACK_TYPE_GRAPHICS
                || type == MEMTRACK_TYPE_OTHER)
                return memtrack_get_memory(pid, type, records, num_records);
            else
                return -ENODEV;
    }
}

struct memtrack_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = MEMTRACK_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = MEMTRACK_HARDWARE_MODULE_ID,
        .name = "aml Memory Tracker HAL",
        .author = "amlogic",
        .methods = &memtrack_module_methods,
    },

    .init = aml_memtrack_init,
    .getMemory = aml_memtrack_get_memory,
};
