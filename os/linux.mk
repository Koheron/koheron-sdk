LINUX_TAG := xilinx-linux-v$(VIVADO_VERSION)
LINUX_URL := https://github.com/Xilinx/linux-xlnx/archive/refs/tags/xilinx-v$(VIVADO_VERSION).tar.gz
LINUX_PATH := $(TMP)/linux-xlnx-$(ARCH)-$(LINUX_TAG)
LINUX_TAR := $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz

DTC_BIN := $(LINUX_PATH)/scripts/dtc/dtc

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@
	$(call ok,$@)

$(LINUX_PATH)/.unpacked: $(LINUX_TAR) | $(LINUX_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

# Paths
LINUX_PATCH_DIR  := $(OS_PATH)/patches/linux
LINUX_PATCH_FILES:= $(shell find $(LINUX_PATCH_DIR) -type f)

# Stamps
LINUX_SYNC_STAMP := $(LINUX_PATH)/.patched
LINUX_CONFIG     := $(LINUX_PATH)/.config
LINUX_BUILD_STAMP:= $(LINUX_PATH)/.built_all

$(LINUX_SYNC_STAMP): $(LINUX_PATH)/.unpacked $(LINUX_PATCH_FILES)
	# Mirror patches into the kernel tree
	rsync -a "$(LINUX_PATCH_DIR)/" "$(LINUX_PATH)/"
	install -d "$(LINUX_PATH)/drivers/koheron"
	echo 'obj-y += koheron/' >> "$(LINUX_PATH)/drivers/Makefile"
	@touch $@

$(LINUX_CONFIG): $(LINUX_PATH)/.unpacked $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig \
                 $(shell find $(OS_PATH)/patches/linux/drivers/koheron -type f)
	# 1) hook once (no Kconfig)
	install -d "$(LINUX_PATH)/drivers/koheron"
	[ -f "$(LINUX_PATH)/drivers/koheron/Makefile" ] || \
	  printf 'obj-y += bram_wc.o\n' >"$(LINUX_PATH)/drivers/koheron/Makefile"
	f="$(LINUX_PATH)/drivers/Makefile"; \
	grep -qxF 'obj-y += koheron/' "$$f" || echo 'obj-y += koheron/' >> "$$f"

	# 2) sync only the koheron subtree
	rsync -a --delete "$(OS_PATH)/patches/linux/drivers/koheron/" \
	                "$(LINUX_PATH)/drivers/koheron/"

	# 3) configure only if needed (no mrproper on normal edits)
	if [ ! -f "$(LINUX_PATH)/.config" ]; then \
	  install -d "$(LINUX_PATH)/arch/$(ARCH)/configs"; \
	  cp "$(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig" \
	     "$(LINUX_PATH)/arch/$(ARCH)/configs"; \
	  $(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) xilinx_$(ZYNQ_TYPE)_defconfig; \
	fi
	@touch $@
	$(call ok,$@)

# normal build
$(LINUX_BUILD_STAMP): $(LINUX_CONFIG)
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) \
	  CROSS_COMPILE=$(GCC_ARCH)- --jobs=$(N_CPUS) $(KERNEL_BIN) dtbs
	@touch $@
	$(call ok,$@)

$(DTC_BIN): $(LINUX_PATH)/.unpacked $(LINUX_BUILD_STAMP)
	@true

.PHONY: clean_linux
clean_linux:
	rm -rf $(LINUX_PATH)