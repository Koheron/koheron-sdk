set -e

echo 'Test config.yml'
python scripts/make.py --test

# Test modiles

for module in address averager peak_detector spectrum
do
	echo 'Test config.yml for module' $module
	python scripts/make.py --split_config_yml $module fpga/modules/
done

for instrument in led_blinker adc_dac laser_controller pulse_generator oscillo spectrum
do
    make NAME=$instrument server
done
