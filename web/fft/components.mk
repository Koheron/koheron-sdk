# One component list for every FFT board. Board entry points follow this include.
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/frequency-input.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(SDK_PATH)/web/fft" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
