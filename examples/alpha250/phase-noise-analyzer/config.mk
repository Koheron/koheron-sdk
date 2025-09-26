NAME := phase-noise-analyzer
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

# FPGA cores only (skip board-specific cores)
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/boxcar_filter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
DRIVERS += $(PROJECT_PATH)/dds.hpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.hpp

# Web assets
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(PROJECT_PATH)/web/dds-frequency/dds-frequency.html
WEB_FILES += $(PROJECT_PATH)/web/dds-frequency/dds-frequency.ts
WEB_FILES += $(PROJECT_PATH)/web/index.html
WEB_FILES += $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(PROJECT_PATH)/web/dds.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator-app.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/reference-clock.html
WEB_FILES += $(PROJECT_PATH)/web/phase-noise-analyzer.ts
WEB_FILES += $(PROJECT_PATH)/web/phase-noise-analyzer-app.ts
WEB_FILES += $(PROJECT_PATH)/web/plot.ts
WEB_FILES += $(PROJECT_PATH)/web/export-file/export-file.html
WEB_FILES += $(PROJECT_PATH)/web/export-file/export-file.ts
WEB_FILES += $(PROJECT_PATH)/web/phase-noise-plot.css