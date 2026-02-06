// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */

#include <asm/arch/sys_proto.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <stdbool.h>
#include <mmc.h>
#include <env.h>
#include <asm/mach-imx/boot_mode.h>
#include <vsprintf.h>



/*
  variable boot_targets contains default sequence of searching devices
  ( mmc2 mmc1 mmc0 usb0 dhcp )
  prepend to it mmc1 or mmc0 in case of booting from sdcard
*/
void board_late_mmc_env_init(void)
{
	char targets[256];
	u32 dev_no;
	u32 boot_device = get_boot_device();

	char *boot_targets = env_get("boot_targets");

	/*printf("Boot dev=%d\n", boot_device);*/

	switch (boot_device) {
		case SD2_BOOT:
		case MMC2_BOOT:
				dev_no = 1;
				break;

		case SD1_BOOT:
		case MMC1_BOOT:
				dev_no = 0;
				break;

		case SD3_BOOT:
		case MMC3_BOOT:
		default:
				/* nothing to change */
				return;
	}

	/* Prepend boot_target env */
	sprintf(targets, "mmc%d %s", dev_no, boot_targets);
	env_set("boot_targets", targets);
}
