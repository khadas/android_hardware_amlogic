# atom is use very specific audio hardware, move it to vendor.
# here only keep the generic implement for all other devices
ifneq "$(findstring atom,$(TARGET_DEVICE))" "atom"
  include $(call all-subdir-makefiles)
endif

