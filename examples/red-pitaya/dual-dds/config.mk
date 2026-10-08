NAME := dual-dds
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

DRIVERS += $(SDK_PATH)/boards/red-pitaya/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/dual_dds.hpp

include $(SDK_PATH)/web/dds-frequency/components.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
WEB_FILES := $(filter-out $(PROJECT_PATH)/web/app.ts,$(WEB_FILES)) $(PROJECT_PATH)/web/app.ts
