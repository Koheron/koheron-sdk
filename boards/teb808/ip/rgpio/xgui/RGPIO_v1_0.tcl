# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  set C_TYP [ipgui::add_param $IPINST -name "C_TYP" -parent ${Page_0} -widget comboBox]
  set_property tooltip {Interface Typ} ${C_TYP}
  set C_ADD_RESERVED [ipgui::add_param $IPINST -name "C_ADD_RESERVED" -parent ${Page_0} -widget comboBox]
  set_property tooltip {Enabled reserved signals. Note this will be use for other purpose on future update} ${C_ADD_RESERVED}


}

proc update_PARAM_VALUE.C_ADD_RESERVED { PARAM_VALUE.C_ADD_RESERVED } {
	# Procedure called to update C_ADD_RESERVED when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_ADD_RESERVED { PARAM_VALUE.C_ADD_RESERVED } {
	# Procedure called to validate C_ADD_RESERVED
	return true
}

proc update_PARAM_VALUE.C_TYP { PARAM_VALUE.C_TYP } {
	# Procedure called to update C_TYP when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_TYP { PARAM_VALUE.C_TYP } {
	# Procedure called to validate C_TYP
	return true
}


proc update_MODELPARAM_VALUE.C_ADD_RESERVED { MODELPARAM_VALUE.C_ADD_RESERVED PARAM_VALUE.C_ADD_RESERVED } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_ADD_RESERVED}] ${MODELPARAM_VALUE.C_ADD_RESERVED}
}

proc update_MODELPARAM_VALUE.C_TYP { MODELPARAM_VALUE.C_TYP PARAM_VALUE.C_TYP } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_TYP}] ${MODELPARAM_VALUE.C_TYP}
}

