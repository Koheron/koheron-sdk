# Build:
# - First-stage boot loader
# - Device tree
# - U-boot
# - Linux kernel
include $(OS_PATH)/toolchain.mk

BOARD := $(shell basename $(BOARD_PATH))

TMP_OS_PATH := $(TMP_PROJECT_PATH)/os
ABS_TMP_OS_PATH := $(abspath $(TMP_OS_PATH))

UBOOT_PATH := $(TMP_OS_PATH)/u-boot-xlnx-$(UBOOT_TAG)
LINUX_PATH := $(TMP_OS_PATH)/linux-xlnx-$(LINUX_TAG)
DTREE_PATH := $(TMP_OS_PATH)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR := $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
LINUX_TAR := $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz
DTREE_TAR := $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

UBOOT_CFLAGS := -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard

TMP_OS_VERSION_FILE := $(TMP_OS_PATH)/version.json

$(TMP_OS_VERSION_FILE): $(KOHERON_VERSION_FILE)
	echo '{ "version": "$(KOHERON_VERSION)" }' > $@

DTB_SWITCH = $(TMP_OS_PATH)/devicetree.dtb
ifdef DTREE_OVERRIDE
DTB_SWITCH = $(TMP_OS_PATH)/devicetree_$(DTREE_LOC)
endif
ABS_DTB_SWITCH  := $(abspath $(DTB_SWITCH))

# Kernel image name per arch
ifeq ($(ARCH),arm)
  KERNEL_BIN := zImage
else ifeq ($(ARCH),arm64)
  KERNEL_BIN := Image
else
  $(error Unsupported ARCH $(ARCH); expected arm or arm64)
endif

FIT_ITS := $(TMP_OS_PATH)/kernel.its
FIT_ITB := $(TMP_OS_PATH)/kernel.itb

BOOT_MEDIUM ?= mmcblk0

.PHONY: os
os: $(INSTRUMENT_ZIP) www api $(TMP_OS_PATH)/$(BOOTCALL) $(FIT_ITB) $(DTB_SWITCH) $(TMP_OS_VERSION_FILE)

# Build image (run as root)
.PHONY: image
image:
	$(DOCKER_ROOT) bash $(OS_PATH)/scripts/ubuntu-$(MODE).sh \
	  $(TMP_PROJECT_PATH) $(OS_PATH) $(TMP_OS_PATH) \
	  $(NAME) $(TMP_OS_VERSION_FILE) $(ZYNQ_TYPE) $(BOOT_MEDIUM)

# Flash image on SD card
.PHONY: flash
flash:
	$(PYTHON) $(OS_PATH)/scripts/flash_all.py $(TMP_PROJECT_PATH)/release.zip

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

$(TMP_OS_PATH)/fsbl/Makefile:  $(TMP_FPGA_PATH)/$(NAME).xsa | $(TMP_OS_PATH)/fsbl/
	$(HSI) $(FPGA_PATH)/hsi/fsbl.tcl $(NAME) $(PROC) $(TMP_OS_PATH)/hard $(@D) $< $(ZYNQ_TYPE)
	@echo [$@] OK

$(TMP_OS_PATH)/fsbl/executable.elf: $(TMP_OS_PATH)/fsbl/Makefile $(FSBL_FILES)
	cp -a $(FSBL_PATH)/. $(TMP_OS_PATH)/fsbl/ 2>/dev/null || true
	@if test -f $(FSBL_PATH)/apply_ps7_init_patch.py; then \
		echo "Patching ps7_init.c ..."; \
		$(PYTHON) $(FSBL_PATH)/apply_ps7_init_patch.py $(TMP_OS_PATH)/fsbl; \
	fi
	source $(VIVADO_PATH)/settings64.sh && $(DOCKER) make -C $(@D) all
	@echo [$@] OK

.PHONY: clean_fsbl
clean_fsbl:
	rm -rf $(TMP_OS_PATH)/fsbl

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@
	@echo [$@] OK

