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
#endif

#endif

