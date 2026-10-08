# Common PNA assets. Include before instrument adapters and entry points.
include $(SDK_PATH)/web/instrument/components.mk
include $(SDK_PATH)/web/instrument/events.mk
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
include $(SDK_PATH)/web/inputs/components.mk
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(SDK_PATH)/web/phase-noise/spectrum.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/dds.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/export-file/export-file.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/export-file/export-file.html
WEB_FILES += $(SDK_PATH)/web/phase-noise/plot.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/sample-rate.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/save-config.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/measurements.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-precision.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-precision.css
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-noise-plot.css
WEB_FILES += $(SDK_PATH)/web/phase-noise/workspace.css
