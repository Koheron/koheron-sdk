# Build OS image
# Must be run in sudo:
# sudo bash image.sh INSTRUMENT_NAME [--release]
#
# where INSTRUMENT_NAME is the instrument that
# must be started the first time the image is used
#
# --release option to build the image in release
# mode (the root file system is read-only)

name=$1

version=$(cat tmp/${name}.version)
image=${name}-${version}.img

if [ "$2" == "--release" ]; then
  release=true
else
  release=false
fi

size=1024
dd if=/dev/zero of=$image bs=1M count=${size}

device=`losetup -f`

losetup ${device} ${image}

sh os/scripts/ubuntu.sh ${device} ${name} ${version} ${release}

losetup -d ${device}
