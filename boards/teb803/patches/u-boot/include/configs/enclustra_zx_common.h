#ifndef __ENCLUSTRA_ZX_COMMON
#define __ENCLUSTRA_ZX_COMMON

/* Select which flash type currently uses Pins */
#define ZX_NONE    (0)
#define ZX_NAND    (1)
#define ZX_QSPI    (2)

#ifndef PHY_ANEG_TIMEOUT
#define PHY_ANEG_TIMEOUT	8000	/* PHY needs a longer aneg time */
#endif

#define MTDIDS_DEFAULT          "nand0=nand"
#define MTDPARTS_DEFAULT        "mtdparts=" \
	                                "nand:5M(nand-linux),"  \
                                "1M(nand-device-tree)," \
                                "1M(nand-bootscript),"  \
				"11M(ubi-env),"		\
                                "-(nand-rootfs)"
#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS
#define ENVIRONMENT_SIZE "0x18000"

#ifndef CONFIG_CMD_ELF
#define CONFIG_CMD_ELF
#endif

/* QSPI Flash Memory Map */
#define QSPI_SIZE                  0x04000000 // We support only 64 MB flashes
#define QSPI_BOOT_OFFSET           0x00000000 // Storage for Bootimage
#define QSPI_BOOTARGS_SIZE         0x00080000 // size 512kB

#ifdef CONFIG_ENV_SIZE
#undef CONFIG_ENV_SIZE
#endif
#define CONFIG_ENV_SIZE QSPI_BOOTARGS_SIZE

/* U-Boot environment is placed at the end of the nor flash */
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH

#ifdef CONFIG_ENV_OFFSET
#undef CONFIG_ENV_OFFSET
#endif
#define CONFIG_ENV_OFFSET QSPI_SIZE - QSPI_BOOTARGS_SIZE


/* U-Boot environment is placed at the ubi */
#elif CONFIG_ENV_IS_IN_UBI

#define CONFIG_ENV_UBI_PART		"ubi-env"
#define CONFIG_ENV_UBI_VOLUME		"uboot-env"
#define CONFIG_UBI_SILENCE_MSG          1

#ifdef CONFIG_ENV_SIZE
#undef CONFIG_ENV_SIZE
#endif
#define CONFIG_ENV_SIZE                 (96 << 10)      /*  96 KiB */

/* U-Boot environment is placed at the mmc device */
#elif CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		1  // mmc1 = emmc, mmc0 = sd
#define CONFIG_SYS_MMC_ENV_PART 	2
#endif

#define ENCLUSTRA_MAC               0xF7B020

/* Default MAC address */
#define ENCLUSTRA_ETHADDR_DEFAULT "00:0A:35:01:02:03"
#define ENCLUSTRA_ETH1ADDR_DEFAULT "00:0A:35:01:02:04"

/* Only one USB controller supported  */
#ifdef CONFIG_USB_MAX_CONTROLLER_COUNT
#undef CONFIG_USB_MAX_CONTROLLER_COUNT
#endif
#define CONFIG_USB_MAX_CONTROLLER_COUNT 1

/* Kernel image name */
#ifdef CONFIG_ARCH_ZYNQMP
#define KERNEL_IMAGE_FILE "Image"
#define DTB_LOADADDR "0x7100000"
#define QSPI_STRING "/amba/spi@ff0f0000/flash@0/partition"
#else
#define KERNEL_IMAGE_FILE "uImage"
#define DTB_LOADADDR "0x6600000"
#define QSPI_STRING "/amba/spi@e000d000/flash@0/partition"
#endif

#ifdef CONFIG_ARCH_ZYNQMP
#define QSPIBOOT_CMD \
    "qspiboot=echo Bootinq on QSPI Flash ...; " \
        "sf probe && "                          \
        "sf read ${bootscript_loadaddr} ${qspi_bootscript_offset} ${bootscript_size} && "\
        "source ${bootscript_loadaddr}\0"
#define NANDBOOT_CMD
#else
#define QSPIBOOT_CMD \
    "qspiboot=echo Bootinq on QSPI Flash ...; " \
        "zx_set_storage QSPI && "               \
        "sf probe && "                          \
        "sf read ${bootscript_loadaddr} ${qspi_bootscript_offset} ${bootscript_size} && "\
        "source ${bootscript_loadaddr}\0"
#define NANDBOOT_CMD \
    "nandboot=echo Booting on NAND ...; "       \
        "zx_set_storage NAND && "               \
        "nand read ${bootscript_loadaddr} nand-bootscript ${bootscript_size} && " \
        "source ${bootscript_loadaddr} \0"
#endif

