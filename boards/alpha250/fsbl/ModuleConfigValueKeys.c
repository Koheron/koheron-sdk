#include "ModuleConfigConstants.h"

//-------------------------------------------------------------------------------------------------
// Mars AX3
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_AX3
 const ModulePropertyValueKey_t MARS_AX3_FPGA_TYPE_VALUE_KEY[4] = {
{1, "Xilinx Artix-7 XC7A35T\0" }, 
{2, "Xilinx Artix-7 XC7A50T\0" }, 
{3, "Xilinx Artix-7 XC7A75T\0" }, 
{4, "Xilinx Artix-7 XC7A100T\0" }
 };
 const ModulePropertyValueKey_t MARS_AX3_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MARS_AX3_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MARS_AX3_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MARS_AX3_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mars MX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_MX1
 const ModulePropertyValueKey_t MARS_MX1_FPGA_TYPE_VALUE_KEY[4] = {
{0, "Xilinx Spartan-6 XC6SLX9\0" }, 
{1, "Xilinx Spartan-6 XC6SLX16\0" }, 
{2, "Xilinx Spartan-6 XC6SLX25\0" }, 
{3, "Xilinx Spartan-6 XC6SLX45\0" }
 };
 const ModulePropertyValueKey_t MARS_MX1_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MARS_MX1_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MARS_MX1_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MARS_MX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
 const ModulePropertyValueKey_t MARS_MX1_CURRENT_MONITOR_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mars MX2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_MX2
 const ModulePropertyValueKey_t MARS_MX2_FPGA_TYPE_VALUE_KEY[2] = {
{0, "Xilinx Spartan-6 XC6SLX25T\0" }, 
{1, "Xilinx Spartan-6 XC6SLX45T\0" }
 };
 const ModulePropertyValueKey_t MARS_MX2_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MARS_MX2_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MARS_MX2_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MARS_MX2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
 const ModulePropertyValueKey_t MARS_MX2_CURRENT_MONITOR_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mars ZX2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_ZX2
 const ModulePropertyValueKey_t MARS_ZX2_SOC_TYPE_VALUE_KEY[2] = {
{0, "Xilinx Zynq-7000 XC7Z010\0" }, 
{1, "Xilinx Zynq-7020 XC7Z020\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX2_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX2_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX2_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mars ZX3
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MARS_ZX3
 const ModulePropertyValueKey_t MARS_ZX3_SOC_TYPE_VALUE_KEY[2] = {
{0, "Xilinx Zynq-7020 XC7Z020\0" }, 
{1, "Xilinx Zynq-7020 XC7Z020\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX3_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX3_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX3_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MARS_ZX3_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury CA1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_CA1
 const ModulePropertyValueKey_t MERCURY_CA1_FPGA_TYPE_VALUE_KEY[5] = {
{0, "Altera Cyclone IV E30\0" }, 
{1, "Altera Cyclone IV E40\0" }, 
{2, "Altera Cyclone IV E55\0" }, 
{3, "Altera Cyclone IV E75\0" }, 
{4, "Altera Cyclone IV E115\0" }
 };
 const ModulePropertyValueKey_t MERCURY_CA1_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_CA1_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_CA1_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MERCURY_CA1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
 const ModulePropertyValueKey_t MERCURY_CA1_CURRENT_MONITOR_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury KX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_KX1
 const ModulePropertyValueKey_t MERCURY_KX1_FPGA_TYPE_VALUE_KEY[6] = {
{0, "Xilinx Kintex-7 XC7K160T-1FBG676C\0" }, 
{1, "Xilinx Kintex-7 XC7K325T-1FBG676C\0" }, 
{2, "Xilinx Kintex-7 XC7K410T-1FBG676C\0" }, 
{3, "Xilinx Kintex-7 XC7K160T-2FFG676I\0" }, 
{4, "Xilinx Kintex-7 XC7K325T-2FFG676I\0" }, 
{5, "Xilinx Kintex-7 XC7K410T-2FFG676I\0" }
 };
 const ModulePropertyValueKey_t MERCURY_KX1_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_KX1_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_KX1_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MERCURY_KX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury SA1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_SA1
 const ModulePropertyValueKey_t MERCURY_SA1_SOC_TYPE_VALUE_KEY[3] = {
{0, "Altera Cyclone V 5CSEBA2U23\0" }, 
{1, "Altera Cyclone V 5CSXFC4C6U23\0" }, 
{2, "Altera Cyclone V 5CSXFC6C6U23\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA1_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA1_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA1_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury SA2
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_SA2
 const ModulePropertyValueKey_t MERCURY_SA2_SOC_TYPE_VALUE_KEY[1] = {
{0, "Altera Cyclone V 5CSTFD6D5F31\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA2_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA2_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_SA2_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury ZX1
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_ZX1
 const ModulePropertyValueKey_t MERCURY_ZX1_SOC_TYPE_VALUE_KEY[3] = {
{0, "Xilinx Zynq-7030 XC7Z030-2FBG676\0" }, 
{1, "Xilinx Zynq-7035 XC7Z035-2FBG676\0" }, 
{2, "Xilinx Zynq-7045 XC7Z045-2FFG676\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX1_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX1_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX1_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

//-------------------------------------------------------------------------------------------------
// Mercury ZX5
//-------------------------------------------------------------------------------------------------

#if TARGET_MODULE == MERCURY_ZX5
 const ModulePropertyValueKey_t MERCURY_ZX5_SOC_TYPE_VALUE_KEY[2] = {
{0, "Xilinx Zynq-7015 XC7Z015\0" }, 
{1, "Xilinx Zynq-7030 XC7Z030\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX5_TEMPERATURE_GRADE_VALUE_KEY[2] = {
{0, "Commercial\0" }, 
{1, "Industrial\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX5_POWER_GRADE_VALUE_KEY[2] = {
{0, "Normal\0" }, 
{1, "Low power\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX5_ETHERNET_SPEED_VALUE_KEY[2] = {
{0, "Fast\0" }, 
{1, "Gigabit\0" }
 };
 const ModulePropertyValueKey_t MERCURY_ZX5_REAL_TIME_CLOCK_EQUIPPED_VALUE_KEY[2] = {
{0, "No\0" }, 
{1, "Yes\0" }
 };
#endif

