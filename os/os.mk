TMP_OS_PATH := $(TMP_PROJECT_PATH)/os
ABS_TMP_OS_PATH := $(abspath $(TMP_OS_PATH))

TMP_OS_BOARD_PATH ?= $(TMP_PROJECT_PATH)/os

UBOOT_TAG ?= xilinx-uboot-v$(VIVADO_VERSION)
DTREE_TAG ?= xilinx_v$(VIVADO_VERSION)


UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz


include $(OS_PATH)/rootfs.mk

UBOOT_PATH ?= $(TMP_OS_BOARD_PATH)/u-boot-xlnx-$(UBOOT_TAG)
DTREE_PATH := $(TMP_OS_PATH)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR := $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
DTREE_TAR := $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

UBOOT_CFLAGS := -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard

DTB_SWITCH = $(TMP_OS_PATH)/devicetree.dtb
ifdef DTREE_OVERRIDE
DTB_SWITCH = $(TMP_OS_PATH)/devicetree_$(DTREE_LOC)
endif
ABS_DTB_SWITCH  := $(abspath $(DTB_SWITCH))

FIT_ITS := $(TMP_OS_PATH)/kernel.its
FIT_ITB := $(TMP_OS_PATH)/kernel.itb

BOOT_MEDIUM ?= mmcblk0

OS_FILES := \
  $(INSTRUMENT_ZIP) \
  $(API_FILES) \
  $(WWW_ASSETS) \
  $(TMP_OS_PATH)/$(BOOTCALL) \
  $(FIT_ITB) \
  $(DTB_SWITCH) \

.PHONY: os
os: $(OS_FILES)

.PHONY: base-rootfs
base-rootfs: $(BASE_ROOTFS_TAR)

.PHONY: clean-base-rootfs
clean-base-rootfs:
	rm -f $(BASE_ROOTFS_TAR)

$(BASE_ROOTFS_TAR): \
  $(OS_PATH)/scripts/build_base_rootfs_tar.sh \
  $(OS_PATH)/scripts/chroot_base_rootfs.sh \
  $(ROOT_TAR_PATH)
	@mkdir -p $(@D)
	@test -s "$(ROOT_TAR_PATH)" || { echo "Missing root tar: $(ROOT_TAR_PATH)"; exit 1; }
	# Optional envs: TIMEZONE, PASSWD
	$(DOCKER_ROOT) bash $(OS_PATH)/scripts/build_base_rootfs_tar.sh \
	  "$(ROOT_TAR_PATH)" "$@" "$(QEMU_BIN)"
	$(call ok,$@)

$(RELEASE_ZIP): $(BASE_ROOTFS_TAR) \
  $(INSTRUMENT_ZIP) \
  $(TMP_OS_PATH)/$(BOOTCALL) $(FIT_ITB) $(DTB_SWITCH) \
  $(OS_PATH)/scripts/build_image.sh \
  $(OVERLAY_TAR) $(MANIFEST_TXT) \
  $(OS_PATH)/scripts/chroot_overlay.sh
	@mkdir -p $(@D)
	@test -s "$(OVERLAY_TAR)" || { echo "Missing overlay tar: $(OVERLAY_TAR)"; exit 1; }
	$(DOCKER_ROOT) env BASE_ROOTFS_TAR="$(BASE_ROOTFS_TAR)" \
	  bash $(OS_PATH)/scripts/build_image.sh \
	    "$(TMP_PROJECT_PATH)" "$(OS_PATH)" "$(TMP_OS_PATH)" \
	    "$(ROOT_TAR_PATH)" "$(OVERLAY_TAR)" "$(QEMU_BIN)" \
	    "$(RELEASE_NAME)"
	$(call ok,$@)

# Build image
.PHONY: image
image: $(RELEASE_ZIP)

# Flash image on SD card
.PHONY: flash
flash:
	python3 $(OS_PATH)/scripts/flash_all.py $(TMP_PROJECT_PATH)/$(RELEASE_NAME).zip

.PHONY: clean_os
clean_os:
	rm -rf $(TMP_OS_PATH)

###############################################################################
# First-stage boot loader
###############################################################################

