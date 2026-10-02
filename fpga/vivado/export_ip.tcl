# Export a portable Vivado IP repository and archive, including embedded XCI.
# vivado -mode batch -source fpga/vivado/export_ip.tcl \
#   -tclargs fpga/ip/axis_accumulator_v1_0 xc7z020clg400-2 tmp/ip-export
if {[llength $argv] != 3} {
    error {Expected: <core directory> <packaging part> <export directory>}
}
set export_core [file normalize [lindex $argv 0]]
set export_root [file normalize [lindex $argv 2]]
if {![file exists $export_core/core_config.tcl]} {error {Core directory has no core_config.tcl}}
if {$export_root eq $export_core} {error {Export into a separate directory}}
set export_name [file tail $export_core]
set argv [list $export_core [lindex $argv 1] $export_root/repository]
source [file join [file dirname [info script]] core.tcl]
set exported_core [ipx::open_core $export_root/repository/$export_name/component.xml]
ipx::check_integrity $exported_core
set export_archive $export_root/$export_name.zip
ipx::archive_core $export_archive $exported_core
puts "Portable IP archive: $export_archive"
puts "IP repository: $export_root/repository/$export_name"
