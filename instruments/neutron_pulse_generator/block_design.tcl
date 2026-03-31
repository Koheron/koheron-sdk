
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


# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_7() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_7() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_6() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_6() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_5() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_5() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_4() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_4() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_3() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_3() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_2() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_2() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1_1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1_1() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve_1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve_1() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve1
proc create_hier_cell_ADD_Halve1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve1() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve
proc create_hier_cell_ADD_Halve { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_15 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_15() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_14 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_14() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_13 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_13() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_12 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_12() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_11 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_11() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_2

  # Create port connections
  connect_bd_net -net Trig_Count_1 [get_bd_pins Trig_Count] [get_bd_pins latch_0/reset]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins c_counter_binary_0/Q] [get_bd_pins xlslice_1/Din] [get_bd_pins xlslice_2/Din]
  connect_bd_net -net clk_1 [get_bd_pins clk] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins latch_0/clk]
  connect_bd_net -net latch_0_q [get_bd_pins c_counter_binary_0/SCLR] [get_bd_pins latch_0/q]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins Count] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins latch_0/set] [get_bd_pins xlslice_2/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_10 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_10() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_9 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_9() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_8 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_8() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_7() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_6() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_5() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_4() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_3() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_2() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: TriggeredCounter
proc create_hier_cell_TriggeredCounter_1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_TriggeredCounter_1() - Empty argument(s)!"}
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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
  create_bd_pin -dir O -from 9 -to 0 Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Output_Width {11} \
CONFIG.SCLR {true} \
 ] $c_counter_binary_0

  # Create instance: latch_0, and set properties
  set latch_0 [ create_bd_cell -type ip -vlnv CCFE:user:latch:1.0 latch_0 ]

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {10} \
CONFIG.DIN_TO {10} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {1} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {9} \
CONFIG.DIN_TO {0} \
CONFIG.DIN_WIDTH {11} \
CONFIG.DOUT_WIDTH {10} \
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

# Hierarchical cell: RoundRobinPulser15
proc create_hier_cell_RoundRobinPulser15 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser15() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {15} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser14
proc create_hier_cell_RoundRobinPulser14 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser14() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {14} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser13
proc create_hier_cell_RoundRobinPulser13 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser13() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {13} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser12
proc create_hier_cell_RoundRobinPulser12 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser12() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {12} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser11
proc create_hier_cell_RoundRobinPulser11 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser11() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {11} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser10
proc create_hier_cell_RoundRobinPulser10 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser10() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {10} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser9
proc create_hier_cell_RoundRobinPulser9 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser9() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {9} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser8
proc create_hier_cell_RoundRobinPulser8 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser8() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {8} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser7
proc create_hier_cell_RoundRobinPulser7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser7() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {7} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser6
proc create_hier_cell_RoundRobinPulser6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser6() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {6} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser5
proc create_hier_cell_RoundRobinPulser5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser5() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {5} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser4
proc create_hier_cell_RoundRobinPulser4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser4() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {4} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser3
proc create_hier_cell_RoundRobinPulser3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser3() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {3} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser2
proc create_hier_cell_RoundRobinPulser2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser2() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {2} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser1
proc create_hier_cell_RoundRobinPulser1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser1() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {1} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: RoundRobinPulser
proc create_hier_cell_RoundRobinPulser { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RoundRobinPulser() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 3 -to 0 a
  create_bd_pin -dir I -from 0 -to 0 din
  create_bd_pin -dir O -from 0 -to 0 dout

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {4} \
 ] $comparator_0

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
CONFIG.C_SIZE {1} \
 ] $util_vector_logic_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {4} \
 ] $xlconstant_0

  # Create port connections
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins a] [get_bd_pins comparator_0/a]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net din_1 [get_bd_pins din] [get_bd_pins util_vector_logic_0/Op2]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins dout] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins comparator_0/b] [get_bd_pins xlconstant_0/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Pulser1
proc create_hier_cell_Pulser1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Pulser1() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 31 -to 0 -type data A
  create_bd_pin -dir I -from 31 -to 0 b
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir O pulse

  # Create instance: comparator_0, and set properties
  set comparator_0 [ create_bd_cell -type ip -vlnv koheron:user:comparator:1.0 comparator_0 ]
  set_property -dict [ list \
CONFIG.DATA_WIDTH {32} \
CONFIG.OPERATION {LT} \
 ] $comparator_0

  # Create instance: one_clock_pulse_0, and set properties
  set one_clock_pulse_0 [ create_bd_cell -type ip -vlnv CCFE:user:one_clock_pulse:1.0 one_clock_pulse_0 ]

  # Create port connections
  connect_bd_net -net A_1 [get_bd_pins A] [get_bd_pins comparator_0/a]
  connect_bd_net -net b_1 [get_bd_pins b] [get_bd_pins comparator_0/b]
  connect_bd_net -net clk_1 [get_bd_pins clk] [get_bd_pins one_clock_pulse_0/clk]
  connect_bd_net -net comparator_0_dout [get_bd_pins comparator_0/dout] [get_bd_pins one_clock_pulse_0/trig]
  connect_bd_net -net one_clock_pulse_0_pulse [get_bd_pins pulse] [get_bd_pins one_clock_pulse_0/pulse]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder7
proc create_hier_cell_Twox2ChannelAdder7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder7() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_7 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_7 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder6
proc create_hier_cell_Twox2ChannelAdder6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder6() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_6 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_6 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder5
proc create_hier_cell_Twox2ChannelAdder5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder5() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_5 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_5 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder4
proc create_hier_cell_Twox2ChannelAdder4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder4() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_4 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_4 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder3
proc create_hier_cell_Twox2ChannelAdder3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder3() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_3 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_3 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder2
proc create_hier_cell_Twox2ChannelAdder2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder2() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_2 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_2 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder1
proc create_hier_cell_Twox2ChannelAdder1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder1() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve_1 $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1_1 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: Twox2ChannelAdder
proc create_hier_cell_Twox2ChannelAdder { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_Twox2ChannelAdder() - Empty argument(s)!"}
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
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Ch1
  create_bd_pin -dir O -from 13 -to 0 Ch2
  create_bd_pin -dir I -from 31 -to 0 Din
  create_bd_pin -dir I -from 31 -to 0 Din1

  # Create instance: ADD_Halve
  create_hier_cell_ADD_Halve $hier_obj ADD_Halve

  # Create instance: ADD_Halve1
  create_hier_cell_ADD_Halve1 $hier_obj ADD_Halve1

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_0

  # Create instance: xlslice_1, and set properties
  set xlslice_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_1

  # Create instance: xlslice_2, and set properties
  set xlslice_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_2 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {29} \
CONFIG.DIN_TO {16} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_2

  # Create instance: xlslice_3, and set properties
  set xlslice_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_3 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {13} \
CONFIG.DIN_TO {0} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_3

  # Create port connections
  connect_bd_net -net ADD_Halve1_Dout [get_bd_pins Ch1] [get_bd_pins ADD_Halve1/Dout]
  connect_bd_net -net ADD_Halve_Dout [get_bd_pins Ch2] [get_bd_pins ADD_Halve/Dout]
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins ADD_Halve/CLK] [get_bd_pins ADD_Halve1/CLK]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins Din] [get_bd_pins xlslice_0/Din] [get_bd_pins xlslice_1/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins Din1] [get_bd_pins xlslice_2/Din] [get_bd_pins xlslice_3/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins ADD_Halve/A] [get_bd_pins xlslice_0/Dout]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins ADD_Halve1/A] [get_bd_pins xlslice_1/Dout]
  connect_bd_net -net xlslice_2_Dout [get_bd_pins ADD_Halve/B] [get_bd_pins xlslice_2/Dout]
  connect_bd_net -net xlslice_3_Dout [get_bd_pins ADD_Halve1/B] [get_bd_pins xlslice_3/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/Twox2ChannelAdder] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus Ch1 -pg 1 -y 210 -defaultsOSRD
preplace portBus Ch2 -pg 1 -y 50 -defaultsOSRD
preplace portBus Din -pg 1 -y 40 -defaultsOSRD
preplace portBus Din1 -pg 1 -y 120 -defaultsOSRD
preplace inst ADD_Halve1 -pg 1 -lvl 2 -y 220 -defaultsOSRD
preplace inst xlslice_0 -pg 1 -lvl 1 -y 10 -defaultsOSRD
preplace inst ADD_Halve -pg 1 -lvl 2 -y 70 -defaultsOSRD
preplace inst xlslice_1 -pg 1 -lvl 1 -y 200 -defaultsOSRD
preplace inst xlslice_2 -pg 1 -lvl 1 -y 110 -defaultsOSRD
preplace inst xlslice_3 -pg 1 -lvl 1 -y 290 -defaultsOSRD
preplace netloc CLK_1 1 0 2 NJ 60 230
preplace netloc xlslice_3_Dout 1 1 1 NJ
preplace netloc xlslice_1_Dout 1 1 1 NJ
preplace netloc PulseFormer10_ShapedPulse 1 0 1 20
preplace netloc ADD_Halve_Dout 1 2 1 420
preplace netloc xlslice_2_Dout 1 1 1 NJ
preplace netloc ADD_Halve1_Dout 1 2 1 420
preplace netloc PulseFormer11_ShapedPulse 1 0 1 30
preplace netloc xlslice_0_Dout 1 1 1 NJ
levelinfo -pg 1 -10 130 330 440 -top -40 -bot 340
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer15
proc create_hier_cell_PulseFormer15 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer15() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_15 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer14
proc create_hier_cell_PulseFormer14 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer14() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_14 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer13
proc create_hier_cell_PulseFormer13 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer13() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_13 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer12
proc create_hier_cell_PulseFormer12 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer12() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_12 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer11
proc create_hier_cell_PulseFormer11 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer11() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_11 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer10
proc create_hier_cell_PulseFormer10 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer10() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_10 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer9
proc create_hier_cell_PulseFormer9 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer9() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_9 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer8
proc create_hier_cell_PulseFormer8 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer8() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_8 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer7
proc create_hier_cell_PulseFormer7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer7() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_7 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer6
proc create_hier_cell_PulseFormer6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer6() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_6 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer5
proc create_hier_cell_PulseFormer5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer5() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_5 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer4
proc create_hier_cell_PulseFormer4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer4() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_4 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer3
proc create_hier_cell_PulseFormer3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer3() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_3 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer2
proc create_hier_cell_PulseFormer2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer2() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_2 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer1
proc create_hier_cell_PulseFormer1 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer1() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter_1 $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: PulseFormer
proc create_hier_cell_PulseFormer { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_PulseFormer() - Empty argument(s)!"}
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
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 BRAM_PORTA

  # Create pins
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 31 -to 0 ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: TriggeredCounter
  create_hier_cell_TriggeredCounter $hier_obj TriggeredCounter

  # Create instance: blk_mem_gen_dac, and set properties
  set blk_mem_gen_dac [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_dac ]
  set_property -dict [ list \
CONFIG.Byte_Size {8} \
CONFIG.Enable_32bit_Address {true} \
CONFIG.Memory_Type {True_Dual_Port_RAM} \
CONFIG.Register_PortA_Output_of_Memory_Primitives {false} \
CONFIG.Register_PortB_Output_of_Memory_Primitives {false} \
CONFIG.Use_Byte_Write_Enable {true} \
CONFIG.Use_RSTA_Pin {true} \
CONFIG.Use_RSTB_Pin {true} \
CONFIG.Write_Depth_A {1024} \
CONFIG.use_bram_block {BRAM_Controller} \
 ] $blk_mem_gen_dac

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

  # Create instance: xlconcat_1, and set properties
  set xlconcat_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_1 ]

  # Create instance: xlconstant_1, and set properties
  set xlconstant_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_1 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {2} \
 ] $xlconstant_1

  # Create interface connections
  connect_bd_intf_net -intf_net axi_bram_ctrl_dac_BRAM_PORTA [get_bd_intf_pins BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_dac/BRAM_PORTA]

  # Create port connections
  connect_bd_net -net BRAM_PORTA_addr_2 [get_bd_pins BRAM_PORTA_addr] [get_bd_pins blk_mem_gen_dac/addra]
  connect_bd_net -net BRAM_PORTA_clk_2 [get_bd_pins BRAM_PORTA_clk] [get_bd_pins blk_mem_gen_dac/clka]
  connect_bd_net -net BRAM_PORTA_din_2 [get_bd_pins BRAM_PORTA_din] [get_bd_pins blk_mem_gen_dac/dina]
  connect_bd_net -net BRAM_PORTA_en_2 [get_bd_pins BRAM_PORTA_en] [get_bd_pins blk_mem_gen_dac/ena]
  connect_bd_net -net BRAM_PORTA_rst_2 [get_bd_pins BRAM_PORTA_rst] [get_bd_pins blk_mem_gen_dac/rsta]
  connect_bd_net -net BRAM_PORTA_we_2 [get_bd_pins BRAM_PORTA_we] [get_bd_pins blk_mem_gen_dac/wea]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins TriggeredCounter/Trig_Count]
  connect_bd_net -net TriggeredCounter_Count [get_bd_pins TriggeredCounter/Count] [get_bd_pins xlconcat_1/In1]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins TriggeredCounter/clk] [get_bd_pins blk_mem_gen_dac/clkb]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins ShapedPulse] [get_bd_pins blk_mem_gen_dac/doutb]
  connect_bd_net -net const_v0_w32_dout [get_bd_pins blk_mem_gen_dac/dinb] [get_bd_pins const_v0_w32/dout]
  connect_bd_net -net const_v0_w4_dout [get_bd_pins blk_mem_gen_dac/web] [get_bd_pins const_v0_w4/dout]
  connect_bd_net -net const_v0_w5_dout [get_bd_pins blk_mem_gen_dac/enb] [get_bd_pins const_v0_w5/dout]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins blk_mem_gen_dac/rstb]
  connect_bd_net -net xlconcat_1_dout [get_bd_pins blk_mem_gen_dac/addrb] [get_bd_pins xlconcat_1/dout]
  connect_bd_net -net xlconstant_1_dout [get_bd_pins xlconcat_1/In0] [get_bd_pins xlconstant_1/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve15
proc create_hier_cell_ADD_Halve15 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve15() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve15] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve14
proc create_hier_cell_ADD_Halve14 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve14() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve14] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve13
proc create_hier_cell_ADD_Halve13 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve13() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve13] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve12
proc create_hier_cell_ADD_Halve12 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve12() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve12] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve11
proc create_hier_cell_ADD_Halve11 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve11() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve11] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve10
proc create_hier_cell_ADD_Halve10 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve10() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve10] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve9
proc create_hier_cell_ADD_Halve9 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve9() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve9] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve8
proc create_hier_cell_ADD_Halve8 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve8() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve8] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve7
proc create_hier_cell_ADD_Halve7 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve7() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve7] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve6
proc create_hier_cell_ADD_Halve6 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve6() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve6] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve5
proc create_hier_cell_ADD_Halve5 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve5() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve5] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve4
proc create_hier_cell_ADD_Halve4 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve4() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve4] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve3
proc create_hier_cell_ADD_Halve3 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve3() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve3] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: ADD_Halve2
proc create_hier_cell_ADD_Halve2 { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_ADD_Halve2() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 13 -to 0 -type data A
  create_bd_pin -dir I -from 13 -to 0 -type data B
  create_bd_pin -dir I CLK
  create_bd_pin -dir O -from 13 -to 0 Dout

  # Create instance: c_addsub_0, and set properties
  set c_addsub_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_addsub:12.0 c_addsub_0 ]
  set_property -dict [ list \
CONFIG.CE {false} \
CONFIG.Out_Width {15} \
 ] $c_addsub_0

  # Need to retain value_src of defaults
  set_property -dict [ list \
CONFIG.Out_Width.VALUE_SRC {DEFAULT} \
 ] $c_addsub_0

  # Create instance: xlslice_4, and set properties
  set xlslice_4 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_4 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {14} \
