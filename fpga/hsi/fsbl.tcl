set project_name [lindex $argv 0]
set proc_name [lindex $argv 1]
set hard_path [lindex $argv 2]
set fsbl_path [lindex $argv 3]
set hwdef_filename [lindex $argv 4]
set zynq [lindex $argv 5]

file mkdir $hard_path
file copy -force $hwdef_filename $hard_path/$project_name.hdf

open_hw_design $hard_path/$project_name.hdf
create_sw_design -proc $proc_name -os standalone fsbl

add_library xilffs
add_library xilrsa

generate_app -proc $proc_name -app ${zynq}_fsbl -dir $fsbl_path

close_hw_design [current_hw_design]
