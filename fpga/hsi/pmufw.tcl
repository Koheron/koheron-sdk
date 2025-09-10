set project_name [lindex $argv 0]
set hard_path [lindex $argv 1]
set pmufw_path [lindex $argv 2]
set hwdef_filename [lindex $argv 3]

# https://wiki.york.ac.uk/display/RTS/ZCU102+Linux

file mkdir $hard_path
file copy -force $hwdef_filename $hard_path/$project_name.xsa

set hwdsgn [hsi::open_hw_design $hard_path/$project_name.xsa]
hsi::generate_app -hw $hwdsgn -os standalone -proc psu_pmu_0 -app zynqmp_pmufw -compile -sw pmufw -dir $pmufw_path


hsi::close_hw_design [hsi::current_hw_design]
