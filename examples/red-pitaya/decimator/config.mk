NAME := decimator
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

CONFIG_YML = $(PROJECT_PATH)/config.yml

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/decimator.hpp

WEB_FILES += $(SDK_PATH)/web/index.html