CONFIG.DIN_TO {1} \
CONFIG.DIN_WIDTH {15} \
CONFIG.DOUT_WIDTH {14} \
 ] $xlslice_4

  # Create port connections
  connect_bd_net -net CLK_1 [get_bd_pins CLK] [get_bd_pins c_addsub_0/CLK]
  connect_bd_net -net c_addsub_0_S [get_bd_pins c_addsub_0/S] [get_bd_pins xlslice_4/Din]
  connect_bd_net -net xlslice_0_Dout [get_bd_pins A] [get_bd_pins c_addsub_0/A]
  connect_bd_net -net xlslice_1_Dout [get_bd_pins B] [get_bd_pins c_addsub_0/B]
  connect_bd_net -net xlslice_4_Dout [get_bd_pins Dout] [get_bd_pins xlslice_4/Dout]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers/ADD_Halve2] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port CLK -pg 1 -y 80 -defaultsOSRD
preplace portBus A -pg 1 -y 40 -defaultsOSRD
preplace portBus B -pg 1 -y 60 -defaultsOSRD
preplace portBus Dout -pg 1 -y 60 -defaultsOSRD
preplace inst xlslice_4 -pg 1 -lvl 2 -y 60 -defaultsOSRD
preplace inst c_addsub_0 -pg 1 -lvl 1 -y 60 -defaultsOSRD
preplace netloc xlslice_4_Dout 1 2 1 NJ
preplace netloc xlslice_1_Dout 1 0 1 NJ
preplace netloc CLK_1 1 0 1 NJ
preplace netloc xlslice_0_Dout 1 0 1 NJ
preplace netloc c_addsub_0_S 1 1 1 NJ
levelinfo -pg 1 -10 100 290 410 -top -10 -bot 130
",
}

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
  create_bd_pin -dir O -from 0 -to 0 Pulse1
  create_bd_pin -dir O -from 0 -to 0 Pulse2
  create_bd_pin -dir O -from 0 -to 0 Pulse3
  create_bd_pin -dir O -from 0 -to 0 Pulse4
  create_bd_pin -dir O -from 0 -to 0 Pulse5
  create_bd_pin -dir O -from 0 -to 0 Pulse6
  create_bd_pin -dir O -from 0 -to 0 Pulse7
  create_bd_pin -dir O -from 0 -to 0 Pulse8
  create_bd_pin -dir O -from 0 -to 0 Pulse9
  create_bd_pin -dir O -from 0 -to 0 Pulse10
  create_bd_pin -dir O -from 0 -to 0 Pulse11
  create_bd_pin -dir O -from 0 -to 0 Pulse12
  create_bd_pin -dir O -from 0 -to 0 Pulse13
  create_bd_pin -dir O -from 0 -to 0 Pulse14
  create_bd_pin -dir O -from 0 -to 0 Pulse15
  create_bd_pin -dir O -from 0 -to 0 Pulse16
  create_bd_pin -dir O -from 31 -to 0 RandValue
  create_bd_pin -dir I -from 0 -to 0 Reset_In
  create_bd_pin -dir I -from 31 -to 0 b
  create_bd_pin -dir I clk

  # Create instance: Pulser1
  create_hier_cell_Pulser1 $hier_obj Pulser1

  # Create instance: RoundRobinPulser
  create_hier_cell_RoundRobinPulser $hier_obj RoundRobinPulser

  # Create instance: RoundRobinPulser1
  create_hier_cell_RoundRobinPulser1 $hier_obj RoundRobinPulser1

  # Create instance: RoundRobinPulser2
  create_hier_cell_RoundRobinPulser2 $hier_obj RoundRobinPulser2

  # Create instance: RoundRobinPulser3
  create_hier_cell_RoundRobinPulser3 $hier_obj RoundRobinPulser3

  # Create instance: RoundRobinPulser4
  create_hier_cell_RoundRobinPulser4 $hier_obj RoundRobinPulser4

  # Create instance: RoundRobinPulser5
  create_hier_cell_RoundRobinPulser5 $hier_obj RoundRobinPulser5

  # Create instance: RoundRobinPulser6
  create_hier_cell_RoundRobinPulser6 $hier_obj RoundRobinPulser6

  # Create instance: RoundRobinPulser7
  create_hier_cell_RoundRobinPulser7 $hier_obj RoundRobinPulser7

  # Create instance: RoundRobinPulser8
  create_hier_cell_RoundRobinPulser8 $hier_obj RoundRobinPulser8

  # Create instance: RoundRobinPulser9
  create_hier_cell_RoundRobinPulser9 $hier_obj RoundRobinPulser9

  # Create instance: RoundRobinPulser10
  create_hier_cell_RoundRobinPulser10 $hier_obj RoundRobinPulser10

  # Create instance: RoundRobinPulser11
  create_hier_cell_RoundRobinPulser11 $hier_obj RoundRobinPulser11

  # Create instance: RoundRobinPulser12
  create_hier_cell_RoundRobinPulser12 $hier_obj RoundRobinPulser12

  # Create instance: RoundRobinPulser13
  create_hier_cell_RoundRobinPulser13 $hier_obj RoundRobinPulser13

  # Create instance: RoundRobinPulser14
  create_hier_cell_RoundRobinPulser14 $hier_obj RoundRobinPulser14

  # Create instance: RoundRobinPulser15
  create_hier_cell_RoundRobinPulser15 $hier_obj RoundRobinPulser15

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {true} \
CONFIG.Output_Width {4} \
 ] $c_counter_binary_0

  # Create instance: lfsr32_0, and set properties
  set lfsr32_0 [ create_bd_cell -type ip -vlnv CCFE:user:lfsr32:1.0 lfsr32_0 ]

  # Create port connections
  connect_bd_net -net Pulser1_pulse [get_bd_pins Pulser1/pulse] [get_bd_pins RoundRobinPulser/din] [get_bd_pins RoundRobinPulser1/din] [get_bd_pins RoundRobinPulser10/din] [get_bd_pins RoundRobinPulser11/din] [get_bd_pins RoundRobinPulser12/din] [get_bd_pins RoundRobinPulser13/din] [get_bd_pins RoundRobinPulser14/din] [get_bd_pins RoundRobinPulser15/din] [get_bd_pins RoundRobinPulser2/din] [get_bd_pins RoundRobinPulser3/din] [get_bd_pins RoundRobinPulser4/din] [get_bd_pins RoundRobinPulser5/din] [get_bd_pins RoundRobinPulser6/din] [get_bd_pins RoundRobinPulser7/din] [get_bd_pins RoundRobinPulser8/din] [get_bd_pins RoundRobinPulser9/din] [get_bd_pins c_counter_binary_0/CE]
  connect_bd_net -net Reset_In_1 [get_bd_pins Reset_In] [get_bd_pins lfsr32_0/reset]
  connect_bd_net -net RoundRobinPulser10_dout [get_bd_pins Pulse11] [get_bd_pins RoundRobinPulser10/dout]
  connect_bd_net -net RoundRobinPulser11_dout [get_bd_pins Pulse12] [get_bd_pins RoundRobinPulser11/dout]
  connect_bd_net -net RoundRobinPulser12_dout [get_bd_pins Pulse13] [get_bd_pins RoundRobinPulser12/dout]
  connect_bd_net -net RoundRobinPulser13_dout [get_bd_pins Pulse14] [get_bd_pins RoundRobinPulser13/dout]
  connect_bd_net -net RoundRobinPulser14_dout [get_bd_pins Pulse15] [get_bd_pins RoundRobinPulser14/dout]
  connect_bd_net -net RoundRobinPulser15_dout [get_bd_pins Pulse16] [get_bd_pins RoundRobinPulser15/dout]
  connect_bd_net -net RoundRobinPulser1_dout [get_bd_pins Pulse2] [get_bd_pins RoundRobinPulser1/dout]
  connect_bd_net -net RoundRobinPulser2_dout [get_bd_pins Pulse3] [get_bd_pins RoundRobinPulser2/dout]
  connect_bd_net -net RoundRobinPulser3_dout [get_bd_pins Pulse4] [get_bd_pins RoundRobinPulser3/dout]
  connect_bd_net -net RoundRobinPulser4_dout [get_bd_pins Pulse5] [get_bd_pins RoundRobinPulser4/dout]
  connect_bd_net -net RoundRobinPulser5_dout [get_bd_pins Pulse6] [get_bd_pins RoundRobinPulser5/dout]
  connect_bd_net -net RoundRobinPulser6_dout [get_bd_pins Pulse7] [get_bd_pins RoundRobinPulser6/dout]
  connect_bd_net -net RoundRobinPulser7_dout [get_bd_pins Pulse8] [get_bd_pins RoundRobinPulser7/dout]
  connect_bd_net -net RoundRobinPulser8_dout [get_bd_pins Pulse9] [get_bd_pins RoundRobinPulser8/dout]
  connect_bd_net -net RoundRobinPulser9_dout [get_bd_pins Pulse10] [get_bd_pins RoundRobinPulser9/dout]
  connect_bd_net -net b_1 [get_bd_pins b] [get_bd_pins Pulser1/b]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins RoundRobinPulser/a] [get_bd_pins RoundRobinPulser1/a] [get_bd_pins RoundRobinPulser10/a] [get_bd_pins RoundRobinPulser11/a] [get_bd_pins RoundRobinPulser12/a] [get_bd_pins RoundRobinPulser13/a] [get_bd_pins RoundRobinPulser14/a] [get_bd_pins RoundRobinPulser15/a] [get_bd_pins RoundRobinPulser2/a] [get_bd_pins RoundRobinPulser3/a] [get_bd_pins RoundRobinPulser4/a] [get_bd_pins RoundRobinPulser5/a] [get_bd_pins RoundRobinPulser6/a] [get_bd_pins RoundRobinPulser7/a] [get_bd_pins RoundRobinPulser8/a] [get_bd_pins RoundRobinPulser9/a] [get_bd_pins c_counter_binary_0/Q]
  connect_bd_net -net clk_1 [get_bd_pins clk] [get_bd_pins Pulser1/clk] [get_bd_pins RoundRobinPulser13/clk] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins lfsr32_0/clock]
  connect_bd_net -net lfsr32_0_rnd [get_bd_pins RandValue] [get_bd_pins Pulser1/A] [get_bd_pins lfsr32_0/rnd]
  connect_bd_net -net register_0_dout [get_bd_pins Pulse1] [get_bd_pins RoundRobinPulser/dout]

  # Restore current instance
  current_bd_instance $oldCurInst
}

