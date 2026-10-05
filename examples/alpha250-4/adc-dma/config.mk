NAME := adc-dma
VERSION := 0.1.0
BOARD_PATH := $(SDK_PATH)/boards/alpha250-4
ENFORCE_TIMING := 1

XDC += $(BOARD_PATH)/config/ports.xdc
TCL_FILES = $(BD_TCL) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl
include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(PROJECT_PATH)/quad_capture_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_dma.hpp
WEB_FILES += $(SDK_PATH)/web/index.html
OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
