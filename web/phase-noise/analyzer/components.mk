# Single-stream analyzer adapter shared by ALPHA250, Red Pitaya and DPLL.
include $(SDK_PATH)/web/phase-noise/components.mk
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/phase-noise-analyzer.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/plot.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/export-file/export-file.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/analyzer/export-file/export-file.html
