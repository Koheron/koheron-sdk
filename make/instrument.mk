INSTRUMENT_RULES := $(lastword $(MAKEFILE_LIST))
include $(dir $(INSTRUMENT_RULES))dependencies.mk

$(eval $(call build_settings_stamp,$(VERSION_FILE),$$(call build_shell_quote,$$(VERSION))))

INSTRUMENT_MANIFEST := $(TMP_PROJECT_PATH)/instrument.json
INSTRUMENT_MANIFEST_SETTINGS := $(TMP_PROJECT_PATH)/.manifest-settings
$(eval $(call build_settings_stamp,$(INSTRUMENT_MANIFEST_SETTINGS),$$(foreach K,BOARD UBUNTU_ARCH KOHERON_VERSION,$$(call build_shell_quote,$$(K)=$$($$(K))))))
$(INSTRUMENT_MANIFEST): $(CONFIG_MK) $(BOARD_MK) $(SDK_PATH)/Makefile $(SDK_PATH)/python/koheron/instrument_manifest.py $(INSTRUMENT_MANIFEST_SETTINGS) | $(TMP_PROJECT_PATH)/
	$(VENV)/bin/python3 $(SDK_PATH)/python/koheron/instrument_manifest.py --board "$(BOARD)" --architecture "$(UBUNTU_ARCH)" --sdk-version "$(KOHERON_VERSION)" --output "$@"

INSTRUMENT_FILES := $(BITSTREAM).bin $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM) $(SERVER) $(DRIVERS_JSON_OUT) $(WEB_ASSETS) $(VERSION_FILE) $(INSTRUMENT_MANIFEST)
INSTRUMENT_PACKAGE_SETTINGS := $(TMP_PROJECT_PATH)/.package-settings
$(eval $(call build_settings_stamp,$(INSTRUMENT_PACKAGE_SETTINGS),$$(foreach F,$$(INSTRUMENT_FILES),$$(call build_shell_quote,$$(F)))))

# Stage a fresh archive beside the destination; failure preserves the last build.
$(INSTRUMENT_ZIP): $(INSTRUMENT_FILES) $(INSTRUMENT_PACKAGE_SETTINGS) $(INSTRUMENT_RULES) | $(TMP_PROJECT_PATH)/
	@package_dir=$$(mktemp -d "$@.tmp.XXXXXXXX"); \
	trap 'rm -rf -- "$$package_dir"' EXIT; \
	trap 'exit 130' INT; trap 'exit 143' TERM; \
	zip --junk-paths "$$package_dir/$(notdir $@)" $(foreach F,$(INSTRUMENT_FILES),$(call build_shell_quote,$(F))); \
	mv -f -- "$$package_dir/$(notdir $@)" "$@"
	$(call ok,$@)

# A missing board copy can be restored without rebuilding the instrument.
BOARD_INSTRUMENT_ZIP := $(TMP)/$(BOARD)/instruments/$(NAME).zip
BOARD_INSTRUMENT_SETTINGS := $(TMP)/$(BOARD)/instruments/.$(NAME)-settings
$(eval $(call build_settings_stamp,$(BOARD_INSTRUMENT_SETTINGS),$$(call build_shell_quote,$$(abspath $$(INSTRUMENT_ZIP)))))
# Every published archive is complete, including if make is interrupted afterward.
.PRECIOUS: $(INSTRUMENT_ZIP) $(BOARD_INSTRUMENT_ZIP)
$(BOARD_INSTRUMENT_ZIP): $(INSTRUMENT_ZIP) $(BOARD_INSTRUMENT_SETTINGS) | $(TMP)/$(BOARD)/instruments/
	@copy_tmp=$$(mktemp "$@.tmp.XXXXXXXX"); \
	trap 'rm -f -- "$$copy_tmp"' EXIT; \
	trap 'exit 130' INT; trap 'exit 143' TERM; \
	cp -- "$<" "$$copy_tmp"; \
	chmod 0644 "$$copy_tmp"; \
	mv -f -- "$$copy_tmp" "$@"
