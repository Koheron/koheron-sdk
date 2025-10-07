NAME := signal-analyzer
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/decimator.hpp
DRIVERS += $(PROJECT_PATH)/decimator.cpp
DRIVERS += $(PROJECT_PATH)/fft.hpp
DRIVERS += $(PROJECT_PATH)/fft.cpp
DRIVERS += $(PROJECT_PATH)/dma.hpp

WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))