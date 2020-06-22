source /tmp/var_dt.tcl

set boot_args {console=ttyPS0,115200 root=/dev/${partition}p2 ro rootfstype=ext4 earlyprintk rootwait}

file mkdir $hard_path
file copy -force $hwdef_filename $hard_path/$project_name.xsa

hsi::set_repo_path $dtree_path

hsi::open_hw_design $hard_path/$project_name.xsa
hsi::create_sw_design -proc $proc_name -os device_tree devicetree

hsi::set_property CONFIG.kernel_version $vivado_version [hsi::get_os]
hsi::set_property CONFIG.bootargs $boot_args [hsi::get_os]

hsi::set_property CONFIG.dt_overlay ${en_overlay} [hsi::get_os]

hsi::generate_bsp -dir $tree_path

hsi::close_sw_design [hsi::current_sw_design]
hsi::close_hw_design [hsi::current_hw_design]
