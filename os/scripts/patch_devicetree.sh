#set -e

tmp_os_path=$1
board_path=$2

(cd ${tmp_os_path} && diff -rupN devicetree.orig devicetree > devicetree.patch)
cp ${tmp_os_path}/devicetree.patch ${board_path}/patches/devicetree.patch
echo "Device tree patched"
