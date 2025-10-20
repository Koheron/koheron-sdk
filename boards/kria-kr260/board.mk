BOARD := kria-kr260
PART := xck26-sfvc784-2LV-c
ZYNQ_TYPE := zynqmp

TMP_OS_BOARD_PATH := $(TMP)/kria-kr260

UBOOT_CONFIG = xilinx_zynqmp_kria_defconfig
PATCHES := boards/kria-kr260/patches
#EXTLINUX_CONF = boards/kria-kr260/config/extlinux.conf
