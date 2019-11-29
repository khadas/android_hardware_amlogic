/*
 * Copyright (C) 2019 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
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

#include "Accessor.h"
#include "AttributeAccessor.h"
#include "../../../../../src/mali_gralloc_buffer.h"
#include "../../../../../src/mali_gralloc_formats.h"
#include "../../../../../src/mali_fourcc.h"

#include <assert.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <log/log.h>
#include <drm_fourcc.h>


namespace arm {
namespace graphics {
namespace privatebuffer {
namespace V1_0 {
namespace implementation {

namespace pb = arm::graphics::privatebuffer::V1_0;

using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::graphics::common::V1_2::PixelFormat;

/**
 * @brief Obtain the FOURCC corresponding to the given Gralloc internal format.
 *
 * @param hnd Private handle where the format information is stored.
 *
 * @return The DRM FOURCC format or DRM_FORMAT_INVALID in case of errors.
 */
static uint32_t drmFourccFromPrivateHandle(const private_handle_t *hnd)
{
	/* Clean the modifier bits in the internal format. */
	struct table_entry
	{
		uint64_t internal;
		uint32_t fourcc;
	};

	static table_entry table[] = {
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888, DRM_FORMAT_ABGR8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888, DRM_FORMAT_ARGB8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGB_565, DRM_FORMAT_RGB565 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888, DRM_FORMAT_XBGR8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGB_888, DRM_FORMAT_BGR888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102, DRM_FORMAT_ABGR2101010 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616, DRM_FORMAT_ABGR16161616F },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YV12, DRM_FORMAT_YVU420 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV12, DRM_FORMAT_NV12 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV16, DRM_FORMAT_NV16 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV21, DRM_FORMAT_NV21 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y0L2, DRM_FORMAT_Y0L2 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y210, DRM_FORMAT_Y210 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_P010, DRM_FORMAT_P010 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_P210, DRM_FORMAT_P210 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y410, DRM_FORMAT_Y410 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT, DRM_FORMAT_YUYV },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I, DRM_FORMAT_YUV420_8BIT },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I, DRM_FORMAT_YUV420_10BIT },

		/* Deprecated legacy formats, mapped to MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT. */
		{ HAL_PIXEL_FORMAT_YCbCr_422_I, DRM_FORMAT_YUYV },
		/* Deprecated legacy formats, mapped to MALI_GRALLOC_FORMAT_INTERNAL_NV21. */
		{ HAL_PIXEL_FORMAT_YCrCb_420_SP, DRM_FORMAT_NV21 },
		/* Format introduced in Android P, mapped to MALI_GRALLOC_FORMAT_INTERNAL_P010. */
		{ HAL_PIXEL_FORMAT_YCBCR_P010, DRM_FORMAT_P010 },
	};

	const uint64_t unmasked_format = hnd->alloc_format;
	const uint64_t internal_format = (unmasked_format & MALI_GRALLOC_INTFMT_FMT_MASK);
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
	{
		if (table[i].internal == internal_format)
		{
			bool afbc = (unmasked_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK);
			/* The internal RGB565 format describes two different component orderings depending on AFBC. */
			if (afbc && internal_format == MALI_GRALLOC_FORMAT_INTERNAL_RGB_565)
			{
				return DRM_FORMAT_BGR565;
			}
			return table[i].fourcc;
		}
	}

	return DRM_FORMAT_INVALID;
}

/**
 * @brief Extract the part of the DRM modifier stored inside the given internal format and private handle.
 *
 * @param hnd Private handle where part of the modifier information is stored.
 * @param internal_format The internal format, where part of the modifier information is stored.
 *
 * @return The information extracted from the argument, in the form of a DRM modifier.
 */
static uint64_t drmModifierFromInternalFormat(const private_handle_t *hnd, uint64_t internal_format)
{
	if ((internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK) == 0)
	{
		return 0;
	}

	uint64_t modifier = 0;

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK)
	{
		modifier |= AFBC_FORMAT_MOD_SPLIT;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
	{
		modifier |= AFBC_FORMAT_MOD_TILED;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)
	{
		modifier |= AFBC_FORMAT_MOD_DB;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_BCH)
	{
		modifier |= AFBC_FORMAT_MOD_BCH;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM)
	{
		modifier |= AFBC_FORMAT_MOD_YTR;
	}

	/* Extract the block-size modifiers. */
	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
	{
		modifier |= (hnd->is_multi_plane() ?
		             AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 : AFBC_FORMAT_MOD_BLOCK_SIZE_32x8);
	}
	else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)
	{
		modifier |= AFBC_FORMAT_MOD_BLOCK_SIZE_64x4;
	}
	else
	{
		modifier |= AFBC_FORMAT_MOD_BLOCK_SIZE_16x16;
	}

	return DRM_FORMAT_MOD_ARM_AFBC(modifier);
}

