NAME := fft
VERSION := 0.2.1

CONFIG = $(PROJECT_PATH)/config.yml

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC := $(BOARD_PATH)/config/ports.xdc

CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/edge_detector_v1_0
CORES += $(SDK_PATH)/fpga/cores/comparator_v1_0
CORES += $(SDK_PATH)/fpga/cores/unrandomizer_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0
