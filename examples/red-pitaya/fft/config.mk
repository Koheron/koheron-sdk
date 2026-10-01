NAME := fft
VERSION := 0.2.2
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0

DRIVERS += $(SDK_PATH)/boards/red-pitaya/drivers/common.hpp
DRIVERS += $(SDK_PATH)/server/drivers/xadc.hpp
DRIVERS += $(PROJECT_PATH)/drivers/fft.hpp
DRIVERS += $(PROJECT_PATH)/drivers/redpitaya_adc_calibration.hpp

WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))

# Reuse the ALPHA250 FFT workspace directly; only board entry points differ.
FFT_WEB_REFERENCE := $(SDK_PATH)/examples/alpha250/fft/web
WEB_FILES += $(FFT_WEB_REFERENCE)/fft.css $(FFT_WEB_REFERENCE)/dds-frequency.html
WEB_FILES += $(wildcard $(FFT_WEB_REFERENCE)/fft/* $(FFT_WEB_REFERENCE)/plot/* $(FFT_WEB_REFERENCE)/export-file/*)
