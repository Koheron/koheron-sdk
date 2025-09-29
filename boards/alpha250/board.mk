BOARD := alpha250
PART := xc7z020clg400-2
ZYNQ_TYPE := zynq

# Linux and U-boot
TMP_OS_BOARD_PATH := $(TMP)/alpha250

UBOOT_CONFIG = zynq_alpha250_defconfig
FSBL_PATH := $(OS_PATH)/alpha/fsbl
PATCHES := boards/alpha250/patches
