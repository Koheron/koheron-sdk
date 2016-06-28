source projects/oscillo/config.tcl

set bram_size [expr 2**($config::bram_addr_width-8)]K

source projects/base/block_design.tcl

source projects/oscillo/oscillo.tcl

set clken [get_and_pin avg0/new_cycle [get_slice_pin [cfg_pin clken_mask] 0 0]]
connect_pins $addr_intercon_name/clken $clken
connect_pins $interconnect_name/clken $clken

##########################################################
# Add EEPROM
##########################################################

source $lib/at93c46d.tcl
