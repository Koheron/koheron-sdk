#set -e

tmp_os_path=$1
board_path=$2

(cd ${tmp_os_path} && diff -rupN overlay.orig overlay > overlay.patch)
cp ${tmp_os_path}/overlay.patch ${board_path}/patches/overlay.patch
echo "Device tree overlay patched"
