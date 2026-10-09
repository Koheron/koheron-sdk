NAME := signal-analyzer
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15

FPGA_LIB_TCL = $(FPGA_TCL_RECORDER)

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/decimator.hpp
DRIVERS += $(PROJECT_PATH)/decimator.cpp
DRIVERS += $(PROJECT_PATH)/fft.hpp
DRIVERS += $(PROJECT_PATH)/fft.cpp
DRIVERS += $(PROJECT_PATH)/dma.hpp

include $(SDK_PATH)/web/fft/components.mk
include $(SDK_PATH)/web/precision-channels/components.mk
include $(SDK_PATH)/web/clock-generator/components.mk
include $(SDK_PATH)/web/temperature-sensor/driver.mk
include $(SDK_PATH)/web/power-monitor/driver.mk
# Alpha15 supplies voltage units, four channel modes, and its own board controls.
WEB_FILES := $(filter-out $(SDK_PATH)/web/fft/controls/input-channel.html $(SDK_PATH)/web/fft/plot/yunit.html,$(WEB_FILES))
WEB_FILES += $(PROJECT_PATH)/web/fft.ts $(PROJECT_PATH)/web/decimator.ts
WEB_FILES += $(PROJECT_PATH)/web/fft/input-channel.html $(PROJECT_PATH)/web/plot/yunit.html
WEB_FILES += $(wildcard $(PROJECT_PATH)/web/adc-range/*.html) $(PROJECT_PATH)/web/adc-range/ltc2387.ts
WEB_FILES += $(PROJECT_PATH)/web/precision-channels/precision-channels.html
WEB_FILES += $(PROJECT_PATH)/web/board-controls.ts $(PROJECT_PATH)/web/index.html $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(SDK_PATH)/web/temperature-sensor/temperature-sensor.html
include $(SDK_PATH)/web/power-monitor/components.mk
include $(SDK_PATH)/web/clock-generator/reference-clock.mk
