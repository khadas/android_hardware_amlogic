LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked, and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_MULTILIB := both

LOCAL_CFLAGS += \
	-Wno-unused-parameter \
	-Wformat

LOCAL_CPPFLAGS += \
	-Wno-unused-parameter \
	-Wformat

LOCAL_MODULE := sensors.amlogic
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_HEADER_LIBRARIES += \
    libhardware_headers

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -DLOG_TAG=\"SensorsHal\" \
	-DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_SRC_FILES := \
	sensors.c \
	nusensors.cpp \
	InputEventReader.cpp \
	SensorBase.cpp \
	Kxtj3Sensor.cpp \
				
LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libutils

include $(BUILD_SHARED_LIBRARY)