static const private_handle_t *getPrivateHandle(const hidl_handle &buffer_handle)
{
	auto hnd = reinterpret_cast<const private_handle_t *>(buffer_handle.getNativeHandle());
	if (private_handle_t::validate(hnd) < 0)
	{
		ALOGE("Error getting buffer pixel format info. Invalid handle.");
		return nullptr;
	}
	return hnd;
}

Return<void> Accessor::getAllocation(const hidl_handle &buffer_handle, getAllocation_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->share_fd, hnd->size);
	return Void();
}

Return<void> Accessor::getAllocatedFormat(const hidl_handle &buffer_handle, getAllocatedFormat_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	uint32_t drm_fourcc = drmFourccFromPrivateHandle(hnd);
	uint64_t drm_modifier = 0;
	if (drm_fourcc != DRM_FORMAT_INVALID)
	{
		drm_modifier |= drmModifierFromInternalFormat(hnd, hnd->alloc_format);
	}
	else
	{
		ALOGE("Error getting the allocated format: returning DRM_FORMAT_INVALID for 0x%" PRIx64 ".",
		      hnd->alloc_format);
	}

	_hidl_cb(pb::Error::NONE, drm_fourcc, drm_modifier);
	return Void();
}

Return<void> Accessor::getRequestedDimensions(const hidl_handle &buffer_handle, getRequestedDimensions_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->width, hnd->height);
	return Void();
}

Return<void> Accessor::getRequestedFormat(const hidl_handle &bufferHandle, getRequestedFormat_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(bufferHandle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, static_cast<PixelFormat>(0));
		return Void();
	}

	_hidl_cb(pb::Error::NONE, static_cast<PixelFormat>(hnd->req_format));
	return Void();
}

Return<void> Accessor::getUsage(const hidl_handle &buffer_handle, getUsage_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, static_cast<pb::BufferUsage>(0));
		return Void();
	}

	pb::BufferUsage usage = static_cast<pb::BufferUsage>(hnd->producer_usage | hnd->consumer_usage);
	_hidl_cb(pb::Error::NONE, usage);
	return Void();
}

Return<void> Accessor::getLayerCount(const hidl_handle &buffer_handle, getLayerCount_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, 0);
		return Void();
	}

	_hidl_cb(pb::Error::NONE, hnd->layer_count);
	return Void();
}

Return<void> Accessor::getPlaneLayout(const hidl_handle &buffer_handle,
                                      getPlaneLayout_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(buffer_handle);
	if (hnd == nullptr)
	{
		_hidl_cb(pb::Error::BAD_HANDLE, NULL);
		return Void();
	}

	std::vector<pb::PlaneLayout> plane_layout;
	for (int i = 0; i < MAX_PLANES && hnd->plane_info[i].byte_stride > 0; ++i)
	{
		pb::PlaneLayout plane;
		plane.offset = hnd->plane_info[i].offset;
		plane.byteStride = hnd->plane_info[i].byte_stride;
		plane.allocWidth = hnd->plane_info[i].alloc_width;
		plane.allocHeight = hnd->plane_info[i].alloc_height;
		plane_layout.push_back(plane);
	}

	_hidl_cb(pb::Error::NONE, plane_layout);
	return Void();
}

Return<void> Accessor::getAttributeAccessor(const hidl_handle& bufferHandle, getAttributeAccessor_cb _hidl_cb)
{
	const private_handle_t *hnd = getPrivateHandle(bufferHandle);
	if (hnd == nullptr)
	{
		_hidl_cb(Error::BAD_HANDLE, NULL);
		return Void();
	}

	const size_t attr_size = PAGE_SIZE;
	void *attr_base = mmap(NULL, attr_size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_attr_fd, 0);

	if (attr_base == MAP_FAILED)
	{
		_hidl_cb(Error::ATTRIBUTE_ACCESS_FAILED, NULL);
		return Void();
	}

	_hidl_cb(Error::NONE, new AttributeAccessor(attr_base, attr_size));
	return Void();
}

IAccessor *HIDL_FETCH_IAccessor(const char *)
{
#ifndef PLATFORM_SDK_VERSION
#error "PLATFORM_SDK_VERSION is not defined"
#endif

#if PLATFORM_SDK_VERSION >= 29
	return new Accessor();
#else
	ALOGE("IAccessor HIDL interface is only supported on Android 10 and above.");
	return nullptr;
#endif
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace privatebuffer
}  // namespace graphics
}  // namespace arm
