make -j CFG=examples/alpha250-4/adc-bram/config.mk
make -j CFG=examples/alpha250-4/fft/config.mk \
	COPY_INSTRUMENTS="adc-bram" image