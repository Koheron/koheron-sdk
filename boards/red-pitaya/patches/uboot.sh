
patches=$1
dir=$2

cp $patches/zynq_red_pitaya_defconfig $dir/configs
cp $patches/zynq-red-pitaya.dts $dir/arch/arm/dts
cp $patches/zynq_red_pitaya.h $dir/include/configs
cp $patches/u-boot-lantiq.c $dir/drivers/net/phy/lantiq.c
