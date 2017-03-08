
################################################################
# This is a generated script based on design: system
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2016.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_msg_id "BD_TCL-109" "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source system_script.tcl

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xc7z010clg400-1
}


# CHANGE DESIGN NAME HERE
set design_name system

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_msg_id "BD_TCL-001" "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_msg_id "BD_TCL-002" "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_msg_id "BD_TCL-004" "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_msg_id "BD_TCL-005" "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_msg_id "BD_TCL-114" "ERROR" $errMsg}
   return $nRet
}

##################################################################
# DESIGN PROCs
##################################################################


# Hierarchical cell: sts
proc create_hier_cell_sts { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_sts() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI

  # Create pins
  create_bd_pin -dir I -from 31 -to 0 device_version
  create_bd_pin -dir I -type clk m_axi_aclk
  create_bd_pin -dir I -from 0 -to 0 -type rst m_axi_aresetn
  create_bd_pin -dir I -type clk s_axi_aclk
  create_bd_pin -dir I -from 0 -to 0 -type rst s_axi_aresetn
  create_bd_pin -dir I -from 31 -to 0 state
  create_bd_pin -dir I -from 30 -to 0 status

  # Create instance: axi_clock_converter_0, and set properties
  set axi_clock_converter_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 ]

  # Create instance: axi_sts_register_0, and set properties
  set axi_sts_register_0 [ create_bd_cell -type ip -vlnv pavel-demin:user:axi_sts_register:1.0 axi_sts_register_0 ]
  set_property -dict [ list \
CONFIG.STS_DATA_WIDTH {416} \
 ] $axi_sts_register_0

  # Create instance: concat_0, and set properties
  set concat_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 concat_0 ]
  set_property -dict [ list \
CONFIG.IN0_WIDTH {32} \
CONFIG.IN10_WIDTH {32} \
CONFIG.IN11_WIDTH {32} \
CONFIG.IN12_WIDTH {32} \
CONFIG.IN1_WIDTH {32} \
CONFIG.IN2_WIDTH {32} \
CONFIG.IN3_WIDTH {32} \
CONFIG.IN4_WIDTH {32} \
CONFIG.IN5_WIDTH {32} \
CONFIG.IN6_WIDTH {32} \
CONFIG.IN7_WIDTH {32} \
CONFIG.IN8_WIDTH {32} \
CONFIG.IN9_WIDTH {32} \
CONFIG.NUM_PORTS {13} \
 ] $concat_0

  # Create instance: dna, and set properties
  set dna [ create_bd_cell -type ip -vlnv pavel-demin:user:dna_reader:1.0 dna ]

  # Create instance: sha_constant_0, and set properties
  set sha_constant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {3917260144} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_0

  # Create instance: sha_constant_1, and set properties
  set sha_constant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {2642006113} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_1

  # Create instance: sha_constant_2, and set properties
  set sha_constant_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_2 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {433507891} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_2

  # Create instance: sha_constant_3, and set properties
  set sha_constant_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_3 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {720869963} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_3

  # Create instance: sha_constant_4, and set properties
  set sha_constant_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_4 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {4108308657} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_4

  # Create instance: sha_constant_5, and set properties
  set sha_constant_5 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_5 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {739594800} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_5

  # Create instance: sha_constant_6, and set properties
  set sha_constant_6 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_6 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {1931457521} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_6

  # Create instance: sha_constant_7, and set properties
  set sha_constant_7 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 sha_constant_7 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {1067749099} \
CONFIG.CONST_WIDTH {32} \
 ] $sha_constant_7

  # Create instance: slice_from31_to0_dna_data, and set properties
  set slice_from31_to0_dna_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from31_to0_dna_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {31} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {57} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from31_to0_dna_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from31_to0_dna_data

  # Create instance: slice_from56_to32_dna_data, and set properties
  set slice_from56_to32_dna_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from56_to32_dna_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {56} \
