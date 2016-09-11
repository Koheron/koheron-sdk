
set bram_size [expr 2**($config::bram_addr_width-8)]K

source instruments/base/block_design.tcl

source instruments/spectrum/spectrum.tcl

source instruments/base/timer.tcl

##########################################################
# Add EEPROM
##########################################################

source $lib/at93c46d.tcl
