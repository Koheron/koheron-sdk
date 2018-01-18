#include "ModuleConfigConstants.h"



//-------------------------------------------------------------------------------------------------
// Mars AX3
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_AX3

char MARS_AX3_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mars AX3";
ModuleConfigProperty_t MARS_AX3_CONFIG_PROPERTIES[MARS_AX3_PROPERTY_COUNT] = {
{ "FPGA type\0", 0x08, 4, 4, 7, 1, 4, 0, 4, (ModulePropertyValueKey_t*)&MARS_AX3_FPGA_TYPE_VALUE_KEY, 0, 0 }, 
{ "FPGA speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_AX3_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_AX3_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_AX3_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_AX3_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR3 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MARS_AX3_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mars MX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_MX1

char MARS_MX1_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mars MX1";
ModuleConfigProperty_t MARS_MX1_CONFIG_PROPERTIES[MARS_MX1_PROPERTY_COUNT] = {
{ "FPGA type\0", 0x08, 4, 4, 7, 0, 3, 0, 4, (ModulePropertyValueKey_t*)&MARS_MX1_FPGA_TYPE_VALUE_KEY, 0, 0 }, 
{ "FPGA speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX1_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX1_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX1_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "Current monitor equipped\0", 0x09, 1, 1, 1, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX1_CURRENT_MONITOR_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR2 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MARS_MX1_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mars MX2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_MX2

char MARS_MX2_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mars MX2";
ModuleConfigProperty_t MARS_MX2_CONFIG_PROPERTIES[MARS_MX2_PROPERTY_COUNT] = {
{ "FPGA type\0", 0x08, 4, 4, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_FPGA_TYPE_VALUE_KEY, 0, 0 }, 
{ "FPGA speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "Current monitor equipped\0", 0x09, 1, 1, 1, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_MX2_CURRENT_MONITOR_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR2 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MARS_MX2_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mars ZX2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_ZX2

char MARS_ZX2_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mars ZX2";
ModuleConfigProperty_t MARS_ZX2_CONFIG_PROPERTIES[MARS_ZX2_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX2_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX2_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX2_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX2_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR3 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MARS_ZX2_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mars ZX3
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_ZX3

char MARS_ZX3_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mars ZX3";
ModuleConfigProperty_t MARS_ZX3_CONFIG_PROPERTIES[MARS_ZX3_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX3_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX3_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX3_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX3_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MARS_ZX3_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR3 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 }, 
{ "NAND flash size (MB)\0", 0x0C, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MARS_ZX3_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury CA1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_CA1

char MERCURY_CA1_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury CA1";
ModuleConfigProperty_t MERCURY_CA1_CONFIG_PROPERTIES[MERCURY_CA1_PROPERTY_COUNT] = {
{ "FPGA type\0", 0x08, 4, 4, 7, 0, 4, 0, 5, (ModulePropertyValueKey_t*)&MERCURY_CA1_FPGA_TYPE_VALUE_KEY, 0, 0 }, 
{ "FPGA speed grade\0", 0x08, 4, 0, 3, 6, 8, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_CA1_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_CA1_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_CA1_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_CA1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "Current monitor equipped\0", 0x09, 1, 1, 1, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_CA1_CURRENT_MONITOR_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR2 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_CA1_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury KX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_KX1

char MERCURY_KX1_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury KX1";
ModuleConfigProperty_t MERCURY_KX1_CONFIG_PROPERTIES[MERCURY_KX1_PROPERTY_COUNT] = {
{ "FPGA type\0", 0x08, 4, 4, 7, 0, 5, 0, 6, (ModulePropertyValueKey_t*)&MERCURY_KX1_FPGA_TYPE_VALUE_KEY, 0, 0 }, 
{ "FPGA speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_KX1_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_KX1_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_KX1_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_KX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR3 RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 }, 
{ "Secondary DDR3 RAM size (MB)\0", 0x0C, 4, 0, 3, 0, 15, 2, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_KX1_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury SA1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_SA1

char MERCURY_SA1_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury SA1";
ModuleConfigProperty_t MERCURY_SA1_CONFIG_PROPERTIES[MERCURY_SA1_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 2, 0, 3, (ModulePropertyValueKey_t*)&MERCURY_SA1_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 6, 8, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA1_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA1_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA1_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB port count\0", 0x0A, 2, 0, 1, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "DDR3L RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 9, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_SA1_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury SA2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_SA2

char MERCURY_SA2_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury+ SA2";
ModuleConfigProperty_t MERCURY_SA2_CONFIG_PROPERTIES[MERCURY_SA2_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 0, 0, 1, (ModulePropertyValueKey_t*)&MERCURY_SA2_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 6, 8, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA2_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA2_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Fast Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Gigabit Ethernet port count\0", 0x09, 1, 3, 3, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_SA2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB 2.0 port count\0", 0x0A, 1, 1, 1, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "USB 3.0 port count\0", 0x0A, 1, 0, 0, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "DDR3L RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 9, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_SA2_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury ZX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_ZX1

char MERCURY_ZX1_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury ZX1";
ModuleConfigProperty_t MERCURY_ZX1_CONFIG_PROPERTIES[MERCURY_ZX1_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 2, 0, 3, (ModulePropertyValueKey_t*)&MERCURY_ZX1_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX1_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX1_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Gigabit Ethernet port count\0", 0x09, 2, 4, 5, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Fast Ethernet port count\0", 0x09, 2, 2, 3, 0, 2, 0, 0, NULL, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 1, 1, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 3, 0, 0, NULL, 0, 0 }, 
{ "DDR3L RAM (PS) size (MB)\0", 0x0B, 4, 4, 7, 0, 8, 8, 0, NULL, 0, 0 }, 
{ "DDR3L RAM (PL) size (MB)\0", 0x0B, 4, 0, 3, 0, 6, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0C, 4, 4, 7, 0, 7, 1, 0, NULL, 0, 0 }, 
{ "NAND flash size (MB)\0", 0x0C, 4, 0, 3, 0, 15, 8, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_ZX1_CONFIG_PROPERTIES;
#endif


//-------------------------------------------------------------------------------------------------
// Mercury ZX5
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_ZX5

char MERCURY_ZX5_MODULE_NAME[MAX_MODULE_NAME_LENGTH_CHARACTERS] = "Mercury ZX5";
ModuleConfigProperty_t MERCURY_ZX5_CONFIG_PROPERTIES[MERCURY_ZX5_PROPERTY_COUNT] = {
{ "SoC type\0", 0x08, 4, 4, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX5_SOC_TYPE_VALUE_KEY, 0, 0 }, 
{ "SoC speed grade\0", 0x08, 4, 0, 3, 1, 3, 0, 0, NULL, 0, 0 }, 
{ "Temperature grade\0", 0x09, 1, 7, 7, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX5_TEMPERATURE_GRADE_VALUE_KEY, 0, 0 }, 
{ "Power grade\0", 0x09, 1, 6, 6, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX5_POWER_GRADE_VALUE_KEY, 0, 0 }, 
{ "Ethernet port count\0", 0x09, 2, 4, 5, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "Ethernet speed\0", 0x09, 1, 3, 3, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX5_ETHERNET_SPEED_VALUE_KEY, 0, 0 }, 
{ "Real-time clock equipped\0", 0x09, 1, 2, 2, 0, 1, 0, 2, (ModulePropertyValueKey_t*)&MERCURY_ZX5_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY, 0, 0 }, 
{ "USB device port count\0", 0x0A, 2, 0, 1, 0, 1, 0, 0, NULL, 0, 0 }, 
{ "DDR3L RAM size (MB)\0", 0x0B, 4, 4, 7, 0, 15, 8, 0, NULL, 0, 0 }, 
{ "SPI flash size (MB)\0", 0x0B, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 }, 
{ "NAND flash size (MB)\0", 0x0C, 4, 0, 3, 0, 15, 1, 0, NULL, 0, 0 } };
ModuleConfigProperty_t* g_pConfigProperties = (ModuleConfigProperty_t*)&MERCURY_ZX5_CONFIG_PROPERTIES;
#endif
