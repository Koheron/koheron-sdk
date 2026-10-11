VNA_COMMON_PATH := $(SDK_PATH)/web/vector-network-analyzer
include $(SDK_PATH)/web/instrument/components.mk
include $(SDK_PATH)/web/instrument/events.mk
include $(SDK_PATH)/web/plot-references/components.mk
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(VNA_COMMON_PATH)/index.html $(VNA_COMMON_PATH)/vna.css
WEB_FILES += $(VNA_COMMON_PATH)/profile.ts $(VNA_COMMON_PATH)/model.ts $(VNA_COMMON_PATH)/driver.ts
WEB_FILES += $(VNA_COMMON_PATH)/plot.ts $(VNA_COMMON_PATH)/workspace.ts
WEB_FILES += $(PROJECT_PATH)/web/app.ts $(VNA_COMMON_PATH)/chirp-worker.js

VNA_WASM_IMAGE ?= koheron-vna-wasm:llvm18
VNA_WASM_CC ?= docker run --rm -u $$(id -u):$$(id -g) -v $(CURDIR):$(CURDIR) -w $(CURDIR) $(VNA_WASM_IMAGE) clang-18
VNA_WASM := $(abspath $(TMP)/$(PROJECT_PATH)/chirp-processor.wasm)
WEB_FILES += $(VNA_WASM)
.PHONY: vna-wasm-builder
vna-wasm-builder:
	@if ! docker image inspect "$(VNA_WASM_IMAGE)" >/dev/null 2>&1; then docker build -t "$(VNA_WASM_IMAGE)" "$(VNA_COMMON_PATH)/processor"; fi
$(VNA_WASM): $(VNA_COMMON_PATH)/processor/processor.c $(VNA_COMMON_PATH)/processor/Dockerfile | vna-wasm-builder
	mkdir -p $(@D)
	$(VNA_WASM_CC) --target=wasm32 -O3 -ffp-contract=off -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-all -Wl,--max-memory=1073741824 -o $@ $<
