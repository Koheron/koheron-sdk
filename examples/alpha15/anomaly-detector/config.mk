NAME := anomaly-detector
VERSION := 0.2.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15
MEMORY_YML = $(PROJECT_PATH)/memory.yml
XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(PROJECT_PATH)/cores/adc_overflow_counter_v1_0
CORES += $(PROJECT_PATH)/cores/anomaly_engine_v1_0
CORES += $(PROJECT_PATH)/cores/dac_exciter_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/anomaly_detector.hpp

WEB_FILES += $(PROJECT_PATH)/web/index.html
WEB_FILES += $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(PROJECT_PATH)/web/anomaly.css
WEB_FILES += $(PROJECT_PATH)/web/brand.svg
