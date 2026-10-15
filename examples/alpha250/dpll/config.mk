NAME := dpll
VERSION := 0.2.0
BOARD_PATH := $(SDK_PATH)/boards/alpha250
ENFORCE_TIMING := 1
MEMORY_YML = $(PROJECT_PATH)/memory.yml
XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc
include $(SDK_PATH)/boards/alpha250/cores/cores.mk
include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
SERVER_EXTRA_CCXXFLAGS += -mfpu=neon
include $(SDK_PATH)/examples/alpha250/dpll/components.mk
WEB_FILES += $(PROJECT_PATH)/web/app.ts
