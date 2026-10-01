set project_name [lindex $argv 0]
set proc_name [lindex $argv 1]
set hard_path [lindex $argv 2]
set fsbl_path [lindex $argv 3]
set hwdef_filename [lindex $argv 4]
set zynq_type [lindex $argv 5]

hsi::open_hw_design $hard_path/$project_name.xsa
hsi::create_sw_design -proc $proc_name -os standalone fsbl

hsi::add_library xilffs
hsi::add_library xilrsa

hsi::generate_app -proc $proc_name -app ${zynq_type}_fsbl -dir $fsbl_path

hsi::close_hw_design [hsi::current_hw_design]
