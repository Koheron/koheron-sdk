# Shared build integration for the non-TI Gen2 packages.
ZYNQ_TYPE := zynq
TMP_OS_BOARD_PATH := $(TMP)/$(BOARD)
# Reuse the read-only EEPROM identity reader and U-Boot patch set.
UBOOT_CONFIG := zynq_red-pitaya_defconfig
UBOOT_CONFIG_FRAGMENTS := $(SDK_PATH)/boards/red-pitaya-gen2/config/u-boot-common.config \
                          $(BOARD_PATH)/config/u-boot.config
PATCHES := $(SDK_PATH)/boards/red-pitaya/patches
BOARD_DTSO := $(SDK_PATH)/boards/$(BOARD)/config/board.dtso
BOARD_DTSO_DEPS := $(SDK_PATH)/boards/red-pitaya-gen2/config/board-common.dtsi

# Presets and inherited Tcl/XDC files must invalidate generated projects too.
TCL_FILES += $(SDK_PATH)/boards/red-pitaya-gen2/common.mk \
             $(wildcard $(BOARD_PATH)/config/*.tcl) \
             $(wildcard $(SDK_PATH)/boards/red-pitaya-gen2/*.tcl) \
             $(wildcard $(SDK_PATH)/boards/red-pitaya-gen2/config/*.tcl) \
             $(wildcard $(SDK_PATH)/boards/red-pitaya/*.tcl) \
             $(wildcard $(SDK_PATH)/boards/red-pitaya/config/*.tcl) \
             $(wildcard $(SDK_PATH)/boards/red-pitaya/config/*.xdc)
