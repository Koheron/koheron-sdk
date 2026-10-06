NAME := dpll
VERSION := 0.2.0

BOARD_PATH := $(SDK_PATH)/boards/alpha250
ENFORCE_TIMING := 1

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/double_saturation_v1_0
CORES += $(SDK_PATH)/fpga/cores/boxcar_filter_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stream_control_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_fixed_decimator_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_cic_decimator_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stream_cdc_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_range_guard_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_quantizer_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_prefilter_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stochastic_round_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_stream_packet_mux_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
DRIVERS += $(PROJECT_PATH)/dpll.hpp
DRIVERS += $(PROJECT_PATH)/dma.hpp

WEB_FILES += $(SDK_PATH)/web/phase-modulator/frequency-input.ts
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' -o -name '*.svg' \))

include $(SDK_PATH)/server/drivers/phase-noise/fft.mk
WEB_FILES += $(SDK_PATH)/web/phase-noise/spectrum.ts $(SDK_PATH)/web/phase-noise/plot.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-precision.ts $(SDK_PATH)/web/phase-noise/phase-precision.css
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web/phase-noise-plot.css
WEB_FILES += $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web/export-file/export-file.ts
WEB_FILES += $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web/export-file/export-file.html
WEB_FILES += $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web/phase-noise-analyzer.ts
WEB_FILES += $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web/plot.ts
