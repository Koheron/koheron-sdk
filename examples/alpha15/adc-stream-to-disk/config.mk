NAME := adc-stream-to-disk
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15
MEMORY_YML = $(PROJECT_PATH)/memory.yml
XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(PROJECT_PATH)/cores/adc_overflow_counter_v1_0
CORES += $(PROJECT_PATH)/cores/adc_sample_tag_v1_0
CORES += $(PROJECT_PATH)/cores/dac_test_tone_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_stream.hpp

WEB_FILES += $(SDK_PATH)/web/index.html
