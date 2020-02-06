PROC := ps7_cortexa9_0
# Arch flags obtain by running on the Zynq:
# gcc -march=native -Q --help=target
GCC_FLAGS := -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3-d16 -mvectorize-with-neon-quad -mfloat-abi=hard
GCC_ARCH := arm-linux-gnueabihf
LINUX_IMAGE := uImage
ARCH := arm