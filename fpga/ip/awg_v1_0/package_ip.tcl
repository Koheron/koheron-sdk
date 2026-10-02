# Called by the SDK packager before ipx::package_project. Only XCI configuration
# files are imported; Vivado regenerates vendor output products for the parent.
source $core_path/integration.tcl
source $core_path/package_settings.tcl
set dds_pm_config [dds_pm::defaults $dds_pm_package_settings]
set_property top awg [current_fileset]
file mkdir $output_path/awg_vendor
foreach {kind name} {carrier dds_pm_carrier modulation dds_pm_modulation} {
    create_ip -name dds_compiler -vendor xilinx.com -library ip -version 6.0 \
        -module_name $name -dir $output_path/awg_vendor
    set_property -dict [dds_pm::${kind}_properties $dds_pm_config 250000000] [get_ips $name]
}
# The package's default RTL widths must match its embedded XCI configurations.
set generic {}
dict for {key value} $dds_pm_package_settings {lappend generic $key=$value}
set_property generic $generic [current_fileset]
