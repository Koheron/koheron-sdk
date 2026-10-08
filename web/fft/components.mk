# One component list for every FFT board. Board entry points follow this include.
include $(SDK_PATH)/web/instrument/components.mk
include $(SDK_PATH)/web/instrument/events.mk
include $(SDK_PATH)/web/instrument/poller.mk
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
include $(SDK_PATH)/web/inputs/components.mk
include $(SDK_PATH)/web/power-monitor/readout.mk
include $(SDK_PATH)/web/temperature-sensor/readout.mk
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(SDK_PATH)/web/fft" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
