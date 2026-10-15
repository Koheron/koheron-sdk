set project_name [lindex $argv 0]
set hard_path [lindex $argv 1]
set pmufw_path [lindex $argv 2]
set hwdef_filename [lindex $argv 3]
set console [lindex $argv 4]

# https://wiki.york.ac.uk/display/RTS/ZCU102+Linux

set hwdsgn [hsi::open_hw_design $hwdef_filename]
hsi::generate_app -hw $hwdsgn -os standalone -proc psu_pmu_0 -app zynqmp_pmufw -sw pmufw -dir $pmufw_path

# As with FSBL, apply console routing after the vendor application template.
if {$console ne ""} {
    set mss_files [glob -directory $pmufw_path */system.mss]
    if {[llength $mss_files] != 1} { error "Expected one generated PMU BSP" }
    set mss [lindex $mss_files 0]
    hsi::open_sw_design $mss
    common::set_property CONFIG.STDIN $console [hsi::get_os]
    common::set_property CONFIG.STDOUT $console [hsi::get_os]
    hsi::generate_bsp -dir [file dirname $mss]
}

hsi::close_hw_design [hsi::current_hw_design]
