# Runtime kernel release is independent of the FPGA/boot toolchain.
include $(dir $(lastword $(MAKEFILE_LIST)))compiler-settings.mk
LINUX_VERSION ?= 2026.1
LINUX_TAG := xilinx-linux-v$(LINUX_VERSION)
LINUX_URL := https://github.com/Xilinx/linux-xlnx/archive/refs/tags/xilinx-v$(LINUX_VERSION).tar.gz
LINUX_PATH := $(TMP)/linux-xlnx-$(ARCH)-$(LINUX_TAG)
LINUX_TAR := $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz
SOURCE_CHECKSUMS := $(OS_PATH)/source-checksums.sha256
DOWNLOAD_VERIFIED := $(OS_PATH)/scripts/download_verified.sh

DTC_BIN := $(LINUX_PATH)/scripts/dtc/dtc

$(LINUX_TAR): $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED)
	mkdir -p $(@D)
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $@ $(LINUX_URL)
	$(call ok,$@)

$(LINUX_PATH)/.unpacked: $(LINUX_TAR) $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED) | $(LINUX_PATH)/
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $(LINUX_TAR) $(LINUX_URL)
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
LINUX_COMPILER_STAMP := $(LINUX_PATH)/.compiler-settings
$(eval $(call compiler_settings_stamp,$(LINUX_COMPILER_STAMP),$(KERNEL_CC)))

$(LINUX_SYNC_STAMP): $(LINUX_PATH)/.unpacked $(LINUX_PATCH_FILES)
	# Mirror patches into the kernel tree
	rsync -a "$(LINUX_PATCH_DIR)/" "$(LINUX_PATH)/"
	install -d "$(LINUX_PATH)/drivers/koheron"
	f="$(LINUX_PATH)/drivers/Makefile"; \
	grep -qxF 'obj-y += koheron/' "$$f" || echo 'obj-y += koheron/' >> "$$f"
	@touch $@

$(LINUX_CONFIG): $(LINUX_SYNC_STAMP) $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig $(LINUX_COMPILER_STAMP)
	# The checked-in defconfig is authoritative when configuration inputs change.
	install -d "$(LINUX_PATH)/arch/$(ARCH)/configs"
	cp "$(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig" \
	   "$(LINUX_PATH)/arch/$(ARCH)/configs"
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) \
	  $(KERNEL_TOOLCHAIN_FLAGS) xilinx_$(ZYNQ_TYPE)_defconfig
	@touch $@
	$(call ok,$@)

# normal build
$(LINUX_BUILD_STAMP): $(LINUX_CONFIG) $(LINUX_SYNC_STAMP) $(DTC_BIN)
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) \
	  $(KERNEL_TOOLCHAIN_FLAGS) --jobs=$(N_CPUS) $(KERNEL_BIN) dtbs
	@touch $@
	$(call ok,$@)

# Instrument overlays need the host compiler, not a full kernel build.
$(DTC_BIN): $(LINUX_CONFIG) $(LINUX_SYNC_STAMP)
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) \
	  $(KERNEL_TOOLCHAIN_FLAGS) --jobs=$(N_CPUS) scripts_dtc
	@test -x $@ && touch $@

.PHONY: clean_linux
clean_linux:
	rm -rf $(LINUX_PATH)
