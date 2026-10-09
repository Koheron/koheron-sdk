NAME := fft
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250-4

FPGA_LIB_TCL = $(FPGA_TCL_RECORDER)

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250-4/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0

include $(SDK_PATH)/boards/alpha250-4/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/fft.hpp
DRIVERS += $(PROJECT_PATH)/fft.cpp

include $(SDK_PATH)/web/fft/components.mk
include $(SDK_PATH)/web/board-controls/alpha-fft.mk
include $(SDK_PATH)/web/temperature-sensor/driver.mk
include $(SDK_PATH)/web/power-monitor/driver.mk
include $(SDK_PATH)/web/precision-channels/adc-driver.mk
WEB_FILES := $(filter-out $(SDK_PATH)/web/fft/controls/input-channel.html,$(WEB_FILES))
WEB_FILES += $(PROJECT_PATH)/web/fft.ts $(PROJECT_PATH)/web/fft/input-channel.html
WEB_FILES += $(PROJECT_PATH)/web/index.html $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(SDK_PATH)/web/temperature-sensor/temperature-sensor.html
include $(SDK_PATH)/web/power-monitor/components.mk
include $(SDK_PATH)/web/precision-channels/io-template.mk
include $(SDK_PATH)/web/clock-generator/reference-clock.mk
include $(SDK_PATH)/web/clock-generator/sampling-frequency.mk
