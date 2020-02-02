
set project_name [lindex $argv 0]
set proc_name [lindex $argv 1]
set dtree_path [lindex $argv 2]
set vivado_version [lindex $argv 3]
set hard_path [lindex $argv 4]
set tree_path [lindex $argv 5]
set hwdef_filename [lindex $argv 6]
set en_overlay [lindex $argv 7]

set boot_args {console=ttyPS0,115200 root=/dev/mmcblk0p2 ro rootfstype=ext4 earlyprintk rootwait}

file mkdir $hard_path
file copy -force $hwdef_filename $hard_path/$project_name.hdf

set_repo_path $dtree_path

open_hw_design $hard_path/$project_name.hdf
create_sw_design -proc $proc_name -os device_tree devicetree

set_property CONFIG.kernel_version $vivado_version [get_os]
set_property CONFIG.bootargs $boot_args [get_os]

set_property CONFIG.dt_overlay ${en_overlay} [get_os]

generate_bsp -dir $tree_path

close_sw_design [current_sw_design]
close_hw_design [current_hw_design]