CONFIG.DIN_TO {32} \
CONFIG.DIN_WIDTH {57} \
CONFIG.DOUT_WIDTH {25} \
 ] $slice_from56_to32_dna_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from56_to32_dna_data

  # Create interface connections
  connect_bd_intf_net -intf_net Conn1 [get_bd_intf_pins S_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
  connect_bd_intf_net -intf_net axi_clock_converter_0_M_AXI [get_bd_intf_pins axi_clock_converter_0/M_AXI] [get_bd_intf_pins axi_sts_register_0/S_AXI]

  # Create port connections
  connect_bd_net -net concat_0_dout [get_bd_pins axi_sts_register_0/sts_data] [get_bd_pins concat_0/dout]
  connect_bd_net -net device_version_1 [get_bd_pins device_version] [get_bd_pins concat_0/In10]
  connect_bd_net -net dna_dna_data [get_bd_pins dna/dna_data] [get_bd_pins slice_from31_to0_dna_data/Din] [get_bd_pins slice_from56_to32_dna_data/Din]
  connect_bd_net -net m_axi_aclk_1 [get_bd_pins m_axi_aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins axi_sts_register_0/aclk] [get_bd_pins dna/aclk]
  connect_bd_net -net m_axi_aresetn_1 [get_bd_pins m_axi_aresetn] [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins axi_sts_register_0/aresetn] [get_bd_pins dna/aresetn]
  connect_bd_net -net s_axi_aclk_1 [get_bd_pins s_axi_aclk] [get_bd_pins axi_clock_converter_0/s_axi_aclk]
  connect_bd_net -net s_axi_aresetn_1 [get_bd_pins s_axi_aresetn] [get_bd_pins axi_clock_converter_0/s_axi_aresetn]
  connect_bd_net -net sha_constant_0_dout [get_bd_pins concat_0/In0] [get_bd_pins sha_constant_0/dout]
  connect_bd_net -net sha_constant_1_dout [get_bd_pins concat_0/In1] [get_bd_pins sha_constant_1/dout]
  connect_bd_net -net sha_constant_2_dout [get_bd_pins concat_0/In2] [get_bd_pins sha_constant_2/dout]
  connect_bd_net -net sha_constant_3_dout [get_bd_pins concat_0/In3] [get_bd_pins sha_constant_3/dout]
  connect_bd_net -net sha_constant_4_dout [get_bd_pins concat_0/In4] [get_bd_pins sha_constant_4/dout]
  connect_bd_net -net sha_constant_5_dout [get_bd_pins concat_0/In5] [get_bd_pins sha_constant_5/dout]
  connect_bd_net -net sha_constant_6_dout [get_bd_pins concat_0/In6] [get_bd_pins sha_constant_6/dout]
  connect_bd_net -net sha_constant_7_dout [get_bd_pins concat_0/In7] [get_bd_pins sha_constant_7/dout]
  connect_bd_net -net slice_from31_to0_dna_data_Dout [get_bd_pins concat_0/In8] [get_bd_pins slice_from31_to0_dna_data/Dout]
  connect_bd_net -net slice_from56_to32_dna_data_Dout [get_bd_pins concat_0/In9] [get_bd_pins slice_from56_to32_dna_data/Dout]
  connect_bd_net -net state_1 [get_bd_pins state] [get_bd_pins concat_0/In12]
  connect_bd_net -net status_1 [get_bd_pins status] [get_bd_pins concat_0/In11]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: cfg
proc create_hier_cell_cfg { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_cfg() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI

  # Create pins
  create_bd_pin -dir O -from 31 -to 0 acquisitionlength
  create_bd_pin -dir O -from 31 -to 0 arm_softtrig
  create_bd_pin -dir O -from 31 -to 0 led
  create_bd_pin -dir I -type clk m_axi_aclk
  create_bd_pin -dir I -from 0 -to 0 -type rst m_axi_aresetn
  create_bd_pin -dir O -from 31 -to 0 operationmode
  create_bd_pin -dir I -type clk s_axi_aclk
  create_bd_pin -dir I -from 0 -to 0 -type rst s_axi_aresetn
  create_bd_pin -dir O -from 31 -to 0 simulationpulseamp
  create_bd_pin -dir O -from 31 -to 0 simulationpulsefreq

  # Create instance: axi_cfg_register_0, and set properties
  set axi_cfg_register_0 [ create_bd_cell -type ip -vlnv pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0 ]
  set_property -dict [ list \
CONFIG.CFG_DATA_WIDTH {192} \
 ] $axi_cfg_register_0

  # Create instance: axi_clock_converter_0, and set properties
  set axi_clock_converter_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 ]

  # Create instance: slice_from127_to96_cfg_data, and set properties
  set slice_from127_to96_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from127_to96_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {127} \
CONFIG.DIN_TO {96} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from127_to96_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from127_to96_cfg_data

  # Create instance: slice_from159_to128_cfg_data, and set properties
  set slice_from159_to128_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from159_to128_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {159} \
CONFIG.DIN_TO {128} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from159_to128_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from159_to128_cfg_data

  # Create instance: slice_from191_to160_cfg_data, and set properties
  set slice_from191_to160_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from191_to160_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {191} \
CONFIG.DIN_TO {160} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from191_to160_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from191_to160_cfg_data

  # Create instance: slice_from31_to0_cfg_data, and set properties
  set slice_from31_to0_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from31_to0_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {31} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from31_to0_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from31_to0_cfg_data

  # Create instance: slice_from63_to32_cfg_data, and set properties
  set slice_from63_to32_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from63_to32_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {63} \
CONFIG.DIN_TO {32} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from63_to32_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from63_to32_cfg_data

  # Create instance: slice_from95_to64_cfg_data, and set properties
  set slice_from95_to64_cfg_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from95_to64_cfg_data ]
  set_property -dict [ list \
CONFIG.DIN_FROM {95} \
CONFIG.DIN_TO {64} \
CONFIG.DIN_WIDTH {192} \
CONFIG.DOUT_WIDTH {32} \
 ] $slice_from95_to64_cfg_data

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from95_to64_cfg_data

  # Create interface connections
  connect_bd_intf_net -intf_net Conn1 [get_bd_intf_pins S_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
  connect_bd_intf_net -intf_net axi_clock_converter_0_M_AXI [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]

  # Create port connections
  connect_bd_net -net axi_cfg_register_0_cfg_data [get_bd_pins axi_cfg_register_0/cfg_data] [get_bd_pins slice_from127_to96_cfg_data/Din] [get_bd_pins slice_from159_to128_cfg_data/Din] [get_bd_pins slice_from191_to160_cfg_data/Din] [get_bd_pins slice_from31_to0_cfg_data/Din] [get_bd_pins slice_from63_to32_cfg_data/Din] [get_bd_pins slice_from95_to64_cfg_data/Din]
  connect_bd_net -net m_axi_aclk_1 [get_bd_pins m_axi_aclk] [get_bd_pins axi_cfg_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
  connect_bd_net -net m_axi_aresetn_1 [get_bd_pins m_axi_aresetn] [get_bd_pins axi_cfg_register_0/aresetn] [get_bd_pins axi_clock_converter_0/m_axi_aresetn]
  connect_bd_net -net s_axi_aclk_1 [get_bd_pins s_axi_aclk] [get_bd_pins axi_clock_converter_0/s_axi_aclk]
  connect_bd_net -net s_axi_aresetn_1 [get_bd_pins s_axi_aresetn] [get_bd_pins axi_clock_converter_0/s_axi_aresetn]
  connect_bd_net -net slice_from127_to96_cfg_data_Dout [get_bd_pins simulationpulseamp] [get_bd_pins slice_from127_to96_cfg_data/Dout]
  connect_bd_net -net slice_from159_to128_cfg_data_Dout [get_bd_pins simulationpulsefreq] [get_bd_pins slice_from159_to128_cfg_data/Dout]
  connect_bd_net -net slice_from191_to160_cfg_data_Dout [get_bd_pins operationmode] [get_bd_pins slice_from191_to160_cfg_data/Dout]
  connect_bd_net -net slice_from31_to0_cfg_data_Dout [get_bd_pins led] [get_bd_pins slice_from31_to0_cfg_data/Dout]
  connect_bd_net -net slice_from63_to32_cfg_data_Dout [get_bd_pins arm_softtrig] [get_bd_pins slice_from63_to32_cfg_data/Dout]
  connect_bd_net -net slice_from95_to64_cfg_data_Dout [get_bd_pins acquisitionlength] [get_bd_pins slice_from95_to64_cfg_data/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: adc_dac
proc create_hier_cell_adc_dac { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_adc_dac() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O -from 13 -to 0 adc1
  create_bd_pin -dir O -from 13 -to 0 adc2
  create_bd_pin -dir O adc_cdcs_o
  create_bd_pin -dir O adc_clk
  create_bd_pin -dir O -from 1 -to 0 adc_clk_source
  create_bd_pin -dir I -from 13 -to 0 adc_dat_a_i
  create_bd_pin -dir I -from 13 -to 0 adc_dat_b_i
  create_bd_pin -dir I clk_in1_n
  create_bd_pin -dir I clk_in1_p
  create_bd_pin -dir I -from 13 -to 0 dac1
  create_bd_pin -dir I -from 13 -to 0 dac2
  create_bd_pin -dir O dac_clk_o
  create_bd_pin -dir O -from 13 -to 0 dac_dat_o
  create_bd_pin -dir O dac_rst_o
  create_bd_pin -dir O dac_sel_o
  create_bd_pin -dir O dac_wrt_o
  create_bd_pin -dir O pwm_clk
  create_bd_pin -dir O ser_clk

  # Create instance: adc, and set properties
  set adc [ create_bd_cell -type ip -vlnv pavel-demin:user:redp_adc:1.0 adc ]

  # Create instance: adc_rst, and set properties
  set adc_rst [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 adc_rst ]

  # Create instance: dac, and set properties
  set dac [ create_bd_cell -type ip -vlnv pavel-demin:user:redp_dac:1.0 dac ]

  # Create instance: pll, and set properties
  set pll [ create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:5.3 pll ]
  set_property -dict [ list \
CONFIG.CLKIN1_JITTER_PS {80.0} \
CONFIG.CLKOUT1_JITTER {119.348} \
CONFIG.CLKOUT1_PHASE_ERROR {96.948} \
CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {125.0} \
CONFIG.CLKOUT1_USED {true} \
CONFIG.CLKOUT2_JITTER {119.348} \
CONFIG.CLKOUT2_PHASE_ERROR {96.948} \
CONFIG.CLKOUT2_REQUESTED_OUT_FREQ {125.0} \
CONFIG.CLKOUT2_USED {true} \
CONFIG.CLKOUT3_JITTER {104.759} \
CONFIG.CLKOUT3_PHASE_ERROR {96.948} \
CONFIG.CLKOUT3_REQUESTED_OUT_FREQ {250.0} \
CONFIG.CLKOUT3_USED {true} \
CONFIG.CLKOUT4_JITTER {104.759} \
CONFIG.CLKOUT4_PHASE_ERROR {96.948} \
CONFIG.CLKOUT4_REQUESTED_OUT_FREQ {250.0} \
CONFIG.CLKOUT4_REQUESTED_PHASE {-45} \
CONFIG.CLKOUT4_USED {true} \
CONFIG.CLKOUT5_JITTER {104.759} \
CONFIG.CLKOUT5_PHASE_ERROR {96.948} \
CONFIG.CLKOUT5_REQUESTED_OUT_FREQ {250.0} \
CONFIG.CLKOUT5_USED {true} \
CONFIG.CLKOUT6_JITTER {104.759} \
CONFIG.CLKOUT6_PHASE_ERROR {96.948} \
CONFIG.CLKOUT6_REQUESTED_OUT_FREQ {250.0} \
CONFIG.CLKOUT6_USED {true} \
CONFIG.MMCM_CLKFBOUT_MULT_F {8} \
CONFIG.MMCM_CLKIN1_PERIOD {8.0} \
CONFIG.MMCM_CLKIN2_PERIOD {10.0} \
CONFIG.MMCM_CLKOUT0_DIVIDE_F {8} \
CONFIG.MMCM_CLKOUT1_DIVIDE {8} \
CONFIG.MMCM_CLKOUT2_DIVIDE {4} \
CONFIG.MMCM_CLKOUT3_DIVIDE {4} \
CONFIG.MMCM_CLKOUT3_PHASE {-45.000} \
CONFIG.MMCM_CLKOUT4_DIVIDE {4} \
CONFIG.MMCM_CLKOUT5_DIVIDE {4} \
CONFIG.MMCM_COMPENSATION {ZHOLD} \
CONFIG.NUM_OUT_CLKS {6} \
CONFIG.PRIMITIVE {PLL} \
CONFIG.PRIM_IN_FREQ {125.0} \
CONFIG.PRIM_SOURCE {Differential_clock_capable_pin} \
CONFIG.USE_RESET {false} \
 ] $pll

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.CLKIN1_JITTER_PS.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT1_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT1_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT2_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT2_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT3_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT3_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT4_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT4_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT5_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT5_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT6_JITTER.VALUE_SRC {DEFAULT} \
CONFIG.CLKOUT6_PHASE_ERROR.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKFBOUT_MULT_F.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKIN1_PERIOD.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKIN2_PERIOD.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT0_DIVIDE_F.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT1_DIVIDE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT2_DIVIDE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT3_DIVIDE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT3_PHASE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT4_DIVIDE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_CLKOUT5_DIVIDE.VALUE_SRC {DEFAULT} \
CONFIG.MMCM_COMPENSATION.VALUE_SRC {DEFAULT} \
CONFIG.NUM_OUT_CLKS.VALUE_SRC {DEFAULT} \
 ] $pll

  # Create port connections
  connect_bd_net -net adc_adc_cdcs_o [get_bd_pins adc_cdcs_o] [get_bd_pins adc/adc_cdcs_o]
  connect_bd_net -net adc_adc_clk_source [get_bd_pins adc_clk_source] [get_bd_pins adc/adc_clk_source]
  connect_bd_net -net adc_adc_dat_a_o [get_bd_pins adc1] [get_bd_pins adc/adc_dat_a_o]
  connect_bd_net -net adc_adc_dat_b_o [get_bd_pins adc2] [get_bd_pins adc/adc_dat_b_o]
  connect_bd_net -net adc_dat_a_i_1 [get_bd_pins adc_dat_a_i] [get_bd_pins adc/adc_dat_a_i]
  connect_bd_net -net adc_dat_b_i_1 [get_bd_pins adc_dat_b_i] [get_bd_pins adc/adc_dat_b_i]
  connect_bd_net -net adc_rst_dout [get_bd_pins adc/adc_rst_i] [get_bd_pins adc_rst/dout]
  connect_bd_net -net clk_in1_n_1 [get_bd_pins clk_in1_n] [get_bd_pins pll/clk_in1_n]
  connect_bd_net -net clk_in1_p_1 [get_bd_pins clk_in1_p] [get_bd_pins pll/clk_in1_p]
  connect_bd_net -net dac1_1 [get_bd_pins dac1] [get_bd_pins dac/dac_dat_a_i]
  connect_bd_net -net dac2_1 [get_bd_pins dac2] [get_bd_pins dac/dac_dat_b_i]
  connect_bd_net -net dac_dac_clk_o [get_bd_pins dac_clk_o] [get_bd_pins dac/dac_clk_o]
  connect_bd_net -net dac_dac_dat_o [get_bd_pins dac_dat_o] [get_bd_pins dac/dac_dat_o]
  connect_bd_net -net dac_dac_rst_o [get_bd_pins dac_rst_o] [get_bd_pins dac/dac_rst_o]
  connect_bd_net -net dac_dac_sel_o [get_bd_pins dac_sel_o] [get_bd_pins dac/dac_sel_o]
  connect_bd_net -net dac_dac_wrt_o [get_bd_pins dac_wrt_o] [get_bd_pins dac/dac_wrt_o]
  connect_bd_net -net pll_clk_out1 [get_bd_pins adc_clk] [get_bd_pins adc/adc_clk] [get_bd_pins pll/clk_out1]
  connect_bd_net -net pll_clk_out2 [get_bd_pins dac/dac_clk_1x] [get_bd_pins pll/clk_out2]
  connect_bd_net -net pll_clk_out3 [get_bd_pins dac/dac_clk_2x] [get_bd_pins pll/clk_out3]
  connect_bd_net -net pll_clk_out4 [get_bd_pins dac/dac_clk_2p] [get_bd_pins pll/clk_out4]
  connect_bd_net -net pll_clk_out5 [get_bd_pins ser_clk] [get_bd_pins pll/clk_out5]
  connect_bd_net -net pll_clk_out6 [get_bd_pins pwm_clk] [get_bd_pins pll/clk_out6]
  connect_bd_net -net pll_locked [get_bd_pins dac/dac_locked] [get_bd_pins pll/locked]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O -from 12 -to 0 Count
  create_bd_pin -dir I Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {14} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {13} \
CONFIG.DIN_WIDTH {14} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {12} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {14} \
CONFIG.DOUT_WIDTH {13} \
 ] $xlslice_1

  # Create port connections
  connect_bd_net -net Trig_Count_1 [get_bd_pins Trig_Count] [get_bd_pins latch_0/reset]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins c_counter_binary_0/Q] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net clk_1 [get_bd_pins clk] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins latch_0/clk]
  connect_bd_net -net latch_0_q [get_bd_pins c_counter_binary_0/SCLR] [get_bd_pins latch_0/q]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins latch_0/set] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins Count] [get_bd_pins xlslice_1/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Set_Reset_State
proc create_hier_cell_Set_Reset_State { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Set_Reset_State() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir I -from 0 -to 0 ResetState
  create_bd_pin -dir I -from 0 -to 0 SetState
  create_bd_pin -dir O State
  create_bd_pin -dir I -type clk clk

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: one_clock_pulse_0, and set properties
  set one_clock_pulse_0 [ create_bd_cell -type ip -vlnv CCFE:user:one_clock_pulse:1.0 one_clock_pulse_0 ]

  # Create instance: one_clock_pulse_1, and set properties
  set one_clock_pulse_1 [ create_bd_cell -type ip -vlnv CCFE:user:one_clock_pulse:1.0 one_clock_pulse_1 ]

  # Create port connections
  connect_bd_net -net ArmState_Dout [get_bd_pins ResetState] [get_bd_pins one_clock_pulse_1/trig]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins latch_0/clk] [get_bd_pins one_clock_pulse_0/clk] [get_bd_pins one_clock_pulse_1/clk]
  connect_bd_net -net latch_0_q [get_bd_pins State] [get_bd_pins latch_0/q]
  connect_bd_net -net one_clock_pulse_0_pulse [get_bd_pins latch_0/set] [get_bd_pins one_clock_pulse_0/pulse]
  connect_bd_net -net one_clock_pulse_1_pulse [get_bd_pins latch_0/reset] [get_bd_pins one_clock_pulse_1/pulse]
  connect_bd_net -net util_vector_logic_1_Res [get_bd_pins SetState] [get_bd_pins one_clock_pulse_0/trig]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RandomPulser
proc create_hier_cell_RandomPulser { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RandomPulser() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O Pulse
  create_bd_pin -dir O -from 31 -to 0 RandValue
  create_bd_pin -dir I -from 0 -to 0 Reset_In
  create_bd_pin -dir I -from 31 -to 0 b
  create_bd_pin -dir I clk

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {32} \
CONFIG.OPERATION {LT} \
 ] $comparator_0

  # Create instance: lfsr32_0, and set properties
  set lfsr32_0 [ create_bd_cell -type ip -vlnv CCFE:user:lfsr32:1.0 lfsr32_0 ]

  # Create instance: one_clock_pulse_0, and set properties
  set one_clock_pulse_0 [ create_bd_cell -type ip -vlnv CCFE:user:one_clock_pulse:1.0 one_clock_pulse_0 ]

  # Create port connections
  connect_bd_net -net Reset_In_1 [get_bd_pins Reset_In] [get_bd_pins lfsr32_0/reset]
  connect_bd_net -net cfg_simulationpulsefreq [get_bd_pins b] [get_bd_pins comparator_0/b]
  connect_bd_net -net clk_1 [get_bd_pins clk] [get_bd_pins lfsr32_0/clock] [get_bd_pins one_clock_pulse_0/clk]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins one_clock_pulse_0/trig]
  connect_bd_net -net lfsr16_0_rnd [get_bd_pins RandValue] [get_bd_pins comparator_0/a] [get_bd_pins lfsr32_0/rnd]
  connect_bd_net -net one_clock_pulse_0_pulse [get_bd_pins Pulse] [get_bd_pins one_clock_pulse_0/pulse]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Counter_Monitor
