# Standard ALPHA250 instrument hardware. Include before instrument-specific inputs.
XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc
include $(SDK_PATH)/boards/alpha250/cores/cores.mk
include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
