set project_name [lindex $argv 0]
set proc_name [lindex $argv 1]
set hard_path [lindex $argv 2]
set fsbl_path [lindex $argv 3]
set hwdef_filename [lindex $argv 4]
set zynq_type [lindex $argv 5]
set console [lindex $argv 6]

hsi::open_hw_design $hard_path/$project_name.xsa
hsi::create_sw_design -proc $proc_name -os standalone fsbl

hsi::add_library xilffs
hsi::add_library xilrsa

hsi::generate_app -proc $proc_name -app ${zynq_type}_fsbl -dir $fsbl_path

# generate_app creates a fresh BSP from the vendor application template. Set
# board-specific console routing on that BSP, after the template is applied.
if {$console ne ""} {
    set mss_files [glob -directory $fsbl_path */system.mss]
    if {[llength $mss_files] != 1} { error "Expected one generated FSBL BSP" }
    set mss [lindex $mss_files 0]
    hsi::open_sw_design $mss
    common::set_property CONFIG.STDIN $console [hsi::get_os]
    common::set_property CONFIG.STDOUT $console [hsi::get_os]
    hsi::generate_bsp -dir [file dirname $mss]
}

hsi::close_hw_design [hsi::current_hw_design]
