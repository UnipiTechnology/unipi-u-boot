/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2020 Faster CZ spol. s r.o.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define ROCKCHIP_DEVICE_SETTINGS
#include <configs/rk3328_common.h>

#ifndef CONFIG_SPL_BUILD

/* add console to extra env variables */
#include <config_distro_bootcmd.h>

#undef  CFG_EXTRA_ENV_SETTINGS
#define _CFG_EXTRA_ENV_SETTINGS \
	ENV_MEM_LAYOUT_SETTINGS \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"console=ttyS2\0" \
	"boot_c_script=if test -e ${devtype} ${devnum}:${distro_bootpart} ${prefix}hwrevision.compatible; then " \
			"load ${devtype} ${devnum}:${distro_bootpart} ${scriptaddr} ${prefix}${script}; " \
			"source ${scriptaddr}; " \
		"else echo INCOMPATIBLE BOOT; " \
		"fi\0" \
	"boot_altboot=setenv boot_prefixes /altboot/ ${boot_prefixes};" \
		"usb start;" \
		"run bootcmd_usb0;" \
		"setenv bootpretryperiod 3000\0" \
	"altbootcmd=" \
		"bootcount reset;" \
		"mmc dev 0 1; mmc read ${scriptaddr} 0 8 && source ${scriptaddr};" \
		"setenv boot_scripts recover.scr ${boot_scripts};" \
		"run bootcmd\0" \
	BOOTENV

#endif

//#define CONFIG_SYS_MMC_ENV_DEV 0

#endif