# Additional files (including fsbl_hooks.c) can be added to the FSBL in $(BOARD_PATH)/patches/fsbl
ifndef FSBL_PATH
FSBL_PATH := $(BOARD_PATH)/patches/fsbl
endif
FSBL_FILES := $(wildcard $(FSBL_PATH)/*.h $(FSBL_PATH)/*.c)

.PHONY: fsbl
fsbl: $(TMP_OS_PATH)/fsbl/executable.elf

$(TMP_OS_PATH)/hard/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xsa | $(TMP_OS_PATH)/hard/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/fsbl/Makefile:  $(TMP_OS_PATH)/hard/$(NAME).xsa | $(TMP_OS_PATH)/fsbl/
	$(HSI) $(FPGA_PATH)/hsi/fsbl.tcl $(NAME) $(PROC) $(TMP_OS_PATH)/hard $(@D) $< $(ZYNQ_TYPE)
	$(call ok,$@)

$(TMP_OS_PATH)/fsbl/executable.elf: $(TMP_OS_PATH)/fsbl/Makefile $(FSBL_FILES)
	cp -a $(FSBL_PATH)/. $(TMP_OS_PATH)/fsbl/ 2>/dev/null || true
	@if test -f $(FSBL_PATH)/apply_ps7_init_patch.py; then \
		echo "Patching ps7_init.c ..."; \
		python3 $(FSBL_PATH)/apply_ps7_init_patch.py $(TMP_OS_PATH)/fsbl; \
	fi
	source $(VIVADO_PATH)/settings64.sh && $(DOCKER) make -C $(@D) all
	$(call ok,$@)

.PHONY: clean_fsbl
clean_fsbl:
	rm -rf $(TMP_OS_PATH)/fsbl

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@
	$(call ok,$@)

$(UBOOT_PATH)/.unpacked: $(UBOOT_TAR) | $(UBOOT_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	touch $@
	$(call ok,$@)

UBOOT_PATCH_FILES := $(shell test -d $(PATCHES)/u-boot && find $(PATCHES)/u-boot -type f)

# Configure U-Boot once to avoid concurrent defconfig/mrproper races
UBOOT_CONFIG_STAMP := $(UBOOT_PATH)/.config

$(UBOOT_CONFIG_STAMP): $(UBOOT_PATH)/.unpacked $(UBOOT_PATCH_FILES)
	cp -a $(PATCHES)/${UBOOT_CONFIG} $(UBOOT_PATH)/ 2>/dev/null || true
	cp -a $(PATCHES)/u-boot/. $(UBOOT_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(UBOOT_PATH) mrproper
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(ARCH) $(UBOOT_CONFIG)
	@touch $@
	$(call ok,$@)

$(TMP_OS_BOARD_PATH)/u-boot.elf: $(UBOOT_CONFIG_STAMP) | $(TMP_BOARD_PATH)/
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(ARCH) CFLAGS="$(UBOOT_CFLAGS) $(GCC_FLAGS)" \
	  CROSS_COMPILE=$(GCC_ARCH)- all
	if [ -f $(UBOOT_PATH)/u-boot.elf ]; then cp $(UBOOT_PATH)/u-boot.elf $@; else cp $(UBOOT_PATH)/u-boot $@; fi
	$(call ok,$@)

$(TMP_OS_PATH)/u-boot.elf: $(TMP_OS_BOARD_PATH)/u-boot.elf | $(TMP_OS_PATH)/
	cp $< $@
	$(call ok,$@)

###############################################################################
# pmufw
###############################################################################

.PHONY: pmufw
pmufw: $(TMP_OS_PATH)/pmu/pmufw.elf

$(TMP_OS_PATH)/pmu/Makefile: $(TMP_FPGA_PATH)/$(NAME).xsa
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/pmufw.tcl $(NAME) $(TMP_OS_PATH)/hard $(@D) $<
	$(call ok,$@)

$(TMP_OS_PATH)/pmu/executable.elf: $(TMP_OS_PATH)/pmu/Makefile
	source $(VITIS_PATH)/settings64.sh && $(DOCKER) make -C $(@D) all

.PHONY: clean_pmufw
clean_pmufw:
	rm -rf $(TMP_OS_PATH)/pmu

###############################################################################
# arm_trusted_firmware
###############################################################################

$(ATRUST_TAR):
	mkdir -p $(@D)
	curl -L $(ARMTRUST_URL) -o $@
	$(call ok,$@)

$(ATRUST_PATH)/.unpacked: $(ATRUST_TAR) | $(ATRUST_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

$(TMP_OS_PATH)/bl31.elf: $(ATRUST_PATH)/.unpacked
	$(DOCKER) make CROSS_COMPILE=$(GCC_ARCH)- PLAT=zynqmp bl31 ZYNQMP_ATF_MEM_BASE=0x10000 ZYNQMP_ATF_MEM_SIZE=0x40000 -C $(ATRUST_PATH)
	cp $</build/zynqmp/release/bl31/bl31.elf $@
	$(call ok,$@)

###############################################################################
# boot.bin
###############################################################################

$(TMP_OS_PATH)/boot.bin: $(TMP_OS_PATH)/fsbl/executable.elf $(BITSTREAM) $(TMP_OS_PATH)/u-boot.elf
	echo "img:{[bootloader] $(TMP_OS_PATH)/fsbl/executable.elf" > $(TMP_OS_PATH)/boot.bif
	echo " $(BITSTREAM)" >> $(TMP_OS_PATH)/boot.bif
	echo " $(TMP_OS_PATH)/u-boot.elf" >> $(TMP_OS_PATH)/boot.bif
	echo " }" >> $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	$(call ok,$@)

$(TMP_OS_PATH)/bootmp.bin: $(TMP_OS_PATH)/pmu/executable.elf $(TMP_OS_PATH)/bl31.elf $(TMP_OS_PATH)/fsbl/executable.elf $(BITSTREAM) $(TMP_OS_PATH)/u-boot.elf
	echo "img:{ [fsbl_config] a53_x64" > $(TMP_OS_PATH)/boot.bif
	echo "[pmufw_image] $(TMP_OS_PATH)/pmu/executable.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[bootloader] $(TMP_OS_PATH)/fsbl/executable.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_device=pl] $(BITSTREAM)" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_cpu=a53-0,exception_level=el-2] $(TMP_OS_PATH)/bl31.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_cpu=a53-0,exception_level=el-2] $(TMP_OS_PATH)/u-boot.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "}" >> $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	cp $(TMP_OS_PATH)/bootmp.bin $(TMP_OS_PATH)/boot.bin
	$(call ok,$@)

###############################################################################
# DEVICE TREE
###############################################################################

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@
	$(call ok,$@)

$(DTREE_PATH)/.unpacked: $(DTREE_TAR) | $(DTREE_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

.PHONY: overlay
overlay: $(TMP_OS_PATH)/pl.dtbo

.PHONY: devicetree
devicetree: $(TMP_OS_PATH)/devicetree/system-top.dts

phony: pl.dtsi
pl.dtsi: $(TMP_OS_PATH)/overlay/pl.dtsi

$(TMP_OS_PATH)/overlay/pl.dtsi: $(TMP_OS_PATH)/hard/$(NAME).xsa $(DTREE_PATH)/.unpacked
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VERSION) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/overlay $< $(BOOT_MEDIUM)
	$(call ok,$@)

$(TMP_OS_PATH)/devicetree/system-top.dts: $(TMP_OS_PATH)/hard/$(NAME).xsa $(DTREE_PATH)/.unpacked $(PATCHES)/devicetree.patch
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VERSION) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/devicetree $< $(BOOT_MEDIUM)
	cp -r $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree.orig
	patch -d $(TMP_OS_PATH) -p -0 < $(PATCHES)/devicetree.patch
	$(call ok,$@)

.PHONY: clean_devicetree
clean_devicetree:
	rm -rf $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree.orig $(TMP_OS_PATH)/devicetree.patch

.PHONY: patch_devicetree
patch_devicetree:
	bash os/scripts/patch_devicetree.sh $(TMP_OS_PATH) $(BOARD_PATH)

.PHONY: patch_overlay
patch_overlay:
	bash os/scripts/patch_overlay.sh $(TMP_OS_PATH) $(PROJECT_PATH)

.PHONY: clean_overlay
clean_overlay:
	rm -rf $(TMP_OS_PATH)/overlay $(TMP_OS_PATH)/overlay.orig

###############################################################################
# LINUX
###############################################################################

$(TMP_OS_PATH)/$(KERNEL_BIN): $(LINUX_BUILD_STAMP) | $(TMP_OS_PATH)/
	cp "$(LINUX_PATH)/arch/$(ARCH)/boot/$(KERNEL_BIN)" "$@"

$(TMP_OS_PATH)/overlay/memory.dtsi: $(MEMORY_YML) $(FPGA_PATH)/memory.dtsi
	$(MAKE_PY) --memory_dtsi $@ $<
	$(call ok,$@)

$(TMP_OS_PATH)/overlay/pl_wrap.dts: $(TMP_OS_PATH)/overlay/pl.dtsi $(TMP_OS_PATH)/overlay/memory.dtsi
	@{ echo '/dts-v1/;'; \
	   echo '/plugin/;'; \
	   echo '/include/ "pl.dtsi"'; \
	   echo '/include/ "memory.dtsi"'; } > $@
	$(call ok,$@)

$(TMP_OS_PATH)/pl.dtbo: $(DTC_BIN) $(TMP_OS_PATH)/overlay/pl.dtsi $(TMP_OS_PATH)/overlay/pl_wrap.dts
	sed -i 's/".bin"/"$(NAME).bit.bin"/g' $(TMP_OS_PATH)/overlay/pl.dtsi
	$(DTC_BIN) -@ -I dts -O dtb -b 0 \
	  -i $(TMP_OS_PATH)/overlay \
	  -o $@ $(TMP_OS_PATH)/overlay/pl_wrap.dts
	$(call ok,$@)

$(TMP_PROJECT_PATH)/pl.dtbo: $(TMP_OS_PATH)/pl.dtbo
	cp $(TMP_OS_PATH)/pl.dtbo  $(TMP_PROJECT_PATH)/pl.dtbo

$(TMP_OS_PATH)/devicetree.dtb: $(DTC_BIN)  $(TMP_OS_PATH)/devicetree/system-top.dts
	$(DOCKER) gcc -I $(TMP_OS_PATH)/devicetree/ -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp -o \
		$(TMP_OS_PATH)/devicetree/system-top.dts.tmp $(TMP_OS_PATH)/devicetree/system-top.dts
	$(DTC_BIN) -I dts -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/devicetree -b 0 -@ $(TMP_OS_PATH)/devicetree/system-top.dts.tmp
	$(call ok,$@)

.PHONY: $(TMP_OS_PATH)/devicetree_linux
$(TMP_OS_PATH)/devicetree_linux: $(DTC_BIN)
	echo ${DTREE_OVERRIDE}
	cp -a $(OS_PATH)/patches/linux/. $(LINUX_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(GCC_ARCH)- dtbs -j$(N_CPUS)
	cp $(LINUX_PATH)/${DTREE_OVERRIDE} $(TMP_OS_PATH)/devicetree.dtb
	$(call ok,$@)

.PHONY: $(TMP_OS_PATH)/devicetree_uboot
$(TMP_OS_PATH)/devicetree_uboot: $(TMP_OS_PATH)/u-boot.elf
	echo ${DTREE_OVERRIDE}
	cp $(UBOOT_PATH)/${DTREE_OVERRIDE} $(TMP_OS_PATH)/devicetree.dtb
	@echo [$(TMP_OS_PATH)/devicetree.dtb] OK

ifeq ($(ARCH),arm)
  FIT_LOAD := 0x03000000
else ifeq ($(ARCH),arm64)
  FIT_LOAD ?= 0x03000000
endif

define ITS_TEMPLATE
/dts-v1/;
/ {
  description = "Linux + DTB (FIT)";
  #address-cells = <1>;
  images {
    kernel_1 {
      description = "Linux kernel";
      data = /incbin/("$(ABS_TMP_OS_PATH)/$(KERNEL_BIN)");
      type = "kernel";
      arch = "$(ARCH)";
      os = "linux";
      compression = "none";
      load = <$(FIT_LOAD)>;
      entry = <$(FIT_LOAD)>;
    };
    fdt_1 {
      description = "Device Tree";
      data = /incbin/("$(ABS_DTB_SWITCH)");
      type = "flat_dt";
      arch = "$(ARCH)";
      compression = "none";
    };
  };
  configurations {
    default = "conf_1";
    conf_1 { kernel = "kernel_1"; fdt = "fdt_1"; };
  };
};
endef

$(FIT_ITS): $(TMP_OS_PATH)/$(KERNEL_BIN) $(DTB_SWITCH) | $(TMP_OS_PATH)/
	@$(file >$@,$(ITS_TEMPLATE))
	$(call ok,$@)

$(FIT_ITB): $(FIT_ITS) | $(TMP_OS_PATH)/
	mkimage -f $< $@
	$(call ok,$@)

###############################################################################
# TEST
###############################################################################

.PHONY: test_os
test_os:
	$(PYTHON) $(OS_PATH)/test_os.py $(HOST)
