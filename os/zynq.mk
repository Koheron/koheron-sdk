PROC := ps7_cortexa9_0
GCC_FLAGS := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard
GCC_ARCH := arm-linux-gnueabihf
LINUX_IMAGE := uImage
ARCH := arm

BOOTCALL := boot.bin