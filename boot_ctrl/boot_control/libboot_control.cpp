/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <libboot_control/libboot_control.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <bootloader_message/bootloader_message.h>
#include <cutils/android_reboot.h>

#include "private/boot_control_definition.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "systemcontrol.h"
#include "ubootenv/Ubootenv.h"

namespace android {
namespace bootable {

using ::android::hardware::boot::V1_1::MergeStatus;


// The number of boot attempts that should be made from a new slot before
// rolling back to the previous slot.
constexpr unsigned int kDefaultBootAttempts = 7;

#define EMMC_USER_PARTITION        "bootloader"
#define EMMC_DEVICE           "/dev/block/by-name/mmcblk0"
#define BOOTLOADER_MAX_SIZE    (4*1024*1024)
/*First 512 bytes in bootloader is signed data*/
#define BOOTLOADER_OFFSET      512
#define GPT_HEADER_SIGNATURE_UBOOT 0x5452415020494645ULL

#define SYS_BOOT_COMPLETE       "/sys/class/tee_info/sys_boot_complete"

static_assert(kDefaultBootAttempts < 8, "tries_remaining field only has 3 bits");

constexpr unsigned int kMaxNumSlots =
    sizeof(bootloader_control::slot_info) / sizeof(bootloader_control::slot_info[0]);
constexpr const char* kSlotSuffixes[kMaxNumSlots] = { "_a", "_b", "_c", "_d" };
constexpr off_t kBootloaderControlOffset = offsetof(bootloader_message_ab, slot_suffix);

static char env_buffer[64];

static uint32_t CRC32(const uint8_t* buf, size_t size) {
  static uint32_t crc_table[256];

  // Compute the CRC-32 table only once.
  if (!crc_table[1]) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t crc = i;
      for (uint32_t j = 0; j < 8; ++j) {
        uint32_t mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      crc_table[i] = crc;
    }
  }

  uint32_t ret = -1;
  for (size_t i = 0; i < size; ++i) {
    ret = (ret >> 8) ^ crc_table[(ret ^ buf[i]) & 0xFF];
  }

