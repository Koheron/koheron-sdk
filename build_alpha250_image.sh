
make -j CFG=examples/alpha15/signal-analyzer/config.mk image

make -j CFG=examples/alpha250-4/adc-bram/config.mk
make -j CFG=examples/alpha250-4/fft/config.mk COPY_INSTRUMENTS="adc-bram" image

make -j CFG=examples/alpha250/phase-noise-analyzer/config.mk
make -j CFG=examples/alpha250/adc-dac-bram/config.mk
make -j CFG=examples/alpha250/adc-dac-dma/config.mk
make -j CFG=examples/alpha250/fft/config.mk COPY_INSTRUMENTS="phase-noise-analyzer adc-dac-bram adc-dac-dma" image