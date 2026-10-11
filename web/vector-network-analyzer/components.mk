VNA_COMMON_PATH := $(SDK_PATH)/web/vector-network-analyzer
include $(SDK_PATH)/make/dependencies.mk
include $(SDK_PATH)/web/instrument/components.mk
include $(SDK_PATH)/web/instrument/events.mk
include $(SDK_PATH)/web/plot-references/components.mk
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(VNA_COMMON_PATH)/index.html $(VNA_COMMON_PATH)/vna.css
WEB_FILES += $(VNA_COMMON_PATH)/profile.ts $(VNA_COMMON_PATH)/model.ts $(VNA_COMMON_PATH)/driver.ts
WEB_FILES += $(VNA_COMMON_PATH)/plot.ts $(VNA_COMMON_PATH)/workspace.ts
WEB_FILES += $(PROJECT_PATH)/web/app.ts $(VNA_COMMON_PATH)/chirp-worker.js

VNA_WASM_IMAGE ?= koheron-vna-wasm:llvm18
VNA_WASM_DEFAULT_CC := $(if $(filter undefined,$(origin VNA_WASM_CC)),1,)
VNA_WASM_CC ?= docker run --rm -u $$(id -u):$$(id -g) -v $(CURDIR):$(CURDIR) -w $(CURDIR) $(VNA_WASM_IMAGE) clang-18
VNA_WASM_FLAGS ?= --target=wasm32 -O3 -ffp-contract=off -nostdlib -fno-builtin -Wl,--no-entry -Wl,--export-all -Wl,--max-memory=1073741824
VNA_WASM := $(abspath $(TMP_PROJECT_PATH)/chirp-processor.wasm)
WEB_FILES += $(VNA_WASM)

VNA_WASM_SETTINGS := $(TMP_PROJECT_PATH)/.vna-wasm-settings
$(eval $(call build_settings_stamp,$(VNA_WASM_SETTINGS),$$(call build_shell_quote,$$(VNA_WASM_CC)) $$(call build_shell_quote,$$(VNA_WASM_FLAGS))))
ifeq ($(VNA_WASM_DEFAULT_CC),1)
VNA_WASM_BUILDER_STAMP := $(TMP_PROJECT_PATH)/.vna-wasm-builder
.PHONY: vna-wasm-builder-force
vna-wasm-builder-force:
$(VNA_WASM_BUILDER_STAMP): vna-wasm-builder-force $(VNA_COMMON_PATH)/processor/Dockerfile $(VNA_COMMON_PATH)/processor/ensure_builder.sh
	bash "$(VNA_COMMON_PATH)/processor/ensure_builder.sh" "$(VNA_WASM_IMAGE)" "$@" "$(abspath $(VNA_COMMON_PATH)/processor)"
endif

$(VNA_WASM): $(VNA_COMMON_PATH)/processor/processor.c $(VNA_WASM_SETTINGS) $(VNA_WASM_BUILDER_STAMP)
	mkdir -p $(@D)
	$(VNA_WASM_CC) $(VNA_WASM_FLAGS) -o $@ $<
