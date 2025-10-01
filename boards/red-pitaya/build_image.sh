make -j CFG=examples/red-pitaya/adc-dac-bram/config.mk
make -j CFG=examples/red-pitaya/fft/config.mk \
		COPY_INSTRUMENTS="adc-dac-bram" image