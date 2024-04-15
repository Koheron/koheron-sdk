# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"

}

proc update_PARAM_VALUE.C_GENERIC { PARAM_VALUE.C_GENERIC } {
	# Procedure called to update C_GENERIC when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_GENERIC { PARAM_VALUE.C_GENERIC } {
	# Procedure called to validate C_GENERIC
	return true
}


proc update_MODELPARAM_VALUE.C_GENERIC { MODELPARAM_VALUE.C_GENERIC PARAM_VALUE.C_GENERIC } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_GENERIC}] ${MODELPARAM_VALUE.C_GENERIC}
}

