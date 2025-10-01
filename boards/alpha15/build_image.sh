make -j CFG=examples/alpha15/adc-dac-bram/config.mk
make -j CFG=examples/alpha15/adc-dac-dma/config.mk
make -j CFG=examples/alpha15/signal-analyzer/config.mk \
	COPY_INSTRUMENTS="adc-dac-bram adc-dac-dma" image