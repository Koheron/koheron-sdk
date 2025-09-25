


make -j CFG=examples/alpha250/phase-noise-analyzer/config.mk
make -j CFG=examples/alpha250/adc-dac-bram/config.mk
make -j CFG=examples/alpha250/adc-dac-dma/config.mk
make -j CFG=examples/alpha250/fft/config.mk COPY_INSTRUMENTS="phase-noise-analyzer adc-dac-bram adc-dac-dma" image