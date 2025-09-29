NAME := loopback
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk

WEB_FILES += $(SDK_PATH)/web/index.html
