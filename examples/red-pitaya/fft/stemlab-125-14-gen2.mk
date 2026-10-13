NAME := fft
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya/stemlab-125-14-gen2
include $(PROJECT_PATH)/common.mk
# Nominal LV input scaling until this board has been calibrated.
SERVER_EXTRA_CCXXFLAGS += -DKOHERON_REDPITAYA_NOMINAL_ADC_CALIBRATION
