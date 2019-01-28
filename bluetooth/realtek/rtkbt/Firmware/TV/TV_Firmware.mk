PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/Firmware/TV/rtl8723b_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723b_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8723b_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723b_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8723bs_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8723bs_VQ0_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_VQ0_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8723bu_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bu_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8723d_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723d_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8723d_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723d_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8723ds_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723ds_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761a_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761a_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8761a_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761a_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761at_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761at_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761au8192ee_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761au8192ee_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761au8812ae_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761au8812ae_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761au_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761au_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8761aw8192eu_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761aw8192eu_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8761aw8192eu_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761aw8192eu_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8821a_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821a_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8821a_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821a_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8821as_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821as_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8821c_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821c_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8821c_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821c_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8821cs_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821cs_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8822b_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8822b_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8822b_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8822b_fw \
	$(LOCAL_PATH)/Firmware/TV/rtl8822bs_fw:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8822bs_fw \

UART_2M := true

ifeq ($(UART_2M),true)
$(warning realtek uart bt use 2m baudrate config)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8723bs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8723bs_VQ0_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_VQ0_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8723ds_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723ds_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8761at_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761at_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8821as_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821as_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8821cs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821cs_config \
	$(LOCAL_PATH)/Firmware/TV/UART_2M/rtl8822bs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8822bs_config \

else
$(warning realtek uart bt use default config)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/Firmware/TV/rtl8723bs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8723bs_VQ0_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723bs_VQ0_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8723ds_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8723ds_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8761at_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8761at_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8821as_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821as_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8821cs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8821cs_config \
	$(LOCAL_PATH)/Firmware/TV/rtl8822bs_config:$(TARGET_COPY_OUT_VENDOR)/firmware/rtl8822bs_config \

endif
