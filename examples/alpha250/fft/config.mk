NAME := fft
VERSION := 0.3.0
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/alpha250
include $(SDK_PATH)/boards/alpha250/instrument.mk

FPGA_LIB_TCL = $(FPGA_TCL_FFT)
FPGA_IMPL_TCL = $(FPGA_TCL_HOLD_FIX)
TCL_EXTRA_FILES += $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0

DRIVERS += $(BOARD_PATH)/drivers/phase-modulator.hpp
DRIVERS += $(PROJECT_PATH)/fft.hpp
DRIVERS += $(PROJECT_PATH)/fft.cpp

include $(SDK_PATH)/web/fft/components.mk
include $(SDK_PATH)/web/board-controls/alpha-fft.mk
include $(SDK_PATH)/web/project.mk
WEB_FILES += $(SDK_PATH)/web/temperature-sensor/temperature-sensor.html
include $(SDK_PATH)/web/power-monitor/components.mk
include $(SDK_PATH)/web/precision-channels/io-template.mk
include $(SDK_PATH)/web/clock-generator/reference-clock.mk
include $(SDK_PATH)/web/clock-generator/sampling-frequency.mk
