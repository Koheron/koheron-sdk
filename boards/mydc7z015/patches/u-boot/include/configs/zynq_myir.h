/*
 * (C) Copyright 2013 Xilinx, Inc.
 * (C) Copyright 2015 Antmicro Ltd
 * (C) Copyright 2017 Koheron SAS
 *
 * Configuration for MYIR MYD-C7Z015 and MYD-C7Z020
 * See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __CONFIG_ZYNQ_MYIR_H
#define __CONFIG_ZYNQ_MYIR_H
#include <configs/zynq-common.h>
#ifdef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#define CONFIG_EXTRA_ENV_SETTINGS	\
        "fdt_high=0x1E000000\0"	\
	"sdboot=echo Importing environment from SD... && mmcinfo && fatload mmc 0 0x2000000 uEnv.txt && env import -t 0x2000000 ${filesize} && boot"
#endif /* __CONFIG_ZYNQ_MYIR_H */