# Hierarchical cell: MultiplePulseShapers
proc create_hier_cell_MultiplePulseShapers { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_MultiplePulseShapers() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 13 -to 0 Ch1ShapedPulse
  create_bd_pin -dir O -from 13 -to 0 Ch2ShapedPulse
  create_bd_pin -dir I -from 0 -to 0 Trig_Count
  create_bd_pin -dir I -from 0 -to 0 Trig_Count1
  create_bd_pin -dir I -from 0 -to 0 Trig_Count2
  create_bd_pin -dir I -from 0 -to 0 Trig_Count3
  create_bd_pin -dir I -from 0 -to 0 Trig_Count4
  create_bd_pin -dir I -from 0 -to 0 Trig_Count5
  create_bd_pin -dir I -from 0 -to 0 Trig_Count6
  create_bd_pin -dir I -from 0 -to 0 Trig_Count7
  create_bd_pin -dir I -from 0 -to 0 Trig_Count8
  create_bd_pin -dir I -from 0 -to 0 Trig_Count9
  create_bd_pin -dir I -from 0 -to 0 Trig_Count10
  create_bd_pin -dir I -from 0 -to 0 Trig_Count11
  create_bd_pin -dir I -from 0 -to 0 Trig_Count12
  create_bd_pin -dir I -from 0 -to 0 Trig_Count13
  create_bd_pin -dir I -from 0 -to 0 Trig_Count14
  create_bd_pin -dir I -from 0 -to 0 Trig_Count15
  create_bd_pin -dir I clk
  create_bd_pin -dir I -from 0 -to 0 rstb

  # Create instance: ADD_Halve2
  create_hier_cell_ADD_Halve2 $hier_obj ADD_Halve2

  # Create instance: ADD_Halve3
  create_hier_cell_ADD_Halve3 $hier_obj ADD_Halve3

  # Create instance: ADD_Halve4
  create_hier_cell_ADD_Halve4 $hier_obj ADD_Halve4

  # Create instance: ADD_Halve5
  create_hier_cell_ADD_Halve5 $hier_obj ADD_Halve5

  # Create instance: ADD_Halve6
  create_hier_cell_ADD_Halve6 $hier_obj ADD_Halve6

  # Create instance: ADD_Halve7
  create_hier_cell_ADD_Halve7 $hier_obj ADD_Halve7

  # Create instance: ADD_Halve8
  create_hier_cell_ADD_Halve8 $hier_obj ADD_Halve8

  # Create instance: ADD_Halve9
  create_hier_cell_ADD_Halve9 $hier_obj ADD_Halve9

  # Create instance: ADD_Halve10
  create_hier_cell_ADD_Halve10 $hier_obj ADD_Halve10

  # Create instance: ADD_Halve11
  create_hier_cell_ADD_Halve11 $hier_obj ADD_Halve11

  # Create instance: ADD_Halve12
  create_hier_cell_ADD_Halve12 $hier_obj ADD_Halve12

  # Create instance: ADD_Halve13
  create_hier_cell_ADD_Halve13 $hier_obj ADD_Halve13

  # Create instance: ADD_Halve14
  create_hier_cell_ADD_Halve14 $hier_obj ADD_Halve14

  # Create instance: ADD_Halve15
  create_hier_cell_ADD_Halve15 $hier_obj ADD_Halve15

  # Create instance: PulseFormer
  create_hier_cell_PulseFormer $hier_obj PulseFormer

  # Create instance: PulseFormer1
  create_hier_cell_PulseFormer1 $hier_obj PulseFormer1

  # Create instance: PulseFormer2
  create_hier_cell_PulseFormer2 $hier_obj PulseFormer2

  # Create instance: PulseFormer3
  create_hier_cell_PulseFormer3 $hier_obj PulseFormer3

  # Create instance: PulseFormer4
  create_hier_cell_PulseFormer4 $hier_obj PulseFormer4

  # Create instance: PulseFormer5
  create_hier_cell_PulseFormer5 $hier_obj PulseFormer5

  # Create instance: PulseFormer6
  create_hier_cell_PulseFormer6 $hier_obj PulseFormer6

  # Create instance: PulseFormer7
  create_hier_cell_PulseFormer7 $hier_obj PulseFormer7

  # Create instance: PulseFormer8
  create_hier_cell_PulseFormer8 $hier_obj PulseFormer8

  # Create instance: PulseFormer9
  create_hier_cell_PulseFormer9 $hier_obj PulseFormer9

  # Create instance: PulseFormer10
  create_hier_cell_PulseFormer10 $hier_obj PulseFormer10

  # Create instance: PulseFormer11
  create_hier_cell_PulseFormer11 $hier_obj PulseFormer11

  # Create instance: PulseFormer12
  create_hier_cell_PulseFormer12 $hier_obj PulseFormer12

  # Create instance: PulseFormer13
  create_hier_cell_PulseFormer13 $hier_obj PulseFormer13

  # Create instance: PulseFormer14
  create_hier_cell_PulseFormer14 $hier_obj PulseFormer14

  # Create instance: PulseFormer15
  create_hier_cell_PulseFormer15 $hier_obj PulseFormer15

  # Create instance: Twox2ChannelAdder
  create_hier_cell_Twox2ChannelAdder $hier_obj Twox2ChannelAdder

  # Create instance: Twox2ChannelAdder1
  create_hier_cell_Twox2ChannelAdder1 $hier_obj Twox2ChannelAdder1

  # Create instance: Twox2ChannelAdder2
  create_hier_cell_Twox2ChannelAdder2 $hier_obj Twox2ChannelAdder2

  # Create instance: Twox2ChannelAdder3
  create_hier_cell_Twox2ChannelAdder3 $hier_obj Twox2ChannelAdder3

  # Create instance: Twox2ChannelAdder4
  create_hier_cell_Twox2ChannelAdder4 $hier_obj Twox2ChannelAdder4

  # Create instance: Twox2ChannelAdder5
  create_hier_cell_Twox2ChannelAdder5 $hier_obj Twox2ChannelAdder5

  # Create instance: Twox2ChannelAdder6
  create_hier_cell_Twox2ChannelAdder6 $hier_obj Twox2ChannelAdder6

  # Create instance: Twox2ChannelAdder7
  create_hier_cell_Twox2ChannelAdder7 $hier_obj Twox2ChannelAdder7

  # Create port connections
  connect_bd_net -net ADD_Halve11_Dout [get_bd_pins ADD_Halve11/Dout] [get_bd_pins ADD_Halve14/A]
  connect_bd_net -net ADD_Halve13_Dout [get_bd_pins ADD_Halve13/Dout] [get_bd_pins ADD_Halve15/B]
  connect_bd_net -net ADD_Halve14_Dout [get_bd_pins Ch2ShapedPulse] [get_bd_pins ADD_Halve14/Dout]
  connect_bd_net -net ADD_Halve15_Dout [get_bd_pins Ch1ShapedPulse] [get_bd_pins ADD_Halve15/Dout]
  connect_bd_net -net ADD_Halve2_Dout [get_bd_pins ADD_Halve11/A] [get_bd_pins ADD_Halve2/Dout]
  connect_bd_net -net ADD_Halve3_Dout [get_bd_pins ADD_Halve10/A] [get_bd_pins ADD_Halve3/Dout]
  connect_bd_net -net ADD_Halve4_Dout [get_bd_pins ADD_Halve11/B] [get_bd_pins ADD_Halve4/Dout]
  connect_bd_net -net ADD_Halve5_Dout [get_bd_pins ADD_Halve10/B] [get_bd_pins ADD_Halve5/Dout]
  connect_bd_net -net ADD_Halve6_Dout [get_bd_pins ADD_Halve12/A] [get_bd_pins ADD_Halve6/Dout]
  connect_bd_net -net ADD_Halve8_Dout [get_bd_pins ADD_Halve12/B] [get_bd_pins ADD_Halve8/Dout]
  connect_bd_net -net ADD_Halve9_Dout [get_bd_pins ADD_Halve13/B] [get_bd_pins ADD_Halve9/Dout]
  connect_bd_net -net A_1 [get_bd_pins ADD_Halve13/A] [get_bd_pins ADD_Halve7/Dout]
  connect_bd_net -net A_2 [get_bd_pins ADD_Halve10/Dout] [get_bd_pins ADD_Halve15/A]
  connect_bd_net -net B_1 [get_bd_pins ADD_Halve12/Dout] [get_bd_pins ADD_Halve14/B]
  connect_bd_net -net Din1_1 [get_bd_pins PulseFormer13/ShapedPulse] [get_bd_pins Twox2ChannelAdder1/Din1]
  connect_bd_net -net Din1_2 [get_bd_pins PulseFormer15/ShapedPulse] [get_bd_pins Twox2ChannelAdder2/Din1]
  connect_bd_net -net Din1_3 [get_bd_pins PulseFormer9/ShapedPulse] [get_bd_pins Twox2ChannelAdder5/Din1]
  connect_bd_net -net PulseFormer10_ShapedPulse [get_bd_pins PulseFormer10/ShapedPulse] [get_bd_pins Twox2ChannelAdder/Din]
  connect_bd_net -net PulseFormer11_ShapedPulse [get_bd_pins PulseFormer11/ShapedPulse] [get_bd_pins Twox2ChannelAdder/Din1]
  connect_bd_net -net PulseFormer12_ShapedPulse [get_bd_pins PulseFormer12/ShapedPulse] [get_bd_pins Twox2ChannelAdder1/Din]
  connect_bd_net -net PulseFormer14_ShapedPulse [get_bd_pins PulseFormer14/ShapedPulse] [get_bd_pins Twox2ChannelAdder2/Din]
  connect_bd_net -net PulseFormer1_ShapedPulse [get_bd_pins PulseFormer1/ShapedPulse] [get_bd_pins Twox2ChannelAdder3/Din1]
  connect_bd_net -net PulseFormer2_ShapedPulse [get_bd_pins PulseFormer2/ShapedPulse] [get_bd_pins Twox2ChannelAdder6/Din]
  connect_bd_net -net PulseFormer3_ShapedPulse [get_bd_pins PulseFormer3/ShapedPulse] [get_bd_pins Twox2ChannelAdder6/Din1]
  connect_bd_net -net PulseFormer4_ShapedPulse [get_bd_pins PulseFormer4/ShapedPulse] [get_bd_pins Twox2ChannelAdder7/Din]
  connect_bd_net -net PulseFormer5_ShapedPulse [get_bd_pins PulseFormer5/ShapedPulse] [get_bd_pins Twox2ChannelAdder7/Din1]
  connect_bd_net -net PulseFormer6_ShapedPulse [get_bd_pins PulseFormer6/ShapedPulse] [get_bd_pins Twox2ChannelAdder4/Din]
  connect_bd_net -net PulseFormer7_ShapedPulse [get_bd_pins PulseFormer7/ShapedPulse] [get_bd_pins Twox2ChannelAdder4/Din1]
  connect_bd_net -net PulseFormer8_ShapedPulse [get_bd_pins PulseFormer8/ShapedPulse] [get_bd_pins Twox2ChannelAdder5/Din]
  connect_bd_net -net PulseFormer_ShapedPulse [get_bd_pins PulseFormer/ShapedPulse] [get_bd_pins Twox2ChannelAdder3/Din]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Trig_Count] [get_bd_pins PulseFormer/Trig_Count]
  connect_bd_net -net Trig_Count1_1 [get_bd_pins Trig_Count1] [get_bd_pins PulseFormer1/Trig_Count]
  connect_bd_net -net Trig_Count1_2 [get_bd_pins Trig_Count2] [get_bd_pins PulseFormer2/Trig_Count]
  connect_bd_net -net Trig_Count1_3 [get_bd_pins Trig_Count3] [get_bd_pins PulseFormer3/Trig_Count]
  connect_bd_net -net Trig_Count1_4 [get_bd_pins Trig_Count4] [get_bd_pins PulseFormer4/Trig_Count]
  connect_bd_net -net Trig_Count1_5 [get_bd_pins Trig_Count5] [get_bd_pins PulseFormer5/Trig_Count]
  connect_bd_net -net Trig_Count1_6 [get_bd_pins Trig_Count6] [get_bd_pins PulseFormer6/Trig_Count]
  connect_bd_net -net Trig_Count1_7 [get_bd_pins Trig_Count7] [get_bd_pins PulseFormer7/Trig_Count]
  connect_bd_net -net Trig_Count1_8 [get_bd_pins Trig_Count8] [get_bd_pins PulseFormer8/Trig_Count]
  connect_bd_net -net Trig_Count1_9 [get_bd_pins Trig_Count9] [get_bd_pins PulseFormer9/Trig_Count]
  connect_bd_net -net Trig_Count1_10 [get_bd_pins Trig_Count10] [get_bd_pins PulseFormer10/Trig_Count]
  connect_bd_net -net Trig_Count1_11 [get_bd_pins Trig_Count11] [get_bd_pins PulseFormer11/Trig_Count]
  connect_bd_net -net Trig_Count1_12 [get_bd_pins Trig_Count12] [get_bd_pins PulseFormer12/Trig_Count]
  connect_bd_net -net Trig_Count1_13 [get_bd_pins Trig_Count13] [get_bd_pins PulseFormer13/Trig_Count]
  connect_bd_net -net Trig_Count1_14 [get_bd_pins Trig_Count14] [get_bd_pins PulseFormer14/Trig_Count]
  connect_bd_net -net Trig_Count1_15 [get_bd_pins Trig_Count15] [get_bd_pins PulseFormer15/Trig_Count]
  connect_bd_net -net Twox2ChannelAdder1_Ch1 [get_bd_pins ADD_Halve3/B] [get_bd_pins Twox2ChannelAdder1/Ch1]
  connect_bd_net -net Twox2ChannelAdder1_Ch2 [get_bd_pins ADD_Halve2/B] [get_bd_pins Twox2ChannelAdder1/Ch2]
  connect_bd_net -net Twox2ChannelAdder2_Ch1 [get_bd_pins ADD_Halve5/A] [get_bd_pins Twox2ChannelAdder2/Ch1]
  connect_bd_net -net Twox2ChannelAdder2_Ch2 [get_bd_pins ADD_Halve4/A] [get_bd_pins Twox2ChannelAdder2/Ch2]
  connect_bd_net -net Twox2ChannelAdder3_Ch1 [get_bd_pins ADD_Halve5/B] [get_bd_pins Twox2ChannelAdder3/Ch1]
  connect_bd_net -net Twox2ChannelAdder3_Ch2 [get_bd_pins ADD_Halve4/B] [get_bd_pins Twox2ChannelAdder3/Ch2]
  connect_bd_net -net Twox2ChannelAdder4_Ch1 [get_bd_pins ADD_Halve9/A] [get_bd_pins Twox2ChannelAdder4/Ch1]
  connect_bd_net -net Twox2ChannelAdder4_Ch2 [get_bd_pins ADD_Halve8/A] [get_bd_pins Twox2ChannelAdder4/Ch2]
  connect_bd_net -net Twox2ChannelAdder5_Ch1 [get_bd_pins ADD_Halve9/B] [get_bd_pins Twox2ChannelAdder5/Ch1]
  connect_bd_net -net Twox2ChannelAdder5_Ch2 [get_bd_pins ADD_Halve8/B] [get_bd_pins Twox2ChannelAdder5/Ch2]
  connect_bd_net -net Twox2ChannelAdder6_Ch1 [get_bd_pins ADD_Halve7/A] [get_bd_pins Twox2ChannelAdder6/Ch1]
  connect_bd_net -net Twox2ChannelAdder6_Ch2 [get_bd_pins ADD_Halve6/A] [get_bd_pins Twox2ChannelAdder6/Ch2]
  connect_bd_net -net Twox2ChannelAdder7_Ch1 [get_bd_pins ADD_Halve7/B] [get_bd_pins Twox2ChannelAdder7/Ch1]
  connect_bd_net -net Twox2ChannelAdder7_Ch2 [get_bd_pins ADD_Halve6/B] [get_bd_pins Twox2ChannelAdder7/Ch2]
  connect_bd_net -net Twox2ChannelAdder_Ch1 [get_bd_pins ADD_Halve3/A] [get_bd_pins Twox2ChannelAdder/Ch1]
  connect_bd_net -net Twox2ChannelAdder_Ch2 [get_bd_pins ADD_Halve2/A] [get_bd_pins Twox2ChannelAdder/Ch2]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins ADD_Halve10/CLK] [get_bd_pins ADD_Halve11/CLK] [get_bd_pins ADD_Halve12/CLK] [get_bd_pins ADD_Halve13/CLK] [get_bd_pins ADD_Halve14/CLK] [get_bd_pins ADD_Halve15/CLK] [get_bd_pins ADD_Halve2/CLK] [get_bd_pins ADD_Halve3/CLK] [get_bd_pins ADD_Halve4/CLK] [get_bd_pins ADD_Halve5/CLK] [get_bd_pins ADD_Halve6/CLK] [get_bd_pins ADD_Halve7/CLK] [get_bd_pins ADD_Halve8/CLK] [get_bd_pins ADD_Halve9/CLK] [get_bd_pins PulseFormer/clk] [get_bd_pins PulseFormer1/clk] [get_bd_pins PulseFormer10/clk] [get_bd_pins PulseFormer11/clk] [get_bd_pins PulseFormer12/clk] [get_bd_pins PulseFormer13/clk] [get_bd_pins PulseFormer14/clk] [get_bd_pins PulseFormer15/clk] [get_bd_pins PulseFormer2/clk] [get_bd_pins PulseFormer3/clk] [get_bd_pins PulseFormer4/clk] [get_bd_pins PulseFormer5/clk] [get_bd_pins PulseFormer6/clk] [get_bd_pins PulseFormer7/clk] [get_bd_pins PulseFormer8/clk] [get_bd_pins PulseFormer9/clk] [get_bd_pins Twox2ChannelAdder/CLK] [get_bd_pins Twox2ChannelAdder1/CLK] [get_bd_pins Twox2ChannelAdder2/CLK] [get_bd_pins Twox2ChannelAdder3/CLK] [get_bd_pins Twox2ChannelAdder4/CLK] [get_bd_pins Twox2ChannelAdder5/CLK] [get_bd_pins Twox2ChannelAdder6/CLK] [get_bd_pins Twox2ChannelAdder7/CLK]
  connect_bd_net -net axi_bram_ctrl_dac_bram_addr_a [get_bd_pins BRAM_PORTA_addr] [get_bd_pins PulseFormer/BRAM_PORTA_addr] [get_bd_pins PulseFormer1/BRAM_PORTA_addr] [get_bd_pins PulseFormer10/BRAM_PORTA_addr] [get_bd_pins PulseFormer11/BRAM_PORTA_addr] [get_bd_pins PulseFormer12/BRAM_PORTA_addr] [get_bd_pins PulseFormer13/BRAM_PORTA_addr] [get_bd_pins PulseFormer14/BRAM_PORTA_addr] [get_bd_pins PulseFormer15/BRAM_PORTA_addr] [get_bd_pins PulseFormer2/BRAM_PORTA_addr] [get_bd_pins PulseFormer3/BRAM_PORTA_addr] [get_bd_pins PulseFormer4/BRAM_PORTA_addr] [get_bd_pins PulseFormer5/BRAM_PORTA_addr] [get_bd_pins PulseFormer6/BRAM_PORTA_addr] [get_bd_pins PulseFormer7/BRAM_PORTA_addr] [get_bd_pins PulseFormer8/BRAM_PORTA_addr] [get_bd_pins PulseFormer9/BRAM_PORTA_addr]
  connect_bd_net -net axi_bram_ctrl_dac_bram_clk_a [get_bd_pins BRAM_PORTA_clk] [get_bd_pins PulseFormer/BRAM_PORTA_clk] [get_bd_pins PulseFormer1/BRAM_PORTA_clk] [get_bd_pins PulseFormer10/BRAM_PORTA_clk] [get_bd_pins PulseFormer11/BRAM_PORTA_clk] [get_bd_pins PulseFormer12/BRAM_PORTA_clk] [get_bd_pins PulseFormer13/BRAM_PORTA_clk] [get_bd_pins PulseFormer14/BRAM_PORTA_clk] [get_bd_pins PulseFormer15/BRAM_PORTA_clk] [get_bd_pins PulseFormer2/BRAM_PORTA_clk] [get_bd_pins PulseFormer3/BRAM_PORTA_clk] [get_bd_pins PulseFormer4/BRAM_PORTA_clk] [get_bd_pins PulseFormer5/BRAM_PORTA_clk] [get_bd_pins PulseFormer6/BRAM_PORTA_clk] [get_bd_pins PulseFormer7/BRAM_PORTA_clk] [get_bd_pins PulseFormer8/BRAM_PORTA_clk] [get_bd_pins PulseFormer9/BRAM_PORTA_clk]
  connect_bd_net -net axi_bram_ctrl_dac_bram_en_a [get_bd_pins BRAM_PORTA_en] [get_bd_pins PulseFormer/BRAM_PORTA_en] [get_bd_pins PulseFormer1/BRAM_PORTA_en] [get_bd_pins PulseFormer10/BRAM_PORTA_en] [get_bd_pins PulseFormer11/BRAM_PORTA_en] [get_bd_pins PulseFormer12/BRAM_PORTA_en] [get_bd_pins PulseFormer13/BRAM_PORTA_en] [get_bd_pins PulseFormer14/BRAM_PORTA_en] [get_bd_pins PulseFormer15/BRAM_PORTA_en] [get_bd_pins PulseFormer2/BRAM_PORTA_en] [get_bd_pins PulseFormer3/BRAM_PORTA_en] [get_bd_pins PulseFormer4/BRAM_PORTA_en] [get_bd_pins PulseFormer5/BRAM_PORTA_en] [get_bd_pins PulseFormer6/BRAM_PORTA_en] [get_bd_pins PulseFormer7/BRAM_PORTA_en] [get_bd_pins PulseFormer8/BRAM_PORTA_en] [get_bd_pins PulseFormer9/BRAM_PORTA_en]
  connect_bd_net -net axi_bram_ctrl_dac_bram_rst_a [get_bd_pins BRAM_PORTA_rst] [get_bd_pins PulseFormer/BRAM_PORTA_rst] [get_bd_pins PulseFormer1/BRAM_PORTA_rst] [get_bd_pins PulseFormer10/BRAM_PORTA_rst] [get_bd_pins PulseFormer11/BRAM_PORTA_rst] [get_bd_pins PulseFormer12/BRAM_PORTA_rst] [get_bd_pins PulseFormer13/BRAM_PORTA_rst] [get_bd_pins PulseFormer14/BRAM_PORTA_rst] [get_bd_pins PulseFormer15/BRAM_PORTA_rst] [get_bd_pins PulseFormer2/BRAM_PORTA_rst] [get_bd_pins PulseFormer3/BRAM_PORTA_rst] [get_bd_pins PulseFormer4/BRAM_PORTA_rst] [get_bd_pins PulseFormer5/BRAM_PORTA_rst] [get_bd_pins PulseFormer6/BRAM_PORTA_rst] [get_bd_pins PulseFormer7/BRAM_PORTA_rst] [get_bd_pins PulseFormer8/BRAM_PORTA_rst] [get_bd_pins PulseFormer9/BRAM_PORTA_rst]
  connect_bd_net -net axi_bram_ctrl_dac_bram_we_a [get_bd_pins BRAM_PORTA_we] [get_bd_pins PulseFormer/BRAM_PORTA_we] [get_bd_pins PulseFormer1/BRAM_PORTA_we] [get_bd_pins PulseFormer10/BRAM_PORTA_we] [get_bd_pins PulseFormer11/BRAM_PORTA_we] [get_bd_pins PulseFormer12/BRAM_PORTA_we] [get_bd_pins PulseFormer13/BRAM_PORTA_we] [get_bd_pins PulseFormer14/BRAM_PORTA_we] [get_bd_pins PulseFormer15/BRAM_PORTA_we] [get_bd_pins PulseFormer2/BRAM_PORTA_we] [get_bd_pins PulseFormer3/BRAM_PORTA_we] [get_bd_pins PulseFormer4/BRAM_PORTA_we] [get_bd_pins PulseFormer5/BRAM_PORTA_we] [get_bd_pins PulseFormer6/BRAM_PORTA_we] [get_bd_pins PulseFormer7/BRAM_PORTA_we] [get_bd_pins PulseFormer8/BRAM_PORTA_we] [get_bd_pins PulseFormer9/BRAM_PORTA_we]
  connect_bd_net -net axi_bram_ctrl_dac_bram_wrdata_a [get_bd_pins BRAM_PORTA_din] [get_bd_pins PulseFormer/BRAM_PORTA_din] [get_bd_pins PulseFormer1/BRAM_PORTA_din] [get_bd_pins PulseFormer10/BRAM_PORTA_din] [get_bd_pins PulseFormer11/BRAM_PORTA_din] [get_bd_pins PulseFormer12/BRAM_PORTA_din] [get_bd_pins PulseFormer13/BRAM_PORTA_din] [get_bd_pins PulseFormer14/BRAM_PORTA_din] [get_bd_pins PulseFormer15/BRAM_PORTA_din] [get_bd_pins PulseFormer2/BRAM_PORTA_din] [get_bd_pins PulseFormer3/BRAM_PORTA_din] [get_bd_pins PulseFormer4/BRAM_PORTA_din] [get_bd_pins PulseFormer5/BRAM_PORTA_din] [get_bd_pins PulseFormer6/BRAM_PORTA_din] [get_bd_pins PulseFormer7/BRAM_PORTA_din] [get_bd_pins PulseFormer8/BRAM_PORTA_din] [get_bd_pins PulseFormer9/BRAM_PORTA_din]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins rstb] [get_bd_pins PulseFormer/rstb] [get_bd_pins PulseFormer1/rstb] [get_bd_pins PulseFormer10/rstb] [get_bd_pins PulseFormer11/rstb] [get_bd_pins PulseFormer12/rstb] [get_bd_pins PulseFormer13/rstb] [get_bd_pins PulseFormer14/rstb] [get_bd_pins PulseFormer15/rstb] [get_bd_pins PulseFormer2/rstb] [get_bd_pins PulseFormer3/rstb] [get_bd_pins PulseFormer4/rstb] [get_bd_pins PulseFormer5/rstb] [get_bd_pins PulseFormer6/rstb] [get_bd_pins PulseFormer7/rstb] [get_bd_pins PulseFormer8/rstb] [get_bd_pins PulseFormer9/rstb]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer/MultiplePulseShapers] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port BRAM_PORTA_rst -pg 1 -y 1770 -defaultsOSRD
