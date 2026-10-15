# Shared DPLL controller, continuous monitor and web workspace.
DPLL_PATH := $(SDK_PATH)/examples/alpha250/dpll
CORES += $(addprefix $(SDK_PATH)/fpga/cores/,axis_constant_v1_0 latched_mux_v1_0 axis_lfsr_v1_0 boxcar_filter_v1_0 phase_unwrapper_v1_0 phase_stream_control_v1_0 phase_fixed_decimator_v1_0 phase_cic_decimator_v1_0 phase_stream_cdc_v1_0 phase_range_guard_v1_0 phase_quantizer_v1_0 phase_prefilter_v1_0 phase_stochastic_round_v1_0 axis_stream_packet_mux_v1_0)
TCL_EXTRA_FILES += $(wildcard $(DPLL_PATH)/tcl/*.tcl) $(wildcard $(DPLL_PATH)/*.v)
FPGA_IMPL_TCL = $(FPGA_TCL_HOLD_FIX) $(DPLL_PATH)/tcl/gain_programming_timing.tcl $(DPLL_PATH)/tcl/replicate_gain_control.tcl $(DPLL_PATH)/tcl/post_route_opt.tcl $(DPLL_PATH)/tcl/reference_launch_routing.tcl
DRIVERS += $(DPLL_PATH)/dpll.hpp $(DPLL_PATH)/dma.hpp
DRIVERS += $(SDK_PATH)/server/external_libs/pffft/pffft.cpp
PNA_PUBLICATION_HZ ?= 30
SERVER_EXTRA_CCXXFLAGS += -DKOHERON_PNA_PUBLICATION_HZ=$(PNA_PUBLICATION_HZ)
WEB_FILES += $(SDK_PATH)/server/external_libs/pffft/pffft-LICENSE.txt
include $(SDK_PATH)/web/phase-noise/analyzer/monitor.mk
include $(SDK_PATH)/web/clock-generator/driver.mk
include $(SDK_PATH)/web/clock-generator/reference-clock.mk
WEB_FILES += $(filter-out $(DPLL_PATH)/web/app.ts,$(wildcard $(DPLL_PATH)/web/*.ts $(DPLL_PATH)/web/*.html $(DPLL_PATH)/web/*.css $(DPLL_PATH)/web/*.svg))
