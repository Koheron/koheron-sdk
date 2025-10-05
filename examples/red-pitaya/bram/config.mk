NAME := bram
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

#DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/common.hpp
DRIVERS += $(PROJECT_PATH)/bram.hpp

WEB_FILES += $(SDK_PATH)/web/index.html