preplace port BRAM_PORTA_en -pg 1 -y 1750 -defaultsOSRD
preplace port BRAM_PORTA_clk -pg 1 -y 1710 -defaultsOSRD
preplace port clk -pg 1 -y 1830 -defaultsOSRD
preplace portBus Trig_Count9 -pg 1 -y 4240 -defaultsOSRD
preplace portBus rstb -pg 1 -y 1850 -defaultsOSRD
preplace portBus Trig_Count -pg 1 -y 1810 -defaultsOSRD
preplace portBus Trig_Count10 -pg 1 -y 120 -defaultsOSRD
preplace portBus Trig_Count1 -pg 1 -y 2080 -defaultsOSRD
preplace portBus Ch2ShapedPulse -pg 1 -y 1780 -defaultsOSRD
preplace portBus Trig_Count11 -pg 1 -y 390 -defaultsOSRD
preplace portBus Trig_Count2 -pg 1 -y 2350 -defaultsOSRD
preplace portBus BRAM_PORTA_din -pg 1 -y 1730 -defaultsOSRD
preplace portBus Trig_Count12 -pg 1 -y 730 -defaultsOSRD
preplace portBus Trig_Count3 -pg 1 -y 2620 -defaultsOSRD
preplace portBus Ch1ShapedPulse -pg 1 -y 1820 -defaultsOSRD
preplace portBus BRAM_PORTA_we -pg 1 -y 1790 -defaultsOSRD
preplace portBus BRAM_PORTA_addr -pg 1 -y 1690 -defaultsOSRD
preplace portBus Trig_Count13 -pg 1 -y 1000 -defaultsOSRD
preplace portBus Trig_Count4 -pg 1 -y 2890 -defaultsOSRD
preplace portBus Trig_Count14 -pg 1 -y 1270 -defaultsOSRD
preplace portBus Trig_Count5 -pg 1 -y 3160 -defaultsOSRD
preplace portBus Trig_Count15 -pg 1 -y 1540 -defaultsOSRD
preplace portBus Trig_Count6 -pg 1 -y 3430 -defaultsOSRD
preplace portBus Trig_Count7 -pg 1 -y 3700 -defaultsOSRD
preplace portBus Trig_Count8 -pg 1 -y 3970 -defaultsOSRD
preplace inst Twox2ChannelAdder4 -pg 1 -lvl 2 -y 3520 -defaultsOSRD
preplace inst PulseFormer11 -pg 1 -lvl 1 -y 340 -defaultsOSRD
preplace inst ADD_Halve12 -pg 1 -lvl 4 -y 3250 -defaultsOSRD
preplace inst Twox2ChannelAdder5 -pg 1 -lvl 2 -y 4040 -defaultsOSRD
preplace inst PulseFormer12 -pg 1 -lvl 1 -y 680 -defaultsOSRD
preplace inst ADD_Halve13 -pg 1 -lvl 4 -y 3380 -defaultsOSRD
preplace inst Twox2ChannelAdder6 -pg 1 -lvl 2 -y 2430 -defaultsOSRD
preplace inst PulseFormer13 -pg 1 -lvl 1 -y 950 -defaultsOSRD
preplace inst PulseFormer1 -pg 1 -lvl 1 -y 2030 -defaultsOSRD
preplace inst ADD_Halve14 -pg 1 -lvl 5 -y 1740 -defaultsOSRD
preplace inst ADD_Halve2 -pg 1 -lvl 3 -y 440 -defaultsOSRD
preplace inst Twox2ChannelAdder7 -pg 1 -lvl 2 -y 2980 -defaultsOSRD
preplace inst PulseFormer14 -pg 1 -lvl 1 -y 1220 -defaultsOSRD
preplace inst PulseFormer2 -pg 1 -lvl 1 -y 2300 -defaultsOSRD
preplace inst ADD_Halve15 -pg 1 -lvl 5 -y 1870 -defaultsOSRD
preplace inst ADD_Halve3 -pg 1 -lvl 3 -y 580 -defaultsOSRD
preplace inst PulseFormer15 -pg 1 -lvl 1 -y 1490 -defaultsOSRD
preplace inst PulseFormer3 -pg 1 -lvl 1 -y 2570 -defaultsOSRD
preplace inst ADD_Halve4 -pg 1 -lvl 3 -y 1570 -defaultsOSRD
preplace inst PulseFormer4 -pg 1 -lvl 1 -y 2840 -defaultsOSRD
preplace inst ADD_Halve5 -pg 1 -lvl 3 -y 1700 -defaultsOSRD
preplace inst PulseFormer5 -pg 1 -lvl 1 -y 3110 -defaultsOSRD
preplace inst ADD_Halve6 -pg 1 -lvl 3 -y 2620 -defaultsOSRD
preplace inst PulseFormer6 -pg 1 -lvl 1 -y 3380 -defaultsOSRD
preplace inst ADD_Halve7 -pg 1 -lvl 3 -y 2750 -defaultsOSRD
preplace inst PulseFormer7 -pg 1 -lvl 1 -y 3650 -defaultsOSRD
preplace inst PulseFormer -pg 1 -lvl 1 -y 1760 -defaultsOSRD
preplace inst ADD_Halve8 -pg 1 -lvl 3 -y 3740 -defaultsOSRD
preplace inst Twox2ChannelAdder -pg 1 -lvl 2 -y 280 -defaultsOSRD
preplace inst PulseFormer8 -pg 1 -lvl 1 -y 3920 -defaultsOSRD
preplace inst ADD_Halve9 -pg 1 -lvl 3 -y 3870 -defaultsOSRD
preplace inst Twox2ChannelAdder1 -pg 1 -lvl 2 -y 840 -defaultsOSRD
preplace inst PulseFormer9 -pg 1 -lvl 1 -y 4190 -defaultsOSRD
preplace inst Twox2ChannelAdder2 -pg 1 -lvl 2 -y 1360 -defaultsOSRD
preplace inst ADD_Halve10 -pg 1 -lvl 4 -y 1050 -defaultsOSRD
preplace inst Twox2ChannelAdder3 -pg 1 -lvl 2 -y 1920 -defaultsOSRD
preplace inst PulseFormer10 -pg 1 -lvl 1 -y 70 -defaultsOSRD
preplace inst ADD_Halve11 -pg 1 -lvl 4 -y 900 -defaultsOSRD
preplace netloc axi_bram_ctrl_dac_bram_rst_a 1 0 1 40
preplace netloc Trig_Count1_5 1 0 1 NJ
preplace netloc proc_sys_reset_adc_clk_peripheral_reset 1 0 1 70
preplace netloc Twox2ChannelAdder7_Ch1 1 2 1 690
preplace netloc Trig_Count1_6 1 0 1 NJ
preplace netloc Twox2ChannelAdder7_Ch2 1 2 1 680
preplace netloc Trig_Count1_7 1 0 1 NJ
preplace netloc ADD_Halve13_Dout 1 4 1 1120
preplace netloc Trig_Count1_8 1 0 1 NJ
preplace netloc Trig_Count1_9 1 0 1 NJ
preplace netloc ADD_Halve14_Dout 1 5 1 1300
preplace netloc PulseFormer1_ShapedPulse 1 1 1 430
preplace netloc ADD_Halve15_Dout 1 5 1 1300
preplace netloc Trig_Count1_10 1 0 1 NJ
preplace netloc Twox2ChannelAdder1_Ch1 1 2 1 660
preplace netloc Twox2ChannelAdder1_Ch2 1 2 1 650
preplace netloc Trig_Count1_11 1 0 1 NJ
preplace netloc axi_bram_ctrl_dac_bram_we_a 1 0 1 50
preplace netloc Twox2ChannelAdder_Ch1 1 2 1 690
preplace netloc Trig_Count1_12 1 0 1 NJ
preplace netloc axi_bram_ctrl_dac_bram_wrdata_a 1 0 1 20
preplace netloc Twox2ChannelAdder_Ch2 1 2 1 650
preplace netloc PulseFormer3_ShapedPulse 1 1 1 450
preplace netloc Trig_Count1_13 1 0 1 NJ
preplace netloc Trig_Count1_14 1 0 1 NJ
preplace netloc PulseFormer10_ShapedPulse 1 1 1 450
preplace netloc Twox2ChannelAdder6_Ch1 1 2 1 660
preplace netloc Trig_Count1_15 1 0 1 NJ
preplace netloc Twox2ChannelAdder6_Ch2 1 2 1 650
preplace netloc Twox2ChannelAdder3_Ch1 1 2 1 660
preplace netloc ADD_Halve11_Dout 1 4 1 1100
preplace netloc ADD_Halve8_Dout 1 3 1 900
preplace netloc Twox2ChannelAdder3_Ch2 1 2 1 650
preplace netloc ADD_Halve2_Dout 1 3 1 910
preplace netloc PulseFormer11_ShapedPulse 1 1 1 430
preplace netloc A_1 1 3 1 870
preplace netloc ADD_Halve9_Dout 1 3 1 910
preplace netloc A_2 1 4 1 1090
preplace netloc PulseFormer2_ShapedPulse 1 1 1 430
preplace netloc PulseFormer8_ShapedPulse 1 1 1 430
preplace netloc PulseFormer6_ShapedPulse 1 1 1 430
preplace netloc PulseFormer5_ShapedPulse 1 1 1 450
preplace netloc ADD_Halve5_Dout 1 3 1 880
preplace netloc B_1 1 4 1 1100
preplace netloc Twox2ChannelAdder2_Ch1 1 2 1 690
preplace netloc ADD_Halve6_Dout 1 3 1 880
preplace netloc Din1_1 1 1 1 430
preplace netloc Twox2ChannelAdder4_Ch1 1 2 1 660
preplace netloc PulseFormer12_ShapedPulse 1 1 1 450
preplace netloc Twox2ChannelAdder2_Ch2 1 2 1 680
preplace netloc Din1_2 1 1 1 430
preplace netloc axi_bram_ctrl_dac_bram_clk_a 1 0 1 10
preplace netloc Twox2ChannelAdder4_Ch2 1 2 1 650
preplace netloc Din1_3 1 1 1 440
preplace netloc RandomPulser_Pulse 1 0 1 NJ
preplace netloc axi_bram_ctrl_dac_bram_en_a 1 0 1 30
preplace netloc Twox2ChannelAdder5_Ch1 1 2 1 690
preplace netloc Twox2ChannelAdder5_Ch2 1 2 1 680
preplace netloc axi_bram_ctrl_dac_bram_addr_a 1 0 1 0
preplace netloc PulseFormer4_ShapedPulse 1 1 1 430
preplace netloc PulseFormer14_ShapedPulse 1 1 1 450
preplace netloc PulseFormer7_ShapedPulse 1 1 1 450
preplace netloc adc_dac_adc_clk 1 0 5 60 480 440 460 670 920 890 1760 1110
preplace netloc PulseFormer_ShapedPulse 1 1 1 430
preplace netloc Trig_Count1_1 1 0 1 NJ
preplace netloc ADD_Halve3_Dout 1 3 1 900
preplace netloc Trig_Count1_2 1 0 1 NJ
preplace netloc Trig_Count1_3 1 0 1 NJ
preplace netloc ADD_Halve4_Dout 1 3 1 870
preplace netloc Trig_Count1_4 1 0 1 NJ
levelinfo -pg 1 -20 260 550 780 1000 1210 1320 -top -70 -bot 4330
",
}

  # Restore current instance
  current_bd_instance $oldCurInst
}

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
  create_bd_pin -dir I -from 31 -to 0 status

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

