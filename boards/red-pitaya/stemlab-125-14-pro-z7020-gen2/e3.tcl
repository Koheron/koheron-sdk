# Add optional E3 and S1 status ports. The instrument supplies the differential
# buffers and logic; the BSP supplies the port names and package constraints.
proc add_e3_ports {} {
  foreach {name direction} {exp_e3p_o O exp_e3n_o O exp_e3p_i I exp_e3n_i I} {
    create_bd_port -dir $direction -from 3 -to 0 $name
  }
  create_bd_port -dir I s1_orient_i
  create_bd_port -dir I s1_link_i
}
