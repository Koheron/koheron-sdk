/*
 *  * (C) Copyright 2013 Xilinx, Inc.
 *   * (C) Copyright 2015 Antmicro Ltd
 *    * (C) Copyright 2015 Koheron SAS
 *     *
 *      * Configuration for Koheron Alpha250
 *       * See zynq-common.h for Zynq common configs
 *        *
 *         * SPDX-License-Identifier: GPL-2.0+
 *          */
#ifndef __CONFIG_ZYNQ_ALPHA250_H
#define __CONFIG_ZYNQ_ALPHA250_H
#include <configs/zynq-common.h>
#ifdef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#define CONFIG_EXTRA_ENV_SETTINGS \
    "fdt_high=0x1E000000\0" \
      "preboot=env import -t 0xFFFFFC00" \
        "sdboot=echo Importing environment from SD... && mmcinfo && fatload mmc 0 0x2000000 uEnv.txt && env import -t 0x2000000 ${filesize} && boot"
#define CONFIG_ZYNQ_SDHCI0
/* Select Micrel PHY */
#define CONFIG_PHY_MICREL
#define PHY_ANEG_TIMEOUT  8000  /* PHY needs a longer aneg time */
/* Unselect Marvell PHY (selected by zynq-common) */
#ifdef CONFIG_PHY_MARVELL
#undef CONFIG_PHY_MARVELL
#endif
#endif /* __CONFIG_ZYNQ_ALPHA250_H */
