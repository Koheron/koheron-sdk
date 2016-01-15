script=$1
sha=`git rev-parse --short HEAD`

image=${2}-${sha}.img

size=512

if [ $# -eq 3 ]
then
  size=$3
fi

dd if=/dev/zero of=$image bs=1M count=$size

device=`losetup -f`

losetup $device $image

sh $script $device

losetup -d $device
