name=$1 

version=$(cat tmp/${name}.version)

image=${name}-${version}.img

size=1024

if [ $# -eq 2 ]
then
  size=$3
fi

dd if=/dev/zero of=$image bs=1M count=$size

device=`losetup -f`

losetup $device $image

sh os/scripts/ubuntu.sh $device $name $version

losetup -d $device
