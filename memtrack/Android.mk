ifeq ($(PRODUCT_SHIPPING_API_LEVEL),29)

LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/hidl_memtrack/Android.mk

endif
