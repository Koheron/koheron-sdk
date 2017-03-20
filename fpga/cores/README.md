# Cores

Cores are written in Verilog HDL and can be tested using commands similar to:

```
make CORE=address_generator_v1_0 test_core
```
They will automatically be compiled into the tmp folder for use in th eproject if the core is listed in the config.yml file defining the instrument:
cores:
  - redp_adc_v1_0
  - redp_dac_v1_0
  - axi_cfg_register_v1_0
  - axi_sts_register_v1_0
etc....

