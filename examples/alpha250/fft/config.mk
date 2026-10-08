NAME := fft
VERSION := 0.3.0
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES = $(BD_TCL) $(wildcard $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl $(wildcard $(FPGA_PATH)/lib/*.tcl) $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(BOARD_PATH)/drivers/phase-modulator.hpp
DRIVERS += $(PROJECT_PATH)/fft.hpp
DRIVERS += $(PROJECT_PATH)/fft.cpp

include $(SDK_PATH)/web/fft/components.mk
include $(SDK_PATH)/web/board-controls/alpha-fft.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
WEB_FILES := $(filter-out $(PROJECT_PATH)/web/app.ts,$(WEB_FILES)) $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(SDK_PATH)/web/temperature-sensor/temperature-sensor.html
include $(SDK_PATH)/web/power-monitor/components.mk
include $(SDK_PATH)/web/precision-channels/io-template.mk
include $(SDK_PATH)/web/clock-generator/reference-clock.mk
include $(SDK_PATH)/web/clock-generator/sampling-frequency.mk