proc create_hier_cell_Counter_Monitor { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Counter_Monitor() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 In0
  create_bd_pin -dir O -from 30 -to 0 dout

  # Create instance: xlconcat_4, and set properties
  set xlconcat_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_4 ]

  # Create instance: xlconstant_2, and set properties
  set xlconstant_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_2 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {16} \
 ] $xlconstant_2

  # Create port connections
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins In0] [get_bd_pins xlconcat_4/In0]
  connect_bd_net -net xlconcat_4_dout [get_bd_pins dout] [get_bd_pins xlconcat_4/dout]
  connect_bd_net -net xlconstant_2_dout1 [get_bd_pins xlconcat_4/In1] [get_bd_pins xlconstant_2/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Averager_1
proc create_hier_cell_Averager_1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Averager_1() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O -from 15 -to 0 AverageVal
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I ce
  create_bd_pin -dir I -type clk clk

  # Create instance: AveADC, and set properties
  set AveADC [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 AveADC ]
  set_property -dict [ list \
CONFIG.DIN_FROM {20} \
CONFIG.DIN_TO {5} \
CONFIG.DIN_WIDTH {21} \
CONFIG.DOUT_WIDTH {16} \
 ] $AveADC

  # Create instance: c_accum_0, and set properties
  set c_accum_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_accum:12.0 c_accum_0 ]
  set_property -dict [ list \
CONFIG.Bypass {false} \
CONFIG.Input_Type {Signed} \
CONFIG.Input_Width {14} \
CONFIG.Output_Width {21} \
CONFIG.SCLR {true} \
 ] $c_accum_0

  # Create instance: register_0, and set properties
  set register_0 [ create_bd_cell -type ip -vlnv CCFE:user:register:1.0 register_0 ]

  # Create port connections
  connect_bd_net -net Acquisition_Control_MHz_clk [get_bd_pins ce] [get_bd_pins c_accum_0/SCLR] [get_bd_pins register_0/ce]
  connect_bd_net -net AveADC_Dout [get_bd_pins AveADC/Dout] [get_bd_pins register_0/din]
  connect_bd_net -net adc_dac_adc1 [get_bd_pins B] [get_bd_pins c_accum_0/B]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins c_accum_0/CLK] [get_bd_pins register_0/clk]
  connect_bd_net -net c_accum_0_Q [get_bd_pins AveADC/Din] [get_bd_pins c_accum_0/Q]
  connect_bd_net -net register_0_dout [get_bd_pins AverageVal] [get_bd_pins register_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Averager_0
proc create_hier_cell_Averager_0 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Averager_0() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O -from 15 -to 0 AverageVal
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I ce
  create_bd_pin -dir I -type clk clk

  # Create instance: AveADC, and set properties
  set AveADC [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 AveADC ]
  set_property -dict [ list \
CONFIG.DIN_FROM {20} \
CONFIG.DIN_TO {5} \
CONFIG.DIN_WIDTH {21} \
CONFIG.DOUT_WIDTH {16} \
 ] $AveADC

  # Create instance: c_accum_0, and set properties
  set c_accum_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_accum:12.0 c_accum_0 ]
  set_property -dict [ list \
CONFIG.Bypass {false} \
CONFIG.Input_Type {Signed} \
CONFIG.Input_Width {14} \
CONFIG.Output_Width {21} \
CONFIG.SCLR {true} \
 ] $c_accum_0

  # Create instance: register_0, and set properties
  set register_0 [ create_bd_cell -type ip -vlnv CCFE:user:register:1.0 register_0 ]

  # Create port connections
  connect_bd_net -net Acquisition_Control_MHz_clk [get_bd_pins ce] [get_bd_pins c_accum_0/SCLR] [get_bd_pins register_0/ce]
  connect_bd_net -net AveADC_Dout [get_bd_pins AveADC/Dout] [get_bd_pins register_0/din]
  connect_bd_net -net adc_dac_adc1 [get_bd_pins B] [get_bd_pins c_accum_0/B]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins c_accum_0/CLK] [get_bd_pins register_0/clk]
  connect_bd_net -net c_accum_0_Q [get_bd_pins AveADC/Din] [get_bd_pins c_accum_0/Q]
  connect_bd_net -net register_0_dout [get_bd_pins AverageVal] [get_bd_pins register_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Acquisition_Control
proc create_hier_cell_Acquisition_Control { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Acquisition_Control() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins

  # Create pins
  create_bd_pin -dir O -from 0 -to 0 Acq_Valid
  create_bd_pin -dir I -from 31 -to 0 Acquistion_length_us
  create_bd_pin -dir O MHz_clk
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I -from 0 -to 0 trig

  # Create instance: TrigState1, and set properties
  set TrigState1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 TrigState1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {23} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {24} \
 ] $TrigState1

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {true} \
CONFIG.Output_Width {24} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: c_counter_binary_1, and set properties
  set c_counter_binary_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_1 ]
  set_property -dict [ list \
CONFIG.Output_Width {7} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_1

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {7} \
 ] $comparator_0

  # Create instance: comparator_1, and set properties
  set comparator_1 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_1 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {24} \
 ] $comparator_1

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: one_clock_pulse_0, and set properties
  set one_clock_pulse_0 [ create_bd_cell -type ip -vlnv CCFE:user:one_clock_pulse:1.0 one_clock_pulse_0 ]

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_OPERATION {not} \
CONFIG.C_SIZE {1} \
CONFIG.LOGO_FILE {data/sym_notgate.png} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {124} \
CONFIG.CONST_WIDTH {7} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net TrigState1_Dout [get_bd_pins TrigState1/Dout] [get_bd_pins comparator_1/b]
  connect_bd_net -net TrigState_Dout [get_bd_pins trig] [get_bd_pins one_clock_pulse_0/trig]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins c_counter_binary_1/CLK] [get_bd_pins latch_0/clk] [get_bd_pins one_clock_pulse_0/clk]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins c_counter_binary_0/Q] [get_bd_pins comparator_1/a]
  connect_bd_net -net c_counter_binary_1_Q [get_bd_pins c_counter_binary_1/Q] [get_bd_pins comparator_0/b]
  connect_bd_net -net cfg_acquisitionlength [get_bd_pins Acquistion_length_us] [get_bd_pins TrigState1/Din]
  connect_bd_net -net comparator_0_dout [get_bd_pins MHz_clk] [get_bd_pins c_counter_binary_0/CE] [get_bd_pins c_counter_binary_1/SCLR] [get_bd_pins comparator_0/dout]
  connect_bd_net -net comparator_1_dout [get_bd_pins comparator_1/dout] [get_bd_pins latch_0/set]
  connect_bd_net -net latch_0_q [get_bd_pins c_counter_binary_0/SCLR] [get_bd_pins latch_0/q] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net one_clock_pulse_0_pulse [get_bd_pins latch_0/reset] [get_bd_pins one_clock_pulse_0/pulse]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins Acq_Valid] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/a] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}


# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set DDR [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 DDR ]
  set FIXED_IO [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_processing_system7:fixedio_rtl:1.0 FIXED_IO ]
  set Vaux0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux0 ]
  set Vaux1 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux1 ]
  set Vaux8 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux8 ]
  set Vaux9 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux9 ]
  set Vp_Vn [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn ]

  # Create ports
  set adc_cdcs_o [ create_bd_port -dir O adc_cdcs_o ]
  set adc_clk_n_i [ create_bd_port -dir I adc_clk_n_i ]
  set adc_clk_p_i [ create_bd_port -dir I adc_clk_p_i ]
  set adc_clk_source [ create_bd_port -dir O -from 1 -to 0 adc_clk_source ]
  set adc_dat_a_i [ create_bd_port -dir I -from 13 -to 0 adc_dat_a_i ]
  set adc_dat_b_i [ create_bd_port -dir I -from 13 -to 0 adc_dat_b_i ]
  set dac_clk_o [ create_bd_port -dir O dac_clk_o ]
  set dac_dat_o [ create_bd_port -dir O -from 13 -to 0 dac_dat_o ]
  set dac_pwm_o [ create_bd_port -dir O -from 3 -to 0 dac_pwm_o ]
  set dac_rst_o [ create_bd_port -dir O dac_rst_o ]
  set dac_sel_o [ create_bd_port -dir O dac_sel_o ]
  set dac_wrt_o [ create_bd_port -dir O dac_wrt_o ]
  set led_o [ create_bd_port -dir O -from 7 -to 0 led_o ]

  # Create instance: Acquisition_Control
  create_hier_cell_Acquisition_Control [current_bd_instance .] Acquisition_Control

  # Create instance: ArmState, and set properties
  set ArmState [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 ArmState ]
  set_property -dict [ list \
CONFIG.DIN_FROM {0} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {1} \
 ] $ArmState

  # Create instance: Averager_0
  create_hier_cell_Averager_0 [current_bd_instance .] Averager_0

  # Create instance: Averager_1
  create_hier_cell_Averager_1 [current_bd_instance .] Averager_1

  # Create instance: Counter_Monitor
  create_hier_cell_Counter_Monitor [current_bd_instance .] Counter_Monitor

  # Create instance: RandomPulser
  create_hier_cell_RandomPulser [current_bd_instance .] RandomPulser

  # Create instance: Set_Reset_State
  create_hier_cell_Set_Reset_State [current_bd_instance .] Set_Reset_State

  # Create instance: TrigState, and set properties
  set TrigState [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 TrigState ]
  set_property -dict [ list \
CONFIG.DIN_FROM {1} \
CONFIG.DIN_TO {1} \
CONFIG.DOUT_WIDTH {1} \
 ] $TrigState

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter [current_bd_instance .] TriggeredCounter

  # Create instance: WillBeInputTigger, and set properties
  set WillBeInputTigger [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 WillBeInputTigger ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
 ] $WillBeInputTigger

  # Create instance: adc_axis_fifo, and set properties
  set adc_axis_fifo [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo ]
  set_property -dict [ list \
CONFIG.C_AXI4_BASEADDR {0x80001000} \
CONFIG.C_AXI4_HIGHADDR {0x80002FFF} \
CONFIG.C_RX_FIFO_DEPTH {16384} \
CONFIG.C_RX_FIFO_PF_THRESHOLD {8192} \
CONFIG.C_USE_RX_CUT_THROUGH {true} \
CONFIG.C_USE_TX_CTRL {0} \
CONFIG.C_USE_TX_DATA {0} \
 ] $adc_axis_fifo

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.C_AXI4_BASEADDR.VALUE_SRC {DEFAULT} \
CONFIG.C_AXI4_HIGHADDR.VALUE_SRC {DEFAULT} \
 ] $adc_axis_fifo

  # Create instance: adc_clock_converter, and set properties
  set adc_clock_converter [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter ]
  set_property -dict [ list \
CONFIG.TDATA_NUM_BYTES {4} \
 ] $adc_clock_converter

  # Create instance: adc_dac
  create_hier_cell_adc_dac [current_bd_instance .] adc_dac

  # Create instance: axi_bram_ctrl_dac, and set properties
  set axi_bram_ctrl_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_dac ]
  set_property -dict [ list \
CONFIG.PROTOCOL {AXI4LITE} \
CONFIG.SINGLE_PORT_BRAM {1} \
 ] $axi_bram_ctrl_dac

  # Create instance: axi_mem_intercon_0, and set properties
  set axi_mem_intercon_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_mem_intercon_0 ]
  set_property -dict [ list \
CONFIG.M02_HAS_REGSLICE {1} \
CONFIG.NUM_MI {4} \
 ] $axi_mem_intercon_0

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.use_bram_block.VALUE_SRC {DEFAULT} \
 ] $blk_mem_gen_dac

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {true} \
CONFIG.Output_Width {16} \
 ] $c_counter_binary_0

  # Create instance: cfg
  create_hier_cell_cfg [current_bd_instance .] cfg

  # Create instance: const_v0_w4, and set properties
  set const_v0_w4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_v0_w4 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {4} \
 ] $const_v0_w4

  # Create instance: const_v0_w5, and set properties
  set const_v0_w5 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_v0_w5 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {1} \
