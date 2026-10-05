NAME := fft
VERSION := 0.3.0
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0

CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES = $(BD_TCL) $(wildcard $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl $(wildcard $(FPGA_PATH)/lib/*.tcl) $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

DRIVERS += $(SDK_PATH)/boards/red-pitaya/drivers/common.hpp
DRIVERS += $(SDK_PATH)/server/drivers/xadc.hpp
DRIVERS += $(BOARD_PATH)/drivers/phase-modulator.hpp
DRIVERS += $(PROJECT_PATH)/drivers/fft.hpp
DRIVERS += $(PROJECT_PATH)/drivers/redpitaya_adc_calibration.hpp

include $(SDK_PATH)/web/fft/components.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
WEB_FILES := $(filter-out $(PROJECT_PATH)/web/app.ts,$(WEB_FILES)) $(PROJECT_PATH)/web/app.ts
