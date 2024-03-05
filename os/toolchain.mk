-include $(OS_PATH)/board.mk
ifneq ("$(wildcard $(BOARD_PATH)/board.mk)","")
-include $(BOARD_PATH)/board.mk
endif
ZYNQ_TYPE ?= zynq
include $(OS_PATH)/$(ZYNQ_TYPE).mk

PATCHES := $(BOARD_PATH)/patches
HSI := source $(VIVADO_PATH)/$(VIVADO_VERSION)/settings64.sh && hsi -nolog -nojournal -mode batch
BOOTGEN := source $(VIVADO_PATH)/$(VIVADO_VERSION)/settings64.sh && bootgen