CONFIG.CONST_WIDTH {1} \
 ] $const_v0_w5

  # Create instance: const_v0_w32, and set properties
  set const_v0_w32 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_v0_w32 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {32} \
 ] $const_v0_w32

  # Create instance: delay1_0, and set properties
  set delay1_0 [ create_bd_cell -type ip -vlnv CCFE:user:delay1:1.0 delay1_0 ]
  set_property -dict [ list \
CONFIG.NBITS {1} \
 ] $delay1_0

  # Create instance: proc_sys_reset_0, and set properties
  set proc_sys_reset_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0 ]

  # Create instance: proc_sys_reset_adc_clk, and set properties
  set proc_sys_reset_adc_clk [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_adc_clk ]

  # Create instance: ps_0, and set properties
  set ps_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps_0 ]
  set_property -dict [ list \
CONFIG.PCW_ACT_CAN_PERIPHERAL_FREQMHZ {10.000000} \
CONFIG.PCW_ACT_DCI_PERIPHERAL_FREQMHZ {10.158730} \
CONFIG.PCW_ACT_ENET0_PERIPHERAL_FREQMHZ {125.000000} \
CONFIG.PCW_ACT_ENET1_PERIPHERAL_FREQMHZ {10.000000} \
CONFIG.PCW_ACT_FPGA0_PERIPHERAL_FREQMHZ {200.000000} \
CONFIG.PCW_ACT_FPGA1_PERIPHERAL_FREQMHZ {200.000000} \
CONFIG.PCW_ACT_FPGA2_PERIPHERAL_FREQMHZ {10.000000} \
CONFIG.PCW_ACT_FPGA3_PERIPHERAL_FREQMHZ {10.000000} \
CONFIG.PCW_ACT_PCAP_PERIPHERAL_FREQMHZ {200.000000} \
CONFIG.PCW_ACT_QSPI_PERIPHERAL_FREQMHZ {125.000000} \
CONFIG.PCW_ACT_SDIO_PERIPHERAL_FREQMHZ {100.000000} \
CONFIG.PCW_ACT_SMC_PERIPHERAL_FREQMHZ {10.000000} \
CONFIG.PCW_ACT_SPI_PERIPHERAL_FREQMHZ {166.666672} \
CONFIG.PCW_ACT_TPIU_PERIPHERAL_FREQMHZ {200.000000} \
CONFIG.PCW_ACT_UART_PERIPHERAL_FREQMHZ {100.000000} \
CONFIG.PCW_APU_CLK_RATIO_ENABLE {6:2:1} \
CONFIG.PCW_APU_PERIPHERAL_FREQMHZ {666.666666} \
CONFIG.PCW_ARMPLL_CTRL_FBDIV {40} \
CONFIG.PCW_CAN0_CAN0_IO {<Select>} \
CONFIG.PCW_CAN0_GRP_CLK_ENABLE {0} \
CONFIG.PCW_CAN0_GRP_CLK_IO {<Select>} \
CONFIG.PCW_CAN0_PERIPHERAL_CLKSRC {External} \
CONFIG.PCW_CAN0_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_CAN1_CAN1_IO {<Select>} \
CONFIG.PCW_CAN1_GRP_CLK_ENABLE {0} \
CONFIG.PCW_CAN1_GRP_CLK_IO {<Select>} \
CONFIG.PCW_CAN1_PERIPHERAL_CLKSRC {External} \
CONFIG.PCW_CAN1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_CAN_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_CAN_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_CAN_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_CAN_PERIPHERAL_FREQMHZ {100} \
CONFIG.PCW_CLK0_FREQ {200000000} \
CONFIG.PCW_CLK1_FREQ {200000000} \
CONFIG.PCW_CLK2_FREQ {10000000} \
CONFIG.PCW_CLK3_FREQ {10000000} \
CONFIG.PCW_CPU_CPU_6X4X_MAX_RANGE {667} \
CONFIG.PCW_CPU_CPU_PLL_FREQMHZ {1333.333} \
CONFIG.PCW_CPU_PERIPHERAL_CLKSRC {ARM PLL} \
CONFIG.PCW_CPU_PERIPHERAL_DIVISOR0 {2} \
CONFIG.PCW_CRYSTAL_PERIPHERAL_FREQMHZ {33.333333} \
CONFIG.PCW_DCI_PERIPHERAL_CLKSRC {DDR PLL} \
CONFIG.PCW_DCI_PERIPHERAL_DIVISOR0 {15} \
CONFIG.PCW_DCI_PERIPHERAL_DIVISOR1 {7} \
CONFIG.PCW_DCI_PERIPHERAL_FREQMHZ {10.159} \
CONFIG.PCW_DDRPLL_CTRL_FBDIV {32} \
CONFIG.PCW_DDR_DDR_PLL_FREQMHZ {1066.667} \
CONFIG.PCW_DDR_HPRLPR_QUEUE_PARTITION {HPR(0)/LPR(32)} \
CONFIG.PCW_DDR_HPR_TO_CRITICAL_PRIORITY_LEVEL {15} \
CONFIG.PCW_DDR_LPR_TO_CRITICAL_PRIORITY_LEVEL {2} \
CONFIG.PCW_DDR_PERIPHERAL_CLKSRC {DDR PLL} \
CONFIG.PCW_DDR_PERIPHERAL_DIVISOR0 {2} \
CONFIG.PCW_DDR_PORT0_HPR_ENABLE {0} \
CONFIG.PCW_DDR_PORT1_HPR_ENABLE {0} \
CONFIG.PCW_DDR_PORT2_HPR_ENABLE {0} \
CONFIG.PCW_DDR_PORT3_HPR_ENABLE {0} \
CONFIG.PCW_DDR_PRIORITY_READPORT_0 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_READPORT_1 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_READPORT_2 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_READPORT_3 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_0 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_1 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_2 {<Select>} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_3 {<Select>} \
CONFIG.PCW_DDR_RAM_HIGHADDR {0x1FFFFFFF} \
CONFIG.PCW_DDR_WRITE_TO_CRITICAL_PRIORITY_LEVEL {2} \
CONFIG.PCW_ENET0_ENET0_IO {MIO 16 .. 27} \
CONFIG.PCW_ENET0_GRP_MDIO_ENABLE {1} \
CONFIG.PCW_ENET0_GRP_MDIO_IO {MIO 52 .. 53} \
CONFIG.PCW_ENET0_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_ENET0_PERIPHERAL_DIVISOR0 {8} \
CONFIG.PCW_ENET0_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_ENET0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_ENET0_PERIPHERAL_FREQMHZ {1000 Mbps} \
CONFIG.PCW_ENET0_RESET_ENABLE {0} \
CONFIG.PCW_ENET0_RESET_IO {<Select>} \
CONFIG.PCW_ENET1_ENET1_IO {<Select>} \
CONFIG.PCW_ENET1_GRP_MDIO_ENABLE {0} \
CONFIG.PCW_ENET1_GRP_MDIO_IO {<Select>} \
CONFIG.PCW_ENET1_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_ENET1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_ENET1_PERIPHERAL_FREQMHZ {1000 Mbps} \
CONFIG.PCW_ENET1_RESET_ENABLE {0} \
CONFIG.PCW_ENET1_RESET_IO {<Select>} \
CONFIG.PCW_ENET_RESET_ENABLE {1} \
CONFIG.PCW_ENET_RESET_POLARITY {Active Low} \
CONFIG.PCW_ENET_RESET_SELECT {Share reset pin} \
CONFIG.PCW_EN_4K_TIMER {0} \
CONFIG.PCW_EN_CLK1_PORT {1} \
CONFIG.PCW_EN_EMIO_SPI0 {1} \
CONFIG.PCW_EN_EMIO_TTC0 {1} \
CONFIG.PCW_EN_ENET0 {1} \
CONFIG.PCW_EN_GPIO {1} \
CONFIG.PCW_EN_I2C0 {1} \
CONFIG.PCW_EN_QSPI {1} \
CONFIG.PCW_EN_SDIO0 {1} \
CONFIG.PCW_EN_SPI0 {1} \
CONFIG.PCW_EN_SPI1 {1} \
CONFIG.PCW_EN_TTC0 {1} \
CONFIG.PCW_EN_UART0 {1} \
CONFIG.PCW_EN_UART1 {1} \
CONFIG.PCW_EN_USB0 {1} \
CONFIG.PCW_FCLK0_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR0 {5} \
CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_FCLK1_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_FCLK1_PERIPHERAL_DIVISOR0 {5} \
CONFIG.PCW_FCLK1_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_FCLK2_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_FCLK3_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR1 {1} \
CONFIG.PCW_FCLK_CLK0_BUF {true} \
CONFIG.PCW_FCLK_CLK1_BUF {true} \
CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {200} \
CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {200} \
CONFIG.PCW_FPGA2_PERIPHERAL_FREQMHZ {50} \
CONFIG.PCW_FPGA3_PERIPHERAL_FREQMHZ {200} \
CONFIG.PCW_FPGA_FCLK0_ENABLE {1} \
CONFIG.PCW_FPGA_FCLK1_ENABLE {1} \
CONFIG.PCW_FTM_CTI_IN0 {<Select>} \
CONFIG.PCW_FTM_CTI_IN1 {<Select>} \
CONFIG.PCW_FTM_CTI_IN2 {<Select>} \
CONFIG.PCW_FTM_CTI_IN3 {<Select>} \
CONFIG.PCW_FTM_CTI_OUT0 {<Select>} \
CONFIG.PCW_FTM_CTI_OUT1 {<Select>} \
CONFIG.PCW_FTM_CTI_OUT2 {<Select>} \
CONFIG.PCW_FTM_CTI_OUT3 {<Select>} \
CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {0} \
CONFIG.PCW_GPIO_EMIO_GPIO_IO {<Select>} \
CONFIG.PCW_GPIO_MIO_GPIO_ENABLE {1} \
CONFIG.PCW_GPIO_MIO_GPIO_IO {MIO} \
CONFIG.PCW_GPIO_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_I2C0_GRP_INT_ENABLE {0} \
CONFIG.PCW_I2C0_GRP_INT_IO {<Select>} \
CONFIG.PCW_I2C0_I2C0_IO {MIO 50 .. 51} \
CONFIG.PCW_I2C0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_I2C0_RESET_ENABLE {0} \
CONFIG.PCW_I2C0_RESET_IO {<Select>} \
CONFIG.PCW_I2C1_GRP_INT_ENABLE {0} \
CONFIG.PCW_I2C1_GRP_INT_IO {<Select>} \
CONFIG.PCW_I2C1_I2C1_IO {<Select>} \
CONFIG.PCW_I2C1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_I2C1_RESET_ENABLE {0} \
CONFIG.PCW_I2C1_RESET_IO {<Select>} \
CONFIG.PCW_I2C_PERIPHERAL_FREQMHZ {111.111115} \
CONFIG.PCW_I2C_RESET_ENABLE {1} \
CONFIG.PCW_I2C_RESET_POLARITY {Active Low} \
CONFIG.PCW_I2C_RESET_SELECT {Share reset pin} \
CONFIG.PCW_IOPLL_CTRL_FBDIV {30} \
CONFIG.PCW_IO_IO_PLL_FREQMHZ {1000.000} \
CONFIG.PCW_MIO_0_DIRECTION {inout} \
CONFIG.PCW_MIO_0_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_0_PULLUP {disabled} \
CONFIG.PCW_MIO_0_SLEW {slow} \
CONFIG.PCW_MIO_10_DIRECTION {inout} \
CONFIG.PCW_MIO_10_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_10_PULLUP {enabled} \
CONFIG.PCW_MIO_10_SLEW {slow} \
CONFIG.PCW_MIO_11_DIRECTION {inout} \
CONFIG.PCW_MIO_11_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_11_PULLUP {enabled} \
CONFIG.PCW_MIO_11_SLEW {slow} \
CONFIG.PCW_MIO_12_DIRECTION {inout} \
CONFIG.PCW_MIO_12_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_12_PULLUP {enabled} \
CONFIG.PCW_MIO_12_SLEW {slow} \
CONFIG.PCW_MIO_13_DIRECTION {inout} \
CONFIG.PCW_MIO_13_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_13_PULLUP {enabled} \
CONFIG.PCW_MIO_13_SLEW {slow} \
CONFIG.PCW_MIO_14_DIRECTION {in} \
CONFIG.PCW_MIO_14_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_14_PULLUP {enabled} \
CONFIG.PCW_MIO_14_SLEW {slow} \
CONFIG.PCW_MIO_15_DIRECTION {out} \
CONFIG.PCW_MIO_15_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_15_PULLUP {enabled} \
CONFIG.PCW_MIO_15_SLEW {slow} \
CONFIG.PCW_MIO_16_DIRECTION {out} \
CONFIG.PCW_MIO_16_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_16_PULLUP {disabled} \
CONFIG.PCW_MIO_16_SLEW {fast} \
CONFIG.PCW_MIO_17_DIRECTION {out} \
CONFIG.PCW_MIO_17_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_17_PULLUP {disabled} \
CONFIG.PCW_MIO_17_SLEW {fast} \
CONFIG.PCW_MIO_18_DIRECTION {out} \
CONFIG.PCW_MIO_18_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_18_PULLUP {disabled} \
CONFIG.PCW_MIO_18_SLEW {fast} \
CONFIG.PCW_MIO_19_DIRECTION {out} \
CONFIG.PCW_MIO_19_IOTYPE {out} \
CONFIG.PCW_MIO_19_PULLUP {disabled} \
CONFIG.PCW_MIO_19_SLEW {fast} \
CONFIG.PCW_MIO_1_DIRECTION {out} \
CONFIG.PCW_MIO_1_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_1_PULLUP {enabled} \
CONFIG.PCW_MIO_1_SLEW {slow} \
CONFIG.PCW_MIO_20_DIRECTION {out} \
CONFIG.PCW_MIO_20_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_20_PULLUP {disabled} \
CONFIG.PCW_MIO_20_SLEW {fast} \
CONFIG.PCW_MIO_21_DIRECTION {out} \
CONFIG.PCW_MIO_21_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_21_PULLUP {disabled} \
CONFIG.PCW_MIO_21_SLEW {fast} \
CONFIG.PCW_MIO_22_DIRECTION {in} \
CONFIG.PCW_MIO_22_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_22_PULLUP {disabled} \
CONFIG.PCW_MIO_22_SLEW {fast} \
CONFIG.PCW_MIO_23_DIRECTION {in} \
CONFIG.PCW_MIO_23_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_23_PULLUP {disabled} \
CONFIG.PCW_MIO_23_SLEW {fast} \
CONFIG.PCW_MIO_24_DIRECTION {in} \
CONFIG.PCW_MIO_24_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_24_PULLUP {disabled} \
CONFIG.PCW_MIO_24_SLEW {fast} \
CONFIG.PCW_MIO_25_DIRECTION {in} \
CONFIG.PCW_MIO_25_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_25_PULLUP {disabled} \
CONFIG.PCW_MIO_25_SLEW {fast} \
CONFIG.PCW_MIO_26_DIRECTION {in} \
CONFIG.PCW_MIO_26_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_26_PULLUP {disabled} \
CONFIG.PCW_MIO_26_SLEW {fast} \
CONFIG.PCW_MIO_27_DIRECTION {in} \
CONFIG.PCW_MIO_27_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_27_PULLUP {disabled} \
CONFIG.PCW_MIO_27_SLEW {fast} \
CONFIG.PCW_MIO_28_DIRECTION {inout} \
CONFIG.PCW_MIO_28_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_28_PULLUP {enabled} \
CONFIG.PCW_MIO_28_SLEW {fast} \
CONFIG.PCW_MIO_29_DIRECTION {in} \
CONFIG.PCW_MIO_29_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_29_PULLUP {enabled} \
CONFIG.PCW_MIO_29_SLEW {fast} \
CONFIG.PCW_MIO_2_DIRECTION {inout} \
CONFIG.PCW_MIO_2_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_2_PULLUP {disabled} \
CONFIG.PCW_MIO_2_SLEW {slow} \
CONFIG.PCW_MIO_30_DIRECTION {out} \
CONFIG.PCW_MIO_30_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_30_PULLUP {enabled} \
CONFIG.PCW_MIO_30_SLEW {fast} \
CONFIG.PCW_MIO_31_DIRECTION {in} \
CONFIG.PCW_MIO_31_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_31_PULLUP {enabled} \
CONFIG.PCW_MIO_31_SLEW {fast} \
CONFIG.PCW_MIO_32_DIRECTION {inout} \
CONFIG.PCW_MIO_32_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_32_PULLUP {enabled} \
CONFIG.PCW_MIO_32_SLEW {fast} \
CONFIG.PCW_MIO_33_DIRECTION {inout} \
CONFIG.PCW_MIO_33_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_33_PULLUP {enabled} \
CONFIG.PCW_MIO_33_SLEW {fast} \
CONFIG.PCW_MIO_34_DIRECTION {inout} \
CONFIG.PCW_MIO_34_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_34_PULLUP {enabled} \
CONFIG.PCW_MIO_34_SLEW {fast} \
CONFIG.PCW_MIO_35_DIRECTION {inout} \
CONFIG.PCW_MIO_35_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_35_PULLUP {enabled} \
CONFIG.PCW_MIO_35_SLEW {fast} \
CONFIG.PCW_MIO_36_DIRECTION {in} \
CONFIG.PCW_MIO_36_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_36_PULLUP {enabled} \
CONFIG.PCW_MIO_36_SLEW {fast} \
CONFIG.PCW_MIO_37_DIRECTION {inout} \
CONFIG.PCW_MIO_37_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_37_PULLUP {enabled} \
CONFIG.PCW_MIO_37_SLEW {fast} \
CONFIG.PCW_MIO_38_DIRECTION {inout} \
CONFIG.PCW_MIO_38_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_38_PULLUP {enabled} \
CONFIG.PCW_MIO_38_SLEW {fast} \
CONFIG.PCW_MIO_39_DIRECTION {inout} \
CONFIG.PCW_MIO_39_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_39_PULLUP {enabled} \
CONFIG.PCW_MIO_39_SLEW {fast} \
CONFIG.PCW_MIO_3_DIRECTION {inout} \
CONFIG.PCW_MIO_3_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_3_PULLUP {disabled} \
CONFIG.PCW_MIO_3_SLEW {slow} \
CONFIG.PCW_MIO_40_DIRECTION {inout} \
CONFIG.PCW_MIO_40_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_40_PULLUP {enabled} \
CONFIG.PCW_MIO_40_SLEW {slow} \
CONFIG.PCW_MIO_41_DIRECTION {inout} \
CONFIG.PCW_MIO_41_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_41_PULLUP {enabled} \
CONFIG.PCW_MIO_41_SLEW {slow} \
CONFIG.PCW_MIO_42_DIRECTION {inout} \
CONFIG.PCW_MIO_42_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_42_PULLUP {enabled} \
CONFIG.PCW_MIO_42_SLEW {slow} \
CONFIG.PCW_MIO_43_DIRECTION {inout} \
CONFIG.PCW_MIO_43_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_43_PULLUP {enabled} \
CONFIG.PCW_MIO_43_SLEW {slow} \
CONFIG.PCW_MIO_44_DIRECTION {inout} \
CONFIG.PCW_MIO_44_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_44_PULLUP {enabled} \
CONFIG.PCW_MIO_44_SLEW {slow} \
CONFIG.PCW_MIO_45_DIRECTION {inout} \
CONFIG.PCW_MIO_45_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_45_PULLUP {enabled} \
CONFIG.PCW_MIO_45_SLEW {slow} \
CONFIG.PCW_MIO_46_DIRECTION {in} \
CONFIG.PCW_MIO_46_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_46_PULLUP {enabled} \
CONFIG.PCW_MIO_46_SLEW {slow} \
CONFIG.PCW_MIO_47_DIRECTION {in} \
CONFIG.PCW_MIO_47_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_47_PULLUP {enabled} \
CONFIG.PCW_MIO_47_SLEW {slow} \
CONFIG.PCW_MIO_48_DIRECTION {out} \
CONFIG.PCW_MIO_48_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_48_PULLUP {enabled} \
CONFIG.PCW_MIO_48_SLEW {slow} \
CONFIG.PCW_MIO_49_DIRECTION {inout} \
CONFIG.PCW_MIO_49_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_49_PULLUP {enabled} \
CONFIG.PCW_MIO_49_SLEW {slow} \
CONFIG.PCW_MIO_4_DIRECTION {inout} \
CONFIG.PCW_MIO_4_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_4_PULLUP {disabled} \
CONFIG.PCW_MIO_4_SLEW {slow} \
CONFIG.PCW_MIO_50_DIRECTION {inout} \
CONFIG.PCW_MIO_50_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_50_PULLUP {enabled} \
CONFIG.PCW_MIO_50_SLEW {slow} \
CONFIG.PCW_MIO_51_DIRECTION {inout} \
CONFIG.PCW_MIO_51_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_51_PULLUP {enabled} \
CONFIG.PCW_MIO_51_SLEW {slow} \
CONFIG.PCW_MIO_52_DIRECTION {out} \
CONFIG.PCW_MIO_52_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_52_PULLUP {enabled} \
CONFIG.PCW_MIO_52_SLEW {slow} \
CONFIG.PCW_MIO_53_DIRECTION {inout} \
CONFIG.PCW_MIO_53_IOTYPE {LVCMOS 2.5V} \
CONFIG.PCW_MIO_53_PULLUP {enabled} \
CONFIG.PCW_MIO_53_SLEW {slow} \
CONFIG.PCW_MIO_5_DIRECTION {inout} \
CONFIG.PCW_MIO_5_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_5_PULLUP {disabled} \
CONFIG.PCW_MIO_5_SLEW {slow} \
CONFIG.PCW_MIO_6_DIRECTION {out} \
CONFIG.PCW_MIO_6_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_6_PULLUP {disabled} \
CONFIG.PCW_MIO_6_SLEW {slow} \
CONFIG.PCW_MIO_7_DIRECTION {out} \
CONFIG.PCW_MIO_7_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_7_PULLUP {disabled} \
CONFIG.PCW_MIO_7_SLEW {slow} \
CONFIG.PCW_MIO_8_DIRECTION {out} \
CONFIG.PCW_MIO_8_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_8_PULLUP {disabled} \
CONFIG.PCW_MIO_8_SLEW {slow} \
CONFIG.PCW_MIO_9_DIRECTION {in} \
CONFIG.PCW_MIO_9_IOTYPE {LVCMOS 3.3V} \
CONFIG.PCW_MIO_9_PULLUP {enabled} \
CONFIG.PCW_MIO_9_SLEW {slow} \
CONFIG.PCW_MIO_TREE_PERIPHERALS {GPIO#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#GPIO#UART 1#UART 1#SPI 1#SPI 1#SPI 1#SPI 1#UART 0#UART 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#Enet 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#SD 0#SD 0#SD 0#SD 0#SD 0#SD 0#SD 0#SD 0#USB Reset#GPIO#I2C 0#I2C 0#Enet 0#Enet 0} \
CONFIG.PCW_MIO_TREE_SIGNALS {gpio[0]#qspi0_ss_b#qspi0_io[0]#qspi0_io[1]#qspi0_io[2]#qspi0_io[3]#qspi0_sclk#gpio[7]#tx#rx#mosi#miso#sclk#ss[0]#rx#tx#tx_clk#txd[0]#txd[1]#txd[2]#txd[3]#tx_ctl#rx_clk#rxd[0]#rxd[1]#rxd[2]#rxd[3]#rx_ctl#data[4]#dir#stp#nxt#data[0]#data[1]#data[2]#data[3]#clk#data[5]#data[6]#data[7]#clk#cmd#data[0]#data[1]#data[2]#data[3]#cd#wp#reset#gpio[49]#scl#sda#mdc#mdio} \
CONFIG.PCW_M_AXI_GP0_FREQMHZ {125} \
CONFIG.PCW_NAND_CYCLES_T_AR {1} \
CONFIG.PCW_NAND_CYCLES_T_CLR {1} \
CONFIG.PCW_NAND_CYCLES_T_RC {11} \
CONFIG.PCW_NAND_CYCLES_T_REA {1} \
CONFIG.PCW_NAND_CYCLES_T_RR {1} \
CONFIG.PCW_NAND_CYCLES_T_WC {11} \
CONFIG.PCW_NAND_CYCLES_T_WP {1} \
CONFIG.PCW_NAND_GRP_D8_ENABLE {0} \
CONFIG.PCW_NAND_GRP_D8_IO {<Select>} \
CONFIG.PCW_NAND_NAND_IO {<Select>} \
CONFIG.PCW_NAND_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_NOR_CS0_T_CEOE {1} \
CONFIG.PCW_NOR_CS0_T_PC {1} \
CONFIG.PCW_NOR_CS0_T_RC {11} \
CONFIG.PCW_NOR_CS0_T_TR {1} \
CONFIG.PCW_NOR_CS0_T_WC {11} \
CONFIG.PCW_NOR_CS0_T_WP {1} \
CONFIG.PCW_NOR_CS0_WE_TIME {0} \
CONFIG.PCW_NOR_CS1_T_CEOE {1} \
CONFIG.PCW_NOR_CS1_T_PC {1} \
CONFIG.PCW_NOR_CS1_T_RC {11} \
CONFIG.PCW_NOR_CS1_T_TR {1} \
CONFIG.PCW_NOR_CS1_T_WC {11} \
CONFIG.PCW_NOR_CS1_T_WP {1} \
CONFIG.PCW_NOR_CS1_WE_TIME {0} \
CONFIG.PCW_NOR_GRP_A25_ENABLE {0} \
CONFIG.PCW_NOR_GRP_A25_IO {<Select>} \
CONFIG.PCW_NOR_GRP_CS0_ENABLE {0} \
CONFIG.PCW_NOR_GRP_CS0_IO {<Select>} \
CONFIG.PCW_NOR_GRP_CS1_ENABLE {0} \
CONFIG.PCW_NOR_GRP_CS1_IO {<Select>} \
CONFIG.PCW_NOR_GRP_SRAM_CS0_ENABLE {0} \
CONFIG.PCW_NOR_GRP_SRAM_CS0_IO {<Select>} \
CONFIG.PCW_NOR_GRP_SRAM_CS1_ENABLE {0} \
CONFIG.PCW_NOR_GRP_SRAM_CS1_IO {<Select>} \
CONFIG.PCW_NOR_GRP_SRAM_INT_ENABLE {0} \
CONFIG.PCW_NOR_GRP_SRAM_INT_IO {<Select>} \
CONFIG.PCW_NOR_NOR_IO {<Select>} \
CONFIG.PCW_NOR_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_NOR_SRAM_CS0_T_CEOE {1} \
CONFIG.PCW_NOR_SRAM_CS0_T_PC {1} \
CONFIG.PCW_NOR_SRAM_CS0_T_RC {11} \
CONFIG.PCW_NOR_SRAM_CS0_T_TR {1} \
CONFIG.PCW_NOR_SRAM_CS0_T_WC {11} \
CONFIG.PCW_NOR_SRAM_CS0_T_WP {1} \
CONFIG.PCW_NOR_SRAM_CS0_WE_TIME {0} \
CONFIG.PCW_NOR_SRAM_CS1_T_CEOE {1} \
CONFIG.PCW_NOR_SRAM_CS1_T_PC {1} \
CONFIG.PCW_NOR_SRAM_CS1_T_RC {11} \
CONFIG.PCW_NOR_SRAM_CS1_T_TR {1} \
CONFIG.PCW_NOR_SRAM_CS1_T_WC {11} \
CONFIG.PCW_NOR_SRAM_CS1_T_WP {1} \
CONFIG.PCW_NOR_SRAM_CS1_WE_TIME {0} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY0 {0.080} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY1 {0.063} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY2 {0.057} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY3 {0.068} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_0 {-0.047} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_1 {-0.025} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_2 {-0.006} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_3 {-0.017} \
CONFIG.PCW_PACKAGE_NAME {clg400} \
CONFIG.PCW_PCAP_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_PCAP_PERIPHERAL_DIVISOR0 {5} \
CONFIG.PCW_PCAP_PERIPHERAL_FREQMHZ {200} \
CONFIG.PCW_PJTAG_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_PJTAG_PJTAG_IO {<Select>} \
CONFIG.PCW_PLL_BYPASSMODE_ENABLE {0} \
CONFIG.PCW_PRESET_BANK0_VOLTAGE {LVCMOS 3.3V} \
CONFIG.PCW_PRESET_BANK1_VOLTAGE {LVCMOS 2.5V} \
CONFIG.PCW_QSPI_GRP_FBCLK_ENABLE {0} \
CONFIG.PCW_QSPI_GRP_FBCLK_IO {<Select>} \
CONFIG.PCW_QSPI_GRP_IO1_ENABLE {0} \
CONFIG.PCW_QSPI_GRP_IO1_IO {<Select>} \
CONFIG.PCW_QSPI_GRP_SINGLE_SS_ENABLE {1} \
CONFIG.PCW_QSPI_GRP_SINGLE_SS_IO {MIO 1 .. 6} \
CONFIG.PCW_QSPI_GRP_SS1_ENABLE {0} \
CONFIG.PCW_QSPI_GRP_SS1_IO {<Select>} \
CONFIG.PCW_QSPI_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_QSPI_PERIPHERAL_DIVISOR0 {8} \
CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_QSPI_PERIPHERAL_FREQMHZ {125} \
CONFIG.PCW_QSPI_QSPI_IO {MIO 1 .. 6} \
CONFIG.PCW_SD0_GRP_CD_ENABLE {1} \
CONFIG.PCW_SD0_GRP_CD_IO {MIO 46} \
CONFIG.PCW_SD0_GRP_POW_ENABLE {0} \
CONFIG.PCW_SD0_GRP_POW_IO {<Select>} \
CONFIG.PCW_SD0_GRP_WP_ENABLE {1} \
CONFIG.PCW_SD0_GRP_WP_IO {MIO 47} \
CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_SD0_SD0_IO {MIO 40 .. 45} \
CONFIG.PCW_SD1_GRP_CD_ENABLE {0} \
CONFIG.PCW_SD1_GRP_CD_IO {<Select>} \
CONFIG.PCW_SD1_GRP_POW_ENABLE {0} \
CONFIG.PCW_SD1_GRP_POW_IO {<Select>} \
CONFIG.PCW_SD1_GRP_WP_ENABLE {0} \
CONFIG.PCW_SD1_GRP_WP_IO {<Select>} \
CONFIG.PCW_SD1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_SD1_SD1_IO {<Select>} \
CONFIG.PCW_SDIO_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_SDIO_PERIPHERAL_DIVISOR0 {10} \
CONFIG.PCW_SDIO_PERIPHERAL_FREQMHZ {100} \
CONFIG.PCW_SDIO_PERIPHERAL_VALID {1} \
CONFIG.PCW_SMC_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_SMC_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_SMC_PERIPHERAL_FREQMHZ {100} \
CONFIG.PCW_SPI0_GRP_SS0_ENABLE {1} \
CONFIG.PCW_SPI0_GRP_SS0_IO {EMIO} \
CONFIG.PCW_SPI0_GRP_SS1_ENABLE {1} \
CONFIG.PCW_SPI0_GRP_SS1_IO {EMIO} \
CONFIG.PCW_SPI0_GRP_SS2_ENABLE {1} \
CONFIG.PCW_SPI0_GRP_SS2_IO {EMIO} \
CONFIG.PCW_SPI0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_SPI0_SPI0_IO {EMIO} \
CONFIG.PCW_SPI1_GRP_SS0_ENABLE {1} \
CONFIG.PCW_SPI1_GRP_SS0_IO {MIO 13} \
CONFIG.PCW_SPI1_GRP_SS1_ENABLE {0} \
CONFIG.PCW_SPI1_GRP_SS1_IO {<Select>} \
CONFIG.PCW_SPI1_GRP_SS2_ENABLE {0} \
CONFIG.PCW_SPI1_GRP_SS2_IO {<Select>} \
CONFIG.PCW_SPI1_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_SPI1_SPI1_IO {MIO 10 .. 15} \
CONFIG.PCW_SPI_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_SPI_PERIPHERAL_DIVISOR0 {6} \
CONFIG.PCW_SPI_PERIPHERAL_FREQMHZ {166.666666} \
CONFIG.PCW_SPI_PERIPHERAL_VALID {1} \
CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {64} \
CONFIG.PCW_S_AXI_HP1_DATA_WIDTH {64} \
CONFIG.PCW_S_AXI_HP2_DATA_WIDTH {64} \
CONFIG.PCW_S_AXI_HP3_DATA_WIDTH {64} \
CONFIG.PCW_TPIU_PERIPHERAL_CLKSRC {External} \
CONFIG.PCW_TPIU_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TPIU_PERIPHERAL_FREQMHZ {200} \
CONFIG.PCW_TRACE_GRP_16BIT_ENABLE {0} \
CONFIG.PCW_TRACE_GRP_16BIT_IO {<Select>} \
CONFIG.PCW_TRACE_GRP_2BIT_ENABLE {0} \
CONFIG.PCW_TRACE_GRP_2BIT_IO {<Select>} \
CONFIG.PCW_TRACE_GRP_32BIT_ENABLE {0} \
CONFIG.PCW_TRACE_GRP_32BIT_IO {<Select>} \
CONFIG.PCW_TRACE_GRP_4BIT_ENABLE {0} \
CONFIG.PCW_TRACE_GRP_4BIT_IO {<Select>} \
CONFIG.PCW_TRACE_GRP_8BIT_ENABLE {0} \
CONFIG.PCW_TRACE_GRP_8BIT_IO {<Select>} \
CONFIG.PCW_TRACE_INTERNAL_WIDTH {2} \
CONFIG.PCW_TRACE_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_TRACE_TRACE_IO {<Select>} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_TTC0_TTC0_IO {EMIO} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_TTC1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_TTC1_TTC1_IO {<Select>} \
CONFIG.PCW_TTC_PERIPHERAL_FREQMHZ {50} \
CONFIG.PCW_UART0_BAUD_RATE {115200} \
CONFIG.PCW_UART0_GRP_FULL_ENABLE {0} \
CONFIG.PCW_UART0_GRP_FULL_IO {<Select>} \
CONFIG.PCW_UART0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_UART0_UART0_IO {MIO 14 .. 15} \
CONFIG.PCW_UART1_BAUD_RATE {115200} \
CONFIG.PCW_UART1_GRP_FULL_ENABLE {0} \
CONFIG.PCW_UART1_GRP_FULL_IO {<Select>} \
CONFIG.PCW_UART1_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_UART1_UART1_IO {MIO 8 .. 9} \
CONFIG.PCW_UART_PERIPHERAL_CLKSRC {IO PLL} \
CONFIG.PCW_UART_PERIPHERAL_DIVISOR0 {10} \
CONFIG.PCW_UART_PERIPHERAL_FREQMHZ {100} \
CONFIG.PCW_UART_PERIPHERAL_VALID {1} \
CONFIG.PCW_UIPARAM_DDR_ADV_ENABLE {0} \
CONFIG.PCW_UIPARAM_DDR_AL {0} \
CONFIG.PCW_UIPARAM_DDR_BANK_ADDR_COUNT {3} \
CONFIG.PCW_UIPARAM_DDR_BL {8} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY0 {0.0} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY1 {0.0} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY2 {0.0} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY3 {0.0} \
CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {16 Bit} \
CONFIG.PCW_UIPARAM_DDR_CL {7} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PACKAGE_LENGTH {54.563} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PACKAGE_LENGTH {54.563} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PACKAGE_LENGTH {54.563} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PACKAGE_LENGTH {54.563} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_STOP_EN {0} \
CONFIG.PCW_UIPARAM_DDR_COL_ADDR_COUNT {10} \
CONFIG.PCW_UIPARAM_DDR_CWL {6} \
CONFIG.PCW_UIPARAM_DDR_DEVICE_CAPACITY {4096 MBits} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_PACKAGE_LENGTH {101.239} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_PACKAGE_LENGTH {79.5025} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_PACKAGE_LENGTH {60.536} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_PACKAGE_LENGTH {71.7715} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_0 {0.0} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_1 {0.0} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_2 {0.0} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_3 {0.0} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_PACKAGE_LENGTH {104.5365} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_PACKAGE_LENGTH {70.676} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_PACKAGE_LENGTH {59.1615} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_LENGTH_MM {0} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_PACKAGE_LENGTH {81.319} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_PROPOGATION_DELAY {160} \
CONFIG.PCW_UIPARAM_DDR_DRAM_WIDTH {16 Bits} \
CONFIG.PCW_UIPARAM_DDR_ECC {Disabled} \
CONFIG.PCW_UIPARAM_DDR_ENABLE {1} \
CONFIG.PCW_UIPARAM_DDR_FREQ_MHZ {533.333333} \
CONFIG.PCW_UIPARAM_DDR_HIGH_TEMP {Normal (0-85)} \
CONFIG.PCW_UIPARAM_DDR_MEMORY_TYPE {DDR 3} \
CONFIG.PCW_UIPARAM_DDR_PARTNO {MT41J256M16 RE-125} \
CONFIG.PCW_UIPARAM_DDR_ROW_ADDR_COUNT {15} \
CONFIG.PCW_UIPARAM_DDR_SPEED_BIN {DDR3_1066F} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_DATA_EYE {1} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_READ_GATE {1} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_WRITE_LEVEL {1} \
CONFIG.PCW_UIPARAM_DDR_T_FAW {40.0} \
CONFIG.PCW_UIPARAM_DDR_T_RAS_MIN {35.0} \
CONFIG.PCW_UIPARAM_DDR_T_RC {48.91} \
CONFIG.PCW_UIPARAM_DDR_T_RCD {7} \
CONFIG.PCW_UIPARAM_DDR_T_RP {7} \
CONFIG.PCW_UIPARAM_DDR_USE_INTERNAL_VREF {0} \
CONFIG.PCW_USB0_PERIPHERAL_ENABLE {1} \
CONFIG.PCW_USB0_PERIPHERAL_FREQMHZ {60} \
CONFIG.PCW_USB0_RESET_ENABLE {1} \
CONFIG.PCW_USB0_RESET_IO {MIO 48} \
CONFIG.PCW_USB0_USB0_IO {MIO 28 .. 39} \
CONFIG.PCW_USB1_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_USB1_PERIPHERAL_FREQMHZ {60} \
CONFIG.PCW_USB1_RESET_ENABLE {0} \
CONFIG.PCW_USB1_RESET_IO {<Select>} \
CONFIG.PCW_USB1_USB1_IO {<Select>} \
CONFIG.PCW_USB_RESET_ENABLE {1} \
CONFIG.PCW_USB_RESET_POLARITY {Active Low} \
CONFIG.PCW_USB_RESET_SELECT {Share reset pin} \
CONFIG.PCW_USE_CROSS_TRIGGER {0} \
CONFIG.PCW_USE_M_AXI_GP0 {1} \
CONFIG.PCW_USE_S_AXI_HP0 {0} \
CONFIG.PCW_WDT_PERIPHERAL_CLKSRC {CPU_1X} \
CONFIG.PCW_WDT_PERIPHERAL_DIVISOR0 {1} \
CONFIG.PCW_WDT_PERIPHERAL_ENABLE {0} \
CONFIG.PCW_WDT_PERIPHERAL_FREQMHZ {133.333333} \
CONFIG.PCW_WDT_WDT_IO {<Select>} \
 ] $ps_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.PCW_ACT_CAN_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_DCI_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_ENET1_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_FPGA2_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_FPGA3_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_PCAP_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_SMC_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ACT_TPIU_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_APU_CLK_RATIO_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_APU_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ARMPLL_CTRL_FBDIV.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN0_CAN0_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN0_GRP_CLK_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN0_GRP_CLK_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN0_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN0_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN1_CAN1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN1_GRP_CLK_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN1_GRP_CLK_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN1_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CAN_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CLK2_FREQ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CLK3_FREQ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CPU_CPU_6X4X_MAX_RANGE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CPU_CPU_PLL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CPU_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CPU_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_CRYSTAL_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DCI_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DCI_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DCI_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DCI_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDRPLL_CTRL_FBDIV.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_DDR_PLL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_HPRLPR_QUEUE_PARTITION.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_HPR_TO_CRITICAL_PRIORITY_LEVEL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_LPR_TO_CRITICAL_PRIORITY_LEVEL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PORT0_HPR_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PORT1_HPR_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PORT2_HPR_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PORT3_HPR_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_READPORT_0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_READPORT_1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_READPORT_2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_READPORT_3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_PRIORITY_WRITEPORT_3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_RAM_HIGHADDR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_DDR_WRITE_TO_CRITICAL_PRIORITY_LEVEL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET0_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET0_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET0_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET0_RESET_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET0_RESET_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_ENET1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_GRP_MDIO_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_GRP_MDIO_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_RESET_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET1_RESET_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_ENET_RESET_POLARITY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_EN_4K_TIMER.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK0_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK1_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK1_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK2_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK3_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FCLK_CLK0_BUF.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FPGA2_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_FPGA_FCLK0_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_GPIO_EMIO_GPIO_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C0_GRP_INT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C0_GRP_INT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C0_RESET_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C0_RESET_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_GRP_INT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_GRP_INT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_I2C1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_RESET_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C1_RESET_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_I2C_RESET_POLARITY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_AR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_CLR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_RC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_REA.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_RR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_WC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_CYCLES_T_WP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_GRP_D8_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_GRP_D8_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_NAND_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NAND_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_CEOE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_PC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_RC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_TR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_WC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_T_WP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS0_WE_TIME.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_CEOE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_PC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_RC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_TR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_WC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_T_WP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_CS1_WE_TIME.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_A25_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_A25_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_CS0_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_CS0_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_CS1_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_CS1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_CS0_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_CS0_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_CS1_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_CS1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_INT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_GRP_SRAM_INT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_NOR_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_CEOE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_PC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_RC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_TR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_WC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_T_WP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS0_WE_TIME.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_CEOE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_PC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_RC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_TR.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_WC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_T_WP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_NOR_SRAM_CS1_WE_TIME.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_BOARD_DELAY3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_DDR_DQS_TO_CLK_DELAY_3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PACKAGE_NAME.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PCAP_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PCAP_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PJTAG_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PJTAG_PJTAG_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PLL_BYPASSMODE_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_PRESET_BANK0_VOLTAGE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_FBCLK_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_FBCLK_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_IO1_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_IO1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_SS1_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_GRP_SS1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_QSPI_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD0_GRP_POW_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD0_GRP_POW_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_CD_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_CD_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_POW_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_POW_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_WP_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_GRP_WP_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SD1_SD1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SDIO_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SDIO_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SMC_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SMC_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SMC_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI1_GRP_SS1_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI1_GRP_SS1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI1_GRP_SS2_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI1_GRP_SS2_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_SPI_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_S_AXI_HP0_DATA_WIDTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_S_AXI_HP1_DATA_WIDTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_S_AXI_HP2_DATA_WIDTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_S_AXI_HP3_DATA_WIDTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TPIU_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TPIU_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TPIU_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_16BIT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_16BIT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_2BIT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_2BIT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_32BIT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_32BIT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_4BIT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_4BIT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_8BIT_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_GRP_8BIT_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_INTERNAL_WIDTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TRACE_TRACE_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK0_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK1_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC0_CLK2_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK0_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK1_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_CLK2_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC1_TTC1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_TTC_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART0_BAUD_RATE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART0_GRP_FULL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART0_GRP_FULL_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART1_BAUD_RATE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART1_GRP_FULL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART1_GRP_FULL_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UART_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_ADV_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_AL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BANK_ADDR_COUNT.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_0_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_1_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_2_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_3_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CLOCK_STOP_EN.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_COL_ADDR_COUNT.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_CWL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_0_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_1_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_2_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_3_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_1.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_2.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_3.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_0_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_1_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_2_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_LENGTH_MM.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_PACKAGE_LENGTH.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_DQ_3_PROPOGATION_DELAY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_ECC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_FREQ_MHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_HIGH_TEMP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_MEMORY_TYPE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_SPEED_BIN.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_DATA_EYE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_READ_GATE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_TRAIN_WRITE_LEVEL.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_T_RAS_MIN.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_T_RCD.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_T_RP.VALUE_SRC {DEFAULT} \
CONFIG.PCW_UIPARAM_DDR_USE_INTERNAL_VREF.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB0_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB1_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB1_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB1_RESET_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB1_RESET_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB1_USB1_IO.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USB_RESET_POLARITY.VALUE_SRC {DEFAULT} \
CONFIG.PCW_USE_CROSS_TRIGGER.VALUE_SRC {DEFAULT} \
CONFIG.PCW_WDT_PERIPHERAL_CLKSRC.VALUE_SRC {DEFAULT} \
CONFIG.PCW_WDT_PERIPHERAL_DIVISOR0.VALUE_SRC {DEFAULT} \
CONFIG.PCW_WDT_PERIPHERAL_ENABLE.VALUE_SRC {DEFAULT} \
CONFIG.PCW_WDT_PERIPHERAL_FREQMHZ.VALUE_SRC {DEFAULT} \
CONFIG.PCW_WDT_WDT_IO.VALUE_SRC {DEFAULT} \
 ] $ps_0

  # Create instance: slice_from13_to0_doutb, and set properties
  set slice_from13_to0_doutb [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from13_to0_doutb ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {32} \
CONFIG.DOUT_WIDTH {14} \
 ] $slice_from13_to0_doutb

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from13_to0_doutb

  # Create instance: slice_from29_to16_doutb, and set properties
  set slice_from29_to16_doutb [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_from29_to16_doutb ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DIN_WIDTH {32} \
CONFIG.DOUT_WIDTH {14} \
 ] $slice_from29_to16_doutb

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.DOUT_WIDTH.VALUE_SRC {DEFAULT} \
 ] $slice_from29_to16_doutb

  # Create instance: sts
  create_hier_cell_sts [current_bd_instance .] sts

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]

  # Create instance: util_vector_logic_1, and set properties
  set util_vector_logic_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_1 ]
  set_property -dict [ list \
CONFIG.C_OPERATION {or} \
CONFIG.C_SIZE {1} \
CONFIG.LOGO_FILE {data/sym_orgate.png} \
 ] $util_vector_logic_1

  # Create instance: xlconcat_0, and set properties
  set xlconcat_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0 ]

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconcat_3, and set properties
  set xlconcat_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_3 ]
  set_property -dict [ list \
CONFIG.NUM_PORTS {5} \
 ] $xlconcat_3

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {28} \
 ] $xlconstant_0

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {15} \
CONFIG.DIN_TO {8} \
CONFIG.DIN_WIDTH {16} \
CONFIG.DOUT_WIDTH {8} \
 ] $xlslice_0

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {0} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {32} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_2

  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_pins axi_mem_intercon_0/S00_AXI] [get_bd_intf_pins ps_0/M_AXI_GP0]
  connect_bd_intf_net -intf_net adc_clock_converter_M_AXIS [get_bd_intf_pins adc_axis_fifo/AXI_STR_RXD] [get_bd_intf_pins adc_clock_converter/M_AXIS]
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins axi_bram_ctrl_dac/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]
  connect_bd_intf_net -intf_net axi_mem_intercon_0_M00_AXI [get_bd_intf_pins axi_mem_intercon_0/M00_AXI] [get_bd_intf_pins cfg/S_AXI]
  connect_bd_intf_net -intf_net axi_mem_intercon_0_M01_AXI [get_bd_intf_pins axi_mem_intercon_0/M01_AXI] [get_bd_intf_pins sts/S_AXI]
  connect_bd_intf_net -intf_net axi_mem_intercon_0_M02_AXI [get_bd_intf_pins axi_bram_ctrl_dac/S_AXI] [get_bd_intf_pins axi_mem_intercon_0/M02_AXI]
  connect_bd_intf_net -intf_net axi_mem_intercon_0_M03_AXI [get_bd_intf_pins adc_axis_fifo/S_AXI] [get_bd_intf_pins axi_mem_intercon_0/M03_AXI]
  connect_bd_intf_net -intf_net ps_0_DDR [get_bd_intf_ports DDR] [get_bd_intf_pins ps_0/DDR]
  connect_bd_intf_net -intf_net ps_0_FIXED_IO [get_bd_intf_ports FIXED_IO] [get_bd_intf_pins ps_0/FIXED_IO]

  # Create port connections
  connect_bd_net -net ARESETN_1 [get_bd_pins axi_mem_intercon_0/ARESETN] [get_bd_pins proc_sys_reset_0/interconnect_aresetn]
  connect_bd_net -net Acquisition_Control_Acq_Valid [get_bd_pins Acquisition_Control/Acq_Valid] [get_bd_pins util_vector_logic_0/Op1] [get_bd_pins xlconcat_3/In2]
  connect_bd_net -net Acquisition_Control_MHz_clk [get_bd_pins Acquisition_Control/MHz_clk] [get_bd_pins Averager_0/ce] [get_bd_pins Averager_1/ce] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net ArmState_Dout [get_bd_pins ArmState/Dout] [get_bd_pins Set_Reset_State/ResetState] [get_bd_pins xlconcat_3/In0]
  connect_bd_net -net Averager_0_AverageVal [get_bd_pins Averager_0/AverageVal] [get_bd_pins xlconcat_0/In0]
  connect_bd_net -net Averager_1_AverageVal [get_bd_pins Averager_1/AverageVal] [get_bd_pins xlconcat_0/In1]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins RandomPulser/Pulse] [get_bd_pins TriggeredCounter/Trig_Count] [get_bd_pins c_counter_binary_0/CE]
  connect_bd_net -net RandomPulser_RandRst [get_bd_pins RandomPulser/RandValue] [get_bd_pins sts/device_version]
  connect_bd_net -net S00_ARESETN_1 [get_bd_pins adc_axis_fifo/s_axi_aresetn] [get_bd_pins adc_clock_converter/m_axis_aresetn] [get_bd_pins axi_bram_ctrl_dac/s_axi_aresetn] [get_bd_pins axi_mem_intercon_0/M00_ARESETN] [get_bd_pins axi_mem_intercon_0/M01_ARESETN] [get_bd_pins axi_mem_intercon_0/M02_ARESETN] [get_bd_pins axi_mem_intercon_0/M03_ARESETN] [get_bd_pins axi_mem_intercon_0/S00_ARESETN] [get_bd_pins cfg/s_axi_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins sts/s_axi_aresetn]
  connect_bd_net -net Set_Reset_State_State [get_bd_pins Set_Reset_State/State] [get_bd_pins xlconcat_3/In3]
  connect_bd_net -net TrigState_Dout [get_bd_pins TrigState/Dout] [get_bd_pins util_vector_logic_1/Op1]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_clk_n_i_1 [get_bd_ports adc_clk_n_i] [get_bd_pins adc_dac/clk_in1_n]
  connect_bd_net -net adc_clk_p_i_1 [get_bd_ports adc_clk_p_i] [get_bd_pins adc_dac/clk_in1_p]
  connect_bd_net -net adc_dac_adc1 [get_bd_pins Averager_0/B] [get_bd_pins adc_dac/adc1]
  connect_bd_net -net adc_dac_adc2 [get_bd_pins Averager_1/B] [get_bd_pins adc_dac/adc2]
  connect_bd_net -net adc_dac_adc_cdcs_o [get_bd_ports adc_cdcs_o] [get_bd_pins adc_dac/adc_cdcs_o]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins Acquisition_Control/clk] [get_bd_pins Averager_0/clk] [get_bd_pins Averager_1/clk] [get_bd_pins RandomPulser/clk] [get_bd_pins Set_Reset_State/clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins adc_clock_converter/s_axis_aclk] [get_bd_pins adc_dac/adc_clk] [get_bd_pins blk_mem_gen_dac/clkb] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins cfg/m_axi_aclk] [get_bd_pins delay1_0/clk] [get_bd_pins proc_sys_reset_adc_clk/slowest_sync_clk] [get_bd_pins sts/m_axi_aclk]
  connect_bd_net -net adc_dac_adc_clk_source [get_bd_ports adc_clk_source] [get_bd_pins adc_dac/adc_clk_source]
  connect_bd_net -net adc_dac_dac_clk_o [get_bd_ports dac_clk_o] [get_bd_pins adc_dac/dac_clk_o]
  connect_bd_net -net adc_dac_dac_dat_o [get_bd_ports dac_dat_o] [get_bd_pins adc_dac/dac_dat_o]
  connect_bd_net -net adc_dac_dac_rst_o [get_bd_ports dac_rst_o] [get_bd_pins adc_dac/dac_rst_o]
  connect_bd_net -net adc_dac_dac_sel_o [get_bd_ports dac_sel_o] [get_bd_pins adc_dac/dac_sel_o]
  connect_bd_net -net adc_dac_dac_wrt_o [get_bd_ports dac_wrt_o] [get_bd_pins adc_dac/dac_wrt_o]
  connect_bd_net -net adc_dat_a_i_1 [get_bd_ports adc_dat_a_i] [get_bd_pins adc_dac/adc_dat_a_i]
  connect_bd_net -net adc_dat_b_i_1 [get_bd_ports adc_dat_b_i] [get_bd_pins adc_dac/adc_dat_b_i]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins blk_mem_gen_dac/doutb] [get_bd_pins slice_from13_to0_doutb/Din] [get_bd_pins slice_from29_to16_doutb/Din]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins c_counter_binary_0/Q] [get_bd_pins xlslice_0/Din]
  connect_bd_net -net cfg_acquisitionlength [get_bd_pins Acquisition_Control/Acquistion_length_us] [get_bd_pins cfg/acquisitionlength]
  connect_bd_net -net cfg_arm_softtrig [get_bd_pins ArmState/Din] [get_bd_pins TrigState/Din] [get_bd_pins cfg/arm_softtrig]
  connect_bd_net -net cfg_operationmode [get_bd_pins cfg/operationmode] [get_bd_pins xlslice_2/Din]
  connect_bd_net -net cfg_simulationpulsefreq [get_bd_pins RandomPulser/b] [get_bd_pins cfg/simulationpulsefreq]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net dac1_1 [get_bd_pins adc_dac/dac1] [get_bd_pins slice_from13_to0_doutb/Dout]
  connect_bd_net -net dac2_1 [get_bd_pins adc_dac/dac2] [get_bd_pins slice_from29_to16_doutb/Dout]
  connect_bd_net -net delay1_0_dout [get_bd_pins delay1_0/dout] [get_bd_pins util_vector_logic_1/Op2]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_aresetn [get_bd_pins adc_clock_converter/s_axis_aresetn] [get_bd_pins cfg/m_axi_aresetn] [get_bd_pins proc_sys_reset_adc_clk/peripheral_aresetn] [get_bd_pins sts/m_axi_aresetn]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins RandomPulser/Reset_In] [get_bd_pins blk_mem_gen_dac/rstb] [get_bd_pins proc_sys_reset_adc_clk/peripheral_reset]
  connect_bd_net -net ps_0_FCLK_CLK0 [get_bd_pins adc_axis_fifo/s_axi_aclk] [get_bd_pins adc_clock_converter/m_axis_aclk] [get_bd_pins axi_bram_ctrl_dac/s_axi_aclk] [get_bd_pins axi_mem_intercon_0/ACLK] [get_bd_pins axi_mem_intercon_0/M00_ACLK] [get_bd_pins axi_mem_intercon_0/M01_ACLK] [get_bd_pins axi_mem_intercon_0/M02_ACLK] [get_bd_pins axi_mem_intercon_0/M03_ACLK] [get_bd_pins axi_mem_intercon_0/S00_ACLK] [get_bd_pins cfg/s_axi_aclk] [get_bd_pins proc_sys_reset_0/slowest_sync_clk] [get_bd_pins ps_0/FCLK_CLK0] [get_bd_pins ps_0/M_AXI_GP0_ACLK] [get_bd_pins sts/s_axi_aclk]
  connect_bd_net -net ps_0_FCLK_RESET0_N [get_bd_pins proc_sys_reset_0/ext_reset_in] [get_bd_pins proc_sys_reset_adc_clk/ext_reset_in] [get_bd_pins ps_0/FCLK_RESET0_N]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins adc_clock_converter/s_axis_tvalid] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net util_vector_logic_1_Res [get_bd_pins Acquisition_Control/trig] [get_bd_pins Set_Reset_State/SetState] [get_bd_pins util_vector_logic_1/Res] [get_bd_pins xlconcat_3/In1]
  connect_bd_net -net xlconcat_0_dout [get_bd_pins adc_clock_converter/s_axis_tdata] [get_bd_pins xlconcat_0/dout]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins Counter_Monitor/In0] [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconcat_3_dout [get_bd_pins sts/state] [get_bd_pins xlconcat_3/dout]
  connect_bd_net -net xlconcat_4_dout [get_bd_pins Counter_Monitor/dout] [get_bd_pins sts/status]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins xlconcat_3/In4] [get_bd_pins xlconstant_0/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]
  connect_bd_net -net xlconstant_2_dout [get_bd_pins WillBeInputTigger/dout] [get_bd_pins delay1_0/din]
  connect_bd_net -net xlslice_0_Dout [get_bd_ports led_o] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins xlslice_2/Dout]

  # Create address segments
  create_bd_addr_seg -range 0x00010000 -offset 0x43C10000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0] SEG_adc_axis_fifo_Mem0
  create_bd_addr_seg -range 0x00008000 -offset 0x40000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs axi_bram_ctrl_dac/S_AXI/Mem0] SEG_axi_bram_ctrl_dac_Mem0
  create_bd_addr_seg -range 0x00001000 -offset 0x60000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs cfg/axi_cfg_register_0/s_axi/reg0] SEG_axi_cfg_register_0_reg0
  create_bd_addr_seg -range 0x00001000 -offset 0x50000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs sts/axi_sts_register_0/s_axi/reg0] SEG_axi_sts_register_0_reg0

  # Perform GUI Layout
  regenerate_bd_layout -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port DDR -pg 1 -y 50 -defaultsOSRD
