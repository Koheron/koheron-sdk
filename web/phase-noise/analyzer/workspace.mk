# Two-channel analyzer controls and embedded signal generator.
include $(SDK_PATH)/web/phase-noise/analyzer/components.mk
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/phase-noise-analyzer-app.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/dds.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/dds-frequency/dds-frequency.html
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/generator.css
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
