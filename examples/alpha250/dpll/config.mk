NAME := dpll
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/double_saturation_v1_0
CORES += $(SDK_PATH)/fpga/cores/boxcar_filter_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

DRIVERS += $(PROJECT_PATH)/dpll.hpp
DRIVERS += $(PROJECT_PATH)/dma.hpp

WEB_FILES += $(PROJECT_PATH)/web/index.html
WEB_FILES += $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(PROJECT_PATH)/web/control.ts
WEB_FILES += $(PROJECT_PATH)/web/dpll.ts
WEB_FILES += $(PROJECT_PATH)/web/variables.css
WEB_FILES += $(PROJECT_PATH)/web/dpll.css
WEB_FILES += $(PROJECT_PATH)/web/block_diagram_path.svg
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator-app.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/reference-clock.html