  return ~ret;
}

static int reboot_device() {
  if (android_reboot(ANDROID_RB_RESTART2, 0, nullptr) == -1) {
    LOG(ERROR) << "Failed to reboot.";
    return -1;
  }
  while (true) pause();
}

// Return the little-endian representation of the CRC-32 of the first fields
// in |boot_ctrl| up to the crc32_le field.
uint32_t BootloaderControlLECRC(const bootloader_control* boot_ctrl) {
  return htole32(
      CRC32(reinterpret_cast<const uint8_t*>(boot_ctrl), offsetof(bootloader_control, crc32_le)));
}

bool LoadBootloaderControl(const std::string& misc_device, bootloader_control* buffer) {
  android::base::unique_fd fd(open(misc_device.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    PLOG(ERROR) << "failed to open " << misc_device;
    return false;
  }
  if (lseek(fd, kBootloaderControlOffset, SEEK_SET) != kBootloaderControlOffset) {
    PLOG(ERROR) << "failed to lseek " << misc_device;
    return false;
  }
  if (!android::base::ReadFully(fd.get(), buffer, sizeof(bootloader_control))) {
    PLOG(ERROR) << "failed to read " << misc_device;
    return false;
  }
  return true;
}

bool UpdateAndSaveBootloaderControl(const std::string& misc_device, bootloader_control* buffer) {
  buffer->crc32_le = BootloaderControlLECRC(buffer);
  android::base::unique_fd fd(open(misc_device.c_str(), O_WRONLY | O_SYNC));
  if (fd.get() == -1) {
    PLOG(ERROR) << "failed to open " << misc_device;
    return false;
  }
  if (lseek(fd.get(), kBootloaderControlOffset, SEEK_SET) != kBootloaderControlOffset) {
    PLOG(ERROR) << "failed to lseek " << misc_device;
    return false;
  }
  if (!android::base::WriteFully(fd.get(), buffer, sizeof(bootloader_control))) {
    PLOG(ERROR) << "failed to write " << misc_device;
    return false;
  }
  return true;
}

int is_valid_gpt_buf(char *buf)
{
    gpt_header *gpt_h;

    /* determine start of GPT Header in the buffer */
    gpt_h = (gpt_header*)(buf + 512);

    LOG(INFO) << "signature: " << GPT_HEADER_SIGNATURE_UBOOT;
    LOG(INFO) << "gpt header signature: " << gpt_h->signature;

    /* Check the GPT header signature */
    if (gpt_h->signature != GPT_HEADER_SIGNATURE_UBOOT) {
        LOG(ERROR) << "gpt header signature " << gpt_h->signature
            << " != " << GPT_HEADER_SIGNATURE_UBOOT;
        return -1;
    }

    return 0;
}

static int get_gpt_mode(void) {
    int size = 0;
    int ret = 0;
    int fd = open(EMMC_DEVICE, O_RDONLY);
    if (fd < 0) {
        LOG(INFO) << "opem mmcblk0 error";
        return -1;
    }

    char buffer[1024];
    size = read(fd, buffer, 1024);
    if (size != 1024) {
        LOG(INFO) << "read mmcblk0 error";
        close(fd);
        return -1;
    }
    close(fd);

    ret = is_valid_gpt_buf(buffer);
    if (ret == 0) {
        LOG(INFO) << "device is gpt mode";
        return 0;
    } else {
        LOG(INFO) << "device is dts mode";
        return 1;
    }
}

static int set_sys_boot_complete(void)
{
    int fd;
    int len;
    char buf[] = "1";

    fd = open(SYS_BOOT_COMPLETE, O_WRONLY);
    if (fd < 0) {
        LOG(INFO) << "open " << SYS_BOOT_COMPLETE << " failed";
        return -1;
    }

    len = write(fd, buf, sizeof(buf));

    close(fd);

    if (len != sizeof(buf))
        return -1;
    else
        return 0;
}

static int get_sys_boot_complete(void)
{
    int fd;
    int len;
    char buf[8] = {0};

    fd = open(SYS_BOOT_COMPLETE, O_RDONLY);
    if (fd < 0) {
        LOG(INFO) << "open " << SYS_BOOT_COMPLETE << " failed";
        return -1;
    }

    len = read(fd, buf, 8);

    close(fd);

    if (strncmp(buf, "1", 1) == 0)
        return 0;
    else
        return -1;
}


bool write_bootloader_img(unsigned int slot, bool gpt_flag)
{
    int iRet = 0;
    char emmcPartitionPath[128];
    int fd = -1;
    int fd2 = -1;
    bool ret = false;
    char* data = NULL;

    memset(emmcPartitionPath, 0, sizeof(emmcPartitionPath));
    if (slot == 0) {
        if (gpt_flag)
            strcpy(emmcPartitionPath, "/dev/block/by-name/bootloader_a");
        else
            strcpy(emmcPartitionPath, "/dev/block/bootloader0");
    } else {
        if (gpt_flag)
            strcpy(emmcPartitionPath, "/dev/block/by-name/bootloader_b");
        else
            strcpy(emmcPartitionPath, "/dev/block/bootloader1");
    }

    data = (char *)malloc(BOOTLOADER_MAX_SIZE);
    if (data == NULL) {
        LOG(ERROR) << "malloc " << BOOTLOADER_MAX_SIZE << " error";
        goto done;
    }
    memset(data, 0, BOOTLOADER_MAX_SIZE);

    LOG(INFO) << "emmcPartitionPath: " << emmcPartitionPath;
    /* read bootloader.img we write in update_engine
     *
     * in dts mode, bootloader_a/_b is link to boot0/boot1
     * /dev/block/platform/soc/fe08c000.mmc/by-name/bootloader_a --> /dev/block/mmcblk0boot0
     * /dev/block/platform/soc/fe08c000.mmc/by-name/bootloader_b --> /dev/block/mmcblk0boot1
     *
     * for gpt mode, bootloader_a/bootloader_b is real partition table
     *
     * update_engine will update bootloader_a or bootloader_b according to current slot
    */
    fd = open(emmcPartitionPath, O_RDWR);
    if (fd < 0) {
        LOG(ERROR) << "failed to open " << emmcPartitionPath;
        goto done;
    }
    iRet = read(fd, data, BOOTLOADER_MAX_SIZE - BOOTLOADER_OFFSET);
    LOG(INFO) << "iRet = " << iRet;
    if (iRet == BOOTLOADER_MAX_SIZE - BOOTLOADER_OFFSET) {
        LOG(INFO) << "read bootloader img successful";
    } else {
        LOG(ERROR) << "read bootloader img failed";
        goto done;
    }

    if (!gpt_flag) {
        LOG(INFO) << "device is null gpt, check bootloader.img";
        if (is_valid_gpt_buf(data + 0x3DFE00)) {
            LOG(INFO) << "no gpt partition table\n";
        } else {
            LOG(ERROR) << "find gpt partition table, can't update\n";
            ret = true;
            goto done;
        }
    }

    /* We use robust to rollback bootloader.img in uboot
     * bootloader  ---> 0
     * boot0       ---> 1
     * boot1       ---> 2
     * It always bootup from 0 first.
     * and will switch bootloader by the order: 0->1->2->0 ...
     * So we write bootloader.img to bootloader here, and set env
     * reboot_status  ---- reboot_next
     * expect_index   ---- 0
     * update_env     ---- 1
     * First 512 bytes in bootloader is signed data
     * we need write bootloader.img to 512 bytes offset
     * but update_engine just write bootloader.img to /dev/block
     * after reboot, if the bootloader index is just expect_index, update env too
     * if the bootloader index isn't expect_index, update error, back to last slot
    */
    if (gpt_flag) {
        LOG(INFO) << "gpt mode, write mmcblk0boot0";
        fd2 = open("/dev/block/mmcblk0boot0", O_RDWR);
        if (fd2 < 0) {
            LOG(ERROR) << "failed to open /dev/block/mmcblk0boot0";
            goto done;
        }
    } else {
        fd2 = open("/dev/block/by-name/bootloader", O_RDWR);
        if (fd2 < 0) {
            LOG(ERROR) << "failed to open /dev/block/by-name/bootloader ";
            goto done;
        }
    }
    iRet = lseek(fd2, BOOTLOADER_OFFSET, SEEK_SET);
    if (iRet == -1) {
        LOG(ERROR) << "failed to lseek ";
        goto done;
    }
    iRet = write(fd2, data, BOOTLOADER_MAX_SIZE - BOOTLOADER_OFFSET);
    LOG(INFO) << "iRet = " << iRet;
    if (iRet == (BOOTLOADER_MAX_SIZE - BOOTLOADER_OFFSET)) {
        LOG(INFO) << "Write Uboot Image successful";
    } else {
        LOG(ERROR) << "Write Uboot Image failed";
        goto done;
    }

    ret = true;

done:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (fd2 >= 0) {
        close(fd2);
        fd2 = -1;
    }
    if (data != NULL) {
        free(data);
        data = NULL;
    }
    return ret;
}

int is_recovery_mode() {
    int ret = access("/system/bin/recovery", F_OK);
    if (ret == 0) {
        LOG(INFO) << "recovery mode";
        return 1;
    } else {
        LOG(INFO) << "android mode";
        return 0;
    }
}

//just use for recovery mode, android mode need use sc_set_bootenv
int set_bootloader_env(const char* name, const char* value)
{
    Ubootenv *ubootenv = new Ubootenv();
    char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, name);

    if (ubootenv->updateValue(ubootenv_name, value)) {
        PLOG(ERROR) << "could not set boot env";
        delete ubootenv;
        return -1;
    }
    delete ubootenv;
    return 0;
}

//just use for recovery mode, android mode need use sc_read_bootenv
char* get_bootloader_env(const char * name)
{
    Ubootenv *ubootenv = new Ubootenv();
    char ubootenv_name[128] = {0};
    const char *ubootenv_var = "ubootenv.var.";
    sprintf(ubootenv_name, "%s%s", ubootenv_var, name);

    char *uboot_env = (char *)ubootenv->getValue(ubootenv_name);
    if (uboot_env == NULL) {
        delete ubootenv;
        return NULL;
    }
    memset(env_buffer, 0, 64);
    strncpy(env_buffer, uboot_env, strlen(uboot_env));
    delete ubootenv;
    return env_buffer;
}

//android mode only can set/get uboot env by systemocontrol service
//or the uboot env you set will be rewrite by systemcontrol if systemcontrol
//setenv after bootctrl of update_engine
void set_bootloader_env_common(const char* name, const char* value) {
    int mode = is_recovery_mode();
    if (mode == 1) {
        set_bootloader_env(name, value);
    } else {
        std::string tmp(value);
        sc_set_bootenv(name, tmp);
    }
}

char* get_bootloader_env_common(const char * name) {
    int mode = is_recovery_mode();
    if (mode == 1) {
        return get_bootloader_env(name);
    } else {
        std::string tmp;
        sc_read_bootenv(name, tmp);
        memset(env_buffer, 0, 64);
        strncpy(env_buffer, tmp.c_str(), strlen(tmp.c_str()));
        return env_buffer;
    }
}


void InitDefaultBootloaderControl(BootControl* control, bootloader_control* boot_ctrl) {
  memset(boot_ctrl, 0, sizeof(*boot_ctrl));

  unsigned int current_slot = control->GetCurrentSlot();
  if (current_slot < kMaxNumSlots) {
    strlcpy(boot_ctrl->slot_suffix, kSlotSuffixes[current_slot], sizeof(boot_ctrl->slot_suffix));
  }
  boot_ctrl->magic = BOOT_CTRL_MAGIC;
  boot_ctrl->version = BOOT_CTRL_VERSION;

  // Figure out the number of slots by checking if the partitions exist,
  // otherwise assume the maximum supported by the header.
  boot_ctrl->nb_slot = kMaxNumSlots;
  std::string base_path = control->misc_device();
  size_t last_path_sep = base_path.rfind('/');
  if (last_path_sep != std::string::npos) {
    // We test the existence of the "boot" partition on each possible slot,
    // which is a partition required by Android Bootloader Requirements.
    base_path = base_path.substr(0, last_path_sep + 1) + "boot";
    int last_existing_slot = -1;
    int first_missing_slot = -1;
    for (unsigned int slot = 0; slot < kMaxNumSlots; ++slot) {
      std::string partition_path = base_path + kSlotSuffixes[slot];
      struct stat part_stat;
      int err = stat(partition_path.c_str(), &part_stat);
      if (!err) {
        last_existing_slot = slot;
        LOG(INFO) << "Found slot: " << kSlotSuffixes[slot];
      } else if (err < 0 && errno == ENOENT && first_missing_slot == -1) {
        first_missing_slot = slot;
      }
    }
    // We only declare that we found the actual number of slots if we found all
    // the boot partitions up to the number of slots, and no boot partition
    // after that. Not finding any of the boot partitions implies a problem so
    // we just leave the number of slots in the maximum value.
    if ((last_existing_slot != -1 && last_existing_slot == first_missing_slot - 1) ||
        (first_missing_slot == -1 && last_existing_slot == kMaxNumSlots - 1)) {
      boot_ctrl->nb_slot = last_existing_slot + 1;
      LOG(INFO) << "Found a system with " << last_existing_slot + 1 << " slots.";
    }
  }

  for (unsigned int slot = 0; slot < kMaxNumSlots; ++slot) {
    slot_metadata entry = {};

    if (slot < boot_ctrl->nb_slot) {
      entry.priority = 7;
      entry.tries_remaining = kDefaultBootAttempts;
      entry.successful_boot = 0;
    } else {
      entry.priority = 0;  // Unbootable
    }

    // When the boot_control stored on disk is invalid, we assume that the
    // current slot is successful. The bootloader should repair this situation
    // before booting and write a valid boot_control slot, so if we reach this
    // stage it means that the misc partition was corrupted since boot.
    if (current_slot == slot) {
      entry.successful_boot = 1;
    }

    boot_ctrl->slot_info[slot] = entry;
  }
  boot_ctrl->recovery_tries_remaining = 0;

  boot_ctrl->crc32_le = BootloaderControlLECRC(boot_ctrl);
}

// Return the index of the slot suffix passed or -1 if not a valid slot suffix.
int SlotSuffixToIndex(const char* suffix) {
  for (unsigned int slot = 0; slot < kMaxNumSlots; ++slot) {
    if (!strcmp(kSlotSuffixes[slot], suffix)) return slot;
  }
  return -1;
}

// Initialize the boot_control_private struct with the information from
// the bootloader_message buffer stored in |boot_ctrl|. Returns whether the
// initialization succeeded.
bool BootControl::Init() {
  if (initialized_) return true;

  // Initialize the current_slot from the read-only property. If the property
  // was not set (from either the command line or the device tree), we can later
  // initialize it from the bootloader_control struct.
  std::string suffix_prop = android::base::GetProperty("ro.boot.slot_suffix", "");
  if (suffix_prop.empty()) {
    LOG(ERROR) << "Slot suffix property is not set";
    return false;
  }
  int current_slot_m = SlotSuffixToIndex(suffix_prop.c_str());
  if (current_slot_m < 0) {
    LOG(ERROR) << "Fail SlotSuffixToIndex return < 0 (" << current_slot_m << " )";
    return false;
  } else
    current_slot_ = current_slot_m;

  std::string err;
  std::string device = get_bootloader_message_blk_device(&err);
  if (device.empty()) {
    LOG(ERROR) << "Could not find bootloader message block device: " << err;
    return false;
  }

  bootloader_control boot_ctrl;
  if (!LoadBootloaderControl(device.c_str(), &boot_ctrl)) {
    LOG(ERROR) << "Failed to load bootloader control block";
    return false;
  }

  // Note that since there isn't a module unload function this memory is leaked.
  // We use `device` below sometimes, so it's not moved out of here.
  misc_device_ = device;
  initialized_ = true;

  // Validate the loaded data, otherwise we will destroy it and re-initialize it
  // with the current information.
  uint32_t computed_crc32 = BootloaderControlLECRC(&boot_ctrl);
  if (boot_ctrl.crc32_le != computed_crc32) {
    LOG(WARNING) << "Invalid boot control found, expected CRC-32 0x" << std::hex << computed_crc32
                 << " but found 0x" << std::hex << boot_ctrl.crc32_le << ". Re-initializing.";
    InitDefaultBootloaderControl(this, &boot_ctrl);
    UpdateAndSaveBootloaderControl(device.c_str(), &boot_ctrl);
  }

  if (!InitMiscVirtualAbMessageIfNeeded()) {
    return false;
  }

  if (boot_ctrl.slot_info[current_slot_].successful_boot == 1) {
    if (get_sys_boot_complete() != 0) {
      set_sys_boot_complete();
      LOG(INFO) << "call set_sys_boot_complete in init";
    }
  }

  LOG(INFO) << "boot_ctrl.roll_flag = " << boot_ctrl.roll_flag;

  if (boot_ctrl.roll_flag == 1) {
    if (unlink("/data/misc/update_engine/prefs/update-state-next-operation") < 0) {
        LOG(ERROR) << "unlink update-state-next-operation failed.";
    }
    boot_ctrl.roll_flag = 0;
    UpdateAndSaveBootloaderControl(device.c_str(), &boot_ctrl);
  }

  num_slots_ = boot_ctrl.nb_slot;
  return true;
}

unsigned int BootControl::GetNumberSlots() {
  return num_slots_;
}

unsigned int BootControl::GetCurrentSlot() {
  return current_slot_;
}

bool BootControl::MarkBootSuccessful() {
  bootloader_control bootctrl;
  bool ret;
  int flag = 0;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  if (bootctrl.slot_info[current_slot_].successful_boot == 0) {
    if (get_sys_boot_complete() != 0) {
      flag = 1;
      set_sys_boot_complete();
      LOG(INFO) << "call set_sys_boot_complete in MarkBootSuccessful";
    }
  }
  bootctrl.slot_info[current_slot_].successful_boot = 1;
  // tries_remaining == 0 means that the slot is not bootable anymore, make
  // sure we mark the current slot as bootable if it succeeds in the last
  // attempt.
  bootctrl.slot_info[current_slot_].tries_remaining = 1;

  ret = UpdateAndSaveBootloaderControl(misc_device_, &bootctrl);
  if (flag ==1) {
    LOG(INFO) << "reboot for ARB";
    reboot_device();
  }
  return ret;
}

bool BootControl::SetBootloaderIndex(const char* boot_num) {
  set_bootloader_env_common("reboot_status", "reboot_next");
  set_bootloader_env_common("expect_index", boot_num);
  set_bootloader_env_common("update_env", "1");
  return true;
}

unsigned int BootControl::GetActiveBootSlot() {
  bootloader_control bootctrl;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  // Use the current slot by default.
  unsigned int active_boot_slot = current_slot_;
  unsigned int max_priority = bootctrl.slot_info[current_slot_].priority;
  // Find the slot with the highest priority.
  for (unsigned int i = 0; i < num_slots_; ++i) {
    if (bootctrl.slot_info[i].priority > max_priority) {
      max_priority = bootctrl.slot_info[i].priority;
      active_boot_slot = i;
    }
  }

  return active_boot_slot;
}

bool BootControl::SetActiveBootSlot(unsigned int slot) {
  if (slot >= kMaxNumSlots || slot >= num_slots_) {
    // Invalid slot number.
    return false;
  }

  bootloader_control bootctrl;
  bool ret = true;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  if (ret) {
    /* check if called from update_engine or vts test,
     * just rewrite when really update
     */
    std::string device_prop = android::base::GetProperty("ro.product.device", "");
    std::string fastbootd_prop = android::base::GetProperty("init.svc.fastbootd", "");
    LOG(INFO) << "device_prop: " << device_prop;
    LOG(INFO) << "fastbootd_prop: " << fastbootd_prop;

    std::string usb_prop = android::base::GetProperty("sys.usb.config", "");
    LOG(INFO) << "usb_prop: " << usb_prop;

    int gpt_mode = get_gpt_mode();
    if (gpt_mode < 0) {
        LOG(INFO) << "get gpt mode failed";
        return false;
    }

    if (device_prop != "generic" && fastbootd_prop != "running" && usb_prop != "fastboot") {
      if (gpt_mode == 0) {
        LOG(INFO) << "set bootloader index for gpt";
        char* write_boot = get_bootloader_env_common("write_boot");
        if (write_boot && (!strcmp(write_boot, "0"))) {
            LOG(INFO) << "need to set write_boot 1";
            set_bootloader_env_common("write_boot", "1");
        } else {
            LOG(INFO) << "need't to set write_boot, now write_boot is NULL or not equal 0 ";
        }
      } else {
        LOG(INFO) << "write bootloader in dts mode";
        ret = write_bootloader_img(slot, false);
        if (ret)
          ret = SetBootloaderIndex("0");
        /* when using dts, the dt will be updated in uboot */
        LOG(INFO) << "update dt in uboot";
        set_bootloader_env_common("update_dt", "1");
        char* update_dt = get_bootloader_env_common("update_dt");
        LOG(INFO) << "update_dt = " << update_dt;
      }
    }
  }

  // Set every other slot with a lower priority than the new "active" slot.
  if (ret) {
    const unsigned int kActivePriority = 15;
    const unsigned int kActiveTries = 6;
    for (unsigned int i = 0; i < num_slots_; ++i) {
      if (i != slot) {
        if (bootctrl.slot_info[i].priority >= kActivePriority)
          bootctrl.slot_info[i].priority = kActivePriority - 1;
      }
    }

    // Note that setting a slot as active doesn't change the successful bit.
    // The successful bit will only be changed by setSlotAsUnbootable().
    bootctrl.slot_info[slot].priority = kActivePriority;
    bootctrl.slot_info[slot].tries_remaining = kActiveTries;

    // Setting the current slot as active is a way to revert the operation that
    // set *another* slot as active at the end of an updater. This is commonly
    // used to cancel the pending update. We should only reset the verity_corrupted
    // bit when attempting a new slot, otherwise the verity bit on the current
    // slot would be flip.
    if (slot != current_slot_) bootctrl.slot_info[slot].verity_corrupted = 0;

    ret = UpdateAndSaveBootloaderControl(misc_device_, &bootctrl);
  }

  return ret;
}

bool BootControl::SetSlotAsUnbootable(unsigned int slot) {
  if (slot >= kMaxNumSlots || slot >= num_slots_) {
    // Invalid slot number.
    return false;
  }

  bootloader_control bootctrl;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  // The only way to mark a slot as unbootable, regardless of the priority is to
  // set the tries_remaining to 0.
  bootctrl.slot_info[slot].successful_boot = 0;
  bootctrl.slot_info[slot].tries_remaining = 0;
  return UpdateAndSaveBootloaderControl(misc_device_, &bootctrl);
}

bool BootControl::IsSlotBootable(unsigned int slot) {
  if (slot >= kMaxNumSlots || slot >= num_slots_) {
    // Invalid slot number.
    return false;
  }

  bootloader_control bootctrl;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  return bootctrl.slot_info[slot].tries_remaining != 0;
}

bool BootControl::IsSlotMarkedSuccessful(unsigned int slot) {
  if (slot >= kMaxNumSlots || slot >= num_slots_) {
    // Invalid slot number.
    return false;
  }

  bootloader_control bootctrl;
  if (!LoadBootloaderControl(misc_device_, &bootctrl)) return false;

  return bootctrl.slot_info[slot].successful_boot && bootctrl.slot_info[slot].tries_remaining;
}

bool BootControl::IsValidSlot(unsigned int slot) {
  return slot < kMaxNumSlots && slot < num_slots_;
}

bool BootControl::SetSnapshotMergeStatus(MergeStatus status) {
  return SetMiscVirtualAbMergeStatus(current_slot_, status);
}

MergeStatus BootControl::GetSnapshotMergeStatus() {
  MergeStatus status;
  if (!GetMiscVirtualAbMergeStatus(current_slot_, &status)) {
    return MergeStatus::UNKNOWN;
  }
  return status;
}

const char* BootControl::GetSuffix(unsigned int slot) {
  if (slot >= kMaxNumSlots || slot >= num_slots_) {
    return nullptr;
  }
  return kSlotSuffixes[slot];
}

bool InitMiscVirtualAbMessageIfNeeded() {
  std::string err;
  misc_virtual_ab_message message;
  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  if (message.version == MISC_VIRTUAL_AB_MESSAGE_VERSION &&
      message.magic == MISC_VIRTUAL_AB_MAGIC_HEADER) {
    // Already initialized.
    return true;
  }

  message = {};
  message.version = MISC_VIRTUAL_AB_MESSAGE_VERSION;
  message.magic = MISC_VIRTUAL_AB_MAGIC_HEADER;
  if (!WriteMiscVirtualAbMessage(message, &err)) {
    LOG(ERROR) << "Could not write merge status: " << err;
    return false;
  }
  return true;
}

bool SetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus status) {
  std::string err;
  misc_virtual_ab_message message;

  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  message.merge_status = static_cast<uint8_t>(status);
  message.source_slot = current_slot;
  if (!WriteMiscVirtualAbMessage(message, &err)) {
    LOG(ERROR) << "Could not write merge status: " << err;
    return false;
  }
  return true;
}

bool GetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus* status) {
  std::string err;
  misc_virtual_ab_message message;

  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  // If the slot reverted after having created a snapshot, then the snapshot will
  // be thrown away at boot. Thus we don't count this as being in a snapshotted
  // state.
  *status = static_cast<MergeStatus>(message.merge_status);
  if (*status == MergeStatus::SNAPSHOTTED && current_slot == message.source_slot) {
    *status = MergeStatus::NONE;
  }
  return true;
}

}  // namespace bootable
}  // namespace android
