# Transitive shared Tcl dependencies for the standard Zynq instruments.
# Keep the conservative default for custom designs that do not opt in.
FPGA_TCL_BASE = $(addprefix $(FPGA_PATH)/lib/,utilities.tcl starting_point.tcl ctl_sts.tcl)
FPGA_TCL_REDP = $(addprefix $(FPGA_PATH)/lib/,redp_adc_dac.tcl xadc.tcl)
FPGA_TCL_BRAM = $(FPGA_TCL_BASE) $(FPGA_PATH)/lib/bram.tcl
FPGA_TCL_RECORDER = $(FPGA_TCL_BRAM) $(FPGA_PATH)/lib/bram_recorder.tcl
FPGA_TCL_FFT = $(FPGA_TCL_RECORDER) $(FPGA_PATH)/lib/power_spectral_density.tcl
FPGA_TCL_PNA = $(FPGA_TCL_BASE) $(addprefix $(FPGA_PATH)/lib/,pna_cordic.tcl pna_filter.tcl)
FPGA_TCL_PNA_SINGLE_STREAM = $(FPGA_TCL_PNA) $(FPGA_PATH)/lib/pna_single_stream.tcl

# These hooks run only during implementation. Editing them must reset impl_1,
# but does not require project regeneration or synthesis.
FPGA_TCL_HOLD_FIX = $(FPGA_PATH)/lib/post_route_hold_fix.tcl
FPGA_TCL_PNA_ROUTE = $(FPGA_TCL_HOLD_FIX) $(FPGA_PATH)/lib/pna_post_route.tcl $(PROJECT_PATH)/post_route.tcl