# Hierarchical cell: RandomPulseSynthesizer
proc create_hier_cell_RandomPulseSynthesizer { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" create_hier_cell_RandomPulseSynthesizer() - Empty argument(s)!"}
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
  create_bd_pin -dir I -from 14 -to 0 BRAM_PORTA_addr
  create_bd_pin -dir I BRAM_PORTA_clk
  create_bd_pin -dir I -from 31 -to 0 BRAM_PORTA_din
  create_bd_pin -dir I BRAM_PORTA_en
  create_bd_pin -dir I BRAM_PORTA_rst
  create_bd_pin -dir I -from 3 -to 0 BRAM_PORTA_we
  create_bd_pin -dir O -from 13 -to 0 DACAShapedPulse
  create_bd_pin -dir O -from 13 -to 0 DACBShapedPulse
  create_bd_pin -dir O -from 0 -to 0 Pulse1
  create_bd_pin -dir I -from 0 -to 0 Reset_In
  create_bd_pin -dir I -from 31 -to 0 b
  create_bd_pin -dir I clk

  # Create instance: MultiplePulseShapers
  create_hier_cell_MultiplePulseShapers $hier_obj MultiplePulseShapers

  # Create instance: RandomPulser
  create_hier_cell_RandomPulser $hier_obj RandomPulser

  # Create port connections
  connect_bd_net -net MultiplePulseShapers_Ch1ShapedPulse [get_bd_pins DACAShapedPulse] [get_bd_pins MultiplePulseShapers/Ch1ShapedPulse]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins Pulse1] [get_bd_pins MultiplePulseShapers/Trig_Count] [get_bd_pins RandomPulser/Pulse1]
  connect_bd_net -net RandomPulser_Pulse2 [get_bd_pins MultiplePulseShapers/Trig_Count1] [get_bd_pins RandomPulser/Pulse2]
  connect_bd_net -net RandomPulser_Pulse3 [get_bd_pins MultiplePulseShapers/Trig_Count2] [get_bd_pins RandomPulser/Pulse3]
  connect_bd_net -net RandomPulser_Pulse4 [get_bd_pins MultiplePulseShapers/Trig_Count3] [get_bd_pins RandomPulser/Pulse4]
  connect_bd_net -net RandomPulser_Pulse5 [get_bd_pins MultiplePulseShapers/Trig_Count4] [get_bd_pins RandomPulser/Pulse5]
  connect_bd_net -net RandomPulser_Pulse6 [get_bd_pins MultiplePulseShapers/Trig_Count5] [get_bd_pins RandomPulser/Pulse6]
  connect_bd_net -net RandomPulser_Pulse7 [get_bd_pins MultiplePulseShapers/Trig_Count6] [get_bd_pins RandomPulser/Pulse7]
  connect_bd_net -net RandomPulser_Pulse8 [get_bd_pins MultiplePulseShapers/Trig_Count7] [get_bd_pins RandomPulser/Pulse8]
  connect_bd_net -net RandomPulser_Pulse9 [get_bd_pins MultiplePulseShapers/Trig_Count8] [get_bd_pins RandomPulser/Pulse9]
  connect_bd_net -net RandomPulser_Pulse10 [get_bd_pins MultiplePulseShapers/Trig_Count9] [get_bd_pins RandomPulser/Pulse10]
  connect_bd_net -net RandomPulser_Pulse11 [get_bd_pins MultiplePulseShapers/Trig_Count10] [get_bd_pins RandomPulser/Pulse11]
  connect_bd_net -net RandomPulser_Pulse12 [get_bd_pins MultiplePulseShapers/Trig_Count11] [get_bd_pins RandomPulser/Pulse12]
  connect_bd_net -net RandomPulser_Pulse13 [get_bd_pins MultiplePulseShapers/Trig_Count12] [get_bd_pins RandomPulser/Pulse13]
  connect_bd_net -net RandomPulser_Pulse14 [get_bd_pins MultiplePulseShapers/Trig_Count13] [get_bd_pins RandomPulser/Pulse14]
  connect_bd_net -net RandomPulser_Pulse15 [get_bd_pins MultiplePulseShapers/Trig_Count14] [get_bd_pins RandomPulser/Pulse15]
  connect_bd_net -net RandomPulser_Pulse16 [get_bd_pins MultiplePulseShapers/Trig_Count15] [get_bd_pins RandomPulser/Pulse16]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins clk] [get_bd_pins MultiplePulseShapers/clk] [get_bd_pins RandomPulser/clk]
  connect_bd_net -net axi_bram_ctrl_dac_bram_addr_a [get_bd_pins BRAM_PORTA_addr] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_addr]
  connect_bd_net -net axi_bram_ctrl_dac_bram_clk_a [get_bd_pins BRAM_PORTA_clk] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_clk]
  connect_bd_net -net axi_bram_ctrl_dac_bram_en_a [get_bd_pins BRAM_PORTA_en] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_en]
  connect_bd_net -net axi_bram_ctrl_dac_bram_rst_a [get_bd_pins BRAM_PORTA_rst] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_rst]
  connect_bd_net -net axi_bram_ctrl_dac_bram_we_a [get_bd_pins BRAM_PORTA_we] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_we]
  connect_bd_net -net axi_bram_ctrl_dac_bram_wrdata_a [get_bd_pins BRAM_PORTA_din] [get_bd_pins MultiplePulseShapers/BRAM_PORTA_din]
  connect_bd_net -net blk_mem_gen_dac_doutb [get_bd_pins DACBShapedPulse] [get_bd_pins MultiplePulseShapers/Ch2ShapedPulse]
  connect_bd_net -net cfg_simulationpulsefreq [get_bd_pins b] [get_bd_pins RandomPulser/b]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins Reset_In] [get_bd_pins MultiplePulseShapers/rstb] [get_bd_pins RandomPulser/Reset_In]

  # Perform GUI Layout
  regenerate_bd_layout -hierarchy [get_bd_cells /RandomPulseSynthesizer] -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port BRAM_PORTA_rst -pg 1 -y 500 -defaultsOSRD