preplace port Vp_Vn -pg 1 -y 100 -defaultsOSRD
preplace port Vaux0 -pg 1 -y 20 -defaultsOSRD
preplace port Vaux1 -pg 1 -y 40 -defaultsOSRD
preplace port adc_clk_p_i -pg 1 -y 750 -defaultsOSRD
preplace port dac_rst_o -pg 1 -y 650 -defaultsOSRD
preplace port dac_clk_o -pg 1 -y 610 -defaultsOSRD
preplace port FIXED_IO -pg 1 -y 70 -defaultsOSRD
preplace port dac_sel_o -pg 1 -y 670 -defaultsOSRD
preplace port adc_cdcs_o -pg 1 -y 550 -defaultsOSRD
preplace port dac_wrt_o -pg 1 -y 690 -defaultsOSRD
preplace port Vaux8 -pg 1 -y 60 -defaultsOSRD
preplace port adc_clk_n_i -pg 1 -y 720 -defaultsOSRD
preplace port Vaux9 -pg 1 -y 80 -defaultsOSRD
preplace portBus adc_dat_b_i -pg 1 -y 1380 -defaultsOSRD
preplace portBus adc_clk_source -pg 1 -y 590 -defaultsOSRD
preplace portBus led_o -pg 1 -y 890 -defaultsOSRD
preplace portBus dac_pwm_o -pg 1 -y 20 -defaultsOSRD
preplace portBus adc_dat_a_i -pg 1 -y 410 -defaultsOSRD
preplace portBus dac_dat_o -pg 1 -y 630 -defaultsOSRD
preplace inst sts -pg 1 -lvl 8 -y 720 -defaultsOSRD
preplace inst slice_from13_to0_doutb -pg 1 -lvl 11 -y 650 -defaultsOSRD
preplace inst const_v0_w5 -pg 1 -lvl 8 -y 1230 -defaultsOSRD
preplace inst xlslice_0 -pg 1 -lvl 12 -y 890 -defaultsOSRD
preplace inst delay1_0 -pg 1 -lvl 2 -y 1150 -defaultsOSRD
preplace inst axi_mem_intercon_0 -pg 1 -lvl 7 -y 310 -defaultsOSRD
preplace inst xlconstant_0 -pg 1 -lvl 6 -y 1160 -defaultsOSRD
preplace inst slice_from29_to16_doutb -pg 1 -lvl 11 -y 740 -defaultsOSRD
preplace inst blk_mem_gen_dac -pg 1 -lvl 9 -y 1130 -defaultsOSRD
preplace inst Averager_0 -pg 1 -lvl 5 -y 490 -defaultsOSRD
preplace inst xlconstant_1 -pg 1 -lvl 10 -y 1300 -defaultsOSRD
preplace inst xlslice_2 -pg 1 -lvl 12 -y 1040 -defaultsOSRD
preplace inst Counter_Monitor -pg 1 -lvl 12 -y 1310 -defaultsOSRD
preplace inst Averager_1 -pg 1 -lvl 5 -y 640 -defaultsOSRD
preplace inst TrigState -pg 1 -lvl 2 -y 1040 -defaultsOSRD
preplace inst xlconcat_0 -pg 1 -lvl 6 -y 570 -defaultsOSRD
preplace inst util_vector_logic_0 -pg 1 -lvl 6 -y 690 -defaultsOSRD
preplace inst proc_sys_reset_adc_clk -pg 1 -lvl 6 -y 850 -defaultsOSRD
preplace inst proc_sys_reset_0 -pg 1 -lvl 6 -y 400 -defaultsOSRD
preplace inst adc_clock_converter -pg 1 -lvl 7 -y 650 -defaultsOSRD
preplace inst xlconcat_1 -pg 1 -lvl 11 -y 1310 -defaultsOSRD
preplace inst util_vector_logic_1 -pg 1 -lvl 3 -y 1050 -defaultsOSRD
preplace inst c_counter_binary_0 -pg 1 -lvl 11 -y 890 -defaultsOSRD
preplace inst TriggeredCounter -pg 1 -lvl 10 -y 980 -defaultsOSRD
preplace inst cfg -pg 1 -lvl 8 -y 990 -defaultsOSRD
preplace inst adc_axis_fifo -pg 1 -lvl 8 -y 350 -defaultsOSRD
preplace inst Set_Reset_State -pg 1 -lvl 6 -y 1020 -defaultsOSRD
preplace inst xlconcat_3 -pg 1 -lvl 7 -y 1120 -defaultsOSRD
preplace inst const_v0_w32 -pg 1 -lvl 8 -y 1140 -defaultsOSRD
preplace inst axi_bram_ctrl_dac -pg 1 -lvl 8 -y 520 -defaultsOSRD
preplace inst adc_dac -pg 1 -lvl 12 -y 620 -defaultsOSRD
preplace inst WillBeInputTigger -pg 1 -lvl 1 -y 1160 -defaultsOSRD
preplace inst RandomPulser -pg 1 -lvl 9 -y 910 -defaultsOSRD
preplace inst ArmState -pg 1 -lvl 5 -y 1050 -defaultsOSRD
preplace inst ps_0 -pg 1 -lvl 6 -y 150 -defaultsOSRD
preplace inst const_v0_w4 -pg 1 -lvl 8 -y 1320 -defaultsOSRD
preplace inst Acquisition_Control -pg 1 -lvl 4 -y 1030 -defaultsOSRD
preplace netloc axi_bram_ctrl_dac_BRAM_PORTA 1 8 1 2540
preplace netloc xlconstant_1_dout 1 10 1 NJ
preplace netloc adc_dac_dac_wrt_o 1 12 1 NJ
preplace netloc xlconstant_2_dout 1 1 1 NJ
preplace netloc adc_dac_dac_clk_o 1 12 1 NJ
preplace netloc adc_dac_adc_clk_source 1 12 1 NJ
preplace netloc Averager_0_AverageVal 1 5 1 1240
preplace netloc adc_dac_adc1 1 4 9 990 400 NJ 490 NJ 490 NJ 430 NJ 430 NJ 430 NJ 430 NJ 430 3620
preplace netloc RandomPulser_Pulse 1 9 2 2860 900 NJ
preplace netloc axi_mem_intercon_0_M02_AXI 1 7 1 2030
preplace netloc axi_mem_intercon_0_M00_AXI 1 7 1 2080
preplace netloc proc_sys_reset_adc_clk_peripheral_reset 1 6 3 NJ 850 NJ 850 2530
preplace netloc dac2_1 1 11 1 NJ
preplace netloc adc_dac_adc2 1 4 9 1010 560 NJ 510 NJ 510 NJ 450 NJ 450 NJ 450 NJ 450 NJ 450 3610
preplace netloc ArmState_Dout 1 5 2 1280 1090 NJ
preplace netloc xlconcat_1_dout 1 8 4 2570 1370 NJ 1370 NJ 1370 3290
preplace netloc util_vector_logic_0_Res 1 6 1 1690
preplace netloc cfg_arm_softtrig 1 1 8 210 1090 NJ 1110 NJ 1110 1000 1210 NJ 1210 NJ 1210 NJ 1090 2500
preplace netloc Set_Reset_State_State 1 6 1 1690
preplace netloc Averager_1_AverageVal 1 5 1 1230
preplace netloc ARESETN_1 1 6 1 1690
preplace netloc delay1_0_dout 1 2 1 420
preplace netloc const_v0_w5_dout 1 8 1 NJ
preplace netloc blk_mem_gen_dac_doutb 1 8 3 2550 980 NJ 910 3080
preplace netloc adc_dat_a_i_1 1 0 12 NJ 410 NJ 410 NJ 410 NJ 410 NJ 410 NJ 500 NJ 500 NJ 440 NJ 440 NJ 440 NJ 440 NJ
preplace netloc adc_dac_adc_clk 1 1 12 200 990 NJ 990 660 950 990 710 1230 760 1720 760 2120 870 2520 990 2850 880 3090 830 NJ 830 3610
preplace netloc adc_dac_adc_cdcs_o 1 12 1 NJ
preplace netloc adc_clk_n_i_1 1 0 12 NJ 720 NJ 720 NJ 720 NJ 720 NJ 720 NJ 630 NJ 540 NJ 590 NJ 590 NJ 590 NJ 590 NJ
preplace netloc Acquisition_Control_MHz_clk 1 4 2 1000 740 NJ
preplace netloc xlconcat_3_dout 1 7 1 2110
preplace netloc dac1_1 1 11 1 NJ
preplace netloc adc_dac_dac_dat_o 1 12 1 NJ
preplace netloc xlconstant_0_dout 1 6 1 NJ
preplace netloc xlconcat_0_dout 1 6 1 1700
preplace netloc ps_0_FCLK_CLK0 1 5 3 1270 300 1710 520 2090
preplace netloc c_counter_binary_0_Q 1 11 1 NJ
preplace netloc S00_AXI_1 1 6 1 1710
preplace netloc xlslice_2_Dout 1 12 1 N
preplace netloc Acquisition_Control_Acq_Valid 1 4 3 1010 730 1260 1110 NJ
preplace netloc ps_0_DDR 1 6 7 NJ 50 NJ 50 NJ 50 NJ 50 NJ 50 NJ 50 NJ
preplace netloc adc_clock_converter_M_AXIS 1 7 1 2050
preplace netloc TriggeredCounter_Count 1 10 1 3090
preplace netloc cfg_simulationpulsefreq 1 8 1 2510
preplace netloc adc_dac_dac_rst_o 1 12 1 NJ
preplace netloc util_vector_logic_1_Res 1 3 4 670 1100 NJ 1100 1270 1100 NJ
preplace netloc ps_0_FCLK_RESET0_N 1 5 2 1280 310 1680
preplace netloc proc_sys_reset_adc_clk_peripheral_aresetn 1 6 2 1700 770 2060
preplace netloc const_v0_w32_dout 1 8 1 NJ
preplace netloc adc_dat_b_i_1 1 0 12 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ 1380 NJ
preplace netloc TrigState_Dout 1 2 1 NJ
preplace netloc ps_0_FIXED_IO 1 6 7 NJ 70 NJ 70 NJ 70 NJ 70 NJ 70 NJ 70 NJ
preplace netloc axi_mem_intercon_0_M03_AXI 1 7 1 2040
preplace netloc xlconcat_4_dout 1 7 6 2140 1370 NJ 1260 NJ 1250 NJ 1250 NJ 1250 3610
preplace netloc cfg_operationmode 1 8 4 NJ 1000 NJ 1040 NJ 1040 NJ
preplace netloc axi_mem_intercon_0_M01_AXI 1 7 1 2130
preplace netloc xlslice_0_Dout 1 12 1 NJ
preplace netloc const_v0_w4_dout 1 8 1 NJ
preplace netloc cfg_acquisitionlength 1 3 6 670 940 NJ 940 NJ 940 NJ 880 NJ 880 2500
preplace netloc adc_dac_dac_sel_o 1 12 1 NJ
preplace netloc adc_clk_p_i_1 1 0 12 NJ 750 NJ 750 NJ 750 NJ 750 NJ 750 NJ 750 NJ 780 NJ 840 NJ 820 NJ 820 NJ 820 NJ
preplace netloc S00_ARESETN_1 1 6 2 1720 530 2100
preplace netloc RandomPulser_RandRst 1 7 3 2130 860 NJ 830 2830
levelinfo -pg 1 0 110 310 540 830 1120 1480 1870 2320 2700 2970 3190 3460 3640 -top 0 -bot 1400
",
}

  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


