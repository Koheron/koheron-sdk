-include $(OS_PATH)/board.mk
ifneq ("$(wildcard $(BOARD_PATH)/board.mk)","")
-include $(BOARD_PATH)/board.mk
endif
ZYNQ_TYPE ?= zynq
VIVADO_VER ?= $(VIVADO_VERSION)
include $(OS_PATH)/$(ZYNQ_TYPE).mk

PATCHES := $(BOARD_PATH)/patches
HSI := source $(VIVADO_PATH)/settings64.sh && xsct
BOOTGEN := source $(VIVADO_PATH)/settings64.sh && bootgen

