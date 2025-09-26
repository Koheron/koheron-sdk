NAME := fft
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250-4

CONFIG_YML = $(PROJECT_PATH)/config.yml

CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0

include $(SDK_PATH)/boards/alpha250-4/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/fft.hpp

WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.html
WEB_FILES += $(SDK_PATH)/web/dds-frequency/dds-frequency.ts
WEB_FILES += $(PROJECT_PATH)/web/index.html
WEB_FILES += $(PROJECT_PATH)/web/app.ts
WEB_FILES += $(PROJECT_PATH)/web/fft.ts
WEB_FILES += $(PROJECT_PATH)/web/fft/fft-window.html
WEB_FILES += $(PROJECT_PATH)/web/fft/input-channel.html
WEB_FILES += $(PROJECT_PATH)/web/fft/fft-app.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(PROJECT_PATH)/web/plot/plot.ts
WEB_FILES += $(PROJECT_PATH)/web/plot/yunit.html
WEB_FILES += $(PROJECT_PATH)/web/plot/peak-detection.html
WEB_FILES += $(PROJECT_PATH)/web/precision-channels/precision-adc.ts
WEB_FILES += $(PROJECT_PATH)/web/precision-channels/precision-dac.ts
WEB_FILES += $(PROJECT_PATH)/web/precision-channels/precision-channels-app.ts
WEB_FILES += $(PROJECT_PATH)/web/precision-channels/precision-channels.html
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/clock-generator-app.ts
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/sampling-frequency.html
WEB_FILES += $(PROJECT_PATH)/web/clock-generator/reference-clock.html
WEB_FILES += $(PROJECT_PATH)/web/export-file/export-file.html
WEB_FILES += $(PROJECT_PATH)/web/export-file/export-file.ts
WEB_FILES += $(PROJECT_PATH)/web/temperature-sensor/temperature-sensor.html
WEB_FILES += $(PROJECT_PATH)/web/temperature-sensor/temperature-sensor.ts
WEB_FILES += $(PROJECT_PATH)/web/temperature-sensor/temperature-sensor-app.ts
WEB_FILES += $(PROJECT_PATH)/web/power-monitor/power-monitor.html
WEB_FILES += $(PROJECT_PATH)/web/power-monitor/power-monitor.ts
WEB_FILES += $(PROJECT_PATH)/web/power-monitor/power-monitor-app.ts