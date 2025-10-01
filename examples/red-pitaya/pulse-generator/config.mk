NAME := pulse-generator
VERSION := 0.1.2

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc
XDC += $(PROJECT_PATH)/expansion_connector.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/pulse_generator_v1_0

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/pulse.hpp

WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.html
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))