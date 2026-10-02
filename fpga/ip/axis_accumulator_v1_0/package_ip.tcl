# Imported as an XCI; output products are regenerated for the parent target.
set_property top axis_accumulator [current_fileset]
file mkdir $output_path/axis_accumulator_vendor
create_ip -name floating_point -vendor xilinx.com -library ip -version 7.1 \
    -module_name axis_accumulator_add -dir $output_path/axis_accumulator_vendor
set_property -dict {
    CONFIG.Operation_Type Add_Subtract
    CONFIG.Add_Sub_Value Add
    CONFIG.A_Precision_Type Single
    CONFIG.Result_Precision_Type Single
    CONFIG.Flow_Control NonBlocking
    CONFIG.C_Optimization Low_Latency
    CONFIG.Maximum_Latency false
    CONFIG.C_Latency 8
    CONFIG.C_Rate 1
    CONFIG.C_Mult_Usage No_Usage
    CONFIG.Has_ACLKEN false
    CONFIG.Has_ARESETn true
} [get_ips axis_accumulator_add]
