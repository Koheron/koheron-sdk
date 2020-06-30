source /tmp/var_fsbl.tcl

file mkdir $hard_path
file copy -force $hwdef_filename $hard_path/$project_name.xsa

hsi::open_hw_design $hard_path/$project_name.xsa
hsi::create_sw_design -proc $proc_name -os standalone fsbl

hsi::add_library xilffs
hsi::add_library xilrsa

hsi::generate_app -proc $proc_name -app ${zynq_type}_fsbl -dir $fsbl_path

hsi::close_hw_design [hsi::current_hw_design]
