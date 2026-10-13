VERSION := 0.3.0
ENFORCE_TIMING := 1


FPGA_LIB_TCL = $(FPGA_TCL_FFT) $(FPGA_TCL_REDP)
FPGA_IMPL_TCL = $(FPGA_TCL_HOLD_FIX)
TCL_EXTRA_FILES += $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/stemlab-125-14/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/stemlab-125-14/config/clocks.xdc

include $(BOARD_PATH)/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/ip/axis_accumulator_v1_0

CORES += $(SDK_PATH)/fpga/ip/awg_v1_0

DRIVERS += $(SDK_PATH)/boards/red-pitaya/stemlab-125-14/drivers/common.hpp
DRIVERS += $(SDK_PATH)/server/drivers/xadc.hpp
DRIVERS += $(SDK_PATH)/boards/red-pitaya/stemlab-125-14/drivers/phase-modulator.hpp
DRIVERS += $(PROJECT_PATH)/drivers/fft.hpp
DRIVERS += $(PROJECT_PATH)/drivers/redpitaya_adc_calibration.hpp

include $(SDK_PATH)/web/fft/components.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
WEB_FILES := $(filter-out $(PROJECT_PATH)/web/app.ts,$(WEB_FILES)) $(PROJECT_PATH)/web/app.ts
