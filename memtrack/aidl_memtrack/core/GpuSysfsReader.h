
#pragma once

#include <inttypes.h>
#include <sys/types.h>

namespace GpuSysfsReader {
uint64_t memtrack_get_gpuMem(int pid = 0);
uint64_t memtrack_get_EGL_Mem(int pid = 0);


constexpr char kSysfsDevicePath[] = "/sys/class/misc/mali0/device";

} // namespace GpuSysfsReader

