# Board bring-up instrument: use the existing SDK LED/register implementation.
NAME := led-blinker
VERSION := 0.1.1
RED_PITAYA_LED_PATH := $(SDK_PATH)/examples/red-pitaya/led-blinker
MEMORY_YML := $(RED_PITAYA_LED_PATH)/memory.yml
BD_TCL := $(RED_PITAYA_LED_PATH)/block_design.tcl
XDC += $(RED_PITAYA_LED_PATH)/constraints.xdc
CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
DRIVERS += $(SDK_PATH)/boards/red-pitaya/drivers/common.hpp
DRIVERS += $(RED_PITAYA_LED_PATH)/led_blinker.hpp
WEB_FILES += $(SDK_PATH)/web/led-blinker.ts
WEB_FILES += $(RED_PITAYA_LED_PATH)/web/index.html
WEB_FILES += $(RED_PITAYA_LED_PATH)/web/app.ts
