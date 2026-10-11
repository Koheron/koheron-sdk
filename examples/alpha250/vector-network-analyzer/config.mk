NAME := vector-network-analyzer
VERSION := 0.2.0
BOARD_PATH := $(SDK_PATH)/boards/alpha250
ENFORCE_TIMING := 1
FPGA_LIB_TCL = $(FPGA_TCL_BASE)

XDC += $(BOARD_PATH)/config/ports.xdc
include $(BOARD_PATH)/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/chirp_generator_v1_0
CORES += $(SDK_PATH)/fpga/cores/vna_capture_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/vector-network-analyzer/vector-network-analyzer.hpp
TCL_EXTRA_FILES += $(SDK_PATH)/fpga/modules/vector-network-analyzer.tcl
include $(SDK_PATH)/web/vector-network-analyzer/components.mk
