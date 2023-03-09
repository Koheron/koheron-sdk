source $board_path/starting_point.tcl

# Add BRAMs for ADC and DAC
# "adc" and "dac" BRAM ranges and offsets are defined in the "memory" part of config.yml
source $sdk_path/fpga/lib/bram.tcl

# -----------------------------------------------------------------------------
# DAC
# -----------------------------------------------------------------------------

add_bram dac

# Add a counter for BRAM addressing
cell koheron:user:address_counter:1.0 address_counter_dac {
  COUNT_WIDTH [get_memory_addr_width dac]
} {
  clken [get_constant_pin 1 1]
  clk adc_dac/adc_clk
  trig [get_slice_pin [ctl_pin trig] 0 0]
}

connect_cell blk_mem_gen_dac {
  addrb address_counter_dac/address
  clkb adc_dac/adc_clk
  enb [get_constant_pin 1 1]
  rstb [get_constant_pin 0 1]
  web [get_constant_pin 0 4]
}

connect_pins adc_dac/dac0 [get_slice_pin blk_mem_gen_dac/doutb 15 0]
connect_pins adc_dac/dac1 [get_slice_pin blk_mem_gen_dac/doutb 31 16]

# -----------------------------------------------------------------------------
# ADC
# -----------------------------------------------------------------------------

connect_pins [get_slice_pin [ctl_pin rf_adc_ctl0] 3 3] adc_dac/adc_clkout_dec
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 0 0] adc_dac/adp5071_sync_en
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 1 1] adc_dac/adp5071_sync_state

for {set i 0} {$i < 2} {incr i} {
  add_bram adc${i} 1

  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 0 0] adc${i}_ctl_range_sel
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 1 1] adc${i}_ctl_testpat
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 2 2] adc${i}_ctl_en

  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 8 4] adc_dac/adc${i}_dco_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 14 9] adc_dac/adc${i}_da_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 20 15] adc_dac/adc${i}_db_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 21 21] adc_dac/adc${i}_delay_reset

  # Add a counter for BRAM addressing
  cell koheron:user:address_counter:1.0 address_counter_adc${i} {
    COUNT_WIDTH [get_memory_addr_width adc${i}]
  } {
    clken adc_dac/adc_valid
    clk adc_dac/adc_clk
    trig [get_slice_pin [ctl_pin trig] 0 0]
  }

  connect_cell blk_mem_gen_adc${i} {
    addrb address_counter_adc${i}/address
    clkb adc_dac/adc_clk
    dinb adc_dac/adc${i}
    enb [get_constant_pin 1 1]
    rstb [get_constant_pin 0 1]
    web address_counter_adc${i}/wen
  }
}

# -----------------------------------------------------------------------------
# Test IOs
# -----------------------------------------------------------------------------

# B34 => outputs
for {set i 0} {$i < 5} {incr i} {
    connect_pins [get_slice_pin [ctl_pin digital_outputs_b34] [expr 2 * $i] [expr 2 * $i]] exp_io_b34_${i}_n
    connect_pins [get_slice_pin [ctl_pin digital_outputs_b34] [expr 2 * $i + 1] [expr 2 * $i + 1]] exp_io_b34_${i}_p
}

# B35 P lines => inputs
connect_pins [sts_pin digital_inputs_b35_p] [get_concat_pin [list exp_io_b35_0_p exp_io_b35_1_p exp_io_b35_2_p exp_io_b35_3_p exp_io_b35_4_p exp_io_b35_5_p exp_io_b35_6_p exp_io_b35_7_p]]

# B35 N lines => outputs
for {set i 0} {$i < 8} {incr i} {
    connect_pins [get_slice_pin [ctl_pin digital_outputs_b35_n] $i $i] exp_io_b35_${i}_n
}
