set display_name {AXI Status Register}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {pavel-demin} $core
set_property VENDOR_DISPLAY_NAME {Pavel Demin} $core
set_property COMPANY_URL {https://github.com/pavel-demin/red-pitaya-notes} $core

core_parameter AXI_ADDR_WIDTH {AXI ADDR WIDTH} {Width of the AXI address bus.}
core_parameter AXI_DATA_WIDTH {AXI DATA WIDTH} {Width of the AXI data bus.}
core_parameter STS_DATA_WIDTH {STS DATA WIDTH} {Width of the status data.}

ipx::add_bus_interface S_AXI $core
set bus [ipx::get_bus_interfaces S_AXI -of_objects $core]
set_property bus_type_vlnv xilinx.com:interface:aximm:1.0 $bus

foreach {port} {
  RREADY
  ARREADY
  ARVALID
  RRESP
  RVALID
  RDATA
  ARADDR
} {
  ipx::add_port_map $port $bus
  set_property physical_name s_axi_[string tolower $port] [ipx::get_port_maps $port -of_objects $bus]
}

ipx::infer_bus_interfaces xilinx.com:interface:aximm_rtl:1.0 [ipx::current_core]
