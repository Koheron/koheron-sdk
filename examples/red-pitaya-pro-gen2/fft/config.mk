BOARD_PATH := $(SDK_PATH)/boards/red-pitaya-pro-gen2
PROJECT_PATH := $(SDK_PATH)/examples/red-pitaya/fft
include $(PROJECT_PATH)/config.mk
# Nominal LV input scaling until this board has been calibrated.
SERVER_EXTRA_CCXXFLAGS += -DKOHERON_REDPITAYA_NOMINAL_ADC_CALIBRATION