$(UBOOT_PATH)/.unpacked: $(UBOOT_TAR) | $(UBOOT_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	touch $@
	@echo [$@] OK

ifeq ("$(UBOOT_CONFIG)","")
UBOOT_CONFIG = zynq_$(BOARD)_defconfig
endif

UBOOT_PATCH_FILES := $(shell test -d $(PATCHES)/u-boot && find $(PATCHES)/u-boot -type f)

$(TMP_OS_PATH)/u-boot.elf: $(UBOOT_PATH)/.unpacked $(UBOOT_PATCH_FILES) | $(TMP_OS_PATH)/
	cp -a $(PATCHES)/${UBOOT_CONFIG} $(UBOOT_PATH)/ 2>/dev/null || true
	cp -a $(PATCHES)/u-boot/. $(UBOOT_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(UBOOT_PATH) mrproper
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(ARCH) $(UBOOT_CONFIG)
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(ARCH) CFLAGS="$(UBOOT_CFLAGS) $(GCC_FLAGS)" \
	  CROSS_COMPILE=$(GCC_ARCH)- all
	if [ -f $(UBOOT_PATH)/u-boot.elf ]; then cp $(UBOOT_PATH)/u-boot.elf $@; else cp $(UBOOT_PATH)/u-boot $@; fi
	@echo [$@] OK

###############################################################################
# pmufw
###############################################################################

.PHONY: pmufw
pmufw: $(TMP_OS_PATH)/pmu/pmufw.elf

$(TMP_OS_PATH)/pmu/Makefile: $(TMP_FPGA_PATH)/$(NAME).xsa
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/pmufw.tcl $(NAME) $(TMP_OS_PATH)/hard $(@D) $<
	@echo [$@] OK

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
	@echo [$@] OK

$(ATRUST_PATH)/.unpacked: $(ATRUST_TAR) | $(ATRUST_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	@echo [$@] OK

$(TMP_OS_PATH)/bl31.elf: $(ATRUST_PATH)/.unpacked
	$(DOCKER) make CROSS_COMPILE=$(GCC_ARCH)- PLAT=zynqmp bl31 ZYNQMP_ATF_MEM_BASE=0x10000 ZYNQMP_ATF_MEM_SIZE=0x40000 -C $(ATRUST_PATH)
	cp $</build/zynqmp/release/bl31/bl31.elf $@
	@echo [$@] OK

###############################################################################
# boot.bin
###############################################################################

$(TMP_OS_PATH)/boot.bin: $(TMP_OS_PATH)/fsbl/executable.elf $(BITSTREAM) $(TMP_OS_PATH)/u-boot.elf
	echo "img:{[bootloader] $(TMP_OS_PATH)/fsbl/executable.elf" > $(TMP_OS_PATH)/boot.bif
	echo " $(BITSTREAM)" >> $(TMP_OS_PATH)/boot.bif
	echo " $(TMP_OS_PATH)/u-boot.elf" >> $(TMP_OS_PATH)/boot.bif
	echo " }" >> $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	@echo [$@] OK

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
	@echo [$@] OK

###############################################################################
# DEVICE TREE
###############################################################################

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@
	@echo [$@] OK

$(DTREE_PATH)/.unpacked: $(DTREE_TAR) | $(DTREE_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	@echo [$@] OK

.PHONY: overlay
overlay: $(TMP_OS_PATH)/pl.dtbo

.PHONY: devicetree
devicetree: $(TMP_OS_PATH)/devicetree/system-top.dts

$(TMP_OS_PATH)/overlay/pl.dtsi: $(TMP_FPGA_PATH)/$(NAME).xsa $(DTREE_PATH)/.unpacked $(PATCHES)/overlay.patch
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VER) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/overlay $(TMP_FPGA_PATH)/$(NAME).xsa $(BOOT_MEDIUM)
	cp -R $(TMP_OS_PATH)/overlay $(TMP_OS_PATH)/overlay.orig
	[[ -f $(PATCHES)/overlay.patch ]] || :
	patch -d $(TMP_OS_PATH) -p -0 < $(PATCHES)/overlay.patch
	@echo [$@] OK

$(TMP_OS_PATH)/devicetree/system-top.dts: $(TMP_FPGA_PATH)/$(NAME).xsa $(DTREE_PATH)/.unpacked $(PATCHES)/devicetree.patch
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VER) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/devicetree $(TMP_FPGA_PATH)/$(NAME).xsa $(BOOT_MEDIUM)
	cp -r $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree.orig
	patch -d $(TMP_OS_PATH) -p -0 < $(PATCHES)/devicetree.patch
	@echo [$@] OK

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

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@
	@echo [$@] OK

$(LINUX_PATH)/.unpacked: $(LINUX_TAR) | $(LINUX_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	@echo [$@] OK

LINUX_PATCH_FILES := $(shell test -d $(PATCHES)/linux && find $(PATCHES)/linux -type f)

$(TMP_OS_PATH)/$(KERNEL_BIN): $(LINUX_PATH)/.unpacked $(LINUX_PATCH_FILES) $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig | $(TMP_OS_PATH)/
	cp $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig $(LINUX_PATH)/arch/$(ARCH)/configs
	cp -a $(PATCHES)/linux/. $(LINUX_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(LINUX_PATH) mrproper
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) xilinx_$(ZYNQ_TYPE)_defconfig
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) \
	  CROSS_COMPILE=$(GCC_ARCH)- --jobs=$(N_CPUS) $(KERNEL_BIN) dtbs
	cp $(LINUX_PATH)/arch/$(ARCH)/boot/$(KERNEL_BIN) $@
	@echo [$@] OK

$(TMP_PROJECT_PATH)/pl.dtbo: $(TMP_OS_PATH)/pl.dtbo
	cp $(TMP_OS_PATH)/pl.dtbo  $(TMP_PROJECT_PATH)/pl.dtbo

$(LINUX_PATH)/scripts/dtc/dtc: $(LINUX_PATH)/.unpacked $(LINUX_PATCH_FILES) $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig
	cp $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig $(LINUX_PATH)/arch/$(ARCH)/configs
	cp -a $(PATCHES)/linux/. $(LINUX_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(LINUX_PATH) mrproper
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) xilinx_$(ZYNQ_TYPE)_defconfig
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(GCC_ARCH)- dtbs -j$(N_CPUS)
	@echo [$@] OK

$(TMP_OS_PATH)/pl.dtbo: $(LINUX_PATH)/scripts/dtc/dtc $(TMP_OS_PATH)/overlay/pl.dtsi
	sed -i 's/".bin"/"$(NAME).bit.bin"/g' $(TMP_OS_PATH)/overlay/pl.dtsi
	$(LINUX_PATH)/scripts/dtc/dtc -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/overlay -b 0 -@ $(TMP_OS_PATH)/overlay/pl.dtsi
	@echo [$@] OK

$(TMP_OS_PATH)/devicetree.dtb: $(LINUX_PATH)/scripts/dtc/dtc  $(TMP_OS_PATH)/devicetree/system-top.dts
	$(DOCKER) gcc -I $(TMP_OS_PATH)/devicetree/ -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp -o \
		$(TMP_OS_PATH)/devicetree/system-top.dts.tmp $(TMP_OS_PATH)/devicetree/system-top.dts
	$(LINUX_PATH)/scripts/dtc/dtc -I dts -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/devicetree -b 0 -@ $(TMP_OS_PATH)/devicetree/system-top.dts.tmp
	@echo [$@] OK

.PHONY: $(TMP_OS_PATH)/devicetree_linux
$(TMP_OS_PATH)/devicetree_linux: $(LINUX_PATH)/scripts/dtc/dtc
	echo ${DTREE_OVERRIDE}
	cp -a $(PATCHES)/linux/. $(LINUX_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(LINUX_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(GCC_ARCH)- dtbs -j$(N_CPUS)
	cp $(LINUX_PATH)/${DTREE_OVERRIDE} $(TMP_OS_PATH)/devicetree.dtb
	@echo [$(TMP_OS_PATH)/devicetree.dtb] OK

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
	@echo "[$@] OK"

$(FIT_ITB): $(FIT_ITS) | $(TMP_OS_PATH)/
	mkimage -f $< $@
	@echo "[$@] OK"

###############################################################################
# HTTP API
###############################################################################

TMP_API_PATH := $(TMP)/api

.PHONY:
api: $(TMP_API_PATH)/wsgi.py \
        $(TMP_API_PATH)/app/__init__.py \
        $(TMP_API_PATH)/app/install_instrument.sh

$(TMP_API_PATH)/wsgi.py: $(OS_PATH)/api/wsgi.py
	mkdir -p $(@D)
	cp $< $@

$(TMP_API_PATH)/app/%: $(OS_PATH)/api/%
	mkdir -p $(@D)
	cp $< $@

# run `systemctl restart uwsgi` on the board
.PHONY:
api_sync: api
	rsync -avz -e "ssh -i /ssh-private-key" $(TMP_API_PATH)/. root@$(HOST):/usr/local/api/

.PHONY:
api_clean:
	rm -rf $(TMP_API_PATH)

###############################################################################
# WWW
###############################################################################

WWW_PATH:= $(OS_PATH)/www
TMP_WWW_PATH:= $(TMP)/www

.PHONY: www
www : $(TMP_WWW_PATH)/koheron.css \
		$(TMP_WWW_PATH)/instruments.js \
		$(TMP_WWW_PATH)/index.html \
		$(TMP_WWW_PATH)/main.css \
		$(TMP_WWW_PATH)/bootstrap.min.js \
		$(TMP_WWW_PATH)/bootstrap.min.css \
		$(TMP_WWW_PATH)/jquery.min.js \
		$(TMP_WWW_PATH)/koheron.svg \
		$(TMP_WWW_PATH)/koheron_logo.svg \
		$(TMP_WWW_PATH)/kbird.ico \
		$(TMP_WWW_PATH)/lato-v11-latin-400.woff2 \
		$(TMP_WWW_PATH)/lato-v11-latin-700.woff2 \
		$(TMP_WWW_PATH)/lato-v11-latin-900.woff2 \
		$(TMP_WWW_PATH)/glyphicons-halflings-regular.woff2 \
		$(TMP_WWW_PATH)/navigation.html \
		$(TMP_WWW_PATH)/html-imports.min.js \
		$(TMP_WWW_PATH)/html-imports.min.js.map

.PHONY: www_sync
www_sync: www
	rsync -avz -e "ssh -i /ssh-private-key" $(TMP_WWW_PATH)/. root@$(HOST):/usr/local/www/

.PHONY: clean_www
clean_www:
	rm -rf $(TMP_WWW_PATH)

WWW_TS_FILES := $(WEB_PATH)/koheron.ts
WWW_TS_FILES += $(WWW_PATH)/instruments.ts
WWW_TS_FILES += $(WWW_PATH)/instruments_widget.ts

$(TMP_WWW_PATH)/instruments.js: $(WWW_TS_FILES)
	mkdir -p $(@D)
	$(TSC) $^ --outFile $@

$(TMP_WWW_PATH)/koheron.css:
	mkdir -p $(@D)
	curl https://assets.koheron.com/css/main.css -o $@

$(TMP_WWW_PATH)/index.html: $(WWW_PATH)/index.html
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/navigation.html: $(WEB_PATH)/navigation.html
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/main.css: $(WEB_PATH)/main.css
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/bootstrap.min.js:
	mkdir -p $(@D)
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js -o $@

$(TMP_WWW_PATH)/bootstrap.min.css:
	mkdir -p $(@D)
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css -o $@

$(TMP_WWW_PATH)/jquery.min.js:
	mkdir -p $(@D)
	curl https://code.jquery.com/jquery-1.12.4.min.js -o $@

$(TMP_WWW_PATH)/koheron.svg:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.svg -o $@

$(TMP_WWW_PATH)/koheron_logo.svg:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron_logo.svg -o $@

$(TMP_WWW_PATH)/kbird.ico:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.ico -o $@

$(TMP_WWW_PATH)/lato-v11-latin-400.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/1YwB1sO8YE1Lyjf12WNiUA.woff2 -o $@

$(TMP_WWW_PATH)/lato-v11-latin-700.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/H2DMvhDLycM56KNuAtbJYA.woff2 -o $@

$(TMP_WWW_PATH)/lato-v11-latin-900.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/tI4j516nok_GrVf4dhunkg.woff2 -o $@

$(TMP_WWW_PATH)/glyphicons-halflings-regular.woff2:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/fonts/glyphicons-halflings-regular.woff2 -o $@

$(TMP_WWW_PATH)/html-imports.min.js:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js -o $@

$(TMP_WWW_PATH)/html-imports.min.js.map:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js.map -o $@

###############################################################################
# TEST
###############################################################################

.PHONY: test_os
test_os:
	$(PYTHON) $(OS_PATH)/test_os.py $(HOST)