NAME := loopback
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk

WEB_FILES += $(SDK_PATH)/web/index.html
