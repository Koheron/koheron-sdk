# SHA

for {set i 0} {$i < 8} {incr i} {
  set sha sha${i}
  connect_constant sha_constant_$i [expr $$sha] 32 $status_name/In$i
}

# DNA

cell pavel-demin:user:dna_reader:1.0 dna_0 {} {
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
}

cell xilinx.com:ip:xlslice:1.0 dna_slice_0 {
  DIN_WIDTH 57
  DIN_FROM  31
  DIN_TO    0
} {
  Din dna_0/dna_data
  Dout $status_name/In8
}

cell xilinx.com:ip:xlslice:1.0 dna_slice_1 {
  DIN_WIDTH 57
  DIN_FROM  56
  DIN_TO    32
} {
  Din dna_0/dna_data
  Dout $status_name/In9
}
