/*
 * Copyright (C) 2014-2019 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef PLATFORM_SDK_VERSION
#error PLATFORM_SDK_VERSION is not defined
#endif

/* Using ashmem is deprecated from Android 10. Use memfd instead. */
/* GPUCORE-21005: Disabled until potential SELinux issues are resolved. */
#define GRALLOC_USE_MEMFD 0

#if GRALLOC_USE_MEMFD
#include <sys/syscall.h>
#include <linux/memfd.h>
#else
#include <cutils/ashmem.h>
#endif

#include <log/log.h>
#include <sys/mman.h>

#if GRALLOC_VERSION_MAJOR == 1
#include <hardware/gralloc1.h>
#elif GRALLOC_VERSION_MAJOR == 0
#include <hardware/gralloc.h>
#endif

#include "mali_gralloc_module.h"
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_buffer_priv.h"

/*
 * Allocate shared memory for attribute storage. Only to be
 * used by gralloc internally.
 *
 * Return 0 on success.
 */
int gralloc_buffer_attr_allocate(private_handle_t *hnd)
{
	int rval = -1;

	if (!hnd)
	{
		goto out;
	}

	if (hnd->share_attr_fd >= 0)
	{
		ALOGW("Warning share attribute fd already exists during create. Closing.");
		close(hnd->share_attr_fd);
	}

#if GRALLOC_USE_MEMFD
	hnd->share_attr_fd = syscall(__NR_memfd_create, "gralloc_shared_attr", MFD_ALLOW_SEALING);
#else
	hnd->share_attr_fd = ashmem_create_region("gralloc_shared_attr", PAGE_SIZE);
#endif
	if (hnd->share_attr_fd < 0)
	{
		ALOGE("Failed to allocate shared attribute region");
		goto err_out;
	}

#if GRALLOC_USE_MEMFD
	rval = ftruncate(hnd->share_attr_fd, PAGE_SIZE);
	if (rval < 0)
	{
		ALOGE("Failed to allocate a page for shared attribute region, err: %d", rval);
		goto err_out;
	}

	rval = fcntl(hnd->share_attr_fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
	if (rval < 0)
	{
		ALOGE("Failed to seal memfd shared attribute region, err: %d", rval);
		goto err_out;
	}
#endif

	/*
	 * Default protection on the shm region is PROT_EXEC | PROT_READ | PROT_WRITE.
	 *
	 * Personality flag READ_IMPLIES_EXEC which is used by some processes, namely gdbserver,
	 * causes a mmap with PROT_READ to be translated to PROT_READ | PROT_EXEC.
	 *
	 * If we were to drop PROT_EXEC here with a call to ashmem_set_prot_region()
	 * this can potentially cause clients to fail importing this gralloc attribute buffer
	 * with EPERM error since PROT_EXEC is not allowed.
	 *
	 * Because of this we keep the PROT_EXEC flag.
	 */

	hnd->attr_base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_attr_fd, 0);

	if (hnd->attr_base != MAP_FAILED)
	{
		/* The attribute region contains signed integers only.
		 * The reason for this is because we can set a value less than 0 for
		 * not-initialized values.
		 */
		attr_region *region = (attr_region *)hnd->attr_base;

		memset(hnd->attr_base, 0xff, PAGE_SIZE);
		munmap(hnd->attr_base, PAGE_SIZE);
		hnd->attr_base = MAP_FAILED;
	}
	else
	{
		ALOGE("Failed to mmap shared attribute region");
		goto err_out;
	}

	rval = 0;
	goto out;

err_out:

	if (hnd->share_attr_fd >= 0)
	{
		close(hnd->share_attr_fd);
		hnd->share_attr_fd = -1;
	}

out:
	return rval;
}

/*
 * Frees the shared memory allocated for attribute storage.
 * Only to be used by gralloc internally.

 * Return 0 on success.
 */
int gralloc_buffer_attr_free(private_handle_t *hnd)
{
	int rval = -1;

	if (!hnd)
	{
		goto out;
	}

	if (hnd->share_attr_fd < 0)
	{
		ALOGE("Shared attribute region not avail to free");
		goto out;
	}

	if (hnd->attr_base != MAP_FAILED)
	{
		ALOGW("Warning shared attribute region mapped at free. Unmapping");
		munmap(hnd->attr_base, PAGE_SIZE);
		hnd->attr_base = MAP_FAILED;
	}

	close(hnd->share_attr_fd);
	hnd->share_attr_fd = -1;
	rval = 0;

out:
	return rval;
}
