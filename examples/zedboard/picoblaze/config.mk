NAME := picoblaze
VERSION := 0.1.1

BOARD_PATH := $(SDK_PATH)/boards/zedboard

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/kcpsm6_v1_0

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/picoblaze.hpp

WEB_FILES += $(SDK_PATH)/web/index.html