preplace port BRAM_PORTA_en -pg 1 -y 480 -defaultsOSRD
preplace port BRAM_PORTA_clk -pg 1 -y 440 -defaultsOSRD
preplace port clk -pg 1 -y 230 -defaultsOSRD
preplace portBus DACBShapedPulse -pg 1 -y 270 -defaultsOSRD
preplace portBus DACAShapedPulse -pg 1 -y 330 -defaultsOSRD
preplace portBus Pulse1 -pg 1 -y 10 -defaultsOSRD
preplace portBus BRAM_PORTA_din -pg 1 -y 460 -defaultsOSRD
preplace portBus b -pg 1 -y 210 -defaultsOSRD
preplace portBus BRAM_PORTA_we -pg 1 -y 520 -defaultsOSRD
preplace portBus BRAM_PORTA_addr -pg 1 -y 420 -defaultsOSRD
preplace portBus Reset_In -pg 1 -y 190 -defaultsOSRD
preplace inst RandomPulser -pg 1 -lvl 1 -y 210 -defaultsOSRD
preplace inst MultiplePulseShapers -pg 1 -lvl 2 -y 300 -defaultsOSRD
preplace netloc axi_bram_ctrl_dac_bram_rst_a 1 0 2 NJ 500 NJ
preplace netloc RandomPulser_Pulse16 1 1 1 280
preplace netloc RandomPulser_Pulse6 1 1 1 340
preplace netloc RandomPulser_Pulse7 1 1 1 450
preplace netloc axi_bram_ctrl_dac_bram_wrdata_a 1 0 2 NJ 460 NJ
preplace netloc RandomPulser_Pulse8 1 1 1 430
preplace netloc RandomPulser_Pulse 1 1 2 480 10 NJ
preplace netloc proc_sys_reset_adc_clk_peripheral_reset 1 0 2 -20 530 NJ
preplace netloc RandomPulser_Pulse9 1 1 1 300
preplace netloc MultiplePulseShapers_Ch1ShapedPulse 1 2 1 980
preplace netloc axi_bram_ctrl_dac_bram_clk_a 1 0 2 NJ 440 NJ
preplace netloc blk_mem_gen_dac_doutb 1 2 1 990
preplace netloc axi_bram_ctrl_dac_bram_we_a 1 0 2 NJ 520 NJ
preplace netloc adc_dac_adc_clk 1 0 2 -30 510 NJ
preplace netloc axi_bram_ctrl_dac_bram_addr_a 1 0 2 NJ 420 NJ
preplace netloc RandomPulser_Pulse10 1 1 1 390
preplace netloc RandomPulser_Pulse11 1 1 1 350
preplace netloc cfg_simulationpulsefreq 1 0 1 NJ
preplace netloc RandomPulser_Pulse12 1 1 1 330
preplace netloc RandomPulser_Pulse2 1 1 1 350
preplace netloc RandomPulser_Pulse13 1 1 1 320
preplace netloc RandomPulser_Pulse3 1 1 1 400
preplace netloc axi_bram_ctrl_dac_bram_en_a 1 0 2 NJ 480 NJ
preplace netloc RandomPulser_Pulse14 1 1 1 310
preplace netloc RandomPulser_Pulse4 1 1 1 380
preplace netloc RandomPulser_Pulse15 1 1 1 290
preplace netloc RandomPulser_Pulse5 1 1 1 360
levelinfo -pg 1 -50 150 800 1010 -top -10 -bot 580
",
}

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

  # Create instance: RandomPulseSynthesizer
  create_hier_cell_RandomPulseSynthesizer [current_bd_instance .] RandomPulseSynthesizer

  # Create instance: Set_Reset_State
  create_hier_cell_Set_Reset_State [current_bd_instance .] Set_Reset_State

  # Create instance: TrigState, and set properties
  set TrigState [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 TrigState ]
  set_property -dict [ list \
CONFIG.DIN_FROM {1} \
CONFIG.DIN_TO {1} \
CONFIG.DOUT_WIDTH {1} \
 ] $TrigState

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

  # Create instance: c_counter_binary_0, and set properties
  set c_counter_binary_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 ]
  set_property -dict [ list \
CONFIG.CE {true} \
CONFIG.Output_Width {16} \
 ] $c_counter_binary_0

  # Create instance: cfg
  create_hier_cell_cfg [current_bd_instance .] cfg

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

  # Create instance: xlconstant_2, and set properties
  set xlconstant_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_2 ]
  set_property -dict [ list \
CONFIG.CONST_VAL {0} \
CONFIG.CONST_WIDTH {32} \
 ] $xlconstant_2

  # Create instance: xlslice_0, and set properties
  set xlslice_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0 ]
  set_property -dict [ list \
CONFIG.DIN_FROM {15} \
CONFIG.DIN_TO {8} \
CONFIG.DIN_WIDTH {16} \
CONFIG.DOUT_WIDTH {8} \
 ] $xlslice_0

  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_pins axi_mem_intercon_0/S00_AXI] [get_bd_intf_pins ps_0/M_AXI_GP0]
  connect_bd_intf_net -intf_net adc_clock_converter_M_AXIS [get_bd_intf_pins adc_axis_fifo/AXI_STR_RXD] [get_bd_intf_pins adc_clock_converter/M_AXIS]
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
  connect_bd_net -net RandomPulseSynthesizer_DACBShapedPulse [get_bd_pins RandomPulseSynthesizer/DACBShapedPulse] [get_bd_pins adc_dac/dac2]
  connect_bd_net -net RandomPulser_Pulse [get_bd_pins RandomPulseSynthesizer/Pulse1] [get_bd_pins c_counter_binary_0/CE]
  connect_bd_net -net S00_ARESETN_1 [get_bd_pins adc_axis_fifo/s_axi_aresetn] [get_bd_pins adc_clock_converter/m_axis_aresetn] [get_bd_pins axi_bram_ctrl_dac/s_axi_aresetn] [get_bd_pins axi_mem_intercon_0/M00_ARESETN] [get_bd_pins axi_mem_intercon_0/M01_ARESETN] [get_bd_pins axi_mem_intercon_0/M02_ARESETN] [get_bd_pins axi_mem_intercon_0/M03_ARESETN] [get_bd_pins axi_mem_intercon_0/S00_ARESETN] [get_bd_pins cfg/s_axi_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins sts/s_axi_aresetn]
  connect_bd_net -net Set_Reset_State_State [get_bd_pins Set_Reset_State/State] [get_bd_pins xlconcat_3/In3]
  connect_bd_net -net TrigState_Dout [get_bd_pins TrigState/Dout] [get_bd_pins util_vector_logic_1/Op1]
  connect_bd_net -net adc_clk_n_i_1 [get_bd_ports adc_clk_n_i] [get_bd_pins adc_dac/clk_in1_n]
  connect_bd_net -net adc_clk_p_i_1 [get_bd_ports adc_clk_p_i] [get_bd_pins adc_dac/clk_in1_p]
  connect_bd_net -net adc_dac_adc1 [get_bd_pins Averager_0/B] [get_bd_pins adc_dac/adc1]
  connect_bd_net -net adc_dac_adc2 [get_bd_pins Averager_1/B] [get_bd_pins adc_dac/adc2]
  connect_bd_net -net adc_dac_adc_cdcs_o [get_bd_ports adc_cdcs_o] [get_bd_pins adc_dac/adc_cdcs_o]
  connect_bd_net -net adc_dac_adc_clk [get_bd_pins Acquisition_Control/clk] [get_bd_pins Averager_0/clk] [get_bd_pins Averager_1/clk] [get_bd_pins RandomPulseSynthesizer/clk] [get_bd_pins Set_Reset_State/clk] [get_bd_pins adc_clock_converter/s_axis_aclk] [get_bd_pins adc_dac/adc_clk] [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins cfg/m_axi_aclk] [get_bd_pins delay1_0/clk] [get_bd_pins proc_sys_reset_adc_clk/slowest_sync_clk] [get_bd_pins sts/m_axi_aclk]
  connect_bd_net -net adc_dac_adc_clk_source [get_bd_ports adc_clk_source] [get_bd_pins adc_dac/adc_clk_source]
  connect_bd_net -net adc_dac_dac_clk_o [get_bd_ports dac_clk_o] [get_bd_pins adc_dac/dac_clk_o]
  connect_bd_net -net adc_dac_dac_dat_o [get_bd_ports dac_dat_o] [get_bd_pins adc_dac/dac_dat_o]
  connect_bd_net -net adc_dac_dac_rst_o [get_bd_ports dac_rst_o] [get_bd_pins adc_dac/dac_rst_o]
  connect_bd_net -net adc_dac_dac_sel_o [get_bd_ports dac_sel_o] [get_bd_pins adc_dac/dac_sel_o]
  connect_bd_net -net adc_dac_dac_wrt_o [get_bd_ports dac_wrt_o] [get_bd_pins adc_dac/dac_wrt_o]
  connect_bd_net -net adc_dat_a_i_1 [get_bd_ports adc_dat_a_i] [get_bd_pins adc_dac/adc_dat_a_i]
  connect_bd_net -net adc_dat_b_i_1 [get_bd_ports adc_dat_b_i] [get_bd_pins adc_dac/adc_dat_b_i]
  connect_bd_net -net axi_bram_ctrl_dac_bram_addr_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_addr] [get_bd_pins axi_bram_ctrl_dac/bram_addr_a]
  connect_bd_net -net axi_bram_ctrl_dac_bram_clk_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_clk] [get_bd_pins axi_bram_ctrl_dac/bram_clk_a]
  connect_bd_net -net axi_bram_ctrl_dac_bram_en_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_en] [get_bd_pins axi_bram_ctrl_dac/bram_en_a]
  connect_bd_net -net axi_bram_ctrl_dac_bram_rst_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_rst] [get_bd_pins axi_bram_ctrl_dac/bram_rst_a]
  connect_bd_net -net axi_bram_ctrl_dac_bram_we_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_we] [get_bd_pins axi_bram_ctrl_dac/bram_we_a]
  connect_bd_net -net axi_bram_ctrl_dac_bram_wrdata_a [get_bd_pins RandomPulseSynthesizer/BRAM_PORTA_din] [get_bd_pins axi_bram_ctrl_dac/bram_wrdata_a]
  connect_bd_net -net c_counter_binary_0_Q [get_bd_pins c_counter_binary_0/Q] [get_bd_pins xlslice_0/Din]
  connect_bd_net -net cfg_acquisitionlength [get_bd_pins Acquisition_Control/Acquistion_length_us] [get_bd_pins cfg/acquisitionlength]
  connect_bd_net -net cfg_arm_softtrig [get_bd_pins ArmState/Din] [get_bd_pins TrigState/Din] [get_bd_pins cfg/arm_softtrig]
  connect_bd_net -net cfg_simulationpulsefreq [get_bd_pins RandomPulseSynthesizer/b] [get_bd_pins cfg/simulationpulsefreq]
  connect_bd_net -net dac1_1 [get_bd_pins RandomPulseSynthesizer/DACAShapedPulse] [get_bd_pins adc_dac/dac1]
  connect_bd_net -net delay1_0_dout [get_bd_pins delay1_0/dout] [get_bd_pins util_vector_logic_1/Op2]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_aresetn [get_bd_pins adc_clock_converter/s_axis_aresetn] [get_bd_pins cfg/m_axi_aresetn] [get_bd_pins proc_sys_reset_adc_clk/peripheral_aresetn] [get_bd_pins sts/m_axi_aresetn]
  connect_bd_net -net proc_sys_reset_adc_clk_peripheral_reset [get_bd_pins RandomPulseSynthesizer/Reset_In] [get_bd_pins proc_sys_reset_adc_clk/peripheral_reset]
  connect_bd_net -net ps_0_FCLK_CLK0 [get_bd_pins adc_axis_fifo/s_axi_aclk] [get_bd_pins adc_clock_converter/m_axis_aclk] [get_bd_pins axi_bram_ctrl_dac/s_axi_aclk] [get_bd_pins axi_mem_intercon_0/ACLK] [get_bd_pins axi_mem_intercon_0/M00_ACLK] [get_bd_pins axi_mem_intercon_0/M01_ACLK] [get_bd_pins axi_mem_intercon_0/M02_ACLK] [get_bd_pins axi_mem_intercon_0/M03_ACLK] [get_bd_pins axi_mem_intercon_0/S00_ACLK] [get_bd_pins cfg/s_axi_aclk] [get_bd_pins proc_sys_reset_0/slowest_sync_clk] [get_bd_pins ps_0/FCLK_CLK0] [get_bd_pins ps_0/M_AXI_GP0_ACLK] [get_bd_pins sts/s_axi_aclk]
  connect_bd_net -net ps_0_FCLK_RESET0_N [get_bd_pins proc_sys_reset_0/ext_reset_in] [get_bd_pins proc_sys_reset_adc_clk/ext_reset_in] [get_bd_pins ps_0/FCLK_RESET0_N]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins adc_clock_converter/s_axis_tvalid] [get_bd_pins util_vector_logic_0/Res]
  connect_bd_net -net util_vector_logic_1_Res [get_bd_pins Acquisition_Control/trig] [get_bd_pins Set_Reset_State/SetState] [get_bd_pins util_vector_logic_1/Res] [get_bd_pins xlconcat_3/In1]
  connect_bd_net -net xlconcat_0_dout [get_bd_pins adc_clock_converter/s_axis_tdata] [get_bd_pins xlconcat_0/dout]
  connect_bd_net -net xlconcat_3_dout [get_bd_pins sts/state] [get_bd_pins xlconcat_3/dout]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins xlconcat_3/In4] [get_bd_pins xlconstant_0/dout]
  connect_bd_net -net xlconstant_2_dout [get_bd_pins WillBeInputTigger/dout] [get_bd_pins delay1_0/din]
  connect_bd_net -net xlconstant_2_dout1 [get_bd_pins axi_bram_ctrl_dac/bram_rddata_a] [get_bd_pins sts/device_version] [get_bd_pins sts/status] [get_bd_pins xlconstant_2/dout]
  connect_bd_net -net xlslice_0_Dout [get_bd_ports led_o] [get_bd_pins xlslice_0/Dout]

  # Create address segments
  create_bd_addr_seg -range 0x00010000 -offset 0x43C10000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0] SEG_adc_axis_fifo_Mem0
  create_bd_addr_seg -range 0x00008000 -offset 0x40000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs axi_bram_ctrl_dac/S_AXI/Mem0] SEG_axi_bram_ctrl_dac_Mem0
  create_bd_addr_seg -range 0x00001000 -offset 0x60000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs cfg/axi_cfg_register_0/s_axi/reg0] SEG_axi_cfg_register_0_reg0
  create_bd_addr_seg -range 0x00001000 -offset 0x50000000 [get_bd_addr_spaces ps_0/Data] [get_bd_addr_segs sts/axi_sts_register_0/s_axi/reg0] SEG_axi_sts_register_0_reg0

  # Perform GUI Layout
  regenerate_bd_layout -layout_string {
   guistr: "# # String gsaved with Nlview 6.5.12  2016-01-29 bk=1.3547 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port DDR -pg 1 -y -280 -defaultsOSRD
preplace port Vp_Vn -pg 1 -y -220 -defaultsOSRD
preplace port Vaux0 -pg 1 -y -300 -defaultsOSRD
preplace port Vaux1 -pg 1 -y -280 -defaultsOSRD
preplace port adc_clk_p_i -pg 1 -y 890 -defaultsOSRD
preplace port dac_rst_o -pg 1 -y 910 -defaultsOSRD
preplace port dac_clk_o -pg 1 -y 870 -defaultsOSRD
preplace port FIXED_IO -pg 1 -y 510 -defaultsOSRD
preplace port dac_sel_o -pg 1 -y 930 -defaultsOSRD
preplace port adc_cdcs_o -pg 1 -y 810 -defaultsOSRD
preplace port dac_wrt_o -pg 1 -y 950 -defaultsOSRD
preplace port Vaux8 -pg 1 -y -260 -defaultsOSRD
preplace port adc_clk_n_i -pg 1 -y 870 -defaultsOSRD
preplace port Vaux9 -pg 1 -y -240 -defaultsOSRD
preplace portBus adc_dat_b_i -pg 1 -y 850 -defaultsOSRD
preplace portBus adc_clk_source -pg 1 -y 850 -defaultsOSRD
preplace portBus led_o -pg 1 -y 530 -defaultsOSRD
preplace portBus dac_pwm_o -pg 1 -y -300 -defaultsOSRD
preplace portBus adc_dat_a_i -pg 1 -y 830 -defaultsOSRD
preplace portBus dac_dat_o -pg 1 -y 890 -defaultsOSRD
preplace inst sts -pg 1 -lvl 17 -y 80 -defaultsOSRD
preplace inst xlslice_0 -pg 1 -lvl 11 -y 530 -defaultsOSRD
preplace inst delay1_0 -pg 1 -lvl 11 -y -90 -defaultsOSRD
preplace inst axi_mem_intercon_0 -pg 1 -lvl 7 -y 30 -defaultsOSRD
preplace inst xlconstant_0 -pg 1 -lvl 15 -y -170 -defaultsOSRD
preplace inst Averager_0 -pg 1 -lvl 16 -y 610 -defaultsOSRD
preplace inst RandomPulseSynthesizer -pg 1 -lvl 9 -y 180 -defaultsOSRD
preplace inst Averager_1 -pg 1 -lvl 16 -y 740 -defaultsOSRD
preplace inst xlconstant_2 -pg 1 -lvl 16 -y 400 -defaultsOSRD
preplace inst TrigState -pg 1 -lvl 13 -y -140 -defaultsOSRD
preplace inst xlconcat_0 -pg 1 -lvl 17 -y 620 -defaultsOSRD
preplace inst util_vector_logic_0 -pg 1 -lvl 16 -y 270 -defaultsOSRD
preplace inst proc_sys_reset_adc_clk -pg 1 -lvl 11 -y 330 -defaultsOSRD
preplace inst proc_sys_reset_0 -pg 1 -lvl 6 -y 0 -defaultsOSRD
preplace inst adc_clock_converter -pg 1 -lvl 18 -y 400 -defaultsOSRD
preplace inst util_vector_logic_1 -pg 1 -lvl 14 -y -130 -defaultsOSRD
preplace inst c_counter_binary_0 -pg 1 -lvl 9 -y 530 -defaultsOSRD
preplace inst cfg -pg 1 -lvl 12 -y 220 -defaultsOSRD
preplace inst adc_axis_fifo -pg 1 -lvl 19 -y -20 -defaultsOSRD
preplace inst Set_Reset_State -pg 1 -lvl 15 -y -60 -defaultsOSRD
preplace inst xlconcat_3 -pg 1 -lvl 16 -y -190 -defaultsOSRD
preplace inst axi_bram_ctrl_dac -pg 1 -lvl 8 -y 150 -defaultsOSRD
preplace inst adc_dac -pg 1 -lvl 10 -y 880 -defaultsOSRD
preplace inst WillBeInputTigger -pg 1 -lvl 10 -y -80 -defaultsOSRD
preplace inst ArmState -pg 1 -lvl 13 -y -40 -defaultsOSRD
preplace inst ps_0 -pg 1 -lvl 5 -y -180 -defaultsOSRD
preplace inst Acquisition_Control -pg 1 -lvl 15 -y 190 -defaultsOSRD
preplace netloc axi_bram_ctrl_dac_bram_rst_a 1 8 1 1660
preplace netloc adc_dac_dac_wrt_o 1 10 10 NJ 950 NJ 950 NJ 950 NJ 950 NJ 950 NJ 950 NJ 950 NJ 950 NJ 950 NJ
preplace netloc xlconstant_2_dout 1 10 1 NJ
preplace netloc adc_dac_dac_clk_o 1 10 10 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ
preplace netloc adc_dac_adc_clk_source 1 10 10 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ
preplace netloc Averager_0_AverageVal 1 16 1 N
preplace netloc axi_bram_ctrl_dac_bram_wrdata_a 1 8 1 N
preplace netloc adc_dac_adc1 1 10 6 2450 590 NJ 590 NJ 590 NJ 590 NJ 590 NJ
preplace netloc RandomPulser_Pulse 1 8 2 1710 350 2090
preplace netloc axi_mem_intercon_0_M02_AXI 1 7 1 1310
preplace netloc axi_mem_intercon_0_M00_AXI 1 7 5 NJ 0 NJ 0 NJ 0 NJ 0 2860
preplace netloc proc_sys_reset_adc_clk_peripheral_reset 1 8 4 1680 310 NJ 230 NJ 230 2810
preplace netloc adc_dac_adc2 1 10 6 2470 720 NJ 720 NJ 720 NJ 720 NJ 720 NJ
preplace netloc ArmState_Dout 1 13 3 NJ -70 3680 -230 NJ
preplace netloc util_vector_logic_0_Res 1 16 2 NJ 270 4590
preplace netloc cfg_arm_softtrig 1 12 1 3240
preplace netloc axi_bram_ctrl_dac_bram_clk_a 1 8 1 N
preplace netloc Set_Reset_State_State 1 15 1 4030
preplace netloc Averager_1_AverageVal 1 16 1 4320
preplace netloc ARESETN_1 1 6 1 980
preplace netloc delay1_0_dout 1 11 3 NJ -190 NJ -190 NJ
preplace netloc axi_bram_ctrl_dac_bram_we_a 1 8 1 1670
preplace netloc adc_dat_a_i_1 1 0 10 NJ 830 NJ 830 NJ 830 NJ 830 NJ 830 NJ 830 NJ 830 NJ 830 NJ 830 NJ
preplace netloc adc_dac_adc_clk 1 8 10 1700 360 NJ 360 2460 200 2840 330 NJ 330 NJ 330 3700 330 4050 330 4330 330 NJ
preplace netloc adc_dac_adc_cdcs_o 1 10 10 NJ 810 NJ 810 NJ 810 NJ 810 NJ 810 NJ 810 NJ 810 NJ 810 NJ 810 NJ
preplace netloc adc_clk_n_i_1 1 0 10 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ 870 NJ
preplace netloc Acquisition_Control_MHz_clk 1 15 1 4030
preplace netloc xlconcat_3_dout 1 16 1 4320
preplace netloc dac1_1 1 9 1 2140
preplace netloc adc_dac_dac_dat_o 1 10 10 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ
preplace netloc xlconstant_0_dout 1 15 1 NJ
preplace netloc xlconcat_0_dout 1 17 1 4590
preplace netloc ps_0_FCLK_CLK0 1 4 15 210 -30 610 -120 990 -150 1320 20 NJ 20 NJ 20 NJ 20 2850 30 NJ 30 NJ 30 NJ 30 NJ 30 4300 -50 4580 -10 NJ
preplace netloc c_counter_binary_0_Q 1 9 2 NJ 530 NJ
preplace netloc axi_bram_ctrl_dac_bram_addr_a 1 8 1 N
preplace netloc S00_AXI_1 1 5 2 NJ -200 NJ
preplace netloc RandomPulseSynthesizer_DACBShapedPulse 1 9 1 2100
preplace netloc Acquisition_Control_Acq_Valid 1 15 1 4050
preplace netloc ps_0_DDR 1 5 15 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ -290 NJ
preplace netloc adc_clock_converter_M_AXIS 1 18 1 4860
preplace netloc cfg_simulationpulsefreq 1 8 5 1690 320 NJ 220 NJ 220 NJ 320 3230
preplace netloc adc_dac_dac_rst_o 1 10 10 NJ 910 NJ 910 NJ 910 NJ 910 NJ 910 NJ 910 NJ 910 NJ 910 NJ 910 NJ
preplace netloc util_vector_logic_1_Res 1 14 2 3690 -220 NJ
preplace netloc ps_0_FCLK_RESET0_N 1 5 6 620 -170 NJ -170 NJ -170 NJ -170 NJ -170 2450
preplace netloc proc_sys_reset_adc_clk_peripheral_aresetn 1 11 7 2860 340 NJ 340 NJ 340 NJ 340 NJ 340 4340 340 NJ
preplace netloc adc_dat_b_i_1 1 0 10 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ 850 NJ
preplace netloc TrigState_Dout 1 13 1 NJ
preplace netloc ps_0_FIXED_IO 1 5 15 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ -280 NJ
preplace netloc axi_mem_intercon_0_M03_AXI 1 7 12 NJ -20 NJ -20 NJ -20 NJ -20 NJ -20 NJ 20 NJ 20 NJ 20 NJ -80 NJ -80 NJ -80 NJ
preplace netloc axi_bram_ctrl_dac_bram_en_a 1 8 1 1650
preplace netloc axi_mem_intercon_0_M01_AXI 1 7 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ 10 NJ
preplace netloc xlslice_0_Dout 1 11 9 NJ 530 NJ 530 NJ 530 NJ 530 NJ 530 NJ 530 NJ 530 NJ 530 N
preplace netloc xlconstant_2_dout1 1 8 9 1640 330 NJ 240 NJ 240 NJ 350 NJ 350 NJ 350 NJ 350 NJ 350 4310
preplace netloc cfg_acquisitionlength 1 12 3 N 170 NJ 170 NJ
preplace netloc adc_dac_dac_sel_o 1 10 10 NJ 930 NJ 930 NJ 930 NJ 930 NJ 930 NJ 930 NJ 930 NJ 930 NJ 930 NJ
preplace netloc adc_clk_p_i_1 1 0 10 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ 890 NJ
preplace netloc S00_ARESETN_1 1 6 13 1000 210 1310 340 NJ 340 NJ 210 NJ 210 2870 110 NJ 110 NJ 110 NJ 110 NJ 110 4290 -60 4600 10 NJ
levelinfo -pg 1 0 40 90 140 190 410 810 1150 1490 1900 2300 2640 3050 3340 3560 3860 4170 4450 4730 5020 5210 -top -330 -bot 1570
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


common::send_msg_id "BD_TCL-1000" "WARNING" "This Tcl script was generated from a block design that has not been validated. It is possible that design <$design_name> may result in errors during validation."

