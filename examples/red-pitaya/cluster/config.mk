NAME := cluster
VERSION := 0.1.1

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

# TODO Fix double xdc
XDC += $(PROJECT_PATH)/ports.xdc
XDC += $(SDK_PATH)/red-pitaya/config/clocks.xdc
XDC += $(PROJECT_PATH)/sata.xdc
XDC += $(PROJECT_PATH)/expansion_connector.xdc

CONFIG_YML = $(PROJECT_PATH)/config.yml

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0
CORES += $(SDK_PATH)/fpga/cores/pulse_generator_v1_0
CORES += $(SDK_PATH)/fpga/cores/edge_detector_v1_0

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/cluster.hpp

WEB_FILES += $(SDK_PATH)/web/index.html