make -j CFG=examples/zedboard/picoblaze/config.mk
make -j CFG=examples/zedboard/led-blinker/config.mk \
		COPY_INSTRUMENTS="picoblaze" image