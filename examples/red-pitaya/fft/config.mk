NAME := fft
VERSION := 0.1.1

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc
XDC += $(PROJECT_PATH)/expansion_connector.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0
CORES += $(SDK_PATH)/fpga/cores/comparator_v1_0
CORES += $(SDK_PATH)/fpga/cores/saturation_v1_0
CORES += $(SDK_PATH)/fpga/cores/pdm_v1_0
CORES += $(SDK_PATH)/fpga/cores/at93c46d_spi_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(SDK_PATH)/server/drivers/xadc.hpp
DRIVERS += $(SDK_PATH)/server/drivers/laser.hpp
DRIVERS += $(SDK_PATH)/server/drivers/eeprom.hpp
DRIVERS += $(PROJECT_PATH)/drivers/fft.hpp
DRIVERS += $(PROJECT_PATH)/drivers/demodulator.hpp
DRIVERS += $(PROJECT_PATH)/drivers/redpitaya_adc_calibration.hpp

WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.html
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(SDK_PATH)/web/laser.ts
WEB_FILES += $(SDK_PATH)/web/laser-control.html
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