#ifdef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#define CONFIG_EXTRA_ENV_SETTINGS               \
    "loadaddr=0x200000\0"                       \
    "ipaddr=192.168.1.113\0"                    \
    "serverip=192.168.1.103\0"                  \
    "serverpath=/srv/nfs/rootfs\0"              \
    "netmask=255.255.255.0\0"                   \
                                                \
    "kernel_image=" KERNEL_IMAGE_FILE "\0"      \
    "ramdisk_image=uramdisk\0"                  \
    "devicetree_image=devicetree.dtb\0"         \
    "bootscript_image=uboot.scr\0"              \
    "bootimage_image=boot.bin\0"		\
    "bootimage_image_full=boot_full.bin\0"      \
    "jffs2_image=rootfs.jffs2\0"                \
    "ubifs_image=rootfs.ubi\0"                  \
                                                \
    "kernel_loadaddr=0x6000000\0"               \
    "devicetree_loadaddr=" DTB_LOADADDR "\0"    \
    "ramdisk_loadaddr=0x2000000\0"              \
    "jffs2_loadaddr=0x2000000\0"                \
    "ubifs_loadaddr=0x2000000\0"                \
    "bootscript_loadaddr=0x1000000\0"           \
    "bootimage_loadaddr=0x4000000\0"		\
    "bootfull_loadaddr=0x2B000000\0"		\
    "nand_kernel_size=0x500000\0"               \
    "nand_devicetree_size=0x10000\0"            \
                                                \
    "def_args=console=ttyPS0,115200 rw earlyprintk\0"\
    "ramdisk_args=setenv bootargs ${def_args} root=/dev/ram\0"\
    "mmc_args=setenv bootargs ${def_args} rootwait root=/dev/mmcblk${mmcdev}p2\0"\
    "nand_args=setenv bootargs ${def_args} rootwait=1 ubi.mtd=4 rootfstype=ubifs root=ubi0:rootfs\0"\
    "qspi_args=setenv bootargs ${def_args} root=/dev/mtdblock5 rootfstype=jffs2 rootwait\0"\
    "net_args=setenv bootargs ${def_args} rootwait root=/dev/nfs nfsroot=${serverip}:${serverpath},v3 ip=dhcp\0"\
                                                \
    "fdt_high=0x20000000\0"                     \
    "initrd_high=0x20000000\0"                  \
    "mtdids=" MTDIDS_DEFAULT "\0"               \
    "mtdparts=" MTDPARTS_DEFAULT "\0"           \
    "env_size=" ENVIRONMENT_SIZE "\0"           \
                                                \
    QSPIBOOT_CMD                                \
                                                \
    "mmcdev=0\0"                                \
                                                \
    "sdboot="                                   \
        "setenv mmcdev 0;"                      \
        "run mmcboot\0"                         \
                                                \
    "emmcboot="                                 \
        "setenv mmcdev 1;"                      \
        "run mmcboot\0"                         \
                                                \
    "mmcboot="                                  \
        "mmc rescan ; "                         \
        "load mmc ${mmcdev} ${bootscript_loadaddr} ${bootscript_image} && "\
        "source ${bootscript_loadaddr}\0"       \
                                                \
    "jtagboot=echo Booting on TFTP ...; "       \
        "tftpboot ${bootscript_loadaddr} ${bootscript_image} && "\
        "source ${bootscript_loadaddr}\0"       \
                                                \
    NANDBOOT_CMD                               \
                                               \
    "usbboot=echo Booting on USB ...; "       	\
	"usb start && "				\
        "load usb 0 ${bootscript_loadaddr} ${bootscript_image} && " \
        "source ${bootscript_loadaddr} \0"      \
                                                \
    "netboot=echo Booting from TFTP/NFS ...; "  \
        "tftpboot ${bootscript_loadaddr} ${bootscript_image} && "\
        "source ${bootscript_loadaddr}\0"       \
                                                \
    "setuptest=mmc rescan && "                  \
        "load mmc 0 0x1000000 setup_script.img && " \
        "source 0x1000000\0"                    \
    "setupqspi=echo Setting up QSPI partitions...; " \
        "fdt addr ${devicetree_loadaddr}; " \
        "fdt set " QSPI_STRING "@qspi-bootimage reg <0x00000000 ${bootimage_size}>; " \
        "fdt set " QSPI_STRING "@qspi-kernel reg <${qspi_kernel_offset} ${kernel_size}>; " \
        "fdt set " QSPI_STRING "@qspi-device-tree reg <${qspi_devicetree_offset} ${devicetree_size}>; " \
        "fdt set " QSPI_STRING "@qspi-bootargs reg <${qspi_env_offset} ${qspi_env_size}>; " \
        "fdt set " QSPI_STRING "@qspi-bootscript reg <${qspi_bootscript_offset} ${bootscript_size}>; " \
        "fdt set " QSPI_STRING "@qspi-rootfs reg <${qspi_rootfs_offset} ${jffs2_size}>;\0"
